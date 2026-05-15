#ifndef PGY_TRANSPILER_EXPR_STDLIB_MAP_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_MAP_BUILTIN_H

typedef enum {
    TRANSPILER_MAP_OP_NONE = 0,
    TRANSPILER_MAP_OP_GET,
    TRANSPILER_MAP_OP_HAS,
    TRANSPILER_MAP_OP_KEYS,
    TRANSPILER_MAP_OP_NEW,
    TRANSPILER_MAP_OP_REMOVE,
    TRANSPILER_MAP_OP_SET,
    TRANSPILER_MAP_OP_SIZE,
} TranspilerMapOp;

typedef struct {
    const char *name;
    size_t argc;
    TranspilerMapOp op;
} TranspilerMapSpec;

static const TranspilerMapSpec kTranspilerMapSpecs[] = {
    {"MapGet", 2, TRANSPILER_MAP_OP_GET},
    {"MapHas", 2, TRANSPILER_MAP_OP_HAS},
    {"MapKeys", 1, TRANSPILER_MAP_OP_KEYS},
    {"MapNew", (size_t)-1, TRANSPILER_MAP_OP_NEW},
    {"MapRemove", 2, TRANSPILER_MAP_OP_REMOVE},
    {"MapSet", 3, TRANSPILER_MAP_OP_SET},
    {"MapSize", 1, TRANSPILER_MAP_OP_SIZE},
};

static int
transpiler_map_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerMapSpec *spec = (const TranspilerMapSpec *)entry;
    return strcmp(name, spec->name);
}

static TranspilerMapOp
transpiler_map_lookup(const char *fn, size_t argc)
{
    const TranspilerMapSpec *spec;

    if (fn == NULL)
        return TRANSPILER_MAP_OP_NONE;
    spec = (const TranspilerMapSpec *)bsearch(
        fn,
        kTranspilerMapSpecs,
        sizeof(kTranspilerMapSpecs) / sizeof(kTranspilerMapSpecs[0]),
        sizeof(kTranspilerMapSpecs[0]),
        transpiler_map_spec_compare);
    if (spec == NULL)
        return TRANSPILER_MAP_OP_NONE;
    if (spec->argc != (size_t)-1 && spec->argc != argc)
        return TRANSPILER_MAP_OP_NONE;
    return spec->op;
}

static char *
emit_call_stdlib_map_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    size_t arg_count = ast_call_arg_count(call);
    TranspilerMapOp map_op = transpiler_map_lookup(fn, arg_count);

    if (map_op == TRANSPILER_MAP_OP_NEW) {
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
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        return strdup_fmt("pgy_map_new_%s()", suffix_buf);
    }
    if (map_op == TRANSPILER_MAP_OP_SET) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        ASTNode *value_arg = ast_call_argument(call, 2);
        char *m = emit_expression(map_arg, ctx);
        char *k = emit_expression(key_arg, ctx);
        char *v = emit_expression(value_arg, ctx);
        const char *map_type = infer_expression_type_name(ctx,
            map_arg);
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
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_set%s_%s(&%s, %s, %s)",
            pgy_hashmap_key_c_infix(key),
            suffix_buf, m, k, v);
        free(m); free(k); free(v);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_GET) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        char *m = emit_expression(map_arg, ctx);
        char *k = emit_expression(key_arg, ctx);
        const char *map_type = infer_expression_type_name(ctx,
            map_arg);
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
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_get%s_%s(&%s, %s)",
            pgy_hashmap_key_c_infix(key),
            suffix_buf, m, k);
        free(m); free(k);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_HAS) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        char *m = emit_expression(map_arg, ctx);
        char *k = emit_expression(key_arg, ctx);
        const char *map_type = infer_expression_type_name(ctx,
            map_arg);
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
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_has%s_%s(&%s, %s)",
            pgy_hashmap_key_c_infix(key),
            suffix_buf, m, k);
        free(m); free(k);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_REMOVE) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        char *m = emit_expression(map_arg, ctx);
        char *k = emit_expression(key_arg, ctx);
        const char *map_type = infer_expression_type_name(ctx,
            map_arg);
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
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_remove%s_%s(&%s, %s)",
            pgy_hashmap_key_c_infix(key),
            suffix_buf, m, k);
        free(m); free(k);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_SIZE) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        char *m = emit_expression(map_arg, ctx);
        const char *map_type = infer_expression_type_name(ctx,
            map_arg);
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
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt("pgy_map_size_%s(&%s)",
            suffix_buf, m);
        free(m);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_KEYS) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        char *m = emit_expression(map_arg, ctx);
        const char *map_type = infer_expression_type_name(ctx,
            map_arg);
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
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_keys%s_%s(&%s)",
            pgy_hashmap_key_c_infix(key),
            suffix_buf, m);
        free(m);
        return result;
    }
    return NULL;
}
#endif /* PGY_TRANSPILER_EXPR_STDLIB_MAP_BUILTIN_H */
