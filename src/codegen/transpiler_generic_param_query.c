/*
 * Copyright (c) 2026 Pergyra Language Project
 * Shared C backend generic-parameter declaration queries.
 */

#include "transpiler_generic_param_query.h"

#include "../parser/ast_api.h"

bool
transpiler_func_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_FUNC_DECL
        && ast_func_generic_params(node) != NULL
        && ast_func_generic_params(node)->count > 0;
}
