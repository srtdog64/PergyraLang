/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend channel stdlib call lowering.
 */

#include "transpiler_expr_stdlib_channel_builtin.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_context.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_format.h"
#include "transpiler_type_mapping.h"

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

static const char *
transpiler_channel_require_inner_type(TranspilerCtx *ctx, ASTNode *expr,
                                      const char *operation,
                                      char *inner_buf,
                                      size_t inner_buf_size)
{
    return transpiler_require_channel_inner_type(ctx, expr, operation,
        inner_buf, inner_buf_size);
}

static bool
transpiler_channel_require_lvalue(TranspilerCtx *ctx, ASTNode *expr,
                                  const char *operation)
{
    if (transpiler_channel_expr_is_c_lvalue(expr))
        return true;
    transpiler_set_channel_lvalue_error(ctx, operation);
    return false;
}

static char *
transpiler_channel_emit_lvalue_arg(TranspilerCtx *ctx,
                                   ASTNode *arg,
                                   const char *builtin_name)
{
    char *rendered = transpiler_emit_channel_lvalue_expr(ctx, arg);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: channel builtin %s could not lower channel argument",
        builtin_name != NULL ? builtin_name : "(unknown)");
    return NULL;
}

static char *
transpiler_channel_emit_arg(TranspilerCtx *ctx,
                            ASTNode *arg,
                            const char *builtin_name,
                            const char *role)
{
    char *rendered = emit_expression(arg, ctx);

    if (rendered != NULL)
        return rendered;

    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "C backend: channel builtin %s could not lower %s argument",
        builtin_name != NULL ? builtin_name : "(unknown)",
        role != NULL ? role : "operand");
    return NULL;
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
    if (!transpiler_channel_require_lvalue(ctx, channel_arg, spec->name))
        return NULL;
    ch = transpiler_channel_emit_lvalue_arg(ctx, channel_arg, spec->name);
    if (ch == NULL)
        return NULL;
    inner = transpiler_channel_require_inner_type(ctx,
        channel_arg, spec->name, inner_buf,
        sizeof(inner_buf));
    if (inner == NULL) {
        free(ch);
        return NULL;
    }

    result = strdup_fmt("pgy_channel_%s_%s(&%s)",
        spec->runtime_op, inner, ch);
    free(ch);
    return result;
}

char *
emit_call_stdlib_channel_builtin(const char *fn, ASTNode *call,
                                 TranspilerCtx *ctx)
{
    char *query_builtin = emit_call_stdlib_channel_query_builtin(fn, call, ctx);
    size_t argc;
    TranspilerChannelOp op;

    if (query_builtin != NULL)
        return query_builtin;

    argc = ast_call_arg_count(call);
    op = transpiler_channel_lookup(fn, argc);

    if (op == TRANSPILER_CHANNEL_OP_TRY_RECV) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        if (!transpiler_channel_require_lvalue(ctx, channel_arg, "TryRecv"))
            return NULL;
        char *ch = transpiler_channel_emit_lvalue_arg(ctx, channel_arg,
            "TryRecv");
        if (ch == NULL)
            return NULL;
        char inner_buf[64];
        const char *inner = transpiler_channel_require_inner_type(ctx,
            channel_arg, "TryRecv",
            inner_buf, sizeof(inner_buf));
        char *result;

        if (inner == NULL) {
            free(ch);
            return NULL;
        }
        result = strdup_fmt(
            "({ PgyRuntimeChannel%sResult _pgy_recv_result = "
            "pgy_lane_channel_try_recv_result_%s(PGY_LANE_PINNED_ZONE, &%s); "
            "_pgy_recv_result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK "
            "? Some_%s(_pgy_recv_result.ok) : None_%s(); })",
            inner, inner, ch, inner, inner);
        free(ch);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_RECV_TIMEOUT) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        if (!transpiler_channel_require_lvalue(ctx, channel_arg,
                "RecvTimeout"))
            return NULL;
        char *ch = transpiler_channel_emit_lvalue_arg(ctx, channel_arg,
            "RecvTimeout");
        char *timeout = ch != NULL
            ? transpiler_channel_emit_arg(ctx, ast_call_argument(call, 1),
                  "RecvTimeout", "timeout")
            : NULL;
        if (ch == NULL || timeout == NULL) {
            free(ch);
            free(timeout);
            return NULL;
        }
        char inner_buf[64];
        const char *inner = transpiler_channel_require_inner_type(ctx,
            channel_arg, "RecvTimeout",
            inner_buf, sizeof(inner_buf));
        char *result;

        if (inner == NULL) {
            free(ch);
            free(timeout);
            return NULL;
        }
        result = strdup_fmt(
            "({ PgyRuntimeChannel%sResult _pgy_recv_result = "
            "pgy_lane_channel_recv_timeout_result_%s(PGY_LANE_PINNED_ZONE, &%s, (uint64_t)(%s)); "
            "_pgy_recv_result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK "
            "? Some_%s(_pgy_recv_result.ok) : None_%s(); })",
            inner, inner, ch, timeout, inner, inner);
        free(ch);
        free(timeout);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_TRY_SEND) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        if (!transpiler_channel_require_lvalue(ctx, channel_arg, "TrySend"))
            return NULL;
        char *ch = transpiler_channel_emit_lvalue_arg(ctx, channel_arg,
            "TrySend");
        char *val = ch != NULL
            ? transpiler_channel_emit_arg(ctx, ast_call_argument(call, 1),
                  "TrySend", "value")
            : NULL;
        if (ch == NULL || val == NULL) {
            free(ch);
            free(val);
            return NULL;
        }
        char inner_buf[64];
        const char *inner = transpiler_channel_require_inner_type(ctx,
            channel_arg, "TrySend",
            inner_buf, sizeof(inner_buf));
        char *result;

        if (inner == NULL) {
            free(ch);
            free(val);
            return NULL;
        }
        result = strdup_fmt(
            "pgy_lane_channel_try_send_%s(PGY_LANE_PINNED_ZONE, &%s, %s)",
            inner, ch, val);
        free(ch);
        free(val);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_TRY_SEND_STATUS) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        if (!transpiler_channel_require_lvalue(ctx, channel_arg,
                "TrySendStatus"))
            return NULL;
        char *ch = transpiler_channel_emit_lvalue_arg(ctx, channel_arg,
            "TrySendStatus");
        char *val = ch != NULL
            ? transpiler_channel_emit_arg(ctx, ast_call_argument(call, 1),
                  "TrySendStatus", "value")
            : NULL;
        if (ch == NULL || val == NULL) {
            free(ch);
            free(val);
            return NULL;
        }
        char inner_buf[64];
        const char *inner = transpiler_channel_require_inner_type(ctx,
            channel_arg, "TrySendStatus",
            inner_buf, sizeof(inner_buf));
        char *result;

        if (inner == NULL) {
            free(ch);
            free(val);
            return NULL;
        }
        result = strdup_fmt(
            "pgy_lane_channel_try_send_status_%s(PGY_LANE_PINNED_ZONE, &%s, %s)",
            inner, ch, val);
        free(ch);
        free(val);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_SEND_TIMEOUT
        || op == TRANSPILER_CHANNEL_OP_SEND_TIMEOUT_STATUS) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        const char *label = op == TRANSPILER_CHANNEL_OP_SEND_TIMEOUT
            ? "SendTimeout"
            : "SendTimeoutStatus";
        const char *runtime_op = op == TRANSPILER_CHANNEL_OP_SEND_TIMEOUT
            ? "send_timeout"
            : "send_timeout_status";
        if (!transpiler_channel_require_lvalue(ctx, channel_arg, label))
            return NULL;
        char *ch = transpiler_channel_emit_lvalue_arg(ctx, channel_arg, label);
        char *val = ch != NULL
            ? transpiler_channel_emit_arg(ctx, ast_call_argument(call, 1),
                  label, "value")
            : NULL;
        char *timeout = val != NULL
            ? transpiler_channel_emit_arg(ctx, ast_call_argument(call, 2),
                  label, "timeout")
            : NULL;
        if (ch == NULL || val == NULL || timeout == NULL) {
            free(ch);
            free(val);
            free(timeout);
            return NULL;
        }
        char inner_buf[64];
        const char *inner = transpiler_channel_require_inner_type(ctx,
            channel_arg, label, inner_buf, sizeof(inner_buf));
        char *result;

        if (inner == NULL) {
            free(ch);
            free(val);
            free(timeout);
            return NULL;
        }
        result = strdup_fmt(
            "pgy_lane_channel_%s_%s(PGY_LANE_PINNED_ZONE, &%s, %s, (uint64_t)(%s))",
            runtime_op, inner, ch, val, timeout);
        free(ch);
        free(val);
        free(timeout);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_CANCEL) {
        char *task = transpiler_channel_emit_arg(ctx,
            ast_call_argument(call, 0), "Cancel", "task");
        if (task == NULL)
            return NULL;
        char *result = strdup_fmt("pgy_lane_cancel(%s)", task);
        free(task);
        return result;
    }
    if (op == TRANSPILER_CHANNEL_OP_IS_CANCELLED)
        return strdup_fmt("pgy_task_is_cancelled()");
    if (op == TRANSPILER_CHANNEL_OP_CLOSE) {
        ASTNode *channel_arg = ast_call_argument(call, 0);
        if (!transpiler_channel_require_lvalue(ctx, channel_arg,
                "ChannelClose"))
            return NULL;
        char *ch = transpiler_channel_emit_lvalue_arg(ctx, channel_arg,
            "ChannelClose");
        if (ch == NULL)
            return NULL;
        char inner_buf[64];
        const char *inner = transpiler_channel_require_inner_type(ctx,
            channel_arg, "ChannelClose", inner_buf, sizeof(inner_buf));
        char *result;

        if (inner == NULL) {
            free(ch);
            return NULL;
        }
        result = strdup_fmt("pgy_channel_close_%s(&%s)", inner, ch);
        free(ch);
        return result;
    }

    return NULL;
}
