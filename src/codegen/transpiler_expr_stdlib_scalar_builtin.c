/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend scalar, math, and string stdlib call lowering.
 */

#include "transpiler_expr_stdlib_scalar_builtin.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_format.h"

typedef struct TranspilerScalarUnarySpec {
    const char *name;
} TranspilerScalarUnarySpec;

static int
transpiler_scalar_unary_spec_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const TranspilerScalarUnarySpec *spec =
        (const TranspilerScalarUnarySpec *)entry;

    return strcmp(name, spec->name);
}

static bool
transpiler_scalar_unary_builtin_name(const char *fn)
{
    static const TranspilerScalarUnarySpec specs[] = {
        { "Acos" },
        { "Asin" },
        { "Atan" },
        { "Ceil" },
        { "Cos" },
        { "Exp" },
        { "Floor" },
        { "Log10" },
        { "Log2" },
        { "MathLog" },
        { "Round" },
        { "Sin" },
        { "Tan" },
    };

    if (fn == NULL)
        return false;

    return bsearch(&fn, specs, sizeof(specs) / sizeof(specs[0]),
        sizeof(specs[0]), transpiler_scalar_unary_spec_compare) != NULL;
}

typedef enum {
    TRANSPILER_SCALAR_OP_NONE = 0,
    TRANSPILER_SCALAR_OP_ABS,
    TRANSPILER_SCALAR_OP_ATAN2,
    TRANSPILER_SCALAR_OP_CLAMP,
    TRANSPILER_SCALAR_OP_CONCAT,
    TRANSPILER_SCALAR_OP_E,
    TRANSPILER_SCALAR_OP_EXIT,
    TRANSPILER_SCALAR_OP_LOWER,
    TRANSPILER_SCALAR_OP_MAX,
    TRANSPILER_SCALAR_OP_MIN,
    TRANSPILER_SCALAR_OP_PI,
    TRANSPILER_SCALAR_OP_POW,
    TRANSPILER_SCALAR_OP_RANDOM,
    TRANSPILER_SCALAR_OP_REPLACE,
    TRANSPILER_SCALAR_OP_SEED_RANDOM,
    TRANSPILER_SCALAR_OP_SPLIT,
    TRANSPILER_SCALAR_OP_SQRT,
    TRANSPILER_SCALAR_OP_STRING_CONTAINS,
    TRANSPILER_SCALAR_OP_STRING_INDEX_OF,
    TRANSPILER_SCALAR_OP_STRING_JOIN,
    TRANSPILER_SCALAR_OP_STRING_LENGTH,
    TRANSPILER_SCALAR_OP_STRING_TRIM,
    TRANSPILER_SCALAR_OP_SUBSTRING,
    TRANSPILER_SCALAR_OP_TO_FLOAT,
    TRANSPILER_SCALAR_OP_TO_INT,
    TRANSPILER_SCALAR_OP_UPPER,
} TranspilerScalarOp;

typedef struct {
    const char *name;
    size_t argc;
    TranspilerScalarOp op;
} TranspilerScalarSpec;

static const TranspilerScalarSpec kTranspilerScalarSpecs[] = {
    {"Abs", 1, TRANSPILER_SCALAR_OP_ABS},
    {"Atan2", 2, TRANSPILER_SCALAR_OP_ATAN2},
    {"Clamp", 3, TRANSPILER_SCALAR_OP_CLAMP},
    {"Concat", 2, TRANSPILER_SCALAR_OP_CONCAT},
    {"Contains", 2, TRANSPILER_SCALAR_OP_STRING_CONTAINS},
    {"E", 0, TRANSPILER_SCALAR_OP_E},
    {"Exit", 1, TRANSPILER_SCALAR_OP_EXIT},
    {"Join", 2, TRANSPILER_SCALAR_OP_STRING_JOIN},
    {"Lower", 1, TRANSPILER_SCALAR_OP_LOWER},
    {"Max", 2, TRANSPILER_SCALAR_OP_MAX},
    {"Min", 2, TRANSPILER_SCALAR_OP_MIN},
    {"PI", (size_t)-1, TRANSPILER_SCALAR_OP_PI},
    {"Pow", 2, TRANSPILER_SCALAR_OP_POW},
    {"Random", (size_t)-1, TRANSPILER_SCALAR_OP_RANDOM},
    {"Replace", 3, TRANSPILER_SCALAR_OP_REPLACE},
    {"SeedRandom", 1, TRANSPILER_SCALAR_OP_SEED_RANDOM},
    {"Split", 2, TRANSPILER_SCALAR_OP_SPLIT},
    {"Sqrt", 1, TRANSPILER_SCALAR_OP_SQRT},
    {"StringConcat", 2, TRANSPILER_SCALAR_OP_CONCAT},
    {"StringContains", 2, TRANSPILER_SCALAR_OP_STRING_CONTAINS},
    {"StringIndexOf", 2, TRANSPILER_SCALAR_OP_STRING_INDEX_OF},
    {"StringJoin", 2, TRANSPILER_SCALAR_OP_STRING_JOIN},
    {"StringLength", 1, TRANSPILER_SCALAR_OP_STRING_LENGTH},
    {"StringReplace", 3, TRANSPILER_SCALAR_OP_REPLACE},
    {"StringSplit", 2, TRANSPILER_SCALAR_OP_SPLIT},
    {"StringTrim", 1, TRANSPILER_SCALAR_OP_STRING_TRIM},
    {"Substring", 3, TRANSPILER_SCALAR_OP_SUBSTRING},
    {"ToFloat", 1, TRANSPILER_SCALAR_OP_TO_FLOAT},
    {"ToInt", 1, TRANSPILER_SCALAR_OP_TO_INT},
    {"ToLower", 1, TRANSPILER_SCALAR_OP_LOWER},
    {"ToUpper", 1, TRANSPILER_SCALAR_OP_UPPER},
    {"Trim", 1, TRANSPILER_SCALAR_OP_STRING_TRIM},
    {"Upper", 1, TRANSPILER_SCALAR_OP_UPPER},
};

static int
transpiler_scalar_spec_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const TranspilerScalarSpec *spec = (const TranspilerScalarSpec *)entry;
    return strcmp(name, spec->name);
}

static TranspilerScalarOp
transpiler_scalar_lookup(const char *fn, size_t argc)
{
    const TranspilerScalarSpec *spec;

    if (fn == NULL)
        return TRANSPILER_SCALAR_OP_NONE;
    spec = (const TranspilerScalarSpec *)bsearch(
        fn,
        kTranspilerScalarSpecs,
        sizeof(kTranspilerScalarSpecs) / sizeof(kTranspilerScalarSpecs[0]),
        sizeof(kTranspilerScalarSpecs[0]),
        transpiler_scalar_spec_compare);
    if (spec == NULL)
        return TRANSPILER_SCALAR_OP_NONE;
    if (spec->argc != (size_t)-1 && spec->argc != argc)
        return TRANSPILER_SCALAR_OP_NONE;
    return spec->op;
}

char *
emit_call_stdlib_scalar_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    TranspilerScalarOp op = transpiler_scalar_lookup(fn, argc);
    ASTNode *a0 = ast_call_argument(call, 0);
    ASTNode *a1 = ast_call_argument(call, 1);
    ASTNode *a2 = ast_call_argument(call, 2);

    if (op == TRANSPILER_SCALAR_OP_ABS) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("((%s) < 0 ? -(%s) : (%s))", arg, arg, arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_MIN) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("((%s) < (%s) ? (%s) : (%s))", a, b, a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_MAX) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("((%s) > (%s) ? (%s) : (%s))", a, b, a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_LENGTH) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_CONTAINS) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringContains(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_INDEX_OF) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringIndexOf(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_EXIT) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("pgy_exit(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_REPLACE) {
        char *s = emit_expression(a0, ctx);
        char *old_s = emit_expression(a1, ctx);
        char *new_s = emit_expression(a2, ctx);
        char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
        free(s); free(old_s); free(new_s);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUBSTRING) {
        char *s = emit_expression(a0, ctx);
        char *start = emit_expression(a1, ctx);
        char *len = emit_expression(a2, ctx);
        char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
        free(s); free(start); free(len);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_TRIM) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("StringTrim(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_UPPER) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToUpper(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_LOWER) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToLower(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_CONCAT) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SPLIT) {
        char *s = emit_expression(a0, ctx);
        char *d = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
        free(s); free(d);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_JOIN) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "StringJoin", "Array"))
            return pergyra_strdup("0");
        char *arr = emit_expression(a0, ctx);
        char *sep = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
        free(arr); free(sep);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_TO_INT) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToInt(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_TO_FLOAT) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToFloat(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SQRT) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("Sqrt(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_POW) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("Pow(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (transpiler_scalar_unary_builtin_name(fn)
        && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("%s(%s)", fn, arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_ATAN2) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("Atan2(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_CLAMP) {
        char *val = emit_expression(a0, ctx);
        char *lo = emit_expression(a1, ctx);
        char *hi = emit_expression(a2, ctx);
        char *result = strdup_fmt("Clamp(%s, %s, %s)", val, lo, hi);
        free(val); free(lo); free(hi);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_PI)
        return pergyra_strdup("PGY_PI");
    if (op == TRANSPILER_SCALAR_OP_E)
        return pergyra_strdup("PGY_E");
    if (op == TRANSPILER_SCALAR_OP_RANDOM) {
        if (argc >= 1) {
            char *arg = emit_expression(a0, ctx);
            char *result = strdup_fmt("Random(%s)", arg);
            free(arg);
            return result;
        }
        return pergyra_strdup("Random(100)");
    }
    if (op == TRANSPILER_SCALAR_OP_SEED_RANDOM) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("SeedRandom(%s)", arg);
        free(arg);
        return result;
    }
    return NULL;
}
