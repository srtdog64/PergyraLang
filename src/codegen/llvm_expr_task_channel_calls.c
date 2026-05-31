/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_task_channel_calls.h"
#include "llvm_expr_task_channel_policy.h"

#include <stdio.h>

#include "llvm_internal_api.h"
#include "llvm_expr_task_calls.h"

static const char *
llvm_required_channel_inner(LLVMGenCtx *ctx, ASTNode *node,
                            const char *callee_name,
                            const char *channel_name,
                            const char *inner)
{
    if (inner != NULL && inner[0] != '\0')
        return inner;

    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s requires concrete Channel<T> metadata for '%s'",
            callee_name != NULL ? callee_name : "channel operation",
            channel_name != NULL ? channel_name : "<channel>");
    }
    return NULL;
}

static LLVMFuncEntry *
llvm_required_channel_function(LLVMGenCtx *ctx, ASTNode *node,
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
            "LLVM %s requires registered runtime function '%s'",
            callee_name != NULL ? callee_name : "channel operation",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

static LLVMValueRef
llvm_task_channel_error(LLVMGenCtx *ctx, ASTNode *node,
                        const char *callee_name,
                        const char *message)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s %s",
            callee_name != NULL ? callee_name : "task/channel operation",
            message != NULL ? message : "could not be lowered");
    }
    return NULL;
}

static bool
llvm_task_channel_format_runtime_name(char *out, size_t out_size,
                                      const char *prefix, const char *inner)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL || inner == NULL)
        return false;
    written = snprintf(out, out_size, "%s_%s", prefix, inner);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_task_channel_format_op_runtime_name(char *out, size_t out_size,
                                         const char *op, const char *inner)
{
    int written;

    if (out == NULL || out_size == 0 || op == NULL || inner == NULL)
        return false;
    written = snprintf(out, out_size, "pgy_channel_%s_%s", op, inner);
    return written >= 0 && (size_t)written < out_size;
}

static bool
llvm_required_channel_var(LLVMGenCtx *ctx, ASTNode *node,
                          const char *callee_name,
                          ASTNode **out_channel,
                          LLVMChannelTarget *out_target)
{
    ASTNode *channel;
    LLVMChannelTarget target;

    if (node == NULL || ast_call_arg_count(node) == 0)
        return false;

    channel = ast_call_argument(node, 0);
    if (!llvm_resolve_channel_target(ctx, channel, channel, callee_name,
            &target))
        return false;
    if (llvm_required_channel_inner(ctx, channel, callee_name, target.name,
            target.inner) == NULL)
        return false;

    if (out_channel != NULL)
        *out_channel = channel;
    if (out_target != NULL)
        *out_target = target;
    return true;
}

LLVMValueRef
llvm_emit_task_channel_call(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{
    size_t argc = ast_call_arg_count(node);
    LLVMTaskChannelOp op;

    if (llvm_is_task_runtime_builtin_name(callee_name))
        return llvm_emit_task_runtime_call(node, ctx, callee_name);

    op = llvm_task_channel_op_lookup(callee_name, argc);

    if (op == LLVM_TASK_CHANNEL_OP_CLOSE) {
        ASTNode *channel = NULL;
        LLVMChannelTarget target;
        char fname[128];
        LLVMFuncEntry *fn;

        if (!llvm_required_channel_var(ctx, node, callee_name,
                &channel, &target))
            return NULL;
        if (!llvm_task_channel_format_runtime_name(fname, sizeof(fname),
                "pgy_channel_close", target.inner)) {
            return llvm_task_channel_error(ctx, channel, callee_name,
                "runtime function name is too long");
        }
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { target.ptr };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
    }

    if (op == LLVM_TASK_CHANNEL_OP_TRY_SEND) {
        ASTNode *channel = NULL;
        LLVMChannelTarget target;
        char fname[128];
        LLVMValueRef val;
        LLVMFuncEntry *fn;

        if (!llvm_required_channel_var(ctx, node, callee_name,
                &channel, &target))
            return NULL;
        val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (val == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower send value expression");
        if (!llvm_task_channel_format_runtime_name(fname, sizeof(fname),
                "pgy_channel_try_send", target.inner)) {
            return llvm_task_channel_error(ctx, channel, callee_name,
                "runtime function name is too long");
        }
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { target.ptr, val };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 2, llvm_tmp_name(ctx));
    }

    if (op == LLVM_TASK_CHANNEL_OP_TRY_SEND_STATUS) {
        ASTNode *channel = NULL;
        LLVMChannelTarget target;
        char closed_name[128];
        char send_name[128];
        LLVMValueRef val;
        LLVMFuncEntry *closed_fn;
        LLVMFuncEntry *send_fn;

        if (!llvm_required_channel_var(ctx, node, callee_name,
                &channel, &target))
            return NULL;
        val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (val == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower send status value expression");
        if (!llvm_task_channel_format_runtime_name(closed_name,
                sizeof(closed_name), "pgy_channel_closed", target.inner)
            || !llvm_task_channel_format_runtime_name(send_name,
                sizeof(send_name), "pgy_channel_try_send", target.inner)) {
            return llvm_task_channel_error(ctx, channel, callee_name,
                "runtime function name is too long");
        }
        closed_fn = llvm_required_channel_function(ctx, channel, callee_name, closed_name);
        send_fn = llvm_required_channel_function(ctx, channel, callee_name, send_name);
        if (closed_fn == NULL || send_fn == NULL)
            return NULL;

        LLVMValueRef closed_args[] = { target.ptr };
        LLVMValueRef send_args[] = { target.ptr, val };
        LLVMValueRef closed = LLVMBuildCall2(ctx->builder, closed_fn->fn_type,
            closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
            send_fn->fn, send_args, 2, llvm_tmp_name(ctx));
        LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
            llvm_tmp_name(ctx));
        return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
    }

    if (op == LLVM_TASK_CHANNEL_OP_SEND_TIMEOUT) {
        ASTNode *channel = NULL;
        LLVMChannelTarget target;
        char fname[128];
        LLVMValueRef val;
        LLVMValueRef timeout;
        LLVMFuncEntry *fn;

        if (!llvm_required_channel_var(ctx, node, callee_name,
                &channel, &target))
            return NULL;
        val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        timeout = llvm_emit_expression(ast_call_argument(node, 2), ctx);
        if (val == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower timeout send value expression");
        if (timeout == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower timeout expression");
        if (LLVMTypeOf(timeout) != ctx->type_i64) {
            timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                ctx->type_i64, llvm_tmp_name(ctx));
        }
        if (!llvm_task_channel_format_runtime_name(fname, sizeof(fname),
                "pgy_channel_send_timeout", target.inner)) {
            return llvm_task_channel_error(ctx, channel, callee_name,
                "runtime function name is too long");
        }
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { target.ptr, val, timeout };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 3, llvm_tmp_name(ctx));
    }

    if (op == LLVM_TASK_CHANNEL_OP_SEND_TIMEOUT_STATUS) {
        ASTNode *channel = NULL;
        LLVMChannelTarget target;
        char closed_name[128];
        char send_name[128];
        LLVMValueRef val;
        LLVMValueRef timeout;
        LLVMFuncEntry *closed_fn;
        LLVMFuncEntry *send_fn;

        if (!llvm_required_channel_var(ctx, node, callee_name,
                &channel, &target))
            return NULL;
        val = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        timeout = llvm_emit_expression(ast_call_argument(node, 2), ctx);
        if (val == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower timeout send status value expression");
        if (timeout == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower timeout status expression");
        if (LLVMTypeOf(timeout) != ctx->type_i64) {
            timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                ctx->type_i64, llvm_tmp_name(ctx));
        }
        if (!llvm_task_channel_format_runtime_name(closed_name,
                sizeof(closed_name), "pgy_channel_closed", target.inner)
            || !llvm_task_channel_format_runtime_name(send_name,
                sizeof(send_name), "pgy_channel_send_timeout", target.inner)) {
            return llvm_task_channel_error(ctx, channel, callee_name,
                "runtime function name is too long");
        }
        closed_fn = llvm_required_channel_function(ctx, channel, callee_name, closed_name);
        send_fn = llvm_required_channel_function(ctx, channel, callee_name, send_name);
        if (closed_fn == NULL || send_fn == NULL)
            return NULL;

        LLVMValueRef closed_args[] = { target.ptr };
        LLVMValueRef send_args[] = { target.ptr, val, timeout };
        LLVMValueRef closed_before = LLVMBuildCall2(ctx->builder,
            closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
            send_fn->fn, send_args, 3, llvm_tmp_name(ctx));
        LLVMValueRef closed_after = LLVMBuildCall2(ctx->builder,
            closed_fn->fn_type, closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef failed = LLVMBuildNot(ctx->builder, ok, llvm_tmp_name(ctx));
        LLVMValueRef failed_and_closed = LLVMBuildAnd(ctx->builder, failed,
            closed_after, llvm_tmp_name(ctx));
        LLVMValueRef closed = LLVMBuildOr(ctx->builder, closed_before,
            failed_and_closed, llvm_tmp_name(ctx));
        LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
            llvm_tmp_name(ctx));
        return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
    }

    if (op == LLVM_TASK_CHANNEL_OP_TRY_RECV
        || op == LLVM_TASK_CHANNEL_OP_RECV_TIMEOUT) {
        ASTNode *channel = NULL;
        LLVMChannelTarget target;
        LLVMTypeRef value_ty;
        LLVMValueRef tmp;
        char fname[128];
        LLVMFuncEntry *fn;

        if (!llvm_required_channel_var(ctx, node, callee_name,
                &channel, &target))
            return NULL;
        value_ty = pergyra_type_to_llvm(ctx, target.inner);
        if (value_ty == NULL)
            return llvm_task_channel_error(ctx, channel, callee_name,
                "requires a concrete LLVM value type for Channel<T>");
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_task_channel_error(ctx, channel, callee_name,
                "could not allocate receive temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);

        if (op == LLVM_TASK_CHANNEL_OP_TRY_RECV) {
            if (!llvm_task_channel_format_runtime_name(fname, sizeof(fname),
                    "pgy_channel_try_recv", target.inner)) {
                return llvm_task_channel_error(ctx, channel, callee_name,
                    "runtime function name is too long");
            }
            fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
            if (fn == NULL)
                return NULL;

            LLVMValueRef args[] = { target.ptr, tmp };
            LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 2, llvm_tmp_name(ctx));
            LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                llvm_tmp_name(ctx));
            return llvm_build_option_value(ctx, value_ty, ok, value);
        }

        LLVMValueRef timeout = llvm_emit_expression(ast_call_argument(node, 1), ctx);
        if (timeout == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower receive timeout expression");
        if (LLVMTypeOf(timeout) != ctx->type_i64) {
            timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                ctx->type_i64, llvm_tmp_name(ctx));
        }
        if (!llvm_task_channel_format_runtime_name(fname, sizeof(fname),
                "pgy_channel_recv_timeout", target.inner)) {
            return llvm_task_channel_error(ctx, channel, callee_name,
                "runtime function name is too long");
        }
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { target.ptr, tmp, timeout };
        LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 3, llvm_tmp_name(ctx));
        LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
            llvm_tmp_name(ctx));
        return llvm_build_option_value(ctx, value_ty, ok, value);
    }

    {
        const char *op = llvm_channel_query_runtime_op(callee_name);
        if (op != NULL && argc == 1) {
            ASTNode *channel = NULL;
            LLVMChannelTarget target;
            char fname[128];
            LLVMFuncEntry *fn;

            if (!llvm_required_channel_var(ctx, node, callee_name,
                    &channel, &target))
                return NULL;
            if (!llvm_task_channel_format_op_runtime_name(fname, sizeof(fname),
                    op, target.inner)) {
                return llvm_task_channel_error(ctx, channel, callee_name,
                    "runtime function name is too long");
            }
            fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
            if (fn == NULL)
                return NULL;

            LLVMValueRef args[] = { target.ptr };
            return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 1, llvm_tmp_name(ctx));
        }
    }

    if (llvm_is_task_channel_builtin_name(callee_name)) {
        return llvm_task_channel_error(ctx, node, callee_name,
            "has unsupported arity for the LLVM task/channel builtin");
    }

    return NULL;
}

#endif
