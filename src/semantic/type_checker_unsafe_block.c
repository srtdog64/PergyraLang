/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Unsafe block statement validation.
 */

#include "type_checker.h"

bool
type_check_unsafe_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || node->type != AST_UNSAFE_BLOCK)
        return true;

    if (ast_unsafe_block_body(node) != NULL)
        type_check_block(ast_unsafe_block_body(node), ctx);
    return ctx == NULL || !ctx->has_error;
}
