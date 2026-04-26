/* Collection stdlib call lowering.
 * Included by transpiler_expr_stdlib_builtin.h inside transpiler.c. */

static char *
emit_call_stdlib_collection_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    if (strcmp(fn, "MapNew") == 0) {
        const char *hint = ctx->active_type_hint;
        const char *value = "Int";
        if (hint != NULL && strncmp(hint, "HashMap<", 8) == 0) {
            const char *hint_value = constructed_arg_name_at(hint, 1);
            if (hint_value != NULL)
                value = hint_value;
        }
        ensure_collection_specialization(ctx, "Map", value);
        return strdup_fmt("pgy_map_new_%s()", collection_runtime_suffix(value));
    }
    if (strcmp(fn, "MapSet") == 0 && call->data.call.arg_count == 3) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        char *v = emit_expression(call->data.call.arguments[2], ctx);
        const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char key_buf[64];
        char value_buf[64];
        const char *key = "String";
        const char *value = "Int";
        if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
            copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
            copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
            key = key_buf;
            value = value_buf;
        }
        ensure_collection_specialization(ctx, "Map", value);
        char *result = strdup_fmt(
            strcmp(key, "Int") == 0 ? "pgy_map_set_i32_%s(&%s, %s, %s)"
            : strcmp(key, "Long") == 0 ? "pgy_map_set_i64_%s(&%s, %s, %s)"
            : strcmp(key, "Bool") == 0 ? "pgy_map_set_bool_%s(&%s, %s, %s)"
            : "pgy_map_set_%s(&%s, %s, %s)",
            collection_runtime_suffix(value), m, k, v);
        free(m); free(k); free(v);
        return result;
    }
    if (strcmp(fn, "MapGet") == 0 && call->data.call.arg_count == 2) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char key_buf[64];
        char value_buf[64];
        const char *key = "String";
        const char *value = "Int";
        if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
            copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
            copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
            key = key_buf;
            value = value_buf;
        }
        ensure_collection_specialization(ctx, "Map", value);
        char *result = strdup_fmt(
            strcmp(key, "Int") == 0 ? "pgy_map_get_i32_%s(&%s, %s)"
            : strcmp(key, "Long") == 0 ? "pgy_map_get_i64_%s(&%s, %s)"
            : strcmp(key, "Bool") == 0 ? "pgy_map_get_bool_%s(&%s, %s)"
            : "pgy_map_get_%s(&%s, %s)",
            collection_runtime_suffix(value), m, k);
        free(m); free(k);
        return result;
    }
    if (strcmp(fn, "MapHas") == 0 && call->data.call.arg_count == 2) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char key_buf[64];
        char value_buf[64];
        const char *key = "String";
        const char *value = "Int";
        if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
            copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
            copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
            key = key_buf;
            value = value_buf;
        }
        ensure_collection_specialization(ctx, "Map", value);
        char *result = strdup_fmt(
            strcmp(key, "Int") == 0 ? "pgy_map_has_i32_%s(&%s, %s)"
            : strcmp(key, "Long") == 0 ? "pgy_map_has_i64_%s(&%s, %s)"
            : strcmp(key, "Bool") == 0 ? "pgy_map_has_bool_%s(&%s, %s)"
            : "pgy_map_has_%s(&%s, %s)",
            collection_runtime_suffix(value), m, k);
        free(m); free(k);
        return result;
    }
    if (strcmp(fn, "MapRemove") == 0 && call->data.call.arg_count == 2) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char key_buf[64];
        char value_buf[64];
        const char *key = "String";
        const char *value = "Int";
        if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
            copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
            copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
            key = key_buf;
            value = value_buf;
        }
        ensure_collection_specialization(ctx, "Map", value);
        char *result = strdup_fmt(
            strcmp(key, "Int") == 0 ? "pgy_map_remove_i32_%s(&%s, %s)"
            : strcmp(key, "Long") == 0 ? "pgy_map_remove_i64_%s(&%s, %s)"
            : strcmp(key, "Bool") == 0 ? "pgy_map_remove_bool_%s(&%s, %s)"
            : "pgy_map_remove_%s(&%s, %s)",
            collection_runtime_suffix(value), m, k);
        free(m); free(k);
        return result;
    }
    if (strcmp(fn, "MapSize") == 0 && call->data.call.arg_count == 1) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *value = (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0)
            ? constructed_arg_name_at(map_type, 1)
            : "Int";
        ensure_collection_specialization(ctx, "Map", value);
        char *result = strdup_fmt("pgy_map_size_%s(&%s)",
            collection_runtime_suffix(value), m);
        free(m);
        return result;
    }
    if (strcmp(fn, "MapKeys") == 0 && call->data.call.arg_count == 1) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        const char *map_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char key_buf[64];
        char value_buf[64];
        const char *key = "String";
        const char *value = "Int";
        if (map_type != NULL && strncmp(map_type, "HashMap<", 8) == 0) {
            copy_constructed_arg_name_at(map_type, 0, key_buf, sizeof(key_buf));
            copy_constructed_arg_name_at(map_type, 1, value_buf, sizeof(value_buf));
            key = key_buf;
            value = value_buf;
        }
        ensure_collection_specialization(ctx, "Map", value);
        char *result = strdup_fmt(
            strcmp(key, "Int") == 0 ? "pgy_map_keys_i32_%s(&%s)"
            : strcmp(key, "Long") == 0 ? "pgy_map_keys_i64_%s(&%s)"
            : strcmp(key, "Bool") == 0 ? "pgy_map_keys_bool_%s(&%s)"
            : "pgy_map_keys_%s(&%s)",
            collection_runtime_suffix(value), m);
        free(m);
        return result;
    }
    if (strcmp(fn, "ListNew") == 0) {
        const char *hint = ctx->active_type_hint;
        const char *inner = (hint != NULL && strncmp(hint, "List<", 5) == 0)
            ? slot_inner_type_name(hint)
            : "Int";
        ensure_collection_specialization(ctx, "List", inner);
        return strdup_fmt("pgy_list_new_%s()", collection_runtime_suffix(inner));
    }
    if (strcmp(fn, "ListPush") == 0 && call->data.call.arg_count == 2) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        char *v = emit_expression(call->data.call.arguments[1], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
            ? slot_inner_type_name(list_type)
            : "Int";
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_push_%s(&%s, %s)",
            collection_runtime_suffix(inner), l, v);
        free(l); free(v); return r;
    }
    if (strcmp(fn, "ListGet") == 0 && call->data.call.arg_count == 2) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        char *i = emit_expression(call->data.call.arguments[1], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
            ? slot_inner_type_name(list_type)
            : "Int";
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_get_%s(&%s, %s)",
            collection_runtime_suffix(inner), l, i);
        free(l); free(i); return r;
    }
    if (strcmp(fn, "ListSet") == 0 && call->data.call.arg_count == 3) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        char *i = emit_expression(call->data.call.arguments[1], ctx);
        char *v = emit_expression(call->data.call.arguments[2], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
            ? slot_inner_type_name(list_type)
            : "Int";
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_set_%s(&%s, %s, %s)",
            collection_runtime_suffix(inner), l, i, v);
        free(l); free(i); free(v); return r;
    }
    if (strcmp(fn, "ListSize") == 0 && call->data.call.arg_count == 1) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
            ? slot_inner_type_name(list_type)
            : "Int";
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_size_%s(&%s)",
            collection_runtime_suffix(inner), l);
        free(l); return r;
    }
    if (strcmp(fn, "ListRemove") == 0 && call->data.call.arg_count == 2) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        char *i = emit_expression(call->data.call.arguments[1], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (list_type != NULL && strncmp(list_type, "List<", 5) == 0)
            ? slot_inner_type_name(list_type)
            : "Int";
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_remove_%s(&%s, %s)",
            collection_runtime_suffix(inner), l, i);
        free(l); free(i); return r;
    }

    #define SET_INFER_INNER(arg0) do { \
        const char *_st = infer_expression_type_name(ctx, arg0); \
        if (_st != NULL && strncmp(_st, "Set<", 4) == 0) \
            set_inner = slot_inner_type_name(_st); \
    } while (0)

    if (strcmp(fn, "SetNew") == 0) {
        const char *hint = ctx->active_type_hint;
        const char *set_inner = (hint != NULL && strncmp(hint, "Set<", 4) == 0)
            ? slot_inner_type_name(hint)
            : "String";
        ensure_collection_specialization(ctx, "Set", set_inner);
        return strdup_fmt("pgy_set_new_%s()", collection_runtime_suffix(set_inner));
    }
    if (strcmp(fn, "SetAdd") == 0 && call->data.call.arg_count == 2) {
        const char *set_inner = "String";
        SET_INFER_INNER(call->data.call.arguments[0]);
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_add_%s(&%s, %s)",
            collection_runtime_suffix(set_inner), s, k);
        free(s); free(k); return r;
    }
    if (strcmp(fn, "SetHas") == 0 && call->data.call.arg_count == 2) {
        const char *set_inner = "String";
        SET_INFER_INNER(call->data.call.arguments[0]);
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_has_%s(&%s, %s)",
            collection_runtime_suffix(set_inner), s, k);
        free(s); free(k); return r;
    }
    if (strcmp(fn, "SetRemove") == 0 && call->data.call.arg_count == 2) {
        const char *set_inner = "String";
        SET_INFER_INNER(call->data.call.arguments[0]);
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_remove_%s(&%s, %s)",
            collection_runtime_suffix(set_inner), s, k);
        free(s); free(k); return r;
    }
    if (strcmp(fn, "SetSize") == 0 && call->data.call.arg_count == 1) {
        const char *set_inner = "String";
        SET_INFER_INNER(call->data.call.arguments[0]);
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_size_%s(&%s)",
            collection_runtime_suffix(set_inner), s);
        free(s); return r;
    }
    #undef SET_INFER_INNER

    if (strcmp(fn, "QueueNew") == 0) {
        const char *hint = ctx->active_type_hint;
        const char *inner = (hint != NULL && strncmp(hint, "Queue<", 6) == 0)
            ? slot_inner_type_name(hint)
            : "Int";
        ensure_collection_specialization(ctx, "Queue", inner);
        return strdup_fmt("pgy_queue_new_%s()", collection_runtime_suffix(inner));
    }
    if (strcmp(fn, "QueuePush") == 0 && call->data.call.arg_count == 2) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        char *v = emit_expression(call->data.call.arguments[1], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
            ? slot_inner_type_name(queue_type)
            : "Int";
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_push_%s(&%s, %s)",
            collection_runtime_suffix(inner), q, v);
        free(q); free(v); return r;
    }
    if (strcmp(fn, "QueuePop") == 0 && call->data.call.arg_count == 1) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
            ? slot_inner_type_name(queue_type)
            : "Int";
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_pop_%s(&%s)",
            collection_runtime_suffix(inner), q);
        free(q); return r;
    }
    if (strcmp(fn, "QueueSize") == 0 && call->data.call.arg_count == 1) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
            ? slot_inner_type_name(queue_type)
            : "Int";
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_size_%s(&%s)",
            collection_runtime_suffix(inner), q);
        free(q); return r;
    }
    if (strcmp(fn, "QueueEmpty") == 0 && call->data.call.arg_count == 1) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        const char *inner = (queue_type != NULL && strncmp(queue_type, "Queue<", 6) == 0)
            ? slot_inner_type_name(queue_type)
            : "Int";
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_empty_%s(&%s)",
            collection_runtime_suffix(inner), q);
        free(q); return r;
    }
    return NULL;
}
