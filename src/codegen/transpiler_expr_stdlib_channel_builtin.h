#ifndef PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H

static char *
emit_call_stdlib_channel_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    if (strcmp(fn, "TryRecv") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "TryRecv",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        const char *c_inner = pergyra_type_to_c(inner);
        char *result = strdup_fmt(
            "({ %s _pgy_recv_tmp; "
            "pgy_channel_try_recv_%s(&%s, &_pgy_recv_tmp) "
            "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
            c_inner, inner, ch, inner, inner);
        free(ch);
        return result;
    }
    if (strcmp(fn, "RecvTimeout") == 0 && call->data.call.arg_count == 2) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char *timeout = emit_expression(call->data.call.arguments[1], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "RecvTimeout",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            free(timeout);
            return pergyra_strdup("0");
        }
        const char *c_inner = pergyra_type_to_c(inner);
        char *result = strdup_fmt(
            "({ %s _pgy_recv_tmp; "
            "pgy_channel_recv_timeout_%s(&%s, &_pgy_recv_tmp, (uint64_t)(%s)) "
            "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
            c_inner, inner, ch, timeout, inner, inner);
        free(ch);
        free(timeout);
        return result;
    }
    if (strcmp(fn, "TrySend") == 0 && call->data.call.arg_count == 2) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char *val = emit_expression(call->data.call.arguments[1], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "TrySend",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            free(val);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_try_send_%s(&%s, %s)", inner, ch, val);
        free(ch);
        free(val);
        return result;
    }
    if (strcmp(fn, "TrySendStatus") == 0 && call->data.call.arg_count == 2) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char *val = emit_expression(call->data.call.arguments[1], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "TrySendStatus",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            free(val);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_try_send_status_%s(&%s, %s)", inner, ch, val);
        free(ch);
        free(val);
        return result;
    }
    if (strcmp(fn, "SendTimeout") == 0 && call->data.call.arg_count == 3) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char *val = emit_expression(call->data.call.arguments[1], ctx);
        char *timeout = emit_expression(call->data.call.arguments[2], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "SendTimeout",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            free(val);
            free(timeout);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_send_timeout_%s(&%s, %s, (uint64_t)(%s))",
            inner, ch, val, timeout);
        free(ch);
        free(val);
        free(timeout);
        return result;
    }
    if (strcmp(fn, "SendTimeoutStatus") == 0 && call->data.call.arg_count == 3) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char *val = emit_expression(call->data.call.arguments[1], ctx);
        char *timeout = emit_expression(call->data.call.arguments[2], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "SendTimeoutStatus",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            free(val);
            free(timeout);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_send_timeout_status_%s(&%s, %s, (uint64_t)(%s))",
            inner, ch, val, timeout);
        free(ch);
        free(val);
        free(timeout);
        return result;
    }
    if (strcmp(fn, "Cancel") == 0 && call->data.call.arg_count == 1) {
        char *task = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("pgy_task_cancel(%s)", task);
        free(task);
        return result;
    }
    if (strcmp(fn, "IsCancelled") == 0 && call->data.call.arg_count == 0) {
        return strdup_fmt("pgy_task_is_cancelled()");
    }
    if (strcmp(fn, "ChannelClose") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "ChannelClose",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_close_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }
    if (strcmp(fn, "ChannelReady") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "ChannelReady",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_ready_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }
    if (strcmp(fn, "ChannelLength") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "ChannelLength",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_length_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }
    if (strcmp(fn, "ChannelCapacity") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "ChannelCapacity",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_capacity_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }
    if (strcmp(fn, "ChannelSpace") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "ChannelSpace",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_space_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }
    if (strcmp(fn, "ChannelFull") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "ChannelFull",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_full_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }
    if (strcmp(fn, "ChannelClosed") == 0 && call->data.call.arg_count == 1) {
        char *ch = emit_expression(call->data.call.arguments[0], ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            call->data.call.arguments[0], "ChannelClosed",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "pgy_channel_closed_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }

    return NULL;
}

#endif /* PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H */
