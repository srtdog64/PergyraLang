/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — HashMap stdlib builtin dispatch.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_collection_policy.h"
#include "diag_codes.h"

static Type *
stdlib_map_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static void
report_unsupported_map_key(ASTNode *expr, const char *name, Type *map_type,
                           SemanticContext *ctx)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        expr->data.call.arguments[0],
        "%s currently supports only %s, got '%s'",
        name,
        type_checker_hashmap_type_policy_text(),
        map_type->name != NULL ? map_type->name : "<type>");
}

static bool
stdlib_map_key_supported(Type *key_type)
{
    return type_checker_hashmap_key_supported(key_type);
}

static void
report_expected_hashmap(ASTNode *expr, const char *name, Type *map_type,
                        SemanticContext *ctx)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
        PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
        expr->data.call.arguments[0],
        "%s expects HashMap<K, T> as first argument, got '%s'",
        name, map_type->name != NULL ? map_type->name : "<type>");
}

Type *
type_check_stdlib_map_call(ASTNode *expr, const char *name,
                           SemanticContext *ctx, bool *handled_out)
{
    if (handled_out != NULL)
        *handled_out = true;

    if (strcmp(name, "MapNew") == 0) {
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return TYPE_UNKNOWN; /* type resolved from let annotation */
    }
    if (strcmp(name, "MapSet") == 0) {
        Type *map_type;
        Type *key_type;
        Type *value_type;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        key_type = stdlib_map_normalize_type(
            type_check_expression(expr->data.call.arguments[1], ctx));
        value_type = stdlib_map_normalize_type(
            type_check_expression(expr->data.call.arguments[2], ctx));
        reject_borrowed_boundary_container_store(
            expr->data.call.arguments[2], value_type, "map", "MapSet", ctx);
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            Type *expected_key = map_type->data.constructed.args[0];
            require_assignable(key_type, expected_key,
                expr->data.call.arguments[1], ctx);
            require_assignable(value_type, map_type->data.constructed.args[1],
                expr->data.call.arguments[2], ctx);
            if (!stdlib_map_key_supported(expected_key))
                report_unsupported_map_key(expr, name, map_type, ctx);
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            report_expected_hashmap(expr, name, map_type, ctx);
        }
        return TYPE_VOID;
    }
    if (strcmp(name, "MapGet") == 0 || strcmp(name, "MapHas") == 0
        || strcmp(name, "MapRemove") == 0) {
        Type *map_type;
        Type *key_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        key_type = stdlib_map_normalize_type(
            type_check_expression(expr->data.call.arguments[1], ctx));
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            Type *expected_key = map_type->data.constructed.args[0];
            require_assignable(key_type, expected_key,
                expr->data.call.arguments[1], ctx);
            if (!stdlib_map_key_supported(expected_key))
                report_unsupported_map_key(expr, name, map_type, ctx);
            if (strcmp(name, "MapGet") == 0)
                return stdlib_map_normalize_type(
                    map_type->data.constructed.args[1]);
        } else if (map_type != NULL && map_type != TYPE_UNKNOWN) {
            report_expected_hashmap(expr, name, map_type, ctx);
        }
        if (strcmp(name, "MapGet") == 0)
            return TYPE_UNKNOWN; /* resolved from context */
        return strcmp(name, "MapHas") == 0 ? TYPE_BOOL : TYPE_VOID;
    }
    if (strcmp(name, "MapSize") == 0) {
        Type *map_type;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        if (map_type != NULL && map_type != TYPE_UNKNOWN
            && !type_is_constructed_named(map_type, "HashMap")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "MapSize expects HashMap<K, T> as first argument, got '%s'",
                map_type->name != NULL ? map_type->name : "<type>");
        }
        return TYPE_INT;
    }
    if (strcmp(name, "MapKeys") == 0) {
        Type *map_type;
        Type *key_type;
        Type *args[1];
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        map_type = stdlib_map_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        if (type_is_constructed_named(map_type, "HashMap")
            && map_type->data.constructed.arg_count == 2) {
            key_type = map_type->data.constructed.args[0];
            if (!stdlib_map_key_supported(key_type))
                report_unsupported_map_key(expr, name, map_type, ctx);
            args[0] = key_type != NULL ? key_type : TYPE_UNKNOWN;
            return type_create_constructed(TYPE_ARRAY, args, 1);
        }
        if (map_type != NULL && map_type != TYPE_UNKNOWN)
            report_expected_hashmap(expr, name, map_type, ctx);
        return TYPE_UNKNOWN;
    }

    if (handled_out != NULL)
        *handled_out = false;
    return TYPE_UNKNOWN;
}
