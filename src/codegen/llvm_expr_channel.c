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

LLVMValueRef
llvm_emit_channel_send_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMVarEntry *ch_var = NULL;
    const char *suffix = NULL;
    if (node->data.channel_send.channel != NULL
        && node->data.channel_send.channel->type == AST_IDENTIFIER) {
        const char *name = node->data.channel_send.channel->data.identifier.name;
        ch_var = llvm_scope_lookup(ctx, name);
        {
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (inner != NULL)
                suffix = inner;
        }
    }
    if (suffix == NULL || suffix[0] == '\0') {
        llvm_expr_set_missing_type_error(ctx, node,
            "channel send expression");
        return LLVMConstInt(ctx->type_i1, 0, 0);
    }
    if (ch_var != NULL) {
        LLVMValueRef val = llvm_emit_expression(
            node->data.channel_send.value, ctx);
        char fname[128];
        snprintf(fname, sizeof(fname), "pgy_channel_send_%s", suffix);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
        if (fn == NULL) {
            llvm_channel_required_runtime_function(ctx, node,
                "channel send expression", "ChannelSend", fname);
            return LLVMConstInt(ctx->type_i1, 0, 0);
        }
        if (val != NULL) {
            LLVMValueRef args[] = { ch_var->alloca, val };
            return LLVMBuildCall2(ctx->builder, fn->fn_type,
                fn->fn, args, 2, llvm_tmp_name(ctx));
        }
    }
    return LLVMConstInt(ctx->type_i1, 0, 0);
}

LLVMValueRef
llvm_emit_channel_recv_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMVarEntry *ch_var = NULL;
    const char *suffix = NULL;
    if (node->data.channel_recv.channel != NULL
        && node->data.channel_recv.channel->type == AST_IDENTIFIER) {
        const char *name = node->data.channel_recv.channel->data.identifier.name;
        ch_var = llvm_scope_lookup(ctx, name);
        {
            const char *inner = llvm_lookup_channel_inner(ctx, name);
            if (inner != NULL)
                suffix = inner;
        }
    }
    if (suffix == NULL || suffix[0] == '\0') {
        llvm_expr_set_missing_type_error(ctx, node,
            "channel receive expression");
        return LLVMConstInt(ctx->type_i32, 0, 0);
    }
    if (ch_var != NULL) {
        char fname[128];
        snprintf(fname, sizeof(fname), "pgy_channel_recv_val_%s", suffix);
        LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
        if (fn == NULL) {
            llvm_channel_required_runtime_function(ctx, node,
                "channel receive expression", "ChannelRecv", fname);
            return LLVMConstInt(ctx->type_i32, 0, 0);
        }
        {
            LLVMValueRef args[] = { ch_var->alloca };
            return LLVMBuildCall2(ctx->builder, fn->fn_type,
                fn->fn, args, 1, llvm_tmp_name(ctx));
        }
    }
    return LLVMConstInt(ctx->type_i32, 0, 0);
}

#endif /* PGY_LLVM_ENABLED */
