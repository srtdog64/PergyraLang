#ifndef PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_BUILTIN_H

/* Collection stdlib call lowering.
 * Included by transpiler_expr_stdlib_builtin.h inside transpiler.c. */

#include "codegen_hashmap_key_policy.h"

static bool
transpiler_collection_copy_type_name(char *out, size_t out_size,
                                     const char *type_name)
{
    size_t len;

    if (out == NULL || out_size == 0 || type_name == NULL)
        return false;

    len = strlen(type_name);
    if (len >= out_size)
        return false;

    memcpy(out, type_name, len + 1);
    return true;
}

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
                bool copied = transpiler_collection_copy_type_name(
                    resolved_buf, sizeof(resolved_buf), rendered);
                free(rendered);
                if (!copied) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C backend: %s resolved HashMap type is too long",
                        operation != NULL ? operation : "HashMap operation");
                    return false;
                }
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
                bool copied = transpiler_collection_copy_type_name(
                    resolved_buf, sizeof(resolved_buf), rendered);
                free(rendered);
                if (!copied) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C backend: %s resolved %s type is too long",
                        operation != NULL ? operation : "collection operation",
                        family != NULL ? family : "collection");
                    return false;
                }
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
            if (!transpiler_collection_copy_type_name(inner_buf,
                    inner_buf_size, inner)) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "C backend: %s inner %s type is too long",
                    operation != NULL ? operation : "collection operation",
                    family != NULL ? family : "collection");
                return false;
            }
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

#include "transpiler_expr_stdlib_map_builtin.h"

static char *
emit_call_stdlib_collection_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    char *map_builtin = emit_call_stdlib_map_builtin(fn, call, ctx);
    if (map_builtin != NULL)
        return map_builtin;

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

#endif /* PGY_TRANSPILER_EXPR_STDLIB_COLLECTION_BUILTIN_H */
