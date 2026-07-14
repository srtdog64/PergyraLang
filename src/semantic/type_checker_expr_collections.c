/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Collection literal and indexed-access type checking.
 */

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_diag_internal.h"
#include "diag_codes.h"

static Type *
expr_collection_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static bool
expr_collection_type_is_sequence_like(const Type *type)
{
    return type_is_constructed_named(type, "Array")
        || type_is_constructed_named(type, "Slice")
        || type_is_constructed_named(type, "List")
        || type_is_constructed_named(type, "Queue");
}

/* The scalar element types nested arrays are monomorphized for on both
 * backends (PgyArray_Array_<T>). Anything else fails closed. */
static bool
expr_collection_type_is_supported_nested_scalar(Type *type)
{
    return type_equals(type, TYPE_INT)
        || type_equals(type, TYPE_LONG)
        || type_equals(type, TYPE_FLOAT)
        || type_equals(type, TYPE_DOUBLE)
        || type_equals(type, TYPE_BOOL)
        || type_equals(type, TYPE_STRING);
}

/* Fail-closed guard for nested sequences. `elem_type` is the element type of a
 * sequence literal/annotation. Returns true (and emits a diagnostic) when the
 * nesting is beyond the supported one level (Array<Array<scalar>>): either the
 * inner element is itself a sequence (depth >= 3) or the supported one-level
 * case has an unsupported inner scalar. Unknown element types are allowed so
 * inference failures surface their own diagnostics rather than this one. */
static bool
expr_collection_reject_unsupported_nested_sequence(ASTNode *node, Type *elem_type,
                                            SemanticContext *ctx)
{
    Type *inner;

    if (elem_type == NULL || type_equals(elem_type, TYPE_UNKNOWN))
        return false;
    if (!expr_collection_type_is_sequence_like(elem_type))
        return false;

    inner = expr_collection_normalize_type(type_get_constructed_arg(elem_type, 0));
    if (inner != NULL && type_equals(inner, TYPE_UNKNOWN))
        return false;

    if (inner == NULL || expr_collection_type_is_sequence_like(inner)
        || !expr_collection_type_is_supported_nested_scalar(inner)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
            node,
            "nested array nesting deeper than two levels (or with this element "
            "type '%s') is not yet supported - use a flatter representation",
            type_name_or_unknown(elem_type));
        return true;
    }
    return false;
}

Type *
type_check_array_literal(ASTNode *expr, SemanticContext *ctx)
{
    /* A `[...]` literal is a sequence: its concrete constructor comes from the
     * binding annotation (Array by default, List/Queue when so declared). */
    Type *seq_ctor = TYPE_ARRAY;
    Type *expected_seq = ctx != NULL ? ctx->expected_collection_type : NULL;
    if (expected_seq != NULL) {
        if (type_is_constructed_named(expected_seq, "List"))
            seq_ctor = TYPE_LIST;
        else if (type_is_constructed_named(expected_seq, "Queue"))
            seq_ctor = TYPE_QUEUE;
    }

    if (ast_array_literal_count(expr) == 0) {
        /* An empty `[]` carries no element type; take the annotation directly
         * when it is a sequence, else a sequence of Unknown. */
        if (expected_seq != NULL
            && (type_is_constructed_named(expected_seq, "Array")
                || type_is_constructed_named(expected_seq, "List")
                || type_is_constructed_named(expected_seq, "Queue"))) {
            Type *expected_elem = expr_collection_normalize_type(
                type_get_constructed_arg(expected_seq, 0));
            if (expr_collection_reject_unsupported_nested_sequence(expr, expected_elem,
                    ctx))
                return TYPE_UNKNOWN;
            return expected_seq;
        }
        return wrap_constructed(seq_ctor, TYPE_UNKNOWN);
    }

    Type *elem_type = type_check_expression(ast_array_literal_element(expr, 0), ctx);
    reject_borrowed_array_literal_store(
        ast_array_literal_element(expr, 0), elem_type, ctx);
    if (elem_type == NULL)
        elem_type = TYPE_UNKNOWN;
    if (type_equals(elem_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
            ast_array_literal_element(expr, 0),
            "Void expression cannot be stored as an array literal element; split the side effect before constructing the array");
        elem_type = TYPE_UNKNOWN;
    }

    for (size_t i = 1; i < ast_array_literal_count(expr); i++) {
        Type *next = type_check_expression(ast_array_literal_element(expr, i), ctx);
        reject_borrowed_array_literal_store(
            ast_array_literal_element(expr, i), next, ctx);
        if (next == NULL)
            next = TYPE_UNKNOWN;
        if (type_equals(next, TYPE_VOID)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                ast_array_literal_element(expr, i),
                "Void expression cannot be stored as an array literal element; split the side effect before constructing the array");
            next = TYPE_UNKNOWN;
        }
        if (!type_is_assignable(next, elem_type) && !type_is_assignable(elem_type, next)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                ast_array_literal_element(expr, i),
                "Array literal element type mismatch: expected '%s', got '%s'",
                type_name_or_unknown(elem_type),
                type_name_or_unknown(next));
            elem_type = TYPE_UNKNOWN;
        }
    }

    if (expr_collection_reject_unsupported_nested_sequence(expr, elem_type, ctx))
        return TYPE_UNKNOWN;

    return wrap_constructed(seq_ctor, elem_type);
}

Type *
type_check_set_literal(ASTNode *expr, SemanticContext *ctx)
{
    /* An empty `{}` carries no element type; defer to the binding annotation
     * by reporting Unknown, which is assignable to any Set<T> (mirrors the
     * empty-map rule). */
    if (ast_set_literal_count(expr) == 0)
        return TYPE_UNKNOWN;

    Type *elem_type = type_check_expression(ast_set_literal_element(expr, 0), ctx);
    if (elem_type == NULL)
        elem_type = TYPE_UNKNOWN;
    if (type_equals(elem_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
            ast_set_literal_element(expr, 0),
            "Void expression cannot be stored as a set literal element; split the side effect before constructing the set");
        elem_type = TYPE_UNKNOWN;
    }

    for (size_t i = 1; i < ast_set_literal_count(expr); i++) {
        Type *next = type_check_expression(ast_set_literal_element(expr, i), ctx);
        if (next == NULL)
            next = TYPE_UNKNOWN;
        if (!type_is_assignable(next, elem_type) && !type_is_assignable(elem_type, next)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
                PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES,
                ast_set_literal_element(expr, i),
                "Set literal element type mismatch: expected '%s', got '%s'",
                type_name_or_unknown(elem_type),
                type_name_or_unknown(next));
            elem_type = TYPE_UNKNOWN;
        }
    }

    return wrap_constructed(TYPE_SET, elem_type);
}

static void
map_literal_unify_entry(SemanticContext *ctx, ASTNode *expr, size_t i,
                        Type **key_type, Type **value_type,
                        Type *k, Type *v)
{
    if (k == NULL)
        k = TYPE_UNKNOWN;
    if (v == NULL)
        v = TYPE_UNKNOWN;
    if (i == 0) {
        *key_type = k;
        *value_type = v;
        return;
    }
    if (!type_is_assignable(k, *key_type) && !type_is_assignable(*key_type, k)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES, ast_map_literal_key(expr, i),
            "Map literal key type mismatch: expected '%s', got '%s'",
            type_name_or_unknown(*key_type), type_name_or_unknown(k));
        *key_type = TYPE_UNKNOWN;
    }
    if (!type_is_assignable(v, *value_type)
        && !type_is_assignable(*value_type, v)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_LITERAL_ELEMENT_TYPE_MISMATCH,
            PGY_FIX_ALIGN_ARRAY_ELEMENT_TYPES, ast_map_literal_value(expr, i),
            "Map literal value type mismatch: expected '%s', got '%s'",
            type_name_or_unknown(*value_type), type_name_or_unknown(v));
        *value_type = TYPE_UNKNOWN;
    }
}

Type *
type_check_map_literal(ASTNode *expr, SemanticContext *ctx)
{
    size_t n = ast_map_literal_count(expr);
    Type *key_type = TYPE_UNKNOWN;
    Type *value_type = TYPE_UNKNOWN;
    Type *args[2];

    /* An empty `{}` carries no entry types; defer to the binding annotation
     * by reporting Unknown, which is assignable to any HashMap<K, V>. */
    if (n == 0)
        return TYPE_UNKNOWN;

    for (size_t i = 0; i < n; i++) {
        Type *k = expr_collection_normalize_type(
            type_check_expression(ast_map_literal_key(expr, i), ctx));
        Type *v = expr_collection_normalize_type(
            type_check_expression(ast_map_literal_value(expr, i), ctx));
        map_literal_unify_entry(ctx, expr, i, &key_type, &value_type, k, v);
    }
    args[0] = key_type;
    args[1] = value_type;
    return type_create_constructed(TYPE_HASHMAP, args, 2);
}

Type *
type_check_array_access(ASTNode *expr, SemanticContext *ctx)
{
    ASTNode *array_node = ast_array_access_array(expr);
    ASTNode *index_node = ast_array_access_index(expr);
    Type *object_type = expr_collection_normalize_type(
        type_check_expression(array_node, ctx));
    Type *index_type  = expr_collection_normalize_type(
        type_check_expression(index_node, ctx));

    if (!type_equals(index_type, TYPE_INT)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ARRAY_ACCESS_INDEX_NON_INT, PGY_FIX_USE_INT_INDEX,
            index_node,
            "Array index must be Int, got '%s'",
            type_name_or_unknown(index_type));
        return TYPE_UNKNOWN;
    }

    if (type_is_constructed_named(object_type, "Array")
        || type_is_constructed_named(object_type, "Slice")) {
        return expr_collection_normalize_type(type_get_constructed_arg(object_type, 0));
    }

    semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_ARRAY_ACCESS_TARGET_NOT_INDEXABLE, PGY_FIX_USE_ARRAY_OR_SLICE,
        array_node,
        "Index access requires Array<T> or Slice<T>, got '%s'",
        type_name_or_unknown(object_type));
    return TYPE_UNKNOWN;
}
