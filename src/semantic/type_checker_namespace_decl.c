/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Namespace declaration statement validation.
 */

#include "type_checker.h"

bool
type_check_namespace_decl(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return true;

    for (size_t i = 0; i < ast_namespace_statement_count(node); i++)
        type_check_statement(ast_namespace_statement(node, i), ctx);
    return ctx == NULL || !ctx->has_error;
}
