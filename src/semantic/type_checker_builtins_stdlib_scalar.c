/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker — scalar/string/math stdlib builtin dispatch.
 */

#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "runtime/pgy_runtime_capability.h"
#include "diag_codes.h"

static Type *
stdlib_scalar_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

typedef Type *(*StdlibScalarHandler)(ASTNode *expr, const char *name,
                                     SemanticContext *ctx);

typedef struct
{
    const char *name;
    StdlibScalarHandler handler;
} StdlibScalarSpec;

static int
stdlib_scalar_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const StdlibScalarSpec *spec = (const StdlibScalarSpec *)entry;

    return strcmp(name, spec->name);
}

static void
stdlib_scalar_require_string_arg(ASTNode *expr, size_t index,
                                 SemanticContext *ctx)
{
    ASTNode *arg = ast_call_argument(expr, index);
    require_assignable(type_check_expression(arg, ctx), TYPE_STRING, arg, ctx);
}

static Type *
stdlib_scalar_check_abs(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    return stdlib_scalar_normalize_type(
        type_check_expression(ast_call_argument(expr, 0), ctx));
}

static Type *
stdlib_scalar_check_minmax(ASTNode *expr, const char *name,
                           SemanticContext *ctx)
{
    Type *a;
    Type *b;
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    a = stdlib_scalar_normalize_type(
        type_check_expression(ast_call_argument(expr, 0), ctx));
    b = stdlib_scalar_normalize_type(
        type_check_expression(ast_call_argument(expr, 1), ctx));
    require_assignable(b, a, ast_call_argument(expr, 1), ctx);
    return a;
}

static Type *
stdlib_scalar_check_string_join(ASTNode *expr, const char *name,
                                SemanticContext *ctx)
{
    Type *arr_type;
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    arr_type = stdlib_scalar_normalize_type(
        type_check_expression(ast_call_argument(expr, 0), ctx));
    if (!type_is_constructed_named(arr_type, "Array")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE, ast_call_argument(expr, 0),
            "StringJoin requires Array<String> as first argument");
    }
    stdlib_scalar_require_string_arg(expr, 1, ctx);
    return TYPE_STRING;
}

static Type *
stdlib_scalar_check_string_length(ASTNode *expr, const char *name,
                                  SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    return TYPE_INT;
}

static Type *
stdlib_scalar_check_string_contains(ASTNode *expr, const char *name,
                                    SemanticContext *ctx)
{
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    stdlib_scalar_require_string_arg(expr, 1, ctx);
    return TYPE_BOOL;
}

static Type *
stdlib_scalar_check_string_index_of(ASTNode *expr, const char *name,
                                    SemanticContext *ctx)
{
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    stdlib_scalar_require_string_arg(expr, 1, ctx);
    return TYPE_INT;
}

static Type *
stdlib_scalar_check_string_replace(ASTNode *expr, const char *name,
                                   SemanticContext *ctx)
{
    if (!check_call_arity(expr, 3, name, ctx))
        return TYPE_UNKNOWN;
    for (size_t i = 0; i < 3; i++)
        stdlib_scalar_require_string_arg(expr, i, ctx);
    return TYPE_STRING;
}

static Type *
stdlib_scalar_check_string_substring(ASTNode *expr, const char *name,
                                     SemanticContext *ctx)
{
    if (!check_call_arity(expr, 3, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 2), ctx),
        TYPE_INT, ast_call_argument(expr, 2), ctx);
    return TYPE_STRING;
}

/* SubEquals(s: String, start: Int, len: Int, other: String) -> Bool --
 * allocation-free Substring(s, start, len) == other. */
static Type *
stdlib_scalar_check_string_sub_equals(ASTNode *expr, const char *name,
                                      SemanticContext *ctx)
{
    if (!check_call_arity(expr, 4, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 2), ctx),
        TYPE_INT, ast_call_argument(expr, 2), ctx);
    stdlib_scalar_require_string_arg(expr, 3, ctx);
    return TYPE_BOOL;
}

/* SubEqualsWithLen(s: String, sourceLen: Int, start: Int, len: Int,
 * other: String) -> Bool -- same window equality, consuming an existing source
 * length fact instead of re-scanning s. */
static Type *
stdlib_scalar_check_string_sub_equals_with_len(ASTNode *expr, const char *name,
                                               SemanticContext *ctx)
{
    if (!check_call_arity(expr, 5, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 2), ctx),
        TYPE_INT, ast_call_argument(expr, 2), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 3), ctx),
        TYPE_INT, ast_call_argument(expr, 3), ctx);
    stdlib_scalar_require_string_arg(expr, 4, ctx);
    return TYPE_BOOL;
}

/* CharCode(s: String, len: Int, i: Int) -> Int -- O(1) byte at index i. */
static Type *
stdlib_scalar_check_string_char_code(ASTNode *expr, const char *name,
                                     SemanticContext *ctx)
{
    if (!check_call_arity(expr, 3, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 2), ctx),
        TYPE_INT, ast_call_argument(expr, 2), ctx);
    return TYPE_INT;
}

/* SubStartsWith(s: String, start: Int, prefix: String) -> Bool --
 * allocation-free "s[start..] begins with prefix". */
static Type *
stdlib_scalar_check_string_sub_starts_with(ASTNode *expr, const char *name,
                                           SemanticContext *ctx)
{
    if (!check_call_arity(expr, 3, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    stdlib_scalar_require_string_arg(expr, 2, ctx);
    return TYPE_BOOL;
}

static Type *
stdlib_scalar_check_string_sub_starts_with_len(ASTNode *expr, const char *name,
                                               SemanticContext *ctx)
{
    if (!check_call_arity(expr, 4, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 2), ctx),
        TYPE_INT, ast_call_argument(expr, 2), ctx);
    stdlib_scalar_require_string_arg(expr, 3, ctx);
    return TYPE_BOOL;
}

/* SubIndexOf(s: String, start: Int, len: Int, needle: String) -> Int --
 * allocation-free StringIndexOf(Substring(s, start, len), needle). */
static Type *
stdlib_scalar_check_string_sub_index_of(ASTNode *expr, const char *name,
                                        SemanticContext *ctx)
{
    if (!check_call_arity(expr, 4, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 2), ctx),
        TYPE_INT, ast_call_argument(expr, 2), ctx);
    stdlib_scalar_require_string_arg(expr, 3, ctx);
    return TYPE_INT;
}

static Type *
stdlib_scalar_check_string_sub_index_of_with_len(ASTNode *expr, const char *name,
                                                 SemanticContext *ctx)
{
    if (!check_call_arity(expr, 5, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 1), ctx),
        TYPE_INT, ast_call_argument(expr, 1), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 2), ctx),
        TYPE_INT, ast_call_argument(expr, 2), ctx);
    require_assignable(type_check_expression(ast_call_argument(expr, 3), ctx),
        TYPE_INT, ast_call_argument(expr, 3), ctx);
    stdlib_scalar_require_string_arg(expr, 4, ctx);
    return TYPE_INT;
}

static Type *
stdlib_scalar_check_string_unary(ASTNode *expr, const char *name,
                                 SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    return TYPE_STRING;
}

static Type *
stdlib_scalar_check_string_concat(ASTNode *expr, const char *name,
                                  SemanticContext *ctx)
{
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    stdlib_scalar_require_string_arg(expr, 1, ctx);
    return TYPE_STRING;
}

static Type *
stdlib_scalar_check_string_split(ASTNode *expr, const char *name,
                                 SemanticContext *ctx)
{
    Type *args[1] = { TYPE_STRING };
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    stdlib_scalar_require_string_arg(expr, 0, ctx);
    stdlib_scalar_require_string_arg(expr, 1, ctx);
    return type_create_constructed(TYPE_ARRAY, args, 1);
}

static Type *
stdlib_scalar_check_to_int(ASTNode *expr, const char *name,
                           SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    type_check_expression(ast_call_argument(expr, 0), ctx);
    return TYPE_INT;
}

static Type *
stdlib_scalar_check_to_float(ASTNode *expr, const char *name,
                             SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    type_check_expression(ast_call_argument(expr, 0), ctx);
    return TYPE_FLOAT;
}

static Type *
stdlib_scalar_check_math_unary_float(ASTNode *expr, const char *name,
                                     SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    type_check_expression(ast_call_argument(expr, 0), ctx);
    return TYPE_FLOAT;
}

static Type *
stdlib_scalar_check_atan2(ASTNode *expr, const char *name,
                          SemanticContext *ctx)
{
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    type_check_expression(ast_call_argument(expr, 0), ctx);
    type_check_expression(ast_call_argument(expr, 1), ctx);
    return TYPE_FLOAT;
}

static Type *
stdlib_scalar_check_clamp(ASTNode *expr, const char *name,
                          SemanticContext *ctx)
{
    Type *val;
    if (!check_call_arity(expr, 3, name, ctx))
        return TYPE_UNKNOWN;
    val = stdlib_scalar_normalize_type(
        type_check_expression(ast_call_argument(expr, 0), ctx));
    type_check_expression(ast_call_argument(expr, 1), ctx);
    type_check_expression(ast_call_argument(expr, 2), ctx);
    return val;
}

static Type *
stdlib_scalar_check_float_const(ASTNode *expr, const char *name,
                                SemanticContext *ctx)
{
    (void)expr;
    (void)name;
    (void)ctx;
    return TYPE_FLOAT;
}

static Type *
stdlib_scalar_check_pow(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    if (!check_call_arity(expr, 2, name, ctx))
        return TYPE_UNKNOWN;
    type_check_expression(ast_call_argument(expr, 0), ctx);
    type_check_expression(ast_call_argument(expr, 1), ctx);
    return TYPE_FLOAT;
}

static Type *
stdlib_scalar_check_random(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    (void)name;
    if (ast_call_arg_count(expr) > 0)
        type_check_expression(ast_call_argument(expr, 0), ctx);
    semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
    semantic_record_capability(ctx, PGY_CAP_RANDOM);
    return TYPE_INT;
}

static Type *
stdlib_scalar_check_seed_random(ASTNode *expr, const char *name,
                                SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    require_assignable(type_check_expression(ast_call_argument(expr, 0), ctx),
        TYPE_INT, ast_call_argument(expr, 0), ctx);
    semantic_record_effect(ctx, EFFECT_NONDETERMINISTIC);
    semantic_record_capability(ctx, PGY_CAP_RANDOM);
    return TYPE_VOID;
}

static Type *
stdlib_scalar_check_exit(ASTNode *expr, const char *name, SemanticContext *ctx)
{
    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    require_assignable(type_check_expression(ast_call_argument(expr, 0), ctx),
        TYPE_INT, ast_call_argument(expr, 0), ctx);
    return TYPE_VOID;
}

static const StdlibScalarSpec stdlib_scalar_specs[] = {
    { "Abs", stdlib_scalar_check_abs },
    { "Acos", stdlib_scalar_check_math_unary_float },
    { "Asin", stdlib_scalar_check_math_unary_float },
    { "Atan", stdlib_scalar_check_math_unary_float },
    { "Atan2", stdlib_scalar_check_atan2 },
    { "Ceil", stdlib_scalar_check_math_unary_float },
    { "CharAtN", stdlib_scalar_check_string_substring },
    { "CharCode", stdlib_scalar_check_string_char_code },
    { "Clamp", stdlib_scalar_check_clamp },
    { "Concat", stdlib_scalar_check_string_concat },
    { "Contains", stdlib_scalar_check_string_contains },
    { "Cos", stdlib_scalar_check_math_unary_float },
    { "E", stdlib_scalar_check_float_const },
    { "Exit", stdlib_scalar_check_exit },
    { "Exp", stdlib_scalar_check_math_unary_float },
    { "Floor", stdlib_scalar_check_math_unary_float },
    { "Join", stdlib_scalar_check_string_join },
    { "Log10", stdlib_scalar_check_math_unary_float },
    { "Log2", stdlib_scalar_check_math_unary_float },
    { "Lower", stdlib_scalar_check_string_unary },
    { "MathLog", stdlib_scalar_check_math_unary_float },
    { "Max", stdlib_scalar_check_minmax },
    { "Min", stdlib_scalar_check_minmax },
    { "PI", stdlib_scalar_check_float_const },
    { "Pow", stdlib_scalar_check_pow },
    { "Random", stdlib_scalar_check_random },
    { "Replace", stdlib_scalar_check_string_replace },
    { "Round", stdlib_scalar_check_math_unary_float },
    { "SeedRandom", stdlib_scalar_check_seed_random },
    { "Sin", stdlib_scalar_check_math_unary_float },
    { "Split", stdlib_scalar_check_string_split },
    { "Sqrt", stdlib_scalar_check_math_unary_float },
    { "StringConcat", stdlib_scalar_check_string_concat },
    { "StringContains", stdlib_scalar_check_string_contains },
    { "StringIndexOf", stdlib_scalar_check_string_index_of },
    { "StringJoin", stdlib_scalar_check_string_join },
    { "StringLength", stdlib_scalar_check_string_length },
    { "StringReplace", stdlib_scalar_check_string_replace },
    { "StringSplit", stdlib_scalar_check_string_split },
    { "StringTrim", stdlib_scalar_check_string_unary },
    { "SubContains", stdlib_scalar_check_string_sub_equals },
    { "SubContainsWithLen", stdlib_scalar_check_string_sub_equals_with_len },
    { "SubEquals", stdlib_scalar_check_string_sub_equals },
    { "SubEqualsWithLen", stdlib_scalar_check_string_sub_equals_with_len },
    { "SubIndexOf", stdlib_scalar_check_string_sub_index_of },
    { "SubIndexOfWithLen", stdlib_scalar_check_string_sub_index_of_with_len },
    { "SubStartsWith", stdlib_scalar_check_string_sub_starts_with },
    { "SubStartsWithLen", stdlib_scalar_check_string_sub_starts_with_len },
    { "Substring", stdlib_scalar_check_string_substring },
    { "Tan", stdlib_scalar_check_math_unary_float },
    { "ToFloat", stdlib_scalar_check_to_float },
    { "ToInt", stdlib_scalar_check_to_int },
    { "ToLower", stdlib_scalar_check_string_unary },
    { "ToUpper", stdlib_scalar_check_string_unary },
    { "Trim", stdlib_scalar_check_string_unary },
    { "Upper", stdlib_scalar_check_string_unary },
};

static const StdlibScalarSpec *
stdlib_scalar_find_spec(const char *name)
{
    const StdlibScalarSpec *match;

    if (name == NULL)
        return NULL;
    match = (const StdlibScalarSpec *)bsearch(
        &name, stdlib_scalar_specs,
        sizeof(stdlib_scalar_specs) / sizeof(stdlib_scalar_specs[0]),
        sizeof(stdlib_scalar_specs[0]), stdlib_scalar_compare);
    return match;
}

Type *
type_check_stdlib_scalar_call(ASTNode *expr, const char *name,
                              SemanticContext *ctx, bool *handled_out)
{
    const StdlibScalarSpec *spec = stdlib_scalar_find_spec(name);

    if (handled_out != NULL)
        *handled_out = spec != NULL;
    if (spec == NULL)
        return TYPE_UNKNOWN;
    if (spec->handler == NULL)
        return TYPE_UNKNOWN;
    return spec->handler(expr, name, ctx);
}
