#ifndef PGY_TRANSPILER_EXPR_STDLIB_MAP_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_MAP_BUILTIN_H

static char *
emit_call_stdlib_map_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
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
        const char *map_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
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
            "pgy_map_set%s_%s(&%s, %s, %s)",
            pgy_hashmap_key_c_infix(key),
            collection_runtime_suffix(value), m, k, v);
        free(m); free(k); free(v);
        return result;
    }
    if (strcmp(fn, "MapGet") == 0 && call->data.call.arg_count == 2) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        const char *map_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
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
            "pgy_map_get%s_%s(&%s, %s)",
            pgy_hashmap_key_c_infix(key),
            collection_runtime_suffix(value), m, k);
        free(m); free(k);
        return result;
    }
    if (strcmp(fn, "MapHas") == 0 && call->data.call.arg_count == 2) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        const char *map_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
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
            "pgy_map_has%s_%s(&%s, %s)",
            pgy_hashmap_key_c_infix(key),
            collection_runtime_suffix(value), m, k);
        free(m); free(k);
        return result;
    }
    if (strcmp(fn, "MapRemove") == 0 && call->data.call.arg_count == 2) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        char *k = emit_expression(call->data.call.arguments[1], ctx);
        const char *map_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
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
            "pgy_map_remove%s_%s(&%s, %s)",
            pgy_hashmap_key_c_infix(key),
            collection_runtime_suffix(value), m, k);
        free(m); free(k);
        return result;
    }
    if (strcmp(fn, "MapSize") == 0 && call->data.call.arg_count == 1) {
        char *m = emit_expression(call->data.call.arguments[0], ctx);
        const char *map_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
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
        const char *map_type = infer_expression_type_name(ctx,
            call->data.call.arguments[0]);
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
            "pgy_map_keys%s_%s(&%s)",
            pgy_hashmap_key_c_infix(key),
            collection_runtime_suffix(value), m);
        free(m);
        return result;
    }
    return NULL;
}
#endif /* PGY_TRANSPILER_EXPR_STDLIB_MAP_BUILTIN_H */
