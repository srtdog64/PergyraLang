#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_boundary_slot_param.h"

static unsigned
llvm_function_emitted_param_count(LLVMGenCtx *ctx, ASTNode *node)
{
    unsigned count = 0;

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        bool is_secure = false;
        FuncParam *p = node->data.func_decl.params[i];
        if (p == NULL || p->name == NULL) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM function parameter requires a concrete name and type metadata");
            continue;
        }
        count++;
        if (llvm_boundary_slot_inner_name(ctx, p, &is_secure) != NULL && is_secure)
            count++;
    }
    return count;
}

static LLVMTypeRef
llvm_decl_implicit_self_placeholder_type(LLVMGenCtx *ctx)
{
    return ctx != NULL ? ctx->type_i32 : NULL;
}

static LLVMTypeRef
llvm_decl_required_param_type(LLVMGenCtx *ctx, ASTNode *func, FuncParam *param)
{
    if (ctx == NULL)
        return NULL;
    if (param != NULL && param->type != NULL)
        return ast_type_to_llvm(ctx, param->type);
    if (param != NULL && llvm_param_is_implicit_self(param))
        return llvm_decl_implicit_self_placeholder_type(ctx);

    llvm_set_error_at_with_hints(ctx, func,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "LLVM function parameter requires explicit type metadata; silent i32 fallback is not allowed");
    return NULL;
}

static ASTNode *
llvm_decl_find_current_host_decl(LLVMGenCtx *ctx)
{
    return llvm_current_host_decl(ctx);
}

static ASTNode *
llvm_decl_find_current_zone_decl(LLVMGenCtx *ctx)
{
    ASTNode *decl = llvm_decl_find_current_host_decl(ctx);
    if (decl != NULL && decl->type == AST_ZONE_DECL)
        return decl;
    return NULL;
}

static void
llvm_decl_zone_authority_backend_error(LLVMGenCtx *ctx, ASTNode *node,
                                       const char *zone_name,
                                       const char *subject_slot_name,
                                       const char *reason)
{
    if (ctx == NULL || ctx->has_error)
        return;

    llvm_set_error_at_with_hints(ctx, node,
        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_INSPECT_MIR_INVENTORY,
        "LLVM zone authority check could not be emitted for zone '%s' subject slot '%s': %s",
        zone_name != NULL ? zone_name : "<unknown>",
        subject_slot_name != NULL ? subject_slot_name : "<unknown>",
        reason != NULL ? reason : "missing backend metadata");
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

static void
llvm_decl_emit_zone_authority_check(LLVMGenCtx *ctx)
{
    ASTNode *zone_decl;
    ASTNode *authority;
    LLVMClassTypeEntry *zone_cls;
    LLVMVarEntry *self_var;
    LLVMFuncEntry *check_fn;
    LLVMValueRef self_value;
    LLVMValueRef field_ptr;
    LLVMValueRef participant_value;
    LLVMTypeRef field_type;
    LLVMValueRef args[4];
    int field_index;
    const char *zone_name;

    if (ctx == NULL)
        return;

    zone_decl = llvm_decl_find_current_zone_decl(ctx);
    if (zone_decl == NULL) {
        if (ctx->current_func_decl != NULL
            && ctx->current_func_decl->type == AST_FUNC_DECL
            && ctx->current_func_decl->data.func_decl.within_zone != NULL) {
            llvm_decl_zone_authority_backend_error(ctx, ctx->current_func_decl,
                ctx->current_func_decl->data.func_decl.within_zone, NULL,
                "current function declares a zone boundary but the zone declaration is missing from LLVM inventory");
        }
        return;
    }

    size_t authority_count = 0;
    ASTNode **authorities = ast_zone_authorities(zone_decl, &authority_count);
    if (authority_count == 0 || authorities == NULL || authorities[0] == NULL) {
        return;
    }

    authority = authorities[0];
    zone_name = ast_zone_name(zone_decl);
    if (authority->type != AST_ZONE_AUTHORITY
        || authority->data.zone_authority.subject_slot_name == NULL) {
        llvm_decl_zone_authority_backend_error(ctx, zone_decl, zone_name, NULL,
            "authority declaration is malformed or lacks a subject slot");
        return;
    }

    zone_cls = zone_name != NULL ? llvm_lookup_class(ctx, zone_name) : NULL;
    self_var = llvm_scope_lookup(ctx, "self");
    check_fn = llvm_lookup_function(ctx, "pgy_zone_authority_check_export");
    if (zone_cls == NULL) {
        llvm_decl_zone_authority_backend_error(ctx, zone_decl, zone_name,
            authority->data.zone_authority.subject_slot_name,
            "zone class layout is missing");
        return;
    }
    if (self_var == NULL) {
        llvm_decl_zone_authority_backend_error(ctx, ctx->current_func_decl,
            zone_name, authority->data.zone_authority.subject_slot_name,
            "implicit self binding is missing");
        return;
    }
    if (check_fn == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM zone authority check runtime export is missing: pgy_zone_authority_check_export");
        return;
    }

    field_index = llvm_class_field_index(zone_cls,
        authority->data.zone_authority.subject_slot_name);
    if (field_index < 0) {
        llvm_decl_zone_authority_backend_error(ctx, zone_decl, zone_name,
            authority->data.zone_authority.subject_slot_name,
            "authority subject slot is missing from the zone class layout");
        return;
    }

    self_value = LLVMBuildLoad2(ctx->builder, self_var->type, self_var->alloca,
        llvm_tmp_name(ctx));
    field_ptr = LLVMBuildStructGEP2(ctx->builder, zone_cls->struct_type,
        self_value, (unsigned)field_index, llvm_tmp_name(ctx));
    field_type = LLVMStructGetTypeAtIndex(zone_cls->struct_type, (unsigned)field_index);
    participant_value = LLVMBuildLoad2(ctx->builder, field_type, field_ptr,
        llvm_tmp_name(ctx));

    args[0] = LLVMBuildBitCast(ctx->builder, self_value, ctx->type_i8ptr,
        llvm_tmp_name(ctx));
    if (LLVMGetTypeKind(field_type) == LLVMPointerTypeKind) {
        args[1] = LLVMBuildBitCast(ctx->builder, participant_value, ctx->type_i8ptr,
            llvm_tmp_name(ctx));
    } else {
        args[1] = LLVMBuildBitCast(ctx->builder, field_ptr, ctx->type_i8ptr,
            llvm_tmp_name(ctx));
    }
    args[2] = LLVMBuildGlobalStringPtr(ctx->builder, zone_name,
        llvm_tmp_name(ctx));
    args[3] = LLVMBuildGlobalStringPtr(ctx->builder,
        authority->data.zone_authority.subject_slot_name, llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, check_fn->fn_type, check_fn->fn, args, 4, "");
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
    if (ctx->has_error || ret_type == NULL)
        return;

    /* Parameter types */
    LLVMTypeRef *param_types = NULL;
    if (emitted_param_count > 0) {
        param_types = pgy_arena_calloc(&ctx->scratch,
                                       emitted_param_count * sizeof(LLVMTypeRef));
        unsigned pidx = 0;
        for (size_t i = 0; i < param_count; i++) {
            bool is_secure = false;
            FuncParam *p = node->data.func_decl.params[i];
            if (p == NULL || p->name == NULL)
                continue;
            LLVMTypeRef pt = llvm_decl_required_param_type(ctx, node, p);
            if (ctx->has_error || pt == NULL)
                return;
            if (p != NULL
                && p->type != NULL
                && p->type->type == AST_TYPE
                && p->type->data.type.name != NULL
                && llvm_type_name_uses_pointer_self(ctx, p->type->data.type.name)) {
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
    ASTNode     *saved_func_decl = ctx->current_func_decl;
    int saved_slot_var_count = ctx->slot_var_count;
    int saved_view_var_count = ctx->view_var_count;
    int saved_device_slot_var_count = ctx->device_slot_var_count;
    int saved_future_var_count = ctx->future_var_count;
    int saved_channel_var_count = ctx->channel_var_count;
    int saved_var_class_count = ctx->var_class_count;
    int saved_projection_borrow_count = ctx->projection_borrow_count;
    int saved_array_var_count = ctx->array_var_count;
    int saved_list_var_count = ctx->list_var_count;
    int saved_set_var_count = ctx->set_var_count;
    int saved_queue_var_count = ctx->queue_var_count;
    int saved_map_var_count = ctx->map_var_count;
    int saved_callable_var_count = ctx->callable_var_count;

    ctx->current_function = fn;
    ctx->current_ret_type = ret_type;
    ctx->current_func_decl = node;

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
            && p->type->type == AST_TYPE
            && p->type->data.type.name != NULL
            && llvm_type_name_uses_pointer_self(ctx, p->type->data.type.name)) {
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

cleanup:
    llvm_scope_pop(ctx);

    ctx->slot_var_count = saved_slot_var_count;
    ctx->view_var_count = saved_view_var_count;
    ctx->device_slot_var_count = saved_device_slot_var_count;
    ctx->future_var_count = saved_future_var_count;
    ctx->channel_var_count = saved_channel_var_count;
    ctx->var_class_count = saved_var_class_count;
    ctx->projection_borrow_count = saved_projection_borrow_count;
    ctx->array_var_count = saved_array_var_count;
    ctx->list_var_count = saved_list_var_count;
    ctx->set_var_count = saved_set_var_count;
    ctx->queue_var_count = saved_queue_var_count;
    ctx->map_var_count = saved_map_var_count;
    ctx->callable_var_count = saved_callable_var_count;

    /* Restore context */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;
    ctx->current_func_decl = saved_func_decl;

    /* Position builder back to the calling context */
    if (saved_fn != NULL) {
        LLVMBasicBlockRef last_bb = LLVMGetLastBasicBlock(saved_fn);
        if (last_bb != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last_bb);
    }
}

#endif /* PGY_LLVM_ENABLED */
