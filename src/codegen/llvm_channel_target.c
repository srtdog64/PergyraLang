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

static ClassField *
llvm_channel_current_host_field(LLVMGenCtx *ctx, const char *field_name)
{
    ASTNode *host_decl = llvm_current_host_decl(ctx);
    size_t field_count = 0;
    ClassField **fields;

    if (host_decl == NULL || host_decl->type != AST_CLASS_DECL
        || field_name == NULL)
        return NULL;

    fields = ast_class_fields(host_decl, &field_count);
    for (size_t i = 0; fields != NULL && i < field_count; i++) {
        ClassField *field = fields[i];
        if (field != NULL && field->name != NULL
            && strcmp(field->name, field_name) == 0)
            return field;
    }
    return NULL;
}

static const char *
llvm_channel_field_inner_type(LLVMGenCtx *ctx, ASTNode *node,
                              ClassField *field,
                              const char *operation_name)
{
    GenericParams *args;
    GenericParam *arg0;
    ASTNode *arg0_type;

    if (field == NULL || field->type == NULL
        || ast_type_name(field->type) == NULL
        || strcmp(ast_type_name(field->type), "Channel") != 0) {
        llvm_channel_target_error(ctx, node, operation_name,
            "requires a Channel<T> local or current-host field");
        return NULL;
    }

    args = ast_type_generic_args(field->type);
    arg0 = ast_generic_param_at(args, 0);
    arg0_type = ast_generic_param_constraint(arg0);
    if (arg0_type == NULL) {
        llvm_channel_target_error(ctx, node, operation_name,
            "requires concrete Channel<T> field metadata");
        return NULL;
    }

    return llvm_keep_rendered_persistent(ctx,
        llvm_render_type_name_in_ctx(ctx, arg0_type),
        "out of memory copying LLVM Channel<T> field inner type");
}

bool
llvm_resolve_channel_target(LLVMGenCtx *ctx, ASTNode *node,
                            ASTNode *channel,
                            const char *operation_name,
                            LLVMChannelTarget *out)
{
    const char *name;
    const char *inner;
    LLVMVarEntry *local;

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
    local = llvm_scope_lookup(ctx, name);
    if (inner != NULL && inner[0] != '\0') {
        if (local == NULL) {
            llvm_channel_target_error(ctx, channel, operation_name,
                "requires registered Channel<T> local storage");
            return false;
        }
        out->name = name;
        out->inner = inner;
        out->ptr = local->alloca;
        return true;
    }
    if (local != NULL) {
        llvm_expr_set_missing_type_error(ctx, node, operation_name);
        return false;
    }

    {
        const char *host_name = llvm_current_host_class_name(ctx);
        LLVMClassTypeEntry *host_cls = host_name != NULL
            ? llvm_lookup_class(ctx, host_name) : NULL;
        ClassField *field = llvm_channel_current_host_field(ctx, name);
        int field_idx = host_cls != NULL
            ? llvm_class_field_index(host_cls, name) : -1;
        LLVMValueRef self_ptr = NULL;

        if (host_cls == NULL || field == NULL || field_idx < 0) {
            llvm_expr_set_missing_type_error(ctx, node, operation_name);
            return false;
        }

        inner = llvm_channel_field_inner_type(ctx, channel, field,
            operation_name);
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
