/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Return-statement ownership boundary checks.
 */

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"

static Type *
ownership_return_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
ownership_return_apply_context(ASTNode *value, Type *value_type,
                               Type *expected_type)
{
    if (value == NULL || expected_type == NULL || expected_type == TYPE_UNKNOWN)
        return value_type;
    if (value->type == AST_ARRAY_LITERAL
        && value->data.array_literal.count == 0
        && type_is_constructed_named(expected_type, "Array")) {
        return expected_type;
    }
    return value_type;
}

bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *ret_type = TYPE_VOID;

    semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_RETURN);

    if (node->data.return_stmt.value != NULL)
        ret_type = ownership_return_normalize_type(
            type_check_expression(node->data.return_stmt.value, ctx));
    ret_type = ownership_return_apply_context(node->data.return_stmt.value,
                                              ret_type,
                                              ctx->current_return);

    if (ctx->current_return != NULL)
        require_assignable(ret_type, ctx->current_return, node, ctx);

    if (node->data.return_stmt.value != NULL) {
        if (semantic_reject_active_slot_owner_escape(
                node->data.return_stmt.value, ctx, "return", "return")) {
            return false;
        }
        if (type_is_read_view(ret_type) || type_is_write_view(ret_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_PIN_ESCAPE,
                PGY_CAUSE_PIN_ESCAPE,
                PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                node->data.return_stmt.value,
                "Pinned view cannot escape through return.\n"
                "Reason:\n"
                "- %s is a lexical capability lease over an owning slot\n"
                "- returning it would let the view outlive cleanup and CFG frontier checks\n"
                "Fix:\n"
                "- return a copied value or projection instead\n"
                "- or keep all ReadView/WriteView use inside the current scope",
                type_is_write_view(ret_type) ? "WriteView<T>" : "ReadView<T>");
            return false;
        }
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
