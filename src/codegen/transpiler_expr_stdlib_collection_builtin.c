/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend List/Set/Queue/HashMap stdlib call lowering.
 */

#include "transpiler_expr_stdlib_collection_builtin.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_collection_runtime_suffix.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_format.h"
#include "transpiler_nominal.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

#define infer_expression_type_name \
    transpiler_expr_infer_type_name
#define ensure_collection_specialization \
    transpiler_collection_ensure_specialization

typedef enum {
    TRANSPILER_COLLECTION_OP_NONE = 0,
    TRANSPILER_COLLECTION_OP_LIST_GET,
    TRANSPILER_COLLECTION_OP_LIST_NEW,
    TRANSPILER_COLLECTION_OP_LIST_PUSH,
    TRANSPILER_COLLECTION_OP_LIST_REMOVE,
    TRANSPILER_COLLECTION_OP_LIST_SET,
    TRANSPILER_COLLECTION_OP_LIST_SIZE,
    TRANSPILER_COLLECTION_OP_SET_ADD,
    TRANSPILER_COLLECTION_OP_SET_HAS,
    TRANSPILER_COLLECTION_OP_SET_NEW,
    TRANSPILER_COLLECTION_OP_SET_REMOVE,
    TRANSPILER_COLLECTION_OP_SET_SIZE,
    TRANSPILER_COLLECTION_OP_SET_VALUES,
} TranspilerCollectionOp;

typedef struct {
    const char *name;
    size_t argc;
    TranspilerCollectionOp op;
} TranspilerCollectionSpec;

static const TranspilerCollectionSpec kTranspilerCollectionSpecs[] = {
    {"ListGet", 2, TRANSPILER_COLLECTION_OP_LIST_GET},
    {"ListNew", (size_t)-1, TRANSPILER_COLLECTION_OP_LIST_NEW},
    {"ListPush", 2, TRANSPILER_COLLECTION_OP_LIST_PUSH},
    {"ListRemove", 2, TRANSPILER_COLLECTION_OP_LIST_REMOVE},
    {"ListSet", 3, TRANSPILER_COLLECTION_OP_LIST_SET},
    {"ListSize", 1, TRANSPILER_COLLECTION_OP_LIST_SIZE},
    {"SetAdd", 2, TRANSPILER_COLLECTION_OP_SET_ADD},
    {"SetHas", 2, TRANSPILER_COLLECTION_OP_SET_HAS},
    {"SetNew", (size_t)-1, TRANSPILER_COLLECTION_OP_SET_NEW},
    {"SetRemove", 2, TRANSPILER_COLLECTION_OP_SET_REMOVE},
    {"SetSize", 1, TRANSPILER_COLLECTION_OP_SET_SIZE},
    {"SetValues", 1, TRANSPILER_COLLECTION_OP_SET_VALUES},
};

static int
transpiler_collection_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerCollectionSpec *spec = (const TranspilerCollectionSpec *)entry;
    return strcmp(name, spec->name);
}

static TranspilerCollectionOp
transpiler_collection_lookup(const char *fn, size_t argc)
{
    const TranspilerCollectionSpec *spec;

    if (fn == NULL)
        return TRANSPILER_COLLECTION_OP_NONE;
    spec = (const TranspilerCollectionSpec *)bsearch(
        fn,
        kTranspilerCollectionSpecs,
        sizeof(kTranspilerCollectionSpecs) / sizeof(kTranspilerCollectionSpecs[0]),
        sizeof(kTranspilerCollectionSpecs[0]),
        transpiler_collection_spec_compare);
    if (spec == NULL)
        return TRANSPILER_COLLECTION_OP_NONE;
    if (spec->argc != (size_t)-1 && spec->argc != argc)
        return TRANSPILER_COLLECTION_OP_NONE;
    return spec->op;
}

static char *
transpiler_collection_emit_arg(TranspilerCtx *ctx,
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
        "C backend: collection builtin %s could not lower %s argument",
        builtin_name != NULL ? builtin_name : "(unknown)",
        role != NULL ? role : "operand");
    return NULL;
}

static bool
transpiler_collection_stable_set_values_supported(const char *inner)
{
    return inner != NULL
        && (strcmp(inner, "Bool") == 0
            || strcmp(inner, "Int") == 0
            || strcmp(inner, "Long") == 0
            || strcmp(inner, "String") == 0);
}

#include "transpiler_expr_stdlib_map_builtin.h"
#include "transpiler_expr_stdlib_queue_builtin.h"

char *
emit_call_stdlib_collection_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    size_t argc = call != NULL ? ast_call_arg_count(call) : 0;
    TranspilerCollectionOp op;
    char *map_builtin = emit_call_stdlib_map_builtin(fn, call, ctx);
    char *queue_builtin;
    if (map_builtin != NULL)
        return map_builtin;

    queue_builtin = emit_call_stdlib_queue_builtin(fn, call, ctx);
    if (queue_builtin != NULL)
        return queue_builtin;

    op = transpiler_collection_lookup(fn, argc);

    if (op == TRANSPILER_COLLECTION_OP_LIST_NEW) {
        const char *hint = ctx->active_type_hint;
        char inner_buf[64];
        const char *inner = NULL;
        if (hint == NULL || !transpiler_require_unary_collection_type(ctx,
                hint, "List", "ListNew", inner_buf, sizeof(inner_buf), &inner)) {
            return NULL;
        }
        ensure_collection_specialization(ctx, "List", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        return strdup_fmt("pgy_list_new_%s()", suffix_buf);
    }
    if (op == TRANSPILER_COLLECTION_OP_LIST_PUSH) {
        ASTNode *list_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, list_arg,
                "ListPush", "List"))
            return NULL;
        char *l = transpiler_collection_emit_arg(ctx, list_arg,
            "ListPush", "list");
        char *v = l != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 1),
                  "ListPush", "value")
            : NULL;
        if (l == NULL || v == NULL) {
            free(l);
            free(v);
            return NULL;
        }
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListPush", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(v);
            return NULL;
        }
        ensure_collection_specialization(ctx, "List", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_list_push_%s(&%s, %s)",
            suffix_buf, l, v);
        free(l); free(v); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_LIST_GET) {
        ASTNode *list_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, list_arg,
                "ListGet", "List"))
            return NULL;
        char *l = transpiler_collection_emit_arg(ctx, list_arg,
            "ListGet", "list");
        char *i = l != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 1),
                  "ListGet", "index")
            : NULL;
        if (l == NULL || i == NULL) {
            free(l);
            free(i);
            return NULL;
        }
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListGet", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i);
            return NULL;
        }
        ensure_collection_specialization(ctx, "List", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_list_get_%s(&%s, %s)",
            suffix_buf, l, i);
        free(l); free(i); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_LIST_SET) {
        ASTNode *list_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, list_arg,
                "ListSet", "List"))
            return NULL;
        char *l = transpiler_collection_emit_arg(ctx, list_arg,
            "ListSet", "list");
        char *i = l != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 1),
                  "ListSet", "index")
            : NULL;
        char *v = i != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 2),
                  "ListSet", "value")
            : NULL;
        if (l == NULL || i == NULL || v == NULL) {
            free(l);
            free(i);
            free(v);
            return NULL;
        }
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListSet", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i); free(v);
            return NULL;
        }
        ensure_collection_specialization(ctx, "List", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_list_set_%s(&%s, %s, %s)",
            suffix_buf, l, i, v);
        free(l); free(i); free(v); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_LIST_SIZE) {
        ASTNode *list_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, list_arg,
                "ListSize", "List"))
            return NULL;
        char *l = transpiler_collection_emit_arg(ctx, list_arg,
            "ListSize", "list");
        if (l == NULL)
            return NULL;
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListSize", inner_buf, sizeof(inner_buf), &inner)) {
            free(l);
            return NULL;
        }
        ensure_collection_specialization(ctx, "List", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_list_size_%s(&%s)",
            suffix_buf, l);
        free(l); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_LIST_REMOVE) {
        ASTNode *list_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, list_arg,
                "ListRemove", "List"))
            return NULL;
        char *l = transpiler_collection_emit_arg(ctx, list_arg,
            "ListRemove", "list");
        char *i = l != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 1),
                  "ListRemove", "index")
            : NULL;
        if (l == NULL || i == NULL) {
            free(l);
            free(i);
            return NULL;
        }
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListRemove", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i);
            return NULL;
        }
        ensure_collection_specialization(ctx, "List", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_list_remove_%s(&%s, %s)",
            suffix_buf, l, i);
        free(l); free(i); return r;
    }

    if (op == TRANSPILER_COLLECTION_OP_SET_NEW) {
        const char *hint = ctx->active_type_hint;
        char inner_buf[64];
        const char *set_inner = NULL;
        if (hint == NULL || !transpiler_require_unary_collection_type(ctx,
                hint, "Set", "SetNew", inner_buf, sizeof(inner_buf), &set_inner)) {
            return NULL;
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        return strdup_fmt("pgy_set_new_%s()", suffix_buf);
    }
    if (op == TRANSPILER_COLLECTION_OP_SET_ADD) {
        ASTNode *set_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, set_arg,
                "SetAdd", "Set"))
            return NULL;
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = transpiler_collection_emit_arg(ctx, set_arg,
            "SetAdd", "set");
        char *k = s != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 1),
                  "SetAdd", "key")
            : NULL;
        if (s == NULL || k == NULL) {
            free(s);
            free(k);
            return NULL;
        }
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetAdd", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return NULL;
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_set_add_%s(&%s, %s)",
            suffix_buf, s, k);
        free(s); free(k); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_SET_HAS) {
        ASTNode *set_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, set_arg,
                "SetHas", "Set"))
            return NULL;
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = transpiler_collection_emit_arg(ctx, set_arg,
            "SetHas", "set");
        char *k = s != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 1),
                  "SetHas", "key")
            : NULL;
        if (s == NULL || k == NULL) {
            free(s);
            free(k);
            return NULL;
        }
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetHas", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return NULL;
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_set_has_%s(&%s, %s)",
            suffix_buf, s, k);
        free(s); free(k); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_SET_REMOVE) {
        ASTNode *set_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, set_arg,
                "SetRemove", "Set"))
            return NULL;
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = transpiler_collection_emit_arg(ctx, set_arg,
            "SetRemove", "set");
        char *k = s != NULL
            ? transpiler_collection_emit_arg(ctx, ast_call_argument(call, 1),
                  "SetRemove", "key")
            : NULL;
        if (s == NULL || k == NULL) {
            free(s);
            free(k);
            return NULL;
        }
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetRemove", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return NULL;
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_set_remove_%s(&%s, %s)",
            suffix_buf, s, k);
        free(s); free(k); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_SET_SIZE) {
        ASTNode *set_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, set_arg,
                "SetSize", "Set"))
            return NULL;
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = transpiler_collection_emit_arg(ctx, set_arg,
            "SetSize", "set");
        if (s == NULL)
            return NULL;
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetSize", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s);
            return NULL;
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_set_size_%s(&%s)",
            suffix_buf, s);
        free(s); return r;
    }
    if (op == TRANSPILER_COLLECTION_OP_SET_VALUES) {
        ASTNode *set_arg = ast_call_argument(call, 0);
        if (!transpiler_require_c_addressable_storage(ctx, set_arg,
                "SetValues", "Set"))
            return NULL;
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = transpiler_collection_emit_arg(ctx, set_arg,
            "SetValues", "set");
        if (s == NULL)
            return NULL;
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetValues", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s);
            return NULL;
        }
        if (!transpiler_collection_stable_set_values_supported(set_inner)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "C backend: SetValues requires stable Set<Bool|Int|Long|String> element metadata");
            free(s);
            return NULL;
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        ensure_collection_specialization(ctx, "Array", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_set_values_%s(&%s)",
            suffix_buf, s);
        free(s); return r;
    }
    return NULL;
}
