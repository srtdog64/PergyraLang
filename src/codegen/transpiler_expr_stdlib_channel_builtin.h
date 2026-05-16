#ifndef PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H

typedef enum {
    TRANSPILER_CHANNEL_OP_NONE = 0,
    TRANSPILER_CHANNEL_OP_CANCEL,
    TRANSPILER_CHANNEL_OP_CLOSE,
    TRANSPILER_CHANNEL_OP_IS_CANCELLED,
    TRANSPILER_CHANNEL_OP_RECV_TIMEOUT,
    TRANSPILER_CHANNEL_OP_SEND_TIMEOUT,
    TRANSPILER_CHANNEL_OP_SEND_TIMEOUT_STATUS,
    TRANSPILER_CHANNEL_OP_TRY_RECV,
    TRANSPILER_CHANNEL_OP_TRY_SEND,
    TRANSPILER_CHANNEL_OP_TRY_SEND_STATUS,
} TranspilerChannelOp;

typedef struct TranspilerChannelSpec {
    const char *name;
    size_t argc;
    TranspilerChannelOp op;
} TranspilerChannelSpec;

typedef struct TranspilerChannelQuerySpec {
    const char *name;
    const char *runtime_op;
} TranspilerChannelQuerySpec;

static int
transpiler_channel_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerChannelSpec *spec = (const TranspilerChannelSpec *)entry;

    return strcmp(name, spec->name);
}

static TranspilerChannelOp
transpiler_channel_lookup(const char *fn, size_t argc)
{
    static const TranspilerChannelSpec specs[] = {
        { "Cancel", 1, TRANSPILER_CHANNEL_OP_CANCEL },
        { "ChannelClose", 1, TRANSPILER_CHANNEL_OP_CLOSE },
        { "IsCancelled", 0, TRANSPILER_CHANNEL_OP_IS_CANCELLED },
        { "RecvTimeout", 2, TRANSPILER_CHANNEL_OP_RECV_TIMEOUT },
        { "SendTimeout", 3, TRANSPILER_CHANNEL_OP_SEND_TIMEOUT },
        { "SendTimeoutStatus", 3, TRANSPILER_CHANNEL_OP_SEND_TIMEOUT_STATUS },
        { "TryRecv", 1, TRANSPILER_CHANNEL_OP_TRY_RECV },
        { "TrySend", 2, TRANSPILER_CHANNEL_OP_TRY_SEND },
        { "TrySendStatus", 2, TRANSPILER_CHANNEL_OP_TRY_SEND_STATUS },
    };
    const TranspilerChannelSpec *spec;

    if (fn == NULL)
        return TRANSPILER_CHANNEL_OP_NONE;
    spec = (const TranspilerChannelSpec *)bsearch(
        fn, specs, sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]), transpiler_channel_spec_compare);
    if (spec == NULL || spec->argc != argc)
        return TRANSPILER_CHANNEL_OP_NONE;
    return spec->op;
}

static int
transpiler_channel_query_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerChannelQuerySpec *spec =
        (const TranspilerChannelQuerySpec *)entry;

    return strcmp(name, spec->name);
}

static const TranspilerChannelQuerySpec *
transpiler_channel_query_spec_lookup(const char *fn)
{
    static const TranspilerChannelQuerySpec specs[] = {
        { "ChannelCapacity", "capacity" },
        { "ChannelClosed", "closed" },
        { "ChannelFull", "full" },
        { "ChannelLength", "length" },
        { "ChannelReady", "ready" },
        { "ChannelSpace", "space" },
    };

    if (fn == NULL)
        return NULL;

    return (const TranspilerChannelQuerySpec *)bsearch(
        &fn, specs, sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]), transpiler_channel_query_spec_compare);
}

static char *
emit_call_stdlib_channel_query_builtin(const char *fn, ASTNode *call,
                                       TranspilerCtx *ctx)
{
    const TranspilerChannelQuerySpec *spec =
        transpiler_channel_query_spec_lookup(fn);
    char *ch;
    char inner_buf[64];
    const char *inner;
    char *result;
    ASTNode *channel_arg;

    if (spec == NULL || ast_call_arg_count(call) != 1)
        return NULL;

    channel_arg = ast_call_argument(call, 0);
    ch = emit_expression(channel_arg, ctx);
    inner = transpiler_require_channel_inner_type(ctx,
        channel_arg, spec->name, inner_buf,
        sizeof(inner_buf));
    if (inner == NULL) {
        free(ch);
        return pergyra_strdup("0");
    }

    result = strdup_fmt("pgy_channel_%s_%s(&%s)",
        spec->runtime_op, inner, ch);
    free(ch);
    return result;
}

static char *
emit_call_stdlib_channel_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    char *query_builtin = emit_call_stdlib_channel_query_builtin(fn, call, ctx);
    if (query_builtin != NULL)
        return query_builtin;

    size_t argc = ast_call_arg_count(call);
    TranspilerChannelOp op = transpiler_channel_lookup(fn, argc);

    if (op == TRANSPILER_CHANNEL_OP_TRY_RECV) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        char *ch = emit_expression(channel_arg, ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            channel_arg, "TryRecv",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char c_inner_buf[128];
        const char *c_inner = NULL;
        if (pergyra_type_to_c_copy(inner, c_inner_buf,
                sizeof(c_inner_buf))) {
            c_inner = c_inner_buf;
        }
        if (c_inner == NULL) {
            free(ch);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "({ %s _pgy_recv_tmp; "
            "pgy_channel_try_recv_%s(&%s, &_pgy_recv_tmp) "
            "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
            c_inner, inner, ch, inner, inner);
        free(ch);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_RECV_TIMEOUT) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        char *ch = emit_expression(channel_arg, ctx);
        char *timeout = emit_expression(ast_call_argument(call, 1), ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            channel_arg, "RecvTimeout",
            inner_buf, sizeof(inner_buf));
        if (inner == NULL) {
            free(ch);
            free(timeout);
            return pergyra_strdup("0");
        }
        char c_inner_buf[128];
        const char *c_inner = NULL;
        if (pergyra_type_to_c_copy(inner, c_inner_buf,
                sizeof(c_inner_buf))) {
            c_inner = c_inner_buf;
        }
        if (c_inner == NULL) {
            free(ch);
            free(timeout);
            return pergyra_strdup("0");
        }
        char *result = strdup_fmt(
            "({ %s _pgy_recv_tmp; "
            "pgy_channel_recv_timeout_%s(&%s, &_pgy_recv_tmp, (uint64_t)(%s)) "
            "? Some_%s(_pgy_recv_tmp) : None_%s(); })",
            c_inner, inner, ch, timeout, inner, inner);
        free(ch);
        free(timeout);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_TRY_SEND) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        char *ch = emit_expression(channel_arg, ctx);
        char *val = emit_expression(ast_call_argument(call, 1), ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            channel_arg, "TrySend",
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
    if (op == TRANSPILER_CHANNEL_OP_TRY_SEND_STATUS) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        char *ch = emit_expression(channel_arg, ctx);
        char *val = emit_expression(ast_call_argument(call, 1), ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            channel_arg, "TrySendStatus",
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
    if (op == TRANSPILER_CHANNEL_OP_SEND_TIMEOUT) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        char *ch = emit_expression(channel_arg, ctx);
        char *val = emit_expression(ast_call_argument(call, 1), ctx);
        char *timeout = emit_expression(ast_call_argument(call, 2), ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            channel_arg, "SendTimeout",
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
    if (op == TRANSPILER_CHANNEL_OP_SEND_TIMEOUT_STATUS) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        char *ch = emit_expression(channel_arg, ctx);
        char *val = emit_expression(ast_call_argument(call, 1), ctx);
        char *timeout = emit_expression(ast_call_argument(call, 2), ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            channel_arg, "SendTimeoutStatus",
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
    if (op == TRANSPILER_CHANNEL_OP_CANCEL) {
        char *task = emit_expression(ast_call_argument(call, 0), ctx);
        char *result = strdup_fmt("pgy_task_cancel(%s)", task);
        free(task);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_IS_CANCELLED) {
        return strdup_fmt("pgy_task_is_cancelled()");
    }
    if (op == TRANSPILER_CHANNEL_OP_CLOSE) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        char *ch = emit_expression(channel_arg, ctx);
        char inner_buf[64];
        const char *inner = transpiler_require_channel_inner_type(ctx,
            channel_arg, "ChannelClose",
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

    return NULL;
}

#endif /* PGY_TRANSPILER_EXPR_STDLIB_CHANNEL_BUILTIN_H */
