/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend Queue stdlib call lowering.
 */

#include "transpiler_expr_stdlib_queue_builtin.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_collection_runtime_suffix.h"
#include "transpiler_context.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_format.h"

typedef enum {
    TRANSPILER_QUEUE_OP_NONE = 0,
    TRANSPILER_QUEUE_OP_EMPTY,
    TRANSPILER_QUEUE_OP_NEW,
    TRANSPILER_QUEUE_OP_POP,
    TRANSPILER_QUEUE_OP_PUSH,
    TRANSPILER_QUEUE_OP_SIZE,
} TranspilerQueueOp;

typedef struct {
    const char *name;
    size_t argc;
    TranspilerQueueOp op;
} TranspilerQueueSpec;

static const TranspilerQueueSpec kTranspilerQueueSpecs[] = {
    {"QueueEmpty", 1, TRANSPILER_QUEUE_OP_EMPTY},
    {"QueueNew", (size_t)-1, TRANSPILER_QUEUE_OP_NEW},
    {"QueuePop", 1, TRANSPILER_QUEUE_OP_POP},
    {"QueuePush", 2, TRANSPILER_QUEUE_OP_PUSH},
    {"QueueSize", 1, TRANSPILER_QUEUE_OP_SIZE},
};

static int
transpiler_queue_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerQueueSpec *spec = (const TranspilerQueueSpec *)entry;
    return strcmp(name, spec->name);
}

static TranspilerQueueOp
transpiler_queue_lookup(const char *fn, size_t argc)
{
    const TranspilerQueueSpec *spec;

    if (fn == NULL)
        return TRANSPILER_QUEUE_OP_NONE;
    spec = (const TranspilerQueueSpec *)bsearch(
        fn,
        kTranspilerQueueSpecs,
        sizeof(kTranspilerQueueSpecs) / sizeof(kTranspilerQueueSpecs[0]),
        sizeof(kTranspilerQueueSpecs[0]),
        transpiler_queue_spec_compare);
    if (spec == NULL)
        return TRANSPILER_QUEUE_OP_NONE;
    if (spec->argc != (size_t)-1 && spec->argc != argc)
        return TRANSPILER_QUEUE_OP_NONE;
    return spec->op;
}

char *
emit_call_stdlib_queue_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    TranspilerQueueOp op = transpiler_queue_lookup(fn,
        call != NULL ? ast_call_arg_count(call) : 0);

    if (op == TRANSPILER_QUEUE_OP_NEW) {
        const char *hint = ctx->active_type_hint;
        char inner_buf[64];
        const char *inner = NULL;
        if (hint == NULL || !transpiler_require_unary_collection_type(ctx,
                hint, "Queue", "QueueNew", inner_buf, sizeof(inner_buf), &inner)) {
            return pergyra_strdup("0");
        }
        transpiler_collection_ensure_specialization(ctx, "Queue", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        return strdup_fmt("pgy_queue_new_%s()", suffix_buf);
    }
    if (op == TRANSPILER_QUEUE_OP_PUSH) {
        ASTNode *queue_arg = ast_call_argument(call, 0);
        char *q = emit_expression(queue_arg, ctx);
        char *v = emit_expression(ast_call_argument(call, 1), ctx);
        const char *queue_type = transpiler_expr_infer_type_name(ctx, queue_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueuePush", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            free(v);
            return pergyra_strdup("0");
        }
        transpiler_collection_ensure_specialization(ctx, "Queue", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_queue_push_%s(&%s, %s)",
            suffix_buf, q, v);
        free(q);
        free(v);
        return r;
    }
    if (op == TRANSPILER_QUEUE_OP_POP) {
        ASTNode *queue_arg = ast_call_argument(call, 0);
        char *q = emit_expression(queue_arg, ctx);
        const char *queue_type = transpiler_expr_infer_type_name(ctx, queue_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueuePop", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            return pergyra_strdup("0");
        }
        transpiler_collection_ensure_specialization(ctx, "Queue", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_queue_pop_%s(&%s)",
            suffix_buf, q);
        free(q);
        return r;
    }
    if (op == TRANSPILER_QUEUE_OP_SIZE) {
        ASTNode *queue_arg = ast_call_argument(call, 0);
        char *q = emit_expression(queue_arg, ctx);
        const char *queue_type = transpiler_expr_infer_type_name(ctx, queue_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueueSize", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            return pergyra_strdup("0");
        }
        transpiler_collection_ensure_specialization(ctx, "Queue", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_queue_size_%s(&%s)",
            suffix_buf, q);
        free(q);
        return r;
    }
    if (op == TRANSPILER_QUEUE_OP_EMPTY) {
        ASTNode *queue_arg = ast_call_argument(call, 0);
        char *q = emit_expression(queue_arg, ctx);
        const char *queue_type = transpiler_expr_infer_type_name(ctx, queue_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueueEmpty", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            return pergyra_strdup("0");
        }
        transpiler_collection_ensure_specialization(ctx, "Queue", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_queue_empty_%s(&%s)",
            suffix_buf, q);
        free(q);
        return r;
    }
    return NULL;
}
