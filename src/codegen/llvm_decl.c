#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

static const char *
llvm_boundary_slot_inner_name(LLVMGenCtx *ctx, FuncParam *param, bool *is_secure_out)
{
    const char *type_name;
    const char *inner_name;
    GenericParams *generic_args;
    LLVMClassTypeEntry *entry;

    if (is_secure_out != NULL)
        *is_secure_out = false;
    if (ctx == NULL || param == NULL || param->type == NULL
        || param->type->type != AST_TYPE
        || param->type->data.type.name == NULL)
        return NULL;
    if (param->mode != PARAM_MODE_OWN && param->mode != PARAM_MODE_REF)
        return NULL;

    type_name = param->type->data.type.name;
    if (strcmp(type_name, "Slot") != 0 && strcmp(type_name, "SecureSlot") != 0)
        return NULL;

    generic_args = param->type->data.type.generic_args;
    if (generic_args == NULL || generic_args->count == 0
        || generic_args->params == NULL || generic_args->params[0] == NULL)
        return NULL;

    inner_name = generic_args->params[0]->name;
    if (inner_name == NULL && generic_args->params[0]->constraint != NULL
        && generic_args->params[0]->constraint->type == AST_TYPE) {
        inner_name = generic_args->params[0]->constraint->data.type.name;
    }
    if (inner_name == NULL)
        return NULL;

    entry = llvm_lookup_class(ctx, inner_name);
    if (entry == NULL || !entry->is_subject)
        return NULL;

    if (is_secure_out != NULL)
        *is_secure_out = (strcmp(type_name, "SecureSlot") == 0);
    return inner_name;
}

static unsigned
llvm_function_emitted_param_count(LLVMGenCtx *ctx, ASTNode *node)
{
    unsigned count = 0;

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        bool is_secure = false;
        FuncParam *p = node->data.func_decl.params[i];
        count++;
        if (llvm_boundary_slot_inner_name(ctx, p, &is_secure) != NULL && is_secure)
            count++;
    }
    return count;
}

static bool
llvm_decl_nominal_uses_pointer_self(LLVMGenCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return false;

    {
        LLVMClassTypeEntry *cls = llvm_lookup_class(ctx, type_name);
        if (cls != NULL && cls->is_pointer_self_host)
            return true;
    }

    if (ctx->hir == NULL)
        return false;

    for (size_t i = 0; i < ctx->hir->item_count; i++) {
        ASTNode *stmt = ctx->hir->items[i].ast;
        if (stmt == NULL)
            continue;
        switch (stmt->type) {
        case AST_CLASS_DECL:
            if (stmt->data.class_decl.name != NULL
                && strcmp(stmt->data.class_decl.name, type_name) == 0
                && stmt->data.class_decl.nominal_kind == NOMINAL_DECL_VESSEL)
                return true;
            break;
        case AST_PARTY_DECL:
            if (stmt->data.party_decl.name != NULL
                && strcmp(stmt->data.party_decl.name, type_name) == 0)
                return true;
            break;
        case AST_SYSTEMIC_DECL:
            if (stmt->data.systemic_decl.name != NULL
                && strcmp(stmt->data.systemic_decl.name, type_name) == 0)
                return true;
            break;
        case AST_WORLD_DECL:
            if (stmt->data.world_decl.name != NULL
                && strcmp(stmt->data.world_decl.name, type_name) == 0)
                return true;
            break;
        case AST_RELATION_DECL:
            if (stmt->data.relation_decl.name != NULL
                && strcmp(stmt->data.relation_decl.name, type_name) == 0)
                return true;
            break;
        case AST_EFFECT_DECL:
            if (stmt->data.effect_decl.name != NULL
                && strcmp(stmt->data.effect_decl.name, type_name) == 0)
                return true;
            break;
        case AST_ZONE_DECL:
            if (stmt->data.zone_decl.name != NULL
                && strcmp(stmt->data.zone_decl.name, type_name) == 0)
                return true;
            break;
        default:
            break;
        }
    }

    return false;
}

/* =================================================================
 * Function declaration emission
 * ================================================================= */

void
llvm_forward_declare_func(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.func_decl.name;
    size_t param_count = node->data.func_decl.param_count;
    unsigned emitted_param_count = llvm_function_emitted_param_count(ctx, node);

    /* Return type */
    LLVMTypeRef ret_type = ctx->type_void;
    if (node->data.func_decl.return_type != NULL)
        ret_type = ast_type_to_llvm(ctx, node->data.func_decl.return_type);

    /* Parameter types */
    LLVMTypeRef *param_types = NULL;
    if (emitted_param_count > 0) {
        param_types = calloc(emitted_param_count, sizeof(LLVMTypeRef));
        unsigned pidx = 0;
        for (size_t i = 0; i < param_count; i++) {
            bool is_secure = false;
            FuncParam *p = node->data.func_decl.params[i];
            LLVMTypeRef pt = (p->type != NULL)
                ? ast_type_to_llvm(ctx, p->type)
                : ctx->type_i32;
            if (p->type != NULL
                && p->type->type == AST_TYPE
                && p->type->data.type.name != NULL
                && llvm_decl_nominal_uses_pointer_self(ctx, p->type->data.type.name)) {
                pt = LLVMPointerType(pt, 0);
            }
            if (llvm_boundary_slot_inner_name(ctx, p, &is_secure) != NULL) {
                param_types[pidx++] = LLVMPointerType(pt, 0);
                if (is_secure) {
                    const char *inner = llvm_boundary_slot_inner_name(ctx, p, NULL);
                    param_types[pidx++] = llvm_secure_token_type(ctx, inner);
                }
            } else {
                param_types[pidx++] = pt;
            }
        }
    }

    LLVMTypeRef fn_type = LLVMFunctionType(ret_type, param_types,
                                            emitted_param_count, 0);
    LLVMValueRef fn = LLVMAddFunction(ctx->module, name, fn_type);
    llvm_register_function(ctx, name, fn, fn_type, ret_type);

    free(param_types);
}

void
llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = node->data.func_decl.name;

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry == NULL)
        return;

    LLVMValueRef fn = entry->fn;
    LLVMTypeRef ret_type = entry->ret_type;

    /* Save context */
    LLVMValueRef saved_fn       = ctx->current_function;
    LLVMTypeRef  saved_ret_type = ctx->current_ret_type;

    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;

    /* Create entry block */
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);

    llvm_scope_push(ctx);

    /* Create allocas for parameters and store incoming values */
    unsigned llvm_pidx = 0;
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        bool is_secure = false;
        const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        LLVMTypeRef pt = (p->type != NULL)
            ? ast_type_to_llvm(ctx, p->type)
            : ctx->type_i32;
        if (p->type != NULL
            && p->type->type == AST_TYPE
            && p->type->data.type.name != NULL
            && llvm_decl_nominal_uses_pointer_self(ctx, p->type->data.type.name)) {
            pt = LLVMPointerType(pt, 0);
        }
        if (inner != NULL) {
            LLVMValueRef slot_ptr = LLVMGetParam(fn, llvm_pidx++);
            llvm_scope_declare(ctx, p->name, slot_ptr, pt);
            llvm_register_slot_var(ctx, p->name, inner, is_secure);
            if (is_secure) {
                LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
                char token_name[256];
                LLVMValueRef token_alloca;
                snprintf(token_name, sizeof(token_name), "%s_token", p->name);
                token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, llvm_pidx++), token_alloca);
                llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
            }
            continue;
        }

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, llvm_pidx++), alloca);
        llvm_scope_declare(ctx, p->name, alloca, pt);

        if (p->type != NULL && p->type->type == AST_TYPE
            && p->type->data.type.name != NULL
            && llvm_lookup_class(ctx, p->type->data.type.name) != NULL) {
            llvm_register_var_class(ctx, p->name, p->type->data.type.name);
        }
    }

    /* Emit body */
    if (node->data.func_decl.body != NULL)
        llvm_emit_block(node->data.func_decl.body, ctx);

    /* Add implicit return if no terminator */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else
            LLVMBuildRet(ctx->builder,
                          LLVMConstInt(ret_type, 0, 0));
    }

    llvm_scope_pop(ctx);

    /* Restore context */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;

    /* Position builder back to the calling context */
    if (saved_fn != NULL) {
        LLVMBasicBlockRef last_bb = LLVMGetLastBasicBlock(saved_fn);
        if (last_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last_bb);
    }
}

#endif /* PGY_LLVM_ENABLED */
