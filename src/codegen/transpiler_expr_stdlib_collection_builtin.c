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
#include "codegen_hashmap_key_policy.h"
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
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "List", inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(inner, suffix_buf, sizeof(suffix_buf));
        return strdup_fmt("pgy_list_new_%s()", suffix_buf);
    }
    if (op == TRANSPILER_COLLECTION_OP_LIST_PUSH) {
        ASTNode *list_arg = ast_call_argument(call, 0);
        char *l = emit_expression(list_arg, ctx);
        char *v = emit_expression(ast_call_argument(call, 1), ctx);
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListPush", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(v);
            return pergyra_strdup("0");
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
        char *l = emit_expression(list_arg, ctx);
        char *i = emit_expression(ast_call_argument(call, 1), ctx);
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListGet", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i);
            return pergyra_strdup("0");
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
        char *l = emit_expression(list_arg, ctx);
        char *i = emit_expression(ast_call_argument(call, 1), ctx);
        char *v = emit_expression(ast_call_argument(call, 2), ctx);
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListSet", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i); free(v);
            return pergyra_strdup("0");
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
        char *l = emit_expression(list_arg, ctx);
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListSize", inner_buf, sizeof(inner_buf), &inner)) {
            free(l);
            return pergyra_strdup("0");
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
        char *l = emit_expression(list_arg, ctx);
        char *i = emit_expression(ast_call_argument(call, 1), ctx);
        const char *list_type = infer_expression_type_name(ctx, list_arg);
        char inner_buf[64];
        const char *inner = NULL;
        if (!transpiler_require_unary_collection_type(ctx, list_type,
                "List", "ListRemove", inner_buf, sizeof(inner_buf), &inner)) {
            free(l); free(i);
            return pergyra_strdup("0");
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
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        return strdup_fmt("pgy_set_new_%s()", suffix_buf);
    }
    if (op == TRANSPILER_COLLECTION_OP_SET_ADD) {
        ASTNode *set_arg = ast_call_argument(call, 0);
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(set_arg, ctx);
        char *k = emit_expression(ast_call_argument(call, 1), ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetAdd", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return pergyra_strdup("0");
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
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(set_arg, ctx);
        char *k = emit_expression(ast_call_argument(call, 1), ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetHas", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return pergyra_strdup("0");
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
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(set_arg, ctx);
        char *k = emit_expression(ast_call_argument(call, 1), ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetRemove", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s); free(k);
            return pergyra_strdup("0");
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
        const char *set_type = infer_expression_type_name(ctx, set_arg);
        char inner_buf[64];
        const char *set_inner = NULL;
        char *s = emit_expression(set_arg, ctx);
        if (!transpiler_require_unary_collection_type(ctx, set_type,
                "Set", "SetSize", inner_buf, sizeof(inner_buf), &set_inner)) {
            free(s);
            return pergyra_strdup("0");
        }
        ensure_collection_specialization(ctx, "Set", set_inner);
        char suffix_buf[128];
        collection_runtime_suffix_copy(set_inner, suffix_buf, sizeof(suffix_buf));
        char *r = strdup_fmt("pgy_set_size_%s(&%s)",
            suffix_buf, s);
        free(s); return r;
    }
    return NULL;
}
