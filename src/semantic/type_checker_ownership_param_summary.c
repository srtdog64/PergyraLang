/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Function parameter ownership escape summary checks.
 */

#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_internal.h"
#include "function_param_publication_owner.h"
#include "slot_analyzer_internal.h"

bool semantic_consume_param_publication(SemanticContext *ctx, const SemanticParamPublicationView *view)
{
    if (!semantic_param_publication_view_current(ctx, view)) return false;
    /* Demand only registered routines; the existing flow owner resolves its
     * recursive closure. No AST root traversal or second type check. */
    for (size_t r = 0; r < view->routine_count; r++) {
        const PendingRoutine *row = &view->routines[r];
        if (ast_func_body(row->decl) == NULL) continue;
        for (size_t p = 0; p < row->count; p++) {
            FuncParam *param = ast_func_param(row->decl, p);
            if (param != NULL && param->mode == PARAM_MODE_REF)
                (void)function_param_flow_summary_for_param(ctx, row->decl, p);
        }
    }
    ASTNode *previous_decl = ctx->current_function_decl;
    uint32_t previous_body = ctx->current_function_body_summary;
    bool previous_tracking = ctx->tracking_function_effects;
    const char *previous_module = ctx->current_module_path;
    ctx->tracking_function_effects = true;
    for (size_t r = 0; r < view->routine_count; r++) {
        const PendingRoutine *row = &view->routines[r];
        if (ast_func_body(row->decl) == NULL) continue;
        ctx->current_function_decl = row->decl;
        ctx->current_module_path = row->module;
        ctx->current_function_body_summary = type_function_body_summary(row->function_type);
        semantic_check_param_summary_escapes(row->decl, row->count, row->types, row->function_type, ctx);
        type_function_set_body_summary(row->function_type, ctx->current_function_body_summary);
    }
    /* MAY is the sole delayed derived bit. Final parameter facts seed it;
     * registered exact call edges propagate it without rechecking bodies. */
    bool changed;
    size_t rounds = 0;
    do {
        changed = false;
        for (size_t b = 0; b < view->body_call_count; b++) {
            const PendingBodyCall *edge = &view->body_calls[b];
            const PendingRoutine *caller = &view->routines[edge->caller_index];
            const PendingRoutine *callee = &view->routines[edge->callee_index];
            uint32_t before = type_function_body_summary(caller->function_type);
            if ((type_function_body_summary(callee->function_type) & BODY_SUMMARY_MAY_ESCAPE_REF) != 0 &&
                (before & BODY_SUMMARY_MAY_ESCAPE_REF) == 0) {
                type_function_set_body_summary(caller->function_type, before | BODY_SUMMARY_MAY_ESCAPE_REF);
                changed = true;
            }
        }
        rounds++;
    } while (changed && rounds <= view->routine_count);
    if (changed) semantic_error(ctx, view->program, "Final MAY body summary closure did not converge");
    for (size_t c = 0; c < view->call_count; c++) {
        const PendingRefCall *row = &view->calls[c];
        unsigned mask = semantic_callable_param_escape_summary(row->callee, row->ordinal, ctx);
        ctx->current_function_decl = row->caller;
        ctx->current_module_path = row->module;
        if (semantic_param_summary_has_any_escape(mask))
            semantic_validate_borrowed_escape(row->site, row->site, ctx, row->type, row->borrowed,
                OWNERSHIP_CONSUMER_HELPER_CALL, NULL, row->destination, NULL, true, NULL, row->fix);
    }
    ctx->current_function_decl = previous_decl;
    ctx->current_function_body_summary = previous_body;
    ctx->tracking_function_effects = previous_tracking;
    ctx->current_module_path = previous_module;
    return !ctx->has_error;
}

void
semantic_check_param_summary_escapes(ASTNode *node,
                                     size_t param_count,
                                     Type **param_types,
                                     Type *function_type,
                                     SemanticContext *ctx)
{
    if (node == NULL || ctx == NULL || (param_count != 0 && param_types == NULL)) {
        return;
    }
    if (semantic_param_publication_pending(ctx)) {
        if (!semantic_param_publication_record_routine(ctx, node, param_count, param_types, function_type))
            semantic_error(ctx, node, "Pending parameter summary identity/allocation failed");
        return;
    }
    if (ast_func_body(node) == NULL) return;
    if (!semantic_param_publication_inputs_ready(ctx)) {
        semantic_error(ctx, node, "Parameter summary requires sealed typed inputs");
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

        summary_mask = semantic_callable_param_escape_summary(
            node, i, ctx);
        type_function_set_param_escape_summary(function_type, i,
            summary_mask);

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

    type_function_finish_param_escape_summaries(function_type);
}
