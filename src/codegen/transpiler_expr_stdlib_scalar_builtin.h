/* Scalar, math, and string stdlib call lowering.
 * Included by transpiler_expr_stdlib_builtin.h inside transpiler.c. */

static char *
emit_call_stdlib_scalar_builtin(const char *fn, ASTNode *call, TranspilerCtx *ctx)
{
    if (strcmp(fn, "Abs") == 0 && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("((%s) < 0 ? -(%s) : (%s))", arg, arg, arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "Min") == 0 && call->data.call.arg_count == 2) {
        char *a = emit_expression(call->data.call.arguments[0], ctx);
        char *b = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("((%s) < (%s) ? (%s) : (%s))", a, b, a, b);
        free(a); free(b);
        return result;
    }
    if (strcmp(fn, "Max") == 0 && call->data.call.arg_count == 2) {
        char *a = emit_expression(call->data.call.arguments[0], ctx);
        char *b = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("((%s) > (%s) ? (%s) : (%s))", a, b, a, b);
        free(a); free(b);
        return result;
    }
    if (strcmp(fn, "StringLength") == 0 && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Contains") == 0 || strcmp(fn, "StringContains") == 0)
        && call->data.call.arg_count == 2) {
        char *a = emit_expression(call->data.call.arguments[0], ctx);
        char *b = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("StringContains(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if ((strcmp(fn, "Replace") == 0 || strcmp(fn, "StringReplace") == 0)
        && call->data.call.arg_count == 3) {
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *old_s = emit_expression(call->data.call.arguments[1], ctx);
        char *new_s = emit_expression(call->data.call.arguments[2], ctx);
        char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
        free(s); free(old_s); free(new_s);
        return result;
    }
    if (strcmp(fn, "Substring") == 0 && call->data.call.arg_count == 3) {
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *start = emit_expression(call->data.call.arguments[1], ctx);
        char *len = emit_expression(call->data.call.arguments[2], ctx);
        char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
        free(s); free(start); free(len);
        return result;
    }
    if ((strcmp(fn, "Trim") == 0 || strcmp(fn, "StringTrim") == 0)
        && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("StringTrim(%s)", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Upper") == 0 || strcmp(fn, "ToUpper") == 0)
        && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("ToUpper(%s)", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Lower") == 0 || strcmp(fn, "ToLower") == 0)
        && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("ToLower(%s)", arg);
        free(arg);
        return result;
    }
    if ((strcmp(fn, "Concat") == 0 || strcmp(fn, "StringConcat") == 0)
        && call->data.call.arg_count == 2) {
        char *a = emit_expression(call->data.call.arguments[0], ctx);
        char *b = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if ((strcmp(fn, "StringSplit") == 0 || strcmp(fn, "Split") == 0)
        && call->data.call.arg_count == 2) {
        char *s = emit_expression(call->data.call.arguments[0], ctx);
        char *d = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
        free(s); free(d);
        return result;
    }
    if ((strcmp(fn, "StringJoin") == 0 || strcmp(fn, "Join") == 0)
        && call->data.call.arg_count == 2) {
        char *arr = emit_expression(call->data.call.arguments[0], ctx);
        char *sep = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
        free(arr); free(sep);
        return result;
    }
    if (strcmp(fn, "ToInt") == 0 && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("ToInt(%s)", arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "ToFloat") == 0 && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("ToFloat(%s)", arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "Sqrt") == 0 && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("Sqrt(%s)", arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "Pow") == 0 && call->data.call.arg_count == 2) {
        char *a = emit_expression(call->data.call.arguments[0], ctx);
        char *b = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("Pow(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if ((strcmp(fn, "Floor") == 0 || strcmp(fn, "Ceil") == 0
        || strcmp(fn, "Round") == 0
        || strcmp(fn, "Sin") == 0 || strcmp(fn, "Cos") == 0
        || strcmp(fn, "Tan") == 0 || strcmp(fn, "Asin") == 0
        || strcmp(fn, "Acos") == 0 || strcmp(fn, "Atan") == 0
        || strcmp(fn, "Exp") == 0 || strcmp(fn, "MathLog") == 0
        || strcmp(fn, "Log10") == 0 || strcmp(fn, "Log2") == 0)
        && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("%s(%s)", fn, arg);
        free(arg);
        return result;
    }
    if (strcmp(fn, "Atan2") == 0 && call->data.call.arg_count == 2) {
        char *a = emit_expression(call->data.call.arguments[0], ctx);
        char *b = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("Atan2(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (strcmp(fn, "Clamp") == 0 && call->data.call.arg_count == 3) {
        char *val = emit_expression(call->data.call.arguments[0], ctx);
        char *lo = emit_expression(call->data.call.arguments[1], ctx);
        char *hi = emit_expression(call->data.call.arguments[2], ctx);
        char *result = strdup_fmt("Clamp(%s, %s, %s)", val, lo, hi);
        free(val); free(lo); free(hi);
        return result;
    }
    if (strcmp(fn, "PI") == 0) return pergyra_strdup("PGY_PI");
    if (strcmp(fn, "E") == 0 && call->data.call.arg_count == 0)
        return pergyra_strdup("PGY_E");
    if (strcmp(fn, "Random") == 0) {
        if (call->data.call.arg_count >= 1) {
            char *arg = emit_expression(call->data.call.arguments[0], ctx);
            char *result = strdup_fmt("Random(%s)", arg);
            free(arg);
            return result;
        }
        return pergyra_strdup("Random(100)");
    }
    if (strcmp(fn, "SeedRandom") == 0 && call->data.call.arg_count == 1) {
        char *arg = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("SeedRandom(%s)", arg);
        free(arg);
        return result;
    }
    return NULL;
}
