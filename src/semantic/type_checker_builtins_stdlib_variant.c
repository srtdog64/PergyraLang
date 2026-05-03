/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker Option/Result stdlib variant builtin dispatch.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "diag_codes.h"

typedef enum StdlibVariantBuiltinKind {
    STDLIB_VARIANT_UNKNOWN = 0,
    STDLIB_VARIANT_IS_OK,
    STDLIB_VARIANT_IS_ERR,
    STDLIB_VARIANT_SOME,
    STDLIB_VARIANT_NONE,
    STDLIB_VARIANT_IS_SOME,
    STDLIB_VARIANT_IS_NONE,
    STDLIB_VARIANT_UNWRAP_OPTION,
    STDLIB_VARIANT_UNWRAP,
    STDLIB_VARIANT_UNWRAP_OR
} StdlibVariantBuiltinKind;

typedef struct StdlibVariantBuiltinSpec {
    const char *name;
    StdlibVariantBuiltinKind kind;
} StdlibVariantBuiltinSpec;

static Type *
stdlib_variant_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static StdlibVariantBuiltinKind
stdlib_variant_builtin_kind(const char *name)
{
    static const StdlibVariantBuiltinSpec specs[] = {
        { "IsErr", STDLIB_VARIANT_IS_ERR },
        { "IsNone", STDLIB_VARIANT_IS_NONE },
        { "IsOk", STDLIB_VARIANT_IS_OK },
        { "IsSome", STDLIB_VARIANT_IS_SOME },
        { "None", STDLIB_VARIANT_NONE },
        { "Some", STDLIB_VARIANT_SOME },
        { "Unwrap", STDLIB_VARIANT_UNWRAP },
        { "UnwrapOption", STDLIB_VARIANT_UNWRAP_OPTION },
        { "UnwrapOr", STDLIB_VARIANT_UNWRAP_OR }
    };
    size_t i;

    if (name == NULL)
        return STDLIB_VARIANT_UNKNOWN;
    for (i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        if (strcmp(name, specs[i].name) == 0)
            return specs[i].kind;
    }
    return STDLIB_VARIANT_UNKNOWN;
}

Type *
type_check_stdlib_variant_builtin_call(ASTNode *expr, const char *name,
                                       SemanticContext *ctx,
                                       bool *handled_out)
{
    StdlibVariantBuiltinKind kind = stdlib_variant_builtin_kind(name);

    if (kind == STDLIB_VARIANT_UNKNOWN) {
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }
    if (handled_out != NULL)
        *handled_out = true;

    switch (kind) {
    case STDLIB_VARIANT_IS_OK:
    case STDLIB_VARIANT_IS_ERR:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        type_check_expression(expr->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case STDLIB_VARIANT_SOME:
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION,
            stdlib_variant_normalize_type(type_check_expression(
                expr->data.call.arguments[0], ctx)));
    case STDLIB_VARIANT_NONE:
        if (!check_call_arity(expr, 0, name, ctx))
            return TYPE_UNKNOWN;
        return wrap_constructed(TYPE_OPTION, TYPE_UNKNOWN);
    case STDLIB_VARIANT_IS_SOME:
    case STDLIB_VARIANT_IS_NONE: {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = stdlib_variant_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        if (!type_is_constructed_named(ot, "Option")) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
                "%s requires Option<T>, got '%s'", name,
                type_name_or_unknown(ot));
            return TYPE_UNKNOWN;
        }
        return TYPE_BOOL;
    }
    case STDLIB_VARIANT_UNWRAP_OPTION: {
        Type *ot;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        ot = stdlib_variant_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        if (type_is_constructed_named(ot, "Option"))
            return stdlib_variant_normalize_type(type_get_constructed_arg(ot, 0));
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, expr->data.call.arguments[0],
            "UnwrapOption requires Option<T>, got '%s'",
            type_name_or_unknown(ot));
        return TYPE_UNKNOWN;
    }
    case STDLIB_VARIANT_UNWRAP: {
        Type *rt;
        if (!check_call_arity(expr, 1, name, ctx))
            return TYPE_UNKNOWN;
        rt = stdlib_variant_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        if (type_is_constructed_named(rt, "Result"))
            return stdlib_variant_normalize_type(type_get_constructed_arg(rt, 0));
        return TYPE_UNKNOWN;
    }
    case STDLIB_VARIANT_UNWRAP_OR: {
        Type *rt;
        if (!check_call_arity(expr, 2, name, ctx))
            return TYPE_UNKNOWN;
        rt = stdlib_variant_normalize_type(
            type_check_expression(expr->data.call.arguments[0], ctx));
        type_check_expression(expr->data.call.arguments[1], ctx);
        if (type_is_constructed_named(rt, "Result"))
            return stdlib_variant_normalize_type(type_get_constructed_arg(rt, 0));
        return TYPE_UNKNOWN;
    }
    default:
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }
}
