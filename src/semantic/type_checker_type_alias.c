/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type-alias statement validation.
 */

#include "type_checker_internal.h"

bool
type_check_type_alias_stmt(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *target_type;

    if (node == NULL || node->type != AST_TYPE_ALIAS)
        return true;

    target_type = ast_type_alias_target_type(node);
    if (target_type != NULL)
        (void)semantic_type_resolution_lookup_metadata_type_ref(ctx,
                                                                target_type);
    return ctx == NULL || !ctx->has_error;
}
