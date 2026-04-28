/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — scalar/string/math stdlib builtin dispatch.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "diag_codes.h"

Type *
type_check_stdlib_scalar_call(ASTNode *expr, const char *name,
                              SemanticContext *ctx, bool *handled_out)
{
    if (handled_out != NULL)
        *handled_out = true;

    if (strcmp(name, "Abs") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return type_check_expression(expr->data.call.arguments[0], ctx);
    }
    if (strcmp(name, "Min") == 0 || strcmp(name, "Max") == 0) {
        Type *a;
        Type *b;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        a = type_check_expression(expr->data.call.arguments[0], ctx);
        b = type_check_expression(expr->data.call.arguments[1], ctx);
        require_assignable(b, a, expr->data.call.arguments[1], ctx);
        return a;
    }
    if (strcmp(name, "StringLength") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "Contains") == 0 || strcmp(name, "StringContains") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    }
    if (strcmp(name, "Replace") == 0 || strcmp(name, "StringReplace") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        for (size_t i = 0; i < 3; i++) {
            require_assignable(type_check_expression(expr->data.call.arguments[i], ctx),
                TYPE_STRING, expr->data.call.arguments[i], ctx);
        }
        return TYPE_STRING;
    }
    if (strcmp(name, "Substring") == 0) {
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[2], ctx),
            TYPE_INT, expr->data.call.arguments[2], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Trim") == 0 || strcmp(name, "StringTrim") == 0
        || strcmp(name, "Upper") == 0 || strcmp(name, "ToUpper") == 0
        || strcmp(name, "Lower") == 0 || strcmp(name, "ToLower") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "Concat") == 0 || strcmp(name, "StringConcat") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "StringSplit") == 0 || strcmp(name, "Split") == 0) {
        Type *args[1] = { TYPE_STRING };
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_STRING, expr->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return type_create_constructed(TYPE_ARRAY, args, 1);
    }
    if (strcmp(name, "StringJoin") == 0 || strcmp(name, "Join") == 0) {
        Type *arr_type;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        arr_type = type_check_expression(expr->data.call.arguments[0], ctx);
        if (!type_is_constructed_named(arr_type, "Array")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "StringJoin requires Array<String> as first argument");
        }
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_STRING, expr->data.call.arguments[1], ctx);
        return TYPE_STRING;
    }
    if (strcmp(name, "ToInt") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "ToFloat") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Sqrt") == 0 || strcmp(name, "Floor") == 0
        || strcmp(name, "Ceil") == 0
        || strcmp(name, "Sin") == 0 || strcmp(name, "Cos") == 0
        || strcmp(name, "Tan") == 0 || strcmp(name, "Asin") == 0
        || strcmp(name, "Acos") == 0 || strcmp(name, "Atan") == 0
        || strcmp(name, "Exp") == 0 || strcmp(name, "MathLog") == 0
        || strcmp(name, "Log10") == 0 || strcmp(name, "Log2") == 0
        || strcmp(name, "Round") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Atan2") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Clamp") == 0) {
        Type *val;
        if (!check_call_arity(expr, 3, name, ctx))
            return TYPE_UNKNOWN;
        val = type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        type_check_expression(expr->data.call.arguments[2], ctx);
        return val;
    }
    if (strcmp(name, "PI") == 0 || strcmp(name, "E") == 0)
        return TYPE_FLOAT;
    if (strcmp(name, "Pow") == 0) {
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        type_check_expression(expr->data.call.arguments[1], ctx);
        return TYPE_FLOAT;
    }
    if (strcmp(name, "Random") == 0) {
        if (expr->data.call.arg_count > 0)
            type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_INT;
    }
    if (strcmp(name, "SeedRandom") == 0) {
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        require_assignable(type_check_expression(expr->data.call.arguments[0], ctx),
            TYPE_INT, expr->data.call.arguments[0], ctx);
        semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
        return TYPE_VOID;
    }

    if (handled_out != NULL)
        *handled_out = false;
    return TYPE_UNKNOWN;
}
