/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM channel target resolution.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"
#include "llvm_inventory_decl_lookup.h"

#include <string.h>

static bool
llvm_channel_target_error(LLVMGenCtx *ctx, ASTNode *node,
                          const char *operation_name,
                          const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s %s",
            operation_name != NULL ? operation_name : "channel operation",
            message != NULL ? message : "requires concrete Channel<T> metadata");
    }
    return false;
}

static const char *
llvm_channel_current_host_field_type_name(LLVMGenCtx *ctx,
                                          const char *field_name,
                                          const char *operation_name)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    const char *host_name = llvm_current_host_class_name(ctx);
    LLVMHostedFieldView field_view;
    size_t field_index = 0;

    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL
        || host_name == NULL || field_name == NULL)
        return NULL;

    field_view = llvm_hosted_class_field_view_from_decl(
        ctx, host_name, host_decl);
    if (llvm_hosted_field_view_missing_mir_metadata(&field_view)) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing channel class-field metadata for '%s'",
            host_name);
        return NULL;
    }
    if (!llvm_hosted_field_view_find_index(
            &field_view, field_name, &field_index)) {
        return NULL;
    }
    (void)operation_name;
    if (field_view.uses_mir_metadata) {
        const char *type_name =
            llvm_hosted_field_view_type_name(&field_view, field_index);
        if (type_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing channel field type-name metadata for '%s.%s'",
                host_name,
                field_name);
        }
        return type_name;
    }

    {
        ASTNode *field_type =
            llvm_hosted_field_view_type(&field_view, field_index);
        return llvm_keep_rendered_persistent(ctx,
            llvm_render_type_name_in_ctx(ctx, field_type),
            "out of memory copying LLVM Channel<T> compatibility field type");
    }
}

static const char *
llvm_channel_field_inner_type_from_name(LLVMGenCtx *ctx, ASTNode *node,
                                        const char *field_type_name,
                                        const char *operation_name)
{
    char inner_buf[256];

    if (field_type_name == NULL
        || pgy_classify_type(field_type_name) != PGY_TK_CHANNEL) {
        llvm_channel_target_error(ctx, node, operation_name,
            "requires a Channel<T> local or current-host field");
        return NULL;
    }

    if (!llvm_constructed_arg_name_copy(field_type_name, 0,
            inner_buf, sizeof(inner_buf))
        || inner_buf[0] == '\0') {
        llvm_channel_target_error(ctx, node, operation_name,
            "requires concrete Channel<T> field metadata");
        return NULL;
    }

    {
        const char *inner = pgy_arena_strdup(&ctx->persistent, inner_buf);
        if (inner == NULL && ctx != NULL && !ctx->has_error)
            llvm_set_error(ctx,
                "out of memory copying LLVM Channel<T> field inner type");
        return inner;
    }
}

const char *
llvm_resolve_channel_target_inner(LLVMGenCtx *ctx, ASTNode *node,
                                  ASTNode *channel,
                                  const char *operation_name)
{
    const char *name;
    const char *inner;

    if (ctx == NULL || channel == NULL || channel->type != AST_IDENTIFIER)
        return NULL;

    name = ast_identifier_name(channel);
    if (name == NULL)
        return NULL;

    inner = llvm_lookup_channel_inner(ctx, name);
    if (inner != NULL && inner[0] != '\0')
        return inner;
    if (llvm_scope_contains(ctx, name))
        return NULL;

    {
        const char *host_name = llvm_current_host_class_name(ctx);
        LLVMClassTypeEntry *host_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        const char *field_type_name = llvm_channel_current_host_field_type_name(
            ctx, name, operation_name);
        int field_idx = host_cls != NULL
            ? llvm_class_field_index(host_cls, name) : -1;

        if (host_cls == NULL || field_type_name == NULL || field_idx < 0)
            return NULL;
        return llvm_channel_field_inner_type_from_name(
            ctx, node, field_type_name, operation_name);
    }
}

bool
llvm_resolve_channel_target(LLVMGenCtx *ctx, ASTNode *node,
                            ASTNode *channel,
                            const char *operation_name,
                            LLVMChannelTarget *out)
{
    const char *name;
    const char *inner;
    LLVMVarEntry local;
    bool has_local;

    if (out != NULL)
        memset(out, 0, sizeof(*out));
    if (ctx == NULL || out == NULL)
        return false;
    if (channel == NULL || channel->type != AST_IDENTIFIER
        || ast_identifier_name(channel) == NULL) {
        llvm_expr_set_missing_type_error(ctx, node, operation_name);
        return false;
    }

    name = ast_identifier_name(channel);
    inner = llvm_lookup_channel_inner(ctx, name);
    has_local = llvm_scope_lookup_snapshot(ctx, name, &local);
    if (inner != NULL && inner[0] != '\0') {
        if (!has_local) {
            llvm_channel_target_error(ctx, channel, operation_name,
                "requires registered Channel<T> local storage");
            return false;
        }
        out->name = name;
        out->inner = inner;
        out->ptr = local.alloca;
        return true;
    }
    if (has_local) {
        llvm_expr_set_missing_type_error(ctx, node, operation_name);
        return false;
    }

    {
        const char *host_name = llvm_current_host_class_name(ctx);
        LLVMClassTypeEntry *host_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        const char *field_type_name = llvm_channel_current_host_field_type_name(
            ctx, name, operation_name);
        int field_idx = host_cls != NULL
            ? llvm_class_field_index(host_cls, name) : -1;
        LLVMValueRef self_ptr = NULL;

        if (host_cls == NULL || field_type_name == NULL || field_idx < 0) {
            llvm_expr_set_missing_type_error(ctx, node, operation_name);
            return false;
        }

        inner = llvm_channel_field_inner_type_from_name(
            ctx, channel, field_type_name, operation_name);
        if (inner == NULL || inner[0] == '\0')
            return false;

        self_ptr = llvm_current_self_base_ptr(ctx, host_cls);
        if (self_ptr == NULL) {
            llvm_channel_target_error(ctx, channel, operation_name,
                "requires current-host self storage for Channel<T> field access");
            return false;
        }

        out->name = name;
        out->inner = inner;
        out->ptr = LLVMBuildStructGEP2(ctx->builder, host_cls->struct_type,
            self_ptr, (unsigned)field_idx, llvm_tmp_name(ctx));
        return true;
    }
}

#endif /* PGY_LLVM_ENABLED */
