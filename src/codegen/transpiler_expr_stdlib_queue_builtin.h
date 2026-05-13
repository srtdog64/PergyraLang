#ifndef PGY_TRANSPILER_EXPR_STDLIB_QUEUE_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_QUEUE_BUILTIN_H

static char *
emit_call_stdlib_queue_builtin(TranspilerCollectionOp op,
                               ASTNode *call,
                               TranspilerCtx *ctx)
{
    if (op == TRANSPILER_COLLECTION_OP_QUEUE_NEW) {
        const char *hint = ctx->active_type_hint;
        char inner_buf[64];
        const char *inner = NULL;
        if (hint == NULL || !transpiler_require_unary_collection_type(ctx,
                hint, "Queue", "QueueNew", inner_buf, sizeof(inner_buf), &inner)) {
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Queue", inner);
        return strdup_fmt("pgy_queue_new_%s()", collection_runtime_suffix(inner));
    }
    if (op == TRANSPILER_COLLECTION_OP_QUEUE_PUSH) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        char *v = emit_expression(call->data.call.arguments[1], ctx);
        const char *queue_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueuePush", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            free(v);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_push_%s(&%s, %s)",
            collection_runtime_suffix(inner), q, v);
        free(q);
        free(v);
        return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_QUEUE_POP) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueuePop", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_pop_%s(&%s)",
            collection_runtime_suffix(inner), q);
        free(q);
        return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_QUEUE_SIZE) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueueSize", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_size_%s(&%s)",
            collection_runtime_suffix(inner), q);
        free(q);
        return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_QUEUE_EMPTY) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueueEmpty", inner_buf, sizeof(inner_buf), &inner)) {
            free(q);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_empty_%s(&%s)",
            collection_runtime_suffix(inner), q);
        free(q);
        return r;
    }
    return NULL;
}

#endif /* PGY_TRANSPILER_EXPR_STDLIB_QUEUE_BUILTIN_H */
