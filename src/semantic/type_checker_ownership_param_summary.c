/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Function parameter ownership escape summary checks.
 */

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_internal.h"

void
semantic_check_param_summary_escapes(ASTNode *node,
                                     size_t param_count,
                                     Type **param_types,
                                     SemanticContext *ctx)
{
    if (node == NULL || ctx == NULL || param_types == NULL
        || ast_func_body(node) == NULL) {
        return;
    }

    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = ast_func_param(node, i);
        unsigned summary_mask;
        OwnershipTypeClass ownership_class;

        if (param == NULL || param->name == NULL || param->type == NULL)
            continue;
        if (param->mode != PARAM_MODE_REF)
            continue;

        ownership_class = semantic_classify_ownership_type(param_types[i], ctx);
        if (ownership_class == OWNERSHIP_TYPE_COPY_ONLY)
            continue;

        summary_mask = semantic_legacy_ast_callable_param_escape_summary(
            node, i, ctx);
        if (semantic_param_summary_has_any_escape(summary_mask)) {
            semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_ESCAPE_REF);
        }
        if (semantic_param_summary_has_return_escape(summary_mask)) {
            semantic_validate_borrowed_escape(
                node, node, ctx, param_types[i], param->name,
                OWNERSHIP_CONSUMER_RETURN, NULL, NULL, NULL,
                false, NULL, NULL);
        }
        if (semantic_param_summary_has_channel_escape(summary_mask)) {
            semantic_validate_borrowed_escape(
                node, node, ctx, param_types[i], param->name,
                OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
                false, NULL, NULL);
        }
        if (semantic_param_summary_has_call_escape(summary_mask)) {
            semantic_validate_borrowed_escape(
                node, node, ctx, param_types[i], param->name,
                OWNERSHIP_CONSUMER_HELPER_CALL, NULL,
                ast_declaration_name(node) != NULL
                    ? ast_declaration_name(node) : "<anonymous>",
                NULL, true, NULL,
                ownership_class == OWNERSHIP_TYPE_MOVE_ONLY ? "slot handle (movable)"
                    : (ownership_class == OWNERSHIP_TYPE_SUBJECT_IDENTITY ? "subject"
                       : (ownership_class == OWNERSHIP_TYPE_ANCHORED_HANDLE
                          ? "slot handle (anchored)"
                          : "value")));
        }
    }
}
