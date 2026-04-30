/* Collection stdlib call lowering.
 * Included by transpiler_expr_stdlib_builtin.h inside transpiler.c. */

static bool
transpiler_require_hashmap_type(TranspilerCtx *ctx, const char *map_type,
                                const char *operation,
                                char *key_buf, size_t key_buf_size,
                                char *value_buf, size_t value_buf_size,
                                const char **key_out,
                                const char **value_out)
{
    const char *resolved_type = map_type;
    char resolved_buf[128];

    if (resolved_type != NULL && strncmp(resolved_type, "HashMap<", 8) != 0) {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(ctx, resolved_type);
        if (alias_decl != NULL && alias_decl->data.type_alias.target_type != NULL) {
            ASTNode *target = resolve_type_alias_target(
                ctx, alias_decl->data.type_alias.target_type);
            char *rendered = render_type_name(target);
            if (rendered != NULL) {
                snprintf(resolved_buf, sizeof(resolved_buf), "%s", rendered);
                free(rendered);
                resolved_type = resolved_buf;
            }
        }
    }

    if (resolved_type != NULL && strncmp(resolved_type, "HashMap<", 8) == 0) {
        copy_constructed_arg_name_at(resolved_type, 0, key_buf, key_buf_size);
        copy_constructed_arg_name_at(resolved_type, 1, value_buf, value_buf_size);
        if (key_buf[0] != '\0' && value_buf[0] != '\0') {
            if (key_out != NULL)
                *key_out = key_buf;
            if (value_out != NULL)
                *value_out = value_buf;
            return true;
        }
    }

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: %s requires concrete HashMap<K, V> metadata",
        operation != NULL ? operation : "HashMap operation");
    return false;
}

static bool
transpiler_require_unary_collection_type(TranspilerCtx *ctx,
                                         const char *type_name,
                                         const char *family,
                                         const char *operation,
                                         char *inner_buf,
                                         size_t inner_buf_size,
                                         const char **inner_out)
{
    const char *resolved_type = type_name;
    char resolved_buf[128];
    size_t family_len = family != NULL ? strlen(family) : 0;

    if (resolved_type != NULL
        && !(family_len > 0
             && strncmp(resolved_type, family, family_len) == 0
             && resolved_type[family_len] == '<')) {
        ASTNode *alias_decl = transpiler_find_type_alias_decl(ctx, resolved_type);
        if (alias_decl != NULL && alias_decl->data.type_alias.target_type != NULL) {
            ASTNode *target = resolve_type_alias_target(
                ctx, alias_decl->data.type_alias.target_type);
            char *rendered = render_type_name(target);
            if (rendered != NULL) {
                snprintf(resolved_buf, sizeof(resolved_buf), "%s", rendered);
                free(rendered);
                resolved_type = resolved_buf;
            }
        }
    }

    if (resolved_type != NULL
        && family_len > 0
        && strncmp(resolved_type, family, family_len) == 0
        && resolved_type[family_len] == '<') {
        const char *inner = slot_inner_type_name(resolved_type);
        if (inner != NULL && inner[0] != '\0') {
            snprintf(inner_buf, inner_buf_size, "%s", inner);
            if (inner_out != NULL)
                *inner_out = inner_buf;
            return true;
        }
    }

    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: %s requires concrete %s<T> metadata",
        operation != NULL ? operation : "collection operation",
        family != NULL ? family : "collection");
    return false;
}

static char *
emit_call_stdlib_collection_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    if (strcmp(fn, "MapNew") == 0) {
        const char *hint = ctx->active_type_hint;
        const char *key = NULL;
        const char *value = NULL;
        char key_buf[64];
        char value_buf[64];
        if (hint != NULL) {
            if (!transpiler_require_hashmap_type(ctx, hint, "MapNew",
                    key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                    &key, &value)) {
                return pergyra_strdup("0");
            }
        } else {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: MapNew requires contextual HashMap<K, V> type hint");
            return pergyra_strdup("0");
        }
        (void)key;
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
        const char *key = NULL;
        const char *value = NULL;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapSet",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k); free(v);
            return pergyra_strdup("0");
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
        const char *key = NULL;
        const char *value = NULL;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapGet",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k);
            return pergyra_strdup("0");
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
        const char *key = NULL;
        const char *value = NULL;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapHas",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k);
            return pergyra_strdup("0");
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
        const char *key = NULL;
        const char *value = NULL;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapRemove",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k);
            return pergyra_strdup("0");
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
        char key_buf[64];
        char value_buf[64];
        const char *key = NULL;
        const char *value = NULL;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapSize",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m);
            return pergyra_strdup("0");
        }
        (void)key;
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
        const char *key = NULL;
        const char *value = NULL;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapKeys",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m);
            return pergyra_strdup("0");
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
        char inner_buf[64];
        const char *inner = NULL;
        if (hint == NULL || !transpiler_require_unary_collection_type(ctx,
                hint, "List", "ListNew", inner_buf, sizeof(inner_buf), &inner)) {
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "List", inner);
        return strdup_fmt("pgy_list_new_%s()", collection_runtime_suffix(inner));
    }
    if (strcmp(fn, "ListPush") == 0 && call->data.call.arg_count == 2) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        char *v = emit_expression(call->data.call.arguments[1], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListPush", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(v);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_push_%s(&%s, %s)",
            collection_runtime_suffix(inner), l, v);
        free(l); free(v); return r;
    }
    if (strcmp(fn, "ListGet") == 0 && call->data.call.arg_count == 2) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        char *i = emit_expression(call->data.call.arguments[1], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListGet", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i);
            return pergyra_strdup("0");
        }
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
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListSet", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i); free(v);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_set_%s(&%s, %s, %s)",
            collection_runtime_suffix(inner), l, i, v);
        free(l); free(i); free(v); return r;
    }
    if (strcmp(fn, "ListSize") == 0 && call->data.call.arg_count == 1) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListSize", inner_buf, sizeof(inner_buf), &inner)) {
            free(l);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_size_%s(&%s)",
            collection_runtime_suffix(inner), l);
        free(l); return r;
    }
    if (strcmp(fn, "ListRemove") == 0 && call->data.call.arg_count == 2) {
        char *l = emit_expression(call->data.call.arguments[0], ctx);
        char *i = emit_expression(call->data.call.arguments[1], ctx);
        const char *list_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListRemove", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "List", inner);
        char *r = strdup_fmt("pgy_list_remove_%s(&%s, %s)",
            collection_runtime_suffix(inner), l, i);
        free(l); free(i); return r;
    }

    if (strcmp(fn, "SetNew") == 0) {
        const char *hint = ctx->active_type_hint;
        char inner_buf[64];
        const char *set_inner = NULL;
        if (hint == NULL || !transpiler_require_unary_collection_type(ctx,
                hint, "Set", "SetNew", inner_buf, sizeof(inner_buf), &set_inner)) {
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        return strdup_fmt("pgy_set_new_%s()", collection_runtime_suffix(set_inner));
    }
    if (strcmp(fn, "SetAdd") == 0 && call->data.call.arg_count == 2) {
        const char *set_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetAdd", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_add_%s(&%s, %s)",
            collection_runtime_suffix(set_inner), s, k);
        free(s); free(k); return r;
    }
    if (strcmp(fn, "SetHas") == 0 && call->data.call.arg_count == 2) {
        const char *set_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetHas", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_has_%s(&%s, %s)",
            collection_runtime_suffix(set_inner), s, k);
        free(s); free(k); return r;
    }
    if (strcmp(fn, "SetRemove") == 0 && call->data.call.arg_count == 2) {
        const char *set_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetRemove", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_remove_%s(&%s, %s)",
            collection_runtime_suffix(set_inner), s, k);
        free(s); free(k); return r;
    }
    if (strcmp(fn, "SetSize") == 0 && call->data.call.arg_count == 1) {
        const char *set_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetSize", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char *r = strdup_fmt("pgy_set_size_%s(&%s)",
            collection_runtime_suffix(set_inner), s);
        free(s); return r;
    }

    if (strcmp(fn, "QueueNew") == 0) {
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
    if (strcmp(fn, "QueuePush") == 0 && call->data.call.arg_count == 2) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        char *v = emit_expression(call->data.call.arguments[1], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, queue_type,
                "Queue", "QueuePush", inner_buf, sizeof(inner_buf), &inner)) {
            free(q); free(v);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Queue", inner);
        char *r = strdup_fmt("pgy_queue_push_%s(&%s, %s)",
            collection_runtime_suffix(inner), q, v);
        free(q); free(v); return r;
    }
    if (strcmp(fn, "QueuePop") == 0 && call->data.call.arg_count == 1) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
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
        free(q); return r;
    }
    if (strcmp(fn, "QueueSize") == 0 && call->data.call.arg_count == 1) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
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
        free(q); return r;
    }
    if (strcmp(fn, "QueueEmpty") == 0 && call->data.call.arg_count == 1) {
        char *q = emit_expression(call->data.call.arguments[0], ctx);
        const char *queue_type = infer_expression_type_name(ctx, call->data.call.arguments[0]);
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
        free(q); return r;
    }
    return NULL;
}
