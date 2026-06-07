#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_boundary_slot_param.h"
#include "llvm_decl_authority.h"

static unsigned
llvm_function_emitted_param_count(LLVMGenCtx *ctx, ASTNode *node,
                                  const MIRRoutine *routine)
{
    unsigned count = 0;
    bool routine_has_signature = llvm_mir_routine_has_signature(routine);
    size_t param_count = routine_has_signature
        ? llvm_mir_routine_param_count(routine)
        : ast_func_param_count(node);

    for (size_t i = 0; i < param_count; i++) {
        bool is_secure = false;
        FuncParam *p = routine_has_signature
            ? llvm_mir_routine_param(routine, i)
            : ast_func_param(node, i);
        const char *param_type_name = routine_has_signature
            ? llvm_mir_routine_param_type_name(routine, i)
            : NULL;
        const char *slot_inner = NULL;
        if (p == NULL || p->name == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM function parameter requires a concrete name and type metadata");
            continue;
        }
        count++;
        slot_inner = param_type_name != NULL
            ? llvm_boundary_slot_inner_name_from_type_name(ctx,
                p,
                param_type_name,
                &is_secure)
            : llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        if (slot_inner != NULL && is_secure)
            count++;
    }
    return count;
}

static LLVMTypeRef
llvm_decl_required_implicit_self_type(LLVMGenCtx *ctx, ASTNode *func)
{
    const char *host_name = NULL;
    LLVMClassTypeEntry *cls = NULL;

    if (ctx == NULL)
        return NULL;

    host_name = llvm_current_host_class_name(ctx);
    cls = host_name != NULL ? llvm_lookup_class(ctx, host_name) : NULL;
    if (cls != NULL) {
        return cls->is_pointer_self_host
            ? LLVMPointerType(cls->struct_type, 0)
            : cls->struct_type;
    }

    llvm_set_error_at_with_hints(ctx, func,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REPORT_COMPILER_BUG,
        "LLVM implicit self parameter requires current host metadata; silent i32 placeholder is not allowed");
    return NULL;
}

static LLVMTypeRef
llvm_decl_required_param_type(LLVMGenCtx *ctx, ASTNode *func, FuncParam *param)
{
    if (ctx == NULL)
        return NULL;
    if (param != NULL && param->type != NULL)
        return ast_type_to_llvm(ctx, param->type);
    if (param != NULL && llvm_param_is_implicit_self(param))
        return llvm_decl_required_implicit_self_type(ctx, func);

    llvm_set_error_at_with_hints(ctx, func,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM function parameter requires explicit type metadata; silent i32 fallback is not allowed");
    return NULL;
}

static LLVMTypeRef
llvm_decl_required_param_type_name_first(LLVMGenCtx *ctx,
                                         ASTNode *func,
                                         FuncParam *param,
                                         const char *type_name)
{
    if (type_name != NULL) {
        LLVMTypeRef type = pergyra_type_to_llvm(ctx, type_name);
        if (type != NULL || (ctx != NULL && ctx->has_error))
            return type;
    }
    return llvm_decl_required_param_type(ctx, func, param);
}

static bool
llvm_decl_token_param_name(LLVMGenCtx *ctx, ASTNode *node,
                           char *out, size_t out_size,
                           const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;

    written = snprintf(out, out_size, "%s_token",
        param_name != NULL ? param_name : "");
    if (written >= 0 && (size_t)written < out_size)
        return true;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_SPEC_LIMIT,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
            "LLVM secure slot parameter token name is too long for '%s'",
            param_name != NULL ? param_name : "<param>");
    }
    return false;
}

/* =================================================================
 * Function declaration emission
 * ================================================================= */

static void
llvm_forward_declare_func_with_signature(ASTNode *node,
                                         const MIRRoutine *routine,
                                         LLVMGenCtx *ctx)
{
    const char *name = ast_declaration_name(node);
    bool routine_has_signature = llvm_mir_routine_has_signature(routine);
    if (routine != NULL && llvm_active_has_mir(ctx)
        && !routine_has_signature) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing function forward signature metadata for '%s'",
            name != NULL ? name : "(anonymous)");
        return;
    }
    size_t param_count = routine_has_signature
        ? llvm_mir_routine_param_count(routine)
        : ast_func_param_count(node);
    unsigned emitted_param_count =
        llvm_function_emitted_param_count(ctx, node, routine);

    /* Return type */
    LLVMTypeRef ret_type = ctx->type_void;
    const char *return_type_name = routine_has_signature
        ? llvm_mir_routine_return_type_name(routine)
        : NULL;
    ASTNode *return_type = routine_has_signature
        ? llvm_mir_routine_return_type(routine)
        : ast_func_return_type(node);
    if (return_type_name != NULL) {
        ret_type = pergyra_type_to_llvm(ctx, return_type_name);
    } else if (routine_has_signature
               && return_type != NULL
               && return_type->type != AST_EVENT_HANDLER_TYPE) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing function declaration return type-name metadata for '%s'",
            name != NULL ? name : "(anonymous)");
        return;
    } else if (return_type != NULL) {
        ret_type = ast_type_to_llvm(ctx, return_type);
    }
    if (ctx->has_error || ret_type == NULL)
        return;

    /* Parameter types */
    LLVMTypeRef *param_types = NULL;
    if (emitted_param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch,
                                       emitted_param_count * sizeof(LLVMTypeRef));
        if (param_types == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_OOM,
                PGY_CAUSE_LLVM_MEMORY_EXHAUSTED,
                PGY_FIX_REDUCE_UNIT_SIZE_OR_RAISE_LIMIT,
                "LLVM function declaration parameter allocation failed for '%s'",
                name != NULL ? name : "(anonymous)");
            return;
        }
        unsigned pidx = 0;
        for (size_t i = 0; i < param_count; i++) {
            bool is_secure = false;
            FuncParam *p = routine_has_signature
                ? llvm_mir_routine_param(routine, i)
                : ast_func_param(node, i);
            const char *param_type_name = routine_has_signature
                ? llvm_mir_routine_param_type_name(routine, i)
                : NULL;
            const char *slot_inner = NULL;
            if (p == NULL || p->name == NULL)
                continue;
            if (routine_has_signature
                && param_type_name == NULL
                && p->type != NULL
                && p->type->type != AST_EVENT_HANDLER_TYPE) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path missing function declaration parameter type-name metadata for '%s'",
                    name != NULL ? name : "(anonymous)");
                return;
            }
            LLVMTypeRef pt = llvm_decl_required_param_type_name_first(
                ctx, node, p, param_type_name);
            if (ctx->has_error || pt == NULL)
                return;
            if (param_type_name != NULL
                ? llvm_type_name_uses_pointer_self(ctx, param_type_name)
                : (p != NULL
                    && p->type != NULL
                    && ast_type_name(p->type) != NULL
                    && llvm_type_name_uses_pointer_self(ctx,
                        ast_type_name(p->type)))) {
                pt = LLVMPointerType(pt, 0);
            }
            slot_inner = param_type_name != NULL
                ? llvm_boundary_slot_inner_name_from_type_name(ctx,
                    p,
                    param_type_name,
                    &is_secure)
                : llvm_boundary_slot_inner_name(ctx, p, &is_secure);
            if (slot_inner != NULL) {
                param_types[pidx++] = LLVMPointerType(pt, 0);
                if (is_secure) {
                    param_types[pidx++] =
                        llvm_secure_token_type(ctx, slot_inner);
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

}

void
llvm_forward_declare_func(ASTNode *node, LLVMGenCtx *ctx)
{
    llvm_forward_declare_func_with_signature(node, NULL, ctx);
}

void
llvm_forward_declare_func_from_mir(const MIRRoutine *routine,
                                   ASTNode *node,
                                   LLVMGenCtx *ctx)
{
    llvm_forward_declare_func_with_signature(node, routine, ctx);
}

void
llvm_emit_func_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *name = ast_declaration_name(node);

    LLVMFuncEntry *entry = llvm_lookup_function(ctx, name);
    if (entry == NULL)
        return;

    LLVMValueRef fn = entry->fn;
    LLVMTypeRef ret_type = entry->ret_type;

    /* Save context */
    LLVMValueRef saved_fn       = ctx->current_function;
    LLVMTypeRef  saved_ret_type = ctx->current_ret_type;
    LLVMTypeRef  saved_function_ret_type = ctx->current_function_ret_type;
    const char  *saved_return_type_name = ctx->current_return_type_name;
    ASTNode     *saved_return_callable_type =
        ctx->current_return_callable_type;
    const char  *saved_within_zone_name = ctx->current_within_zone_name;
    ASTNode     *saved_func_decl = ctx->current_func_decl;
    LLVMBasicBlockRef saved_bb = LLVMGetInsertBlock(ctx->builder);
    LLVMLexicalRegistrySnapshot lexical_snapshot =
        llvm_lexical_registry_snapshot(ctx);

    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;
    ctx->current_function_ret_type = ret_type;
    ctx->current_within_zone_name = ast_func_within_zone(node);
    ctx->current_func_decl = node;
    {
        ASTNode *return_type = ast_func_return_type(node);
        ctx->current_return_type_name =
            return_type != NULL
                ? llvm_stmt_render_type_annotation_copy(ctx, return_type)
                : NULL;
        ctx->current_return_callable_type =
            return_type != NULL && return_type->type == AST_EVENT_HANDLER_TYPE
                ? return_type
                : NULL;
    }

    /* Create entry block */
    LLVMBasicBlockRef bb = LLVMAppendBasicBlockInContext(
        ctx->context, fn, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, bb);

    llvm_scope_push(ctx);

    /* Create allocas for parameters and store incoming values */
    unsigned llvm_pidx = 0;
    for (size_t i = 0; i < ast_func_param_count(node); i++) {
        FuncParam *p = ast_func_param(node, i);
        bool is_secure = false;
        if (p == NULL || p->name == NULL)
            continue;
        const char *inner = llvm_boundary_slot_inner_name(ctx, p, &is_secure);
        LLVMTypeRef pt = llvm_decl_required_param_type(ctx, node, p);
        /* For 'self' parameter in class methods, use the class struct pointer
         * type instead of the default i32. */
        if (llvm_param_is_implicit_self(p)) {
            const char *host_name = llvm_current_host_class_name(ctx);
            LLVMClassTypeEntry *cls = host_name != NULL
                ? llvm_lookup_class(ctx, host_name)
                : NULL;
            if (cls != NULL) {
                pt = cls->is_pointer_self_host
                    ? LLVMPointerType(cls->struct_type, 0)
                    : cls->struct_type;
            }
        }
        if (ctx->has_error || pt == NULL)
            goto cleanup;
        if (p != NULL
            && p->type != NULL
            && ast_type_name(p->type) != NULL
            && llvm_type_name_uses_pointer_self(ctx, ast_type_name(p->type))) {
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
                if (!llvm_decl_token_param_name(ctx, node, token_name,
                        sizeof(token_name), p->name))
                    goto cleanup;
                token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
                LLVMBuildStore(ctx->builder, LLVMGetParam(fn, llvm_pidx++), token_alloca);
                llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca, token_ty);
            }
            continue;
        }

        LLVMValueRef alloca = llvm_create_entry_alloca(ctx, pt, p->name);
        LLVMBuildStore(ctx->builder, LLVMGetParam(fn, llvm_pidx++), alloca);
        llvm_scope_declare(ctx, p->name, alloca, pt);
        llvm_register_typed_var(ctx, p->name, p->type);
    }

    llvm_decl_emit_zone_authority_check(ctx);
    if (ctx->has_error)
        goto cleanup;

    /* Emit body */
    if (ast_func_body(node) != NULL)
        llvm_emit_block(ast_func_body(node), ctx);

    /* Add implicit return if no terminator */
    if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL) {
        if (ret_type == ctx->type_void)
            LLVMBuildRetVoid(ctx->builder);
        else {
            if (!ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_CFG_MISSING_RETURN,
                    PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
                    "LLVM non-Void function '%s' reached backend without an all-path return terminator",
                    ast_declaration_name(node) != NULL
                        ? ast_declaration_name(node)
                        : "<anonymous>");
            }
            LLVMBuildUnreachable(ctx->builder);
        }
    }

cleanup:
    llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);

    /* Restore context */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;
    ctx->current_function_ret_type = saved_function_ret_type;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    ctx->current_within_zone_name = saved_within_zone_name;
    ctx->current_func_decl = saved_func_decl;

    /* Position builder back to the exact calling context. */
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
}

#endif /* PGY_LLVM_ENABLED */
