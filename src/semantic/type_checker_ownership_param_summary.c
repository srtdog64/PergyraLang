/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Function parameter ownership escape summary checks.
 */

#include "slot_summary.h"
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

        summary_mask = slot_analyze_legacy_ast_param_summary_in_program(
            ast_func_body(node), param->name, ctx->program_root);
        if ((summary_mask & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                | SLOT_PARAM_SUMMARY_CALL_ESCAPE)) != 0) {
            semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_ESCAPE_REF);
        }
        if ((summary_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
            semantic_validate_borrowed_escape(
                node, node, ctx, param_types[i], param->name,
                OWNERSHIP_CONSUMER_RETURN, NULL, NULL, NULL,
                false, NULL, NULL);
        }
        if ((summary_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0) {
            semantic_validate_borrowed_escape(
                node, node, ctx, param_types[i], param->name,
                OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
                false, NULL, NULL);
        }
        if ((summary_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) {
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
