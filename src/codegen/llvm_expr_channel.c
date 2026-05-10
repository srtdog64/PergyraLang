/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend channel expression emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <stdio.h>

static LLVMFuncEntry *
llvm_channel_required_runtime_function(LLVMGenCtx *ctx,
                                       ASTNode *node,
                                       const char *family_name,
                                       const char *callee_name,
                                       const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s builtin '%s' requires registered runtime function '%s'",
            family_name != NULL ? family_name : "runtime",
            callee_name != NULL ? callee_name : "<unknown>",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static LLVMValueRef
llvm_channel_expr_error(LLVMGenCtx *ctx, ASTNode *node, const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "%s",
            message != NULL ? message
                : "LLVM channel expression could not be lowered");
    }
    return NULL;
}

static bool
llvm_channel_format_runtime_name(char *out, size_t out_size,
                                 const char *prefix, const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || suffix == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", prefix, suffix);
    return written >= 0 && (size_t)written < out_size;
}

static LLVMVarEntry *
llvm_channel_required_binding(LLVMGenCtx *ctx, ASTNode *node,
                              ASTNode *channel, const char *operation_name,
                              const char **suffix_out)
{
    if (suffix_out != NULL)
        *suffix_out = NULL;
    if (channel == NULL || channel->type != AST_IDENTIFIER
        || channel->data.identifier.name == NULL) {
        llvm_expr_set_missing_type_error(ctx, node, operation_name);
        return NULL;
    }

    const char *name = channel->data.identifier.name;
    LLVMVarEntry *ch_var = llvm_scope_lookup(ctx, name);
    const char *inner = llvm_lookup_channel_inner(ctx, name);
    if (inner == NULL || inner[0] == '\0') {
        llvm_expr_set_missing_type_error(ctx, node, operation_name);
        return NULL;
    }
    if (ch_var == NULL) {
        llvm_set_error_at_with_hints(ctx, channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s requires registered Channel<T> local '%s'",
            operation_name, name);
        return NULL;
    }

    if (suffix_out != NULL)
        *suffix_out = inner;
    return ch_var;
}

LLVMValueRef
llvm_emit_channel_send_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *suffix = NULL;
    LLVMVarEntry *ch_var = llvm_channel_required_binding(ctx, node,
        node->data.channel_send.channel, "channel send expression", &suffix);
    if (ch_var == NULL)
        return NULL;

    LLVMValueRef val = llvm_emit_expression(node->data.channel_send.value, ctx);
    char fname[128];
    if (!llvm_channel_format_runtime_name(fname, sizeof(fname),
            "pgy_channel_send", suffix)) {
        return llvm_channel_expr_error(ctx, node,
            "LLVM channel send expression runtime function name is too long");
    }
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
    if (fn == NULL) {
        llvm_channel_required_runtime_function(ctx, node,
            "channel send expression", "ChannelSend", fname);
        return NULL;
    }
    if (val != NULL) {
        LLVMValueRef args[] = { ch_var->alloca, val };
        return LLVMBuildCall2(ctx->builder, fn->fn_type,
            fn->fn, args, 2, llvm_tmp_name(ctx));
    }
    return llvm_channel_expr_error(ctx, node,
        "LLVM channel send expression could not lower value expression");
}

LLVMValueRef
llvm_emit_channel_recv_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    const char *suffix = NULL;
    LLVMVarEntry *ch_var = llvm_channel_required_binding(ctx, node,
        node->data.channel_recv.channel, "channel receive expression", &suffix);
    if (ch_var == NULL)
        return NULL;

    char fname[128];
    if (!llvm_channel_format_runtime_name(fname, sizeof(fname),
            "pgy_channel_recv_val", suffix)) {
        return llvm_channel_expr_error(ctx, node,
            "LLVM channel receive expression runtime function name is too long");
    }
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
    if (fn == NULL) {
        llvm_channel_required_runtime_function(ctx, node,
            "channel receive expression", "ChannelRecv", fname);
        return NULL;
    }
    LLVMValueRef args[] = { ch_var->alloca };
    return LLVMBuildCall2(ctx->builder, fn->fn_type,
        fn->fn, args, 1, llvm_tmp_name(ctx));
}

#endif /* PGY_LLVM_ENABLED */
