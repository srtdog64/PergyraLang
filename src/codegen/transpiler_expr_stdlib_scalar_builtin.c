/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend scalar and math stdlib call lowering, and the dispatch that
 * hands the string family to transpiler_expr_stdlib_scalar_string.c.
 */

#include "transpiler_expr_stdlib_scalar_builtin.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_expr_stdlib_scalar_internal.h"
#include "transpiler_expr_stdlib_scalar_unary.h"
#include "transpiler_format.h"

typedef struct {
    const char *name;
    size_t argc;
    TranspilerScalarOp op;
} TranspilerScalarSpec;

static const TranspilerScalarSpec kTranspilerScalarSpecs[] = {
    {"Abs", 1, TRANSPILER_SCALAR_OP_ABS},
    {"Atan2", 2, TRANSPILER_SCALAR_OP_ATAN2},
    {"CharAtN", 3, TRANSPILER_SCALAR_OP_CHAR_AT_N},
    {"CharCode", 3, TRANSPILER_SCALAR_OP_CHAR_CODE},
    {"CheckedAdd", 2, TRANSPILER_SCALAR_OP_CHECKED_ADD},
    {"CheckedMul", 2, TRANSPILER_SCALAR_OP_CHECKED_MUL},
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
    {"SubContains", 4, TRANSPILER_SCALAR_OP_SUB_CONTAINS},
    {"SubContainsWithLen", 5, TRANSPILER_SCALAR_OP_SUB_CONTAINS_WITH_LEN},
    {"SubEquals", 4, TRANSPILER_SCALAR_OP_SUB_EQUALS},
    {"SubEqualsWithLen", 5, TRANSPILER_SCALAR_OP_SUB_EQUALS_WITH_LEN},
    {"SubIndexOf", 4, TRANSPILER_SCALAR_OP_SUB_INDEX_OF},
    {"SubIndexOfWithLen", 5, TRANSPILER_SCALAR_OP_SUB_INDEX_OF_WITH_LEN},
    {"SubStartsWith", 3, TRANSPILER_SCALAR_OP_SUB_STARTS_WITH},
    {"SubStartsWithLen", 4, TRANSPILER_SCALAR_OP_SUB_STARTS_WITH_LEN},
    {"Substring", 3, TRANSPILER_SCALAR_OP_SUBSTRING},
    {"SubstringWithLen", 4, TRANSPILER_SCALAR_OP_SUBSTRING_WITH_LEN},
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
transpiler_scalar_emit_arg(TranspilerCtx *ctx,
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
        "C backend: scalar builtin %s could not lower %s argument",
        builtin_name != NULL ? builtin_name : "(unknown)",
        role != NULL ? role : "operand");
    return NULL;
}

char *
transpiler_scalar_emit_sub_with_explicit_source_len(
    TranspilerCtx *ctx,
    ASTNode *call,
    const char *builtin_name,
    const char *runtime_name,
    const char *tail_role,
    bool has_window_len)
{
    ASTNode *a0 = ast_call_argument(call, 0);
    ASTNode *a1 = ast_call_argument(call, 1);
    ASTNode *a2 = ast_call_argument(call, 2);
    ASTNode *tail_arg = ast_call_argument(call, has_window_len ? 4 : 3);
    char *s = transpiler_scalar_emit_arg(ctx, a0, builtin_name, "source");
    char *slen = s != NULL
        ? transpiler_scalar_emit_arg(ctx, a1, builtin_name, "source length")
        : NULL;
    char *start = slen != NULL
        ? transpiler_scalar_emit_arg(ctx, a2, builtin_name, "start")
        : NULL;
    char *len = has_window_len && start != NULL
        ? transpiler_scalar_emit_arg(ctx, ast_call_argument(call, 3),
            builtin_name, "length")
        : NULL;
    char *tail = (!has_window_len || len != NULL) && start != NULL
        ? transpiler_scalar_emit_arg(ctx, tail_arg, builtin_name, tail_role)
        : NULL;
    char *result = NULL;

    if (s == NULL || slen == NULL || start == NULL || tail == NULL
        || (has_window_len && len == NULL)) {
        free(s); free(slen); free(start); free(len); free(tail);
        return NULL;
    }
    if (has_window_len) {
        result = strdup_fmt("%s(%s, %s, %s, %s, %s)", runtime_name,
            s, slen, start, len, tail);
    } else {
        result = strdup_fmt("%s(%s, %s, %s, %s)", runtime_name,
            s, slen, start, tail);
    }
    free(s); free(slen); free(start); free(len); free(tail);
    return result;
}

char *
emit_call_stdlib_scalar_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    TranspilerScalarOp op = transpiler_scalar_lookup(fn, argc);
    ASTNode *a0 = ast_call_argument(call, 0);
    ASTNode *a1 = ast_call_argument(call, 1);
    ASTNode *a2 = ast_call_argument(call, 2);

    (void)a2;

    /* The string family lives in its own owner; ask before running any
     * numeric branch so the two sets stay disjoint by construction. */
    if (transpiler_scalar_op_is_string(op))
        return transpiler_scalar_emit_string(op, fn, call, ctx);

    if (op == TRANSPILER_SCALAR_OP_ABS) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt(
            "(({ __auto_type _pgy_abs_value = (%s); "
            "_pgy_abs_value < 0 ? -_pgy_abs_value : _pgy_abs_value; }))",
            arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_MIN) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "left");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "right")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        char *result = strdup_fmt(
            "(({ __auto_type _pgy_min_left = (%s); "
            "__auto_type _pgy_min_right = (%s); "
            "_pgy_min_left < _pgy_min_right "
            "? _pgy_min_left : _pgy_min_right; }))",
            a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_CHECKED_ADD
        || op == TRANSPILER_SCALAR_OP_CHECKED_MUL) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "left");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "right")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        const char *callee = (op == TRANSPILER_SCALAR_OP_CHECKED_ADD)
            ? "pgy_checked_add_i32_export"
            : "pgy_checked_mul_i32_export";
        char *result = strdup_fmt("%s((int32_t)(%s), (int32_t)(%s))",
                                  callee, a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_MAX) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "left");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "right")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        char *result = strdup_fmt(
            "(({ __auto_type _pgy_max_left = (%s); "
            "__auto_type _pgy_max_right = (%s); "
            "_pgy_max_left > _pgy_max_right "
            "? _pgy_max_left : _pgy_max_right; }))",
            a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_EXIT) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "code");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("pgy_exit(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_TO_INT) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("ToInt(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_TO_FLOAT) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("ToFloat(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SQRT) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("Sqrt(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_POW) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "base");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "exponent")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        char *result = strdup_fmt("Pow(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (transpiler_scalar_unary_builtin_name(fn)
        && argc == 1) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("%s(%s)", fn, arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_ATAN2) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "y");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "x")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        char *result = strdup_fmt("Atan2(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_CLAMP) {
        char *val = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        char *lo = val != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "min")
            : NULL;
        char *hi = lo != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "max")
            : NULL;
        if (val == NULL || lo == NULL || hi == NULL) {
            free(val);
            free(lo);
            free(hi);
            return NULL;
        }
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
            char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "limit");
            if (arg == NULL)
                return NULL;
            char *result = strdup_fmt("Random(%s)", arg);
            free(arg);
            return result;
        }
        return pergyra_strdup("Random(100)");
    }
    if (op == TRANSPILER_SCALAR_OP_SEED_RANDOM) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "seed");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("SeedRandom(%s)", arg);
        free(arg);
        return result;
    }
    return NULL;
}
