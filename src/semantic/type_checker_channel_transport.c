/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker channel transport policy and diagnostics.
 */

#include <stdlib.h>

#include "diag_codes.h"
#include "type_checker_channel_transport_internal.h"
#include "type_checker_ownership_support_internal.h"

bool
semantic_check_channel_send_borrowed_transfer(ASTNode *value_expr,
                                              SemanticContext *ctx,
                                              const char *value_label,
                                              const char *provenance_label,
                                              const char *snapshot_label,
                                              const char *named_binding_fix)
{
    const char *borrowed_root_name;
    Type *value_type;

    if (value_expr == NULL || ctx == NULL) {
        return false;
    }

    value_type = type_check_expression(value_expr, ctx);
    borrowed_root_name = semantic_borrowed_boundary_root_name(value_expr, ctx);

    if (semantic_channel_transfer_requires_named_binding(value_expr,
            borrowed_root_name)) {
        char *source_path = semantic_assignment_target_path(value_expr);
        semantic_report_named_channel_transfer_required(
            value_expr, ctx, "channel send",
            value_label != NULL ? value_label : "boundary value",
            source_path,
            named_binding_fix != NULL ? named_binding_fix
                                      : "bind the value first in a local variable");
        free(source_path);
        return true;
    }

    if (borrowed_root_name != NULL) {
        semantic_validate_borrowed_escape(
            value_expr, value_expr, ctx, value_type, borrowed_root_name,
            OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
            false, NULL, NULL);
        (void)provenance_label;
        (void)snapshot_label;
        return true;
    }

    return false;
}

bool
semantic_check_borrowed_channel_transfer(ASTNode *value_expr,
                                         Type *value_type,
                                         SemanticContext *ctx,
                                         const char *value_label,
                                         const char *named_binding_fix)
{
    const char *borrowed_root_name;

    if (value_expr == NULL || ctx == NULL)
        return false;

    borrowed_root_name = semantic_borrowed_boundary_root_name(value_expr, ctx);
    if (semantic_channel_transfer_requires_named_binding(value_expr,
            borrowed_root_name)) {
        char *source_path = semantic_assignment_target_path(value_expr);
        semantic_report_named_channel_transfer_required(
            value_expr, ctx, "send",
            value_label != NULL ? value_label : "boundary value",
            source_path,
            named_binding_fix != NULL ? named_binding_fix
                                      : "bind the value first in a local variable");
        free(source_path);
        return true;
    }

    if (borrowed_root_name != NULL) {
        semantic_validate_borrowed_escape(
            value_expr, value_expr, ctx, value_type, borrowed_root_name,
            OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
            false, NULL, NULL);
        return true;
    }

    return false;
}

bool
semantic_validate_channel_transport_ownership(ASTNode *value_expr,
                                              Type *value_type,
                                              SemanticContext *ctx,
                                              const char *transport_name,
                                              OwnershipTypeClass expected_class,
                                              OwnershipTypeClass element_ownership,
                                              OwnershipTypeClass value_ownership,
                                              const char *contract_label,
                                              const char *expected_name,
                                              const char *actual_name,
                                              const char *value_label,
                                              const char *named_binding_fix)
{
    if (element_ownership != expected_class
        && value_ownership != expected_class) {
        return false;
    }

    if (element_ownership != expected_class
        || value_ownership != expected_class) {
        semantic_report_channel_transport_mismatch(
            value_expr,
            ctx,
            transport_name,
            contract_label,
            expected_name,
            actual_name);
        return true;
    }

    if (semantic_check_borrowed_channel_transfer(
            value_expr, value_type, ctx, value_label, named_binding_fix)) {
        return true;
    }

    return false;
}

void
semantic_report_channel_transport_policy(ASTNode *site,
                                         SemanticContext *ctx,
                                         const char *transport_name,
                                         const char *why_text,
                                         const char *fix_text)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
        PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
        PGY_FIX_KEEP_HANDLE_LOCAL_OR_SEND_INNER_VALUE,
        site,
        "%s.\n"
        "Reason:\n"
        "- %s\n"
        "Fix:\n"
        "- %s",
        transport_name != NULL ? transport_name : "Channel transport is not allowed",
        why_text != NULL ? why_text : "channel transport policy is not closed for this value yet",
        fix_text != NULL ? fix_text : "keep the value local or use a supported transport path");
}

void
semantic_report_channel_transport_mismatch(ASTNode *site,
                                           SemanticContext *ctx,
                                           const char *transport_name,
                                           const char *contract_label,
                                           const char *expected_name,
                                           const char *actual_name)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
        PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
        PGY_FIX_ALIGN_CHANNEL_ELEMENT_TYPE,
        site,
        "%s %s mismatch: expected '%s', got '%s'.\n"
        "Reason:\n"
        "- channel element type and sent value must agree on the same %s contract\n"
        "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
        "Fix:\n"
        "- send a value of type '%s'\n"
        "- or change the channel element type to match '%s'",
        transport_name != NULL ? transport_name : "Channel send",
        contract_label != NULL ? contract_label : "value",
        expected_name != NULL ? expected_name : "<expected>",
        actual_name != NULL ? actual_name : "<actual>",
        contract_label != NULL ? contract_label : "value",
        expected_name != NULL ? expected_name : "<expected>",
        actual_name != NULL ? actual_name : "<actual>",
        expected_name != NULL ? expected_name : "<expected>",
        actual_name != NULL ? actual_name : "<actual>");
}

bool
semantic_channel_transfer_requires_named_binding(ASTNode *value_expr,
                                                 const char *borrowed_root_name)
{
    if (value_expr == NULL)
        return false;
    return value_expr->type != AST_IDENTIFIER && borrowed_root_name == NULL;
}

void
semantic_report_named_channel_transfer_required(ASTNode *site,
                                                SemanticContext *ctx,
                                                const char *transport_name,
                                                const char *value_label,
                                                const char *source_path,
                                                const char *bind_fix)
{
    semantic_error_with_hints(ctx, PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
        PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
        PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_SEND,
        site,
        "%s %s must transfer from a named variable%s%s%s.\n"
        "Reason:\n"
        "- %s transfer at a channel boundary must point to one concrete source binding\n"
        "- unnamed expressions make moved-here provenance ambiguous\n"
        "Fix:\n"
        "- %s\n"
        "- then send that named variable",
        value_label != NULL ? value_label : "boundary value",
        transport_name != NULL ? transport_name : "send",
        source_path != NULL ? " instead of '" : "",
        source_path != NULL ? source_path : "",
        source_path != NULL ? "'" : "",
        value_label != NULL ? value_label : "boundary value",
        bind_fix != NULL ? bind_fix : "bind the value first in a local variable");
}
