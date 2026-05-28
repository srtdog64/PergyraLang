/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend literal expression lowering.
 */

#include "transpiler_expr_literal_emit.h"

#include <stdint.h>
#include <stdlib.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "transpiler_format.h"

char *
emit_literal_expression(ASTNode *node)
{
    if (node == NULL)
        return pergyra_strdup("0");

    switch (node->type) {
    case AST_NUMBER:
        if (ast_number_is_long(node))
            return strdup_fmt("%lldLL",
                (long long)(int64_t)ast_number_value(node));
        if (ast_number_is_float(node))
            return strdup_fmt("((float)%g)", ast_number_value(node));
        if (ast_number_value(node) == (int64_t)ast_number_value(node))
            return strdup_fmt("%lld",
                (long long)(int64_t)ast_number_value(node));
        return strdup_fmt("%g", ast_number_value(node));
    case AST_STRING: {
        char *escaped = escape_c_string_literal(ast_string_value(node));
        char *result = strdup_fmt("\"%s\"", escaped);
        free(escaped);
        return result;
    }
    case AST_BOOLEAN:
        return pergyra_strdup(ast_boolean_value(node) ? "true" : "false");
    default:
        return pergyra_strdup("0");
    }
}
