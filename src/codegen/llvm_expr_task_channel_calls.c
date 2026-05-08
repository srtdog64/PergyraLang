/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_expr_task_channel_calls.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"

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

static LLVMVarEntry *
llvm_required_channel_var(LLVMGenCtx *ctx, ASTNode *channel,
                          const char *callee_name)
{
    const char *name;
    LLVMVarEntry *entry;

    if (channel == NULL || channel->type != AST_IDENTIFIER) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, channel,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM %s requires a channel identifier with concrete Channel<T> metadata",
                callee_name != NULL ? callee_name : "channel operation");
        }
        return NULL;
    }

    name = channel->data.identifier.name;
    entry = llvm_scope_lookup(ctx, name);
    if (entry == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM %s requires channel binding '%s' to be registered as Channel<T>",
            callee_name != NULL ? callee_name : "channel operation",
            name != NULL ? name : "<channel>");
    }
    return entry;
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

static LLVMFuncEntry *
llvm_required_task_function(LLVMGenCtx *ctx, ASTNode *node,
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
            "LLVM %s requires registered task runtime function '%s'",
            callee_name != NULL ? callee_name : "task operation",
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
llvm_is_task_channel_builtin_name(const char *callee_name)
{
    if (callee_name == NULL)
        return false;
    return strcmp(callee_name, "Cancel") == 0
        || strcmp(callee_name, "IsCancelled") == 0
        || strcmp(callee_name, "ChannelClose") == 0
        || strcmp(callee_name, "TrySend") == 0
        || strcmp(callee_name, "TrySendStatus") == 0
        || strcmp(callee_name, "SendTimeout") == 0
        || strcmp(callee_name, "SendTimeoutStatus") == 0
        || strcmp(callee_name, "TryRecv") == 0
        || strcmp(callee_name, "RecvTimeout") == 0
        || strcmp(callee_name, "ChannelReady") == 0
        || strcmp(callee_name, "ChannelLength") == 0
        || strcmp(callee_name, "ChannelCapacity") == 0
        || strcmp(callee_name, "ChannelSpace") == 0
        || strcmp(callee_name, "ChannelFull") == 0
        || strcmp(callee_name, "ChannelClosed") == 0;
}

static bool
llvm_channel_arg(LLVMGenCtx *ctx, ASTNode *node, const char *callee_name,
                 ASTNode **out_channel, LLVMVarEntry **out_var,
                 const char **out_inner)
{
    ASTNode *channel;
    LLVMVarEntry *var;
    const char *name;
    const char *inner;

    if (node == NULL || node->data.call.arg_count == 0)
        return false;

    channel = node->data.call.arguments[0];
    var = llvm_required_channel_var(ctx, channel, callee_name);
    if (var == NULL)
        return false;

    name = channel->data.identifier.name;
    inner = llvm_lookup_channel_inner(ctx, name);
    inner = llvm_required_channel_inner(ctx, channel, callee_name, name, inner);
    if (inner == NULL)
        return false;

    if (out_channel != NULL)
        *out_channel = channel;
    if (out_var != NULL)
        *out_var = var;
    if (out_inner != NULL)
        *out_inner = inner;
    return true;
}

LLVMValueRef
llvm_emit_task_channel_call(ASTNode *node, LLVMGenCtx *ctx, const char *callee_name)
{
    if (strcmp(callee_name, "Cancel") == 0 && node->data.call.arg_count == 1) {
        LLVMFuncEntry *fn = llvm_required_task_function(ctx, node, callee_name,
            "pgy_task_cancel_export");
        LLVMValueRef task = llvm_emit_expression(node->data.call.arguments[0], ctx);
        if (fn == NULL)
            return NULL;
        if (task == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower task handle expression");

        LLVMValueRef args[] = { task };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
    }

    if (strcmp(callee_name, "IsCancelled") == 0 && node->data.call.arg_count == 0) {
        LLVMFuncEntry *fn = llvm_required_task_function(ctx, node, callee_name,
            "pgy_task_is_cancelled_export");
        if (fn == NULL)
            return NULL;
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            NULL, 0, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "ChannelClose") == 0 && node->data.call.arg_count == 1) {
        ASTNode *channel = NULL;
        LLVMVarEntry *ch_var = NULL;
        const char *inner = NULL;
        char fname[128];
        LLVMFuncEntry *fn;

        if (!llvm_channel_arg(ctx, node, callee_name, &channel, &ch_var, &inner))
            return NULL;
        snprintf(fname, sizeof(fname), "pgy_channel_close_%s", inner);
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { ch_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 1, "");
    }

    if (strcmp(callee_name, "TrySend") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = NULL;
        LLVMVarEntry *ch_var = NULL;
        const char *inner = NULL;
        char fname[128];
        LLVMValueRef val;
        LLVMFuncEntry *fn;

        if (!llvm_channel_arg(ctx, node, callee_name, &channel, &ch_var, &inner))
            return NULL;
        val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (val == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower send value expression");
        snprintf(fname, sizeof(fname), "pgy_channel_try_send_%s", inner);
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { ch_var->alloca, val };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 2, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "TrySendStatus") == 0 && node->data.call.arg_count == 2) {
        ASTNode *channel = NULL;
        LLVMVarEntry *ch_var = NULL;
        const char *inner = NULL;
        char closed_name[128];
        char send_name[128];
        LLVMValueRef val;
        LLVMFuncEntry *closed_fn;
        LLVMFuncEntry *send_fn;

        if (!llvm_channel_arg(ctx, node, callee_name, &channel, &ch_var, &inner))
            return NULL;
        val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (val == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower send status value expression");
        snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s", inner);
        snprintf(send_name, sizeof(send_name), "pgy_channel_try_send_%s", inner);
        closed_fn = llvm_required_channel_function(ctx, channel, callee_name, closed_name);
        send_fn = llvm_required_channel_function(ctx, channel, callee_name, send_name);
        if (closed_fn == NULL || send_fn == NULL)
            return NULL;

        LLVMValueRef closed_args[] = { ch_var->alloca };
        LLVMValueRef send_args[] = { ch_var->alloca, val };
        LLVMValueRef closed = LLVMBuildCall2(ctx->builder, closed_fn->fn_type,
            closed_fn->fn, closed_args, 1, llvm_tmp_name(ctx));
        LLVMValueRef ok = LLVMBuildCall2(ctx->builder, send_fn->fn_type,
            send_fn->fn, send_args, 2, llvm_tmp_name(ctx));
        LLVMValueRef has_value = LLVMBuildOr(ctx->builder, closed, ok,
            llvm_tmp_name(ctx));
        return llvm_build_option_value(ctx, ctx->type_i1, has_value, ok);
    }

    if (strcmp(callee_name, "SendTimeout") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = NULL;
        LLVMVarEntry *ch_var = NULL;
        const char *inner = NULL;
        char fname[128];
        LLVMValueRef val;
        LLVMValueRef timeout;
        LLVMFuncEntry *fn;

        if (!llvm_channel_arg(ctx, node, callee_name, &channel, &ch_var, &inner))
            return NULL;
        val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
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
        snprintf(fname, sizeof(fname), "pgy_channel_send_timeout_%s", inner);
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { ch_var->alloca, val, timeout };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 3, llvm_tmp_name(ctx));
    }

    if (strcmp(callee_name, "SendTimeoutStatus") == 0 && node->data.call.arg_count == 3) {
        ASTNode *channel = NULL;
        LLVMVarEntry *ch_var = NULL;
        const char *inner = NULL;
        char closed_name[128];
        char send_name[128];
        LLVMValueRef val;
        LLVMValueRef timeout;
        LLVMFuncEntry *closed_fn;
        LLVMFuncEntry *send_fn;

        if (!llvm_channel_arg(ctx, node, callee_name, &channel, &ch_var, &inner))
            return NULL;
        val = llvm_emit_expression(node->data.call.arguments[1], ctx);
        timeout = llvm_emit_expression(node->data.call.arguments[2], ctx);
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
        snprintf(closed_name, sizeof(closed_name), "pgy_channel_closed_%s", inner);
        snprintf(send_name, sizeof(send_name), "pgy_channel_send_timeout_%s", inner);
        closed_fn = llvm_required_channel_function(ctx, channel, callee_name, closed_name);
        send_fn = llvm_required_channel_function(ctx, channel, callee_name, send_name);
        if (closed_fn == NULL || send_fn == NULL)
            return NULL;

        LLVMValueRef closed_args[] = { ch_var->alloca };
        LLVMValueRef send_args[] = { ch_var->alloca, val, timeout };
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

    if ((strcmp(callee_name, "TryRecv") == 0 && node->data.call.arg_count == 1)
        || (strcmp(callee_name, "RecvTimeout") == 0 && node->data.call.arg_count == 2)) {
        ASTNode *channel = NULL;
        LLVMVarEntry *ch_var = NULL;
        const char *inner = NULL;
        LLVMTypeRef value_ty;
        LLVMValueRef tmp;
        char fname[128];
        LLVMFuncEntry *fn;

        if (!llvm_channel_arg(ctx, node, callee_name, &channel, &ch_var, &inner))
            return NULL;
        value_ty = pergyra_type_to_llvm(ctx, inner);
        if (value_ty == NULL)
            return llvm_task_channel_error(ctx, channel, callee_name,
                "requires a concrete LLVM value type for Channel<T>");
        tmp = llvm_create_entry_alloca(ctx, value_ty, llvm_tmp_name(ctx));
        if (tmp == NULL)
            return llvm_task_channel_error(ctx, channel, callee_name,
                "could not allocate receive temporary");
        LLVMBuildStore(ctx->builder, LLVMConstNull(value_ty), tmp);

        if (strcmp(callee_name, "TryRecv") == 0) {
            snprintf(fname, sizeof(fname), "pgy_channel_try_recv_%s", inner);
            fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
            if (fn == NULL)
                return NULL;

            LLVMValueRef args[] = { ch_var->alloca, tmp };
            LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
                args, 2, llvm_tmp_name(ctx));
            LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
                llvm_tmp_name(ctx));
            return llvm_build_option_value(ctx, value_ty, ok, value);
        }

        LLVMValueRef timeout = llvm_emit_expression(node->data.call.arguments[1], ctx);
        if (timeout == NULL)
            return llvm_task_channel_error(ctx, node, callee_name,
                "could not lower receive timeout expression");
        if (LLVMTypeOf(timeout) != ctx->type_i64) {
            timeout = LLVMBuildSExtOrBitCast(ctx->builder, timeout,
                ctx->type_i64, llvm_tmp_name(ctx));
        }
        snprintf(fname, sizeof(fname), "pgy_channel_recv_timeout_%s", inner);
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { ch_var->alloca, tmp, timeout };
        LLVMValueRef ok = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 3, llvm_tmp_name(ctx));
        LLVMValueRef value = LLVMBuildLoad2(ctx->builder, value_ty, tmp,
            llvm_tmp_name(ctx));
        return llvm_build_option_value(ctx, value_ty, ok, value);
    }

    if ((strcmp(callee_name, "ChannelReady") == 0
         || strcmp(callee_name, "ChannelLength") == 0
         || strcmp(callee_name, "ChannelCapacity") == 0
         || strcmp(callee_name, "ChannelSpace") == 0
         || strcmp(callee_name, "ChannelFull") == 0
         || strcmp(callee_name, "ChannelClosed") == 0)
        && node->data.call.arg_count == 1) {
        ASTNode *channel = NULL;
        LLVMVarEntry *ch_var = NULL;
        const char *inner = NULL;
        const char *op =
            strcmp(callee_name, "ChannelReady") == 0 ? "ready" :
            strcmp(callee_name, "ChannelLength") == 0 ? "length" :
            strcmp(callee_name, "ChannelCapacity") == 0 ? "capacity" :
            strcmp(callee_name, "ChannelSpace") == 0 ? "space" :
            strcmp(callee_name, "ChannelFull") == 0 ? "full" :
            "closed";
        char fname[128];
        LLVMFuncEntry *fn;

        if (!llvm_channel_arg(ctx, node, callee_name, &channel, &ch_var, &inner))
            return NULL;
        snprintf(fname, sizeof(fname), "pgy_channel_%s_%s", op, inner);
        fn = llvm_required_channel_function(ctx, channel, callee_name, fname);
        if (fn == NULL)
            return NULL;

        LLVMValueRef args[] = { ch_var->alloca };
        return LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn,
            args, 1, llvm_tmp_name(ctx));
    }

    if (llvm_is_task_channel_builtin_name(callee_name)) {
        return llvm_task_channel_error(ctx, node, callee_name,
            "has unsupported arity for the LLVM task/channel builtin");
    }

    return NULL;
}

#endif
