#ifndef PGY_TRANSPILER_EXPR_STDLIB_SCALAR_BUILTIN_H
#define PGY_TRANSPILER_EXPR_STDLIB_SCALAR_BUILTIN_H

/* Scalar, math, and string stdlib call lowering.
 * Included by transpiler_expr_stdlib_builtin.h inside transpiler.c. */

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

static char *
emit_call_stdlib_scalar_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    ASTNode *a0 = ast_call_argument(call, 0);
    ASTNode *a1 = ast_call_argument(call, 1);
    ASTNode *a2 = ast_call_argument(call, 2);

    if (strcmp(fn, "Abs") == 0 && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("((%s) < 0 ? -(%s) : (%s))", arg, arg, arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "Min") == 0 && argc == 2) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("((%s) < (%s) ? (%s) : (%s))", a, b, a, b);
        free(a); free(b);
        return result;
    }
    if (strcmp(fn, "Max") == 0 && argc == 2) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("((%s) > (%s) ? (%s) : (%s))", a, b, a, b);
        free(a); free(b);
        return result;
    }
    if (strcmp(fn, "StringLength") == 0 && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Contains") == 0 || strcmp(fn, "StringContains") == 0)
        && argc == 2) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringContains(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if ((strcmp(fn, "Replace") == 0 || strcmp(fn, "StringReplace") == 0)
        && argc == 3) {
        char *s = emit_expression(a0, ctx);
        char *old_s = emit_expression(a1, ctx);
        char *new_s = emit_expression(a2, ctx);
        char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
        free(s); free(old_s); free(new_s);
        return result;
    }
    if (strcmp(fn, "Substring") == 0 && argc == 3) {
        char *s = emit_expression(a0, ctx);
        char *start = emit_expression(a1, ctx);
        char *len = emit_expression(a2, ctx);
        char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
        free(s); free(start); free(len);
        return result;
    }
    if ((strcmp(fn, "Trim") == 0 || strcmp(fn, "StringTrim") == 0)
        && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("StringTrim(%s)", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Upper") == 0 || strcmp(fn, "ToUpper") == 0)
        && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToUpper(%s)", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Lower") == 0 || strcmp(fn, "ToLower") == 0)
        && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToLower(%s)", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Concat") == 0 || strcmp(fn, "StringConcat") == 0)
        && argc == 2) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if ((strcmp(fn, "StringSplit") == 0 || strcmp(fn, "Split") == 0)
        && argc == 2) {
        char *s = emit_expression(a0, ctx);
        char *d = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
        free(s); free(d);
        return result;
    }
    if ((strcmp(fn, "StringJoin") == 0 || strcmp(fn, "Join") == 0)
        && argc == 2) {
        char *arr = emit_expression(a0, ctx);
        char *sep = emit_expression(a1, ctx);
        char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
        free(arr); free(sep);
        return result;
    }
    if (strcmp(fn, "ToInt") == 0 && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToInt(%s)", arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "ToFloat") == 0 && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("ToFloat(%s)", arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "Sqrt") == 0 && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("Sqrt(%s)", arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "Pow") == 0 && argc == 2) {
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
    if (strcmp(fn, "Atan2") == 0 && argc == 2) {
        char *a = emit_expression(a0, ctx);
        char *b = emit_expression(a1, ctx);
        char *result = strdup_fmt("Atan2(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (strcmp(fn, "Clamp") == 0 && argc == 3) {
        char *val = emit_expression(a0, ctx);
        char *lo = emit_expression(a1, ctx);
        char *hi = emit_expression(a2, ctx);
        char *result = strdup_fmt("Clamp(%s, %s, %s)", val, lo, hi);
        free(val); free(lo); free(hi);
        return result;
    }
    if (strcmp(fn, "PI") == 0) return pergyra_strdup("PGY_PI");
    if (strcmp(fn, "E") == 0 && argc == 0)
        return pergyra_strdup("PGY_E");
    if (strcmp(fn, "Random") == 0) {
        if (argc >= 1) {
            char *arg = emit_expression(a0, ctx);
            char *result = strdup_fmt("Random(%s)", arg);
            free(arg);
            return result;
        }
        return pergyra_strdup("Random(100)");
    }
    if (strcmp(fn, "SeedRandom") == 0 && argc == 1) {
        char *arg = emit_expression(a0, ctx);
        char *result = strdup_fmt("SeedRandom(%s)", arg);
        free(arg);
        return result;
    }
    return NULL;
}

#endif /* PGY_TRANSPILER_EXPR_STDLIB_SCALAR_BUILTIN_H */
