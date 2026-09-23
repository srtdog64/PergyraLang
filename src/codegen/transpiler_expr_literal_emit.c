/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend literal expression lowering.
 */

#include "transpiler_expr_literal_emit.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_format.h"

char *
emit_literal_expression(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_NUMBER:
        if (ast_number_is_long(node)) {
            int64_t value = ast_number_exact_long_value(node);
            if (value == INT64_MIN)
                return pergyra_strdup("(-9223372036854775807LL - 1LL)");
            return strdup_fmt("%lldLL", (long long)value);
        }
        /* %.17g round-trips the parsed double, so the (float) cast rounds
         * the same value LLVMConstReal does; %g kept six significant
         * digits and made native C the leg that disagreed. */
        if (ast_number_is_float(node))
            return strdup_fmt("((float)%.17g)", ast_number_value(node));
        /* C spells -2147483648 as the negation of a long constant, so
         * arithmetic over it would run in 64 bits instead of Int's 32. */
        if (ast_number_value(node) == -2147483648.0)
            return pergyra_strdup("(-2147483647 - 1)");
        if (ast_number_value(node) == (int64_t)ast_number_value(node))
            return strdup_fmt("%lld",
                (long long)(int64_t)ast_number_value(node));
        return strdup_fmt("%.17g", ast_number_value(node));
    case AST_STRING: {
        char *escaped = escape_c_string_literal(ast_string_value(node));
        if (escaped == NULL)
            return NULL;
        char *result = strdup_fmt("\"%s\"", escaped);
        free(escaped);
        return result;
    }
    case AST_BOOLEAN:
        return pergyra_strdup(ast_boolean_value(node) ? "true" : "false");
    default:
        return NULL;
    }
}
