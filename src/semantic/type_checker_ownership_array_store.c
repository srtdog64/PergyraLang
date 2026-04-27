/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Array literal ownership-boundary checks.
 */

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"

void
reject_borrowed_array_literal_store(ASTNode *value_expr,
                                    const Type *stored_value_type,
                                    SemanticContext *ctx)
{
    if (value_expr == NULL || stored_value_type == NULL || ctx == NULL)
        return;
    if (semantic_reject_active_slot_owner_escape(
            value_expr, ctx, "array literal", "array literal storage")) {
        return;
    }
    semantic_validate_borrowed_escape(
        value_expr, value_expr, ctx, stored_value_type, NULL,
        OWNERSHIP_CONSUMER_CONTAINER_STORE, NULL,
        "array literal", "array literal storage",
        false, NULL, NULL);
}
