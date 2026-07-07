/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend channel expression emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "codegen_channel_runtime_abi.h"
#include "llvm_internal.h"
#include "../common/execution_lane_kind.h"
#include "../parser/ast_api.h"

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

LLVMValueRef
llvm_emit_channel_send_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMChannelTarget target;
    if (!llvm_resolve_channel_target(ctx, node, ast_channel_send_channel(node),
            "channel send expression", &target))
        return NULL;

    LLVMValueRef val = llvm_emit_expression(ast_channel_send_value(node), ctx);
    char fname[128];
    if (!pgy_lane_channel_runtime_name(fname, sizeof(fname),
            "send", target.inner)) {
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
        LLVMValueRef args[] = {
            LLVMConstInt(ctx->type_i32, PGY_LANE_PINNED_ZONE, 0),
            target.ptr,
            val
        };
        return LLVMBuildCall2(ctx->builder, fn->fn_type,
            fn->fn, args, 3, llvm_tmp_name(ctx));
    }
    return llvm_channel_expr_error(ctx, node,
        "LLVM channel send expression could not lower value expression");
}

LLVMValueRef
llvm_emit_channel_recv_expr(ASTNode *node, LLVMGenCtx *ctx)
{
    LLVMChannelTarget target;
    if (!llvm_resolve_channel_target(ctx, node, ast_channel_recv_channel(node),
            "channel receive expression", &target))
        return NULL;

    char fname[128];
    if (!pgy_lane_channel_runtime_name(fname, sizeof(fname),
            "recv_val", target.inner)) {
        return llvm_channel_expr_error(ctx, node,
            "LLVM channel receive expression runtime function name is too long");
    }
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, fname);
    if (fn == NULL) {
        llvm_channel_required_runtime_function(ctx, node,
            "channel receive expression", "ChannelRecv", fname);
        return NULL;
    }
    LLVMValueRef args[] = {
        LLVMConstInt(ctx->type_i32, PGY_LANE_PINNED_ZONE, 0),
        target.ptr
    };
    return LLVMBuildCall2(ctx->builder, fn->fn_type,
        fn->fn, args, 2, llvm_tmp_name(ctx));
}

#endif /* PGY_LLVM_ENABLED */
