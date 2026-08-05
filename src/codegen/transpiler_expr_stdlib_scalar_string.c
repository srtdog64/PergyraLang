/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend string and substring stdlib call lowering.
 *
 * Split out of transpiler_expr_stdlib_scalar_builtin.c, which held the numeric
 * and string families behind one dispatch and reached the production owner
 * line cap. The two families share only the op enum and the argument emitters;
 * see transpiler_expr_stdlib_scalar_internal.h.
 */

#include "transpiler_expr_stdlib_scalar_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_expr_stdlib_collection_support.h"
#include "transpiler_format.h"

bool
transpiler_scalar_op_is_string(TranspilerScalarOp op)
{
    switch (op) {
    case TRANSPILER_SCALAR_OP_CHAR_AT_N:
    case TRANSPILER_SCALAR_OP_CHAR_CODE:
    case TRANSPILER_SCALAR_OP_CONCAT:
    case TRANSPILER_SCALAR_OP_LOWER:
    case TRANSPILER_SCALAR_OP_REPLACE:
    case TRANSPILER_SCALAR_OP_SPLIT:
    case TRANSPILER_SCALAR_OP_STRING_CONTAINS:
    case TRANSPILER_SCALAR_OP_STRING_INDEX_OF:
    case TRANSPILER_SCALAR_OP_STRING_JOIN:
    case TRANSPILER_SCALAR_OP_STRING_LENGTH:
    case TRANSPILER_SCALAR_OP_STRING_TRIM:
    case TRANSPILER_SCALAR_OP_SUBSTRING:
    case TRANSPILER_SCALAR_OP_SUBSTRING_WITH_LEN:
    case TRANSPILER_SCALAR_OP_SUB_CONTAINS:
    case TRANSPILER_SCALAR_OP_SUB_CONTAINS_WITH_LEN:
    case TRANSPILER_SCALAR_OP_SUB_EQUALS:
    case TRANSPILER_SCALAR_OP_SUB_EQUALS_WITH_LEN:
    case TRANSPILER_SCALAR_OP_SUB_INDEX_OF:
    case TRANSPILER_SCALAR_OP_SUB_INDEX_OF_WITH_LEN:
    case TRANSPILER_SCALAR_OP_SUB_STARTS_WITH:
    case TRANSPILER_SCALAR_OP_SUB_STARTS_WITH_LEN:
    case TRANSPILER_SCALAR_OP_UPPER:
        return true;
    default:
        return false;
    }
}

char *
transpiler_scalar_emit_string(TranspilerScalarOp op,
                              const char *fn,
                              ASTNode *call,
                              TranspilerCtx *ctx)
{
    size_t argc = ast_call_arg_count(call);
    ASTNode *a0 = ast_call_argument(call, 0);
    ASTNode *a1 = ast_call_argument(call, 1);
    ASTNode *a2 = ast_call_argument(call, 2);

    (void)argc;
    (void)a2;

    if (op == TRANSPILER_SCALAR_OP_STRING_LENGTH) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("((int32_t)strlen(%s))", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_CONTAINS) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "haystack");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "needle")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        char *result = strdup_fmt("StringContains(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_INDEX_OF) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "haystack");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "needle")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        char *result = strdup_fmt("StringIndexOf(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_REPLACE) {
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *old_s = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "old")
            : NULL;
        char *new_s = old_s != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "new")
            : NULL;
        if (s == NULL || old_s == NULL || new_s == NULL) {
            free(s);
            free(old_s);
            free(new_s);
            return NULL;
        }
        char *result = strdup_fmt("StringReplace(%s, %s, %s)", s, old_s, new_s);
        free(s); free(old_s); free(new_s);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUBSTRING) {
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *start = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "start")
            : NULL;
        char *len = start != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "length")
            : NULL;
        if (s == NULL || start == NULL || len == NULL) {
            free(s);
            free(start);
            free(len);
            return NULL;
        }
        char *result = strdup_fmt("Substring(%s, %s, %s)", s, start, len);
        free(s); free(start); free(len);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUBSTRING_WITH_LEN) {
        ASTNode *a3 = ast_call_argument(call, 3);
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *slen = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "source length")
            : NULL;
        char *start = slen != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "start")
            : NULL;
        char *len = start != NULL
            ? transpiler_scalar_emit_arg(ctx, a3, fn, "length")
            : NULL;
        if (s == NULL || slen == NULL || start == NULL || len == NULL) {
            free(s);
            free(slen);
            free(start);
            free(len);
            return NULL;
        }
        char *result = strdup_fmt("SubstringWithLen(%s, %s, %s, %s)",
                                  s, slen, start, len);
        free(s); free(slen); free(start); free(len);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_CHAR_AT_N) {
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *slen = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "length")
            : NULL;
        char *idx = slen != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "index")
            : NULL;
        if (s == NULL || slen == NULL || idx == NULL) {
            free(s);
            free(slen);
            free(idx);
            return NULL;
        }
        char *result = strdup_fmt("CharAtN(%s, %s, %s)", s, slen, idx);
        free(s); free(slen); free(idx);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_CHAR_CODE) {
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *slen = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "length")
            : NULL;
        char *idx = slen != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "index")
            : NULL;
        if (s == NULL || slen == NULL || idx == NULL) {
            free(s);
            free(slen);
            free(idx);
            return NULL;
        }
        char *result = strdup_fmt("CharCode(%s, %s, %s)", s, slen, idx);
        free(s); free(slen); free(idx);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_INDEX_OF) {
        ASTNode *a3 = ast_call_argument(call, 3);
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *start = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "start")
            : NULL;
        char *len = start != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "length")
            : NULL;
        char *needle = len != NULL
            ? transpiler_scalar_emit_arg(ctx, a3, fn, "needle")
            : NULL;
        if (s == NULL || start == NULL || len == NULL || needle == NULL) {
            free(s);
            free(start);
            free(len);
            free(needle);
            return NULL;
        }
        char *result = strdup_fmt("SubIndexOf(%s, %s, %s, %s)", s, start, len, needle);
        free(s); free(start); free(len); free(needle);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_INDEX_OF_WITH_LEN) {
        return transpiler_scalar_emit_sub_with_explicit_source_len(ctx, call,
            fn, "SubIndexOfWithLen", "needle", true);
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_EQUALS) {
        ASTNode *a3 = ast_call_argument(call, 3);
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *start = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "start")
            : NULL;
        char *len = start != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "length")
            : NULL;
        char *other = len != NULL
            ? transpiler_scalar_emit_arg(ctx, a3, fn, "other")
            : NULL;
        if (s == NULL || start == NULL || len == NULL || other == NULL) {
            free(s);
            free(start);
            free(len);
            free(other);
            return NULL;
        }
        char *result = strdup_fmt("SubEquals(%s, %s, %s, %s)", s, start, len, other);
        free(s); free(start); free(len); free(other);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_EQUALS_WITH_LEN) {
        return transpiler_scalar_emit_sub_with_explicit_source_len(ctx, call,
            fn, "SubEqualsWithLen", "other", true);
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_CONTAINS) {
        ASTNode *a3 = ast_call_argument(call, 3);
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *start = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "start")
            : NULL;
        char *len = start != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "length")
            : NULL;
        char *needle = len != NULL
            ? transpiler_scalar_emit_arg(ctx, a3, fn, "needle")
            : NULL;
        if (s == NULL || start == NULL || len == NULL || needle == NULL) {
            free(s);
            free(start);
            free(len);
            free(needle);
            return NULL;
        }
        char *result = strdup_fmt("SubContains(%s, %s, %s, %s)", s, start, len, needle);
        free(s); free(start); free(len); free(needle);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_CONTAINS_WITH_LEN) {
        return transpiler_scalar_emit_sub_with_explicit_source_len(ctx, call,
            fn, "SubContainsWithLen", "needle", true);
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_STARTS_WITH) {
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *start = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "start")
            : NULL;
        char *prefix = start != NULL
            ? transpiler_scalar_emit_arg(ctx, a2, fn, "prefix")
            : NULL;
        if (s == NULL || start == NULL || prefix == NULL) {
            free(s);
            free(start);
            free(prefix);
            return NULL;
        }
        char *result = strdup_fmt("SubStartsWith(%s, %s, %s)", s, start, prefix);
        free(s); free(start); free(prefix);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SUB_STARTS_WITH_LEN) {
        return transpiler_scalar_emit_sub_with_explicit_source_len(ctx, call,
            fn, "SubStartsWithLen", "prefix", false);
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_TRIM) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("StringTrim(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_UPPER) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("ToUpper(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_LOWER) {
        char *arg = transpiler_scalar_emit_arg(ctx, a0, fn, "value");
        if (arg == NULL)
            return NULL;
        char *result = strdup_fmt("ToLower(%s)", arg);
        free(arg);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_CONCAT) {
        char *a = transpiler_scalar_emit_arg(ctx, a0, fn, "left");
        char *b = a != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "right")
            : NULL;
        if (a == NULL || b == NULL) {
            free(a);
            free(b);
            return NULL;
        }
        char *result = strdup_fmt("StringConcat(%s, %s)", a, b);
        free(a); free(b);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_SPLIT) {
        char *s = transpiler_scalar_emit_arg(ctx, a0, fn, "source");
        char *d = s != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "delimiter")
            : NULL;
        if (s == NULL || d == NULL) {
            free(s);
            free(d);
            return NULL;
        }
        char *result = strdup_fmt("StringSplit(%s, %s)", s, d);
        free(s); free(d);
        return result;
    }
    if (op == TRANSPILER_SCALAR_OP_STRING_JOIN) {
        if (!transpiler_require_c_addressable_storage(ctx, a0,
                "StringJoin", "Array"))
            return NULL;
        char *arr = transpiler_scalar_emit_arg(ctx, a0, fn, "array");
        char *sep = arr != NULL
            ? transpiler_scalar_emit_arg(ctx, a1, fn, "separator")
            : NULL;
        if (arr == NULL || sep == NULL) {
            free(arr);
            free(sep);
            return NULL;
        }
        char *result = strdup_fmt("StringJoin(&%s, %s)", arr, sep);
        free(arr); free(sep);
        return result;
    }
    /* Unreachable while transpiler_scalar_op_is_string agrees with the
     * branches above; NULL keeps the dispatcher's existing contract if it
     * ever stops agreeing. */
    return NULL;
}
