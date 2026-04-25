/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Return-statement ownership boundary checks.
 */

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"

bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *ret_type = TYPE_VOID;

    semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_RETURN);

    if (node->data.return_stmt.value != NULL)
        ret_type = type_check_expression(node->data.return_stmt.value, ctx);

    if (ctx->current_return != NULL)
        require_assignable(ret_type, ctx->current_return, node, ctx);

    if (node->data.return_stmt.value != NULL) {
        semantic_validate_borrowed_escape(
            node, node->data.return_stmt.value, ctx, ret_type, NULL,
            OWNERSHIP_CONSUMER_RETURN, NULL, NULL, NULL,
            false, NULL, NULL);
    }

    if (node->data.return_stmt.value != NULL
        && type_is_qubit(ret_type)
        && node->data.return_stmt.value->type == AST_IDENTIFIER) {
        consume_qubit_value(node->data.return_stmt.value, ctx, "returned");
    }

    return !ctx->has_error;
}
