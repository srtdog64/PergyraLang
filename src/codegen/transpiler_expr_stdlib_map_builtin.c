/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend HashMap stdlib call lowering.
 */

#include "transpiler_expr_stdlib_map_builtin.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "codegen_hashmap_key_policy.h"
#include "transpiler_collection_runtime_suffix.h"
#include "transpiler_context.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_format.h"

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
transpiler_map_emit_arg(TranspilerCtx *ctx,
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
        "C backend: map builtin %s could not lower %s argument",
        builtin_name != NULL ? builtin_name : "(unknown)",
        role != NULL ? role : "operand");
    return NULL;
}

static const char *
transpiler_map_require_supported_key(TranspilerCtx *ctx,
                                     const char *builtin_name,
                                     const char *key_name)
{
    const char *infix = pgy_hashmap_key_c_infix(key_name);

    if (infix != NULL)
        return infix;
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
        "C backend: map builtin %s requires stable HashMap<Bool|Int|Long|String, T> key metadata",
        builtin_name != NULL ? builtin_name : "(unknown)");
    return NULL;
}

char *
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
                return NULL;
            }
        } else {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: MapNew requires contextual HashMap<K, V> type hint");
            return NULL;
        }
        (void)key;
        transpiler_collection_ensure_specialization(ctx, "Map", value);
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        return strdup_fmt("pgy_map_new_%s()", suffix_buf);
    }
    if (map_op == TRANSPILER_MAP_OP_SET) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        ASTNode *value_arg = ast_call_argument(call, 2);
        if (!transpiler_require_c_addressable_storage(ctx, map_arg,
                "MapSet", "HashMap"))
            return NULL;
        char *m = transpiler_map_emit_arg(ctx, map_arg, "MapSet", "map");
        char *k = m != NULL
            ? transpiler_map_emit_arg(ctx, key_arg, "MapSet", "key")
            : NULL;
        char *v = k != NULL
            ? transpiler_map_emit_arg(ctx, value_arg, "MapSet", "value")
            : NULL;
        if (m == NULL || k == NULL || v == NULL) {
            free(m);
            free(k);
            free(v);
            return NULL;
        }
        const char *map_type = transpiler_expr_infer_type_name(ctx, map_arg);
        char key_buf[64];
        char value_buf[64];
        const char *key = NULL;
        const char *value = NULL;
        const char *key_infix;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapSet",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k); free(v);
            return NULL;
        }
        key_infix = transpiler_map_require_supported_key(ctx, "MapSet", key);
        if (key_infix == NULL) {
            free(m); free(k); free(v);
            return NULL;
        }
        transpiler_collection_ensure_specialization(ctx, "Map", value);
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_set%s_%s(&%s, %s, %s)",
            key_infix,
            suffix_buf, m, k, v);
        free(m); free(k); free(v);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_GET) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        if (!transpiler_require_c_addressable_storage(ctx, map_arg,
                "MapGet", "HashMap"))
            return NULL;
        char *m = transpiler_map_emit_arg(ctx, map_arg, "MapGet", "map");
        char *k = m != NULL
            ? transpiler_map_emit_arg(ctx, key_arg, "MapGet", "key")
            : NULL;
        if (m == NULL || k == NULL) {
            free(m);
            free(k);
            return NULL;
        }
        const char *map_type = transpiler_expr_infer_type_name(ctx, map_arg);
        char key_buf[64];
        char value_buf[64];
        const char *key = NULL;
        const char *value = NULL;
        const char *key_infix;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapGet",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k);
            return NULL;
        }
        key_infix = transpiler_map_require_supported_key(ctx, "MapGet", key);
        if (key_infix == NULL) {
            free(m); free(k);
            return NULL;
        }
        transpiler_collection_ensure_specialization(ctx, "Map", value);
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_get%s_%s(&%s, %s)",
            key_infix,
            suffix_buf, m, k);
        free(m); free(k);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_HAS) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        if (!transpiler_require_c_addressable_storage(ctx, map_arg,
                "MapHas", "HashMap"))
            return NULL;
        char *m = transpiler_map_emit_arg(ctx, map_arg, "MapHas", "map");
        char *k = m != NULL
            ? transpiler_map_emit_arg(ctx, key_arg, "MapHas", "key")
            : NULL;
        if (m == NULL || k == NULL) {
            free(m);
            free(k);
            return NULL;
        }
        const char *map_type = transpiler_expr_infer_type_name(ctx, map_arg);
        char key_buf[64];
        char value_buf[64];
        const char *key = NULL;
        const char *value = NULL;
        const char *key_infix;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapHas",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k);
            return NULL;
        }
        key_infix = transpiler_map_require_supported_key(ctx, "MapHas", key);
        if (key_infix == NULL) {
            free(m); free(k);
            return NULL;
        }
        transpiler_collection_ensure_specialization(ctx, "Map", value);
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_has%s_%s(&%s, %s)",
            key_infix,
            suffix_buf, m, k);
        free(m); free(k);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_REMOVE) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        ASTNode *key_arg = ast_call_argument(call, 1);
        if (!transpiler_require_c_addressable_storage(ctx, map_arg,
                "MapRemove", "HashMap"))
            return NULL;
        char *m = transpiler_map_emit_arg(ctx, map_arg, "MapRemove", "map");
        char *k = m != NULL
            ? transpiler_map_emit_arg(ctx, key_arg, "MapRemove", "key")
            : NULL;
        if (m == NULL || k == NULL) {
            free(m);
            free(k);
            return NULL;
        }
        const char *map_type = transpiler_expr_infer_type_name(ctx, map_arg);
        char key_buf[64];
        char value_buf[64];
        const char *key = NULL;
        const char *value = NULL;
        const char *key_infix;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapRemove",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m); free(k);
            return NULL;
        }
        key_infix = transpiler_map_require_supported_key(ctx, "MapRemove", key);
        if (key_infix == NULL) {
            free(m); free(k);
            return NULL;
        }
        transpiler_collection_ensure_specialization(ctx, "Map", value);
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_remove%s_%s(&%s, %s)",
            key_infix,
            suffix_buf, m, k);
        free(m); free(k);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_SIZE) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, map_arg,
                "MapSize", "HashMap"))
            return NULL;
        char *m = transpiler_map_emit_arg(ctx, map_arg, "MapSize", "map");
        if (m == NULL)
            return NULL;
        const char *map_type = transpiler_expr_infer_type_name(ctx, map_arg);
        char key_buf[64];
        char value_buf[64];
        const char *key = NULL;
        const char *value = NULL;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapSize",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m);
            return NULL;
        }
        (void)key;
        transpiler_collection_ensure_specialization(ctx, "Map", value);
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt("pgy_map_size_%s(&%s)",
            suffix_buf, m);
        free(m);
        return result;
    }
    if (map_op == TRANSPILER_MAP_OP_KEYS) {
        ASTNode *map_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, map_arg,
                "MapKeys", "HashMap"))
            return NULL;
        char *m = transpiler_map_emit_arg(ctx, map_arg, "MapKeys", "map");
        if (m == NULL)
            return NULL;
        const char *map_type = transpiler_expr_infer_type_name(ctx, map_arg);
        char key_buf[64];
        char value_buf[64];
        const char *key = NULL;
        const char *value = NULL;
        const char *key_infix;
        if (!transpiler_require_hashmap_type(ctx, map_type, "MapKeys",
                key_buf, sizeof(key_buf), value_buf, sizeof(value_buf),
                &key, &value)) {
            free(m);
            return NULL;
        }
        key_infix = transpiler_map_require_supported_key(ctx, "MapKeys", key);
        if (key_infix == NULL) {
            free(m);
            return NULL;
        }
        transpiler_collection_ensure_specialization(ctx, "Map", value);
        char suffix_buf[128];
        collection_runtime_suffix_copy(value, suffix_buf, sizeof(suffix_buf));
        char *result = strdup_fmt(
            "pgy_map_keys%s_%s(&%s)",
            key_infix,
            suffix_buf, m);
        free(m);
        return result;
    }
    return NULL;
}
