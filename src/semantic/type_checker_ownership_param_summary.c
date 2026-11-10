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
#include "function_param_publication_internal.h"
#include "slot_analyzer_internal.h"
#include "function_param_flow_loan_order.h"
#include "typed_map_query_fact_owner.h"
#include "loan_copy_landing_owner.h"
#include "formal_string_store_landing_owner.h"
#include "indexed_string_named_call_owner.h"
#include "indexed_string_borrow_owner.h"
#include "array_storage_deferred_preservation_owner.h"
#include "default_aggregate_move_rule.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The bindings a loan's backing reached. */
typedef struct {
    uint32_t *ids;
    FunctionParamFlowResultPath *paths;
    size_t count;
    const ASTNode **preserving_assignments;
    size_t preserving_assignment_count;
} LoanCarriers;

static void loan_carriers_release(LoanCarriers *carriers)
{
    if (carriers == NULL) return;
    free(carriers->ids);
    free(carriers->paths);
    free(carriers->preserving_assignments);
    *carriers = (LoanCarriers){0};
}

static bool loan_assignment_restores_owner_path(SemanticContext *ctx,
    ASTNode *caller, const ASTNode *assignment, uint32_t owner,
    const FunctionParamFlowResultPath *owner_path)
{
    if (ctx == NULL || caller == NULL || assignment == NULL ||
        assignment->type != AST_ASSIGNMENT || owner == 0 ||
        owner_path == NULL || !owner_path->present)
        return false;
    FunctionParamFlowResultPath target_path = {0};
    if (!function_param_flow_place_path_current(ctx, caller,
            ast_assignment_target(assignment), owner, &target_path) ||
        !target_path.present || target_path.count != owner_path->count)
        return false;
    for (size_t i = 0; i < target_path.count; i++)
        if (target_path.fields[i] != owner_path->fields[i]) return false;
    return true;
}

static bool sparse_loan_admitted(SemanticContext *ctx,
    const SemanticParamPublicationView *view, ASTNode *caller, ASTNode *seed,
    uint32_t owner_slot_id, uint32_t restore_owner_id,
    const PendingRefCall *return_loan, unsigned allowed_effects,
    ASTNode **escape_site, ASTNode **escape_carrier, const FunctionParamFlowLoanEscapes *copied,
    const FunctionParamFlowResultPath *owner_path,
    const FunctionParamFlowResultPath *seed_path, LoanCarriers *carriers);

static bool formal_escape_effects_fail_closed(
    FunctionParamFlowSummaryStore *store, ASTNode *decl, size_t formal,
    unsigned *effects);

/* A returned loan of this routine's own ref formal is already represented by
 * that formal's exact outward facet. Keep the formal as the diagnostic owner
 * for an unrepairable channel transfer instead of emitting a second refusal
 * at the inner getter call. A local owner has no such formal fact and must
 * still be refused at the getter boundary. */
static bool returned_loan_channel_escape_owned_by_formal_current(
    SemanticContext *ctx, ASTNode *caller, const ASTNode *loan_site,
    const ASTNode *escape)
{
    if (ctx == NULL || caller == NULL || loan_site == NULL ||
        escape == NULL || escape->type != AST_CHANNEL_SEND ||
        ctx->function_param_flow_summaries == NULL)
        return false;
    size_t formal = SIZE_MAX;
    bool backing = function_param_flow_value_backing_formal_current(ctx,
        caller, loan_site, &formal);
    if (!backing) {
        const ASTNode *root = loan_site;
        while (root != NULL && (root->type == AST_MEMBER_ACCESS ||
                root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS
                ? ast_member_object(root) : ast_array_access_array(root);
        uint32_t binding = root != NULL
            ? semantic_param_publication_binding(ctx, root) : 0;
        for (size_t p = 0; p < ast_func_param_count(caller); p++) {
            FuncParam *candidate = ast_func_param(caller, p);
            if (candidate != NULL && candidate->mode == PARAM_MODE_REF &&
                binding != 0 && binding == ast_func_param_stable_id(candidate)) {
                formal = p;
                backing = true;
                break;
            }
        }
    }
    if (!backing || formal >= ast_func_param_count(caller))
        return false;
    FuncParam *parameter = ast_func_param(caller, formal);
    unsigned effects = SLOT_PARAM_SUMMARY_NONE;
    bool current = formal_escape_effects_fail_closed(
        ctx->function_param_flow_summaries, caller, formal, &effects);
    return parameter != NULL && parameter->mode == PARAM_MODE_REF && current &&
        (effects & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0;
}

/* A copy can end a returned loan only when the boundary can take ownership of
 * that copy. A `ref` formal is still a borrow; if it is the exact escape site,
 * cloning the inner result would merely hide the caller-to-callee provenance
 * while leaving the copy without a cleanup owner. */
static bool escaped_loan_copy_boundary_repairable(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const ASTNode *escape,
    const ASTNode *source, ASTNode *caller, const ASTNode *loan_site)
{
    if (ctx == NULL || view == NULL || escape == NULL || source == NULL)
        return true;
    if (escape->type == AST_CHANNEL_SEND)
        return false;
    if (escape->type != AST_CALL)
        return true;
    uint32_t builtin = 0;
    if (ast_call_semantic_callee_builtin_kind(escape, &builtin) &&
        builtin == BUILTIN_WRITE) {
        size_t backing_formal = SIZE_MAX;
        /* Writing a compiler-owned local copy transfers that copy into the
         * Slot. A loan still backed by one of this routine's ref formals does
         * not grant teardown authority for a hidden Slot payload, and remains
         * a borrow refusal. */
        bool formal_backed = caller != NULL && loan_site != NULL &&
            function_param_flow_value_backing_formal_current(ctx, caller,
                loan_site, &backing_formal);
        const ASTNode *root = loan_site;
        while (root != NULL && (root->type == AST_MEMBER_ACCESS ||
                root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS
                ? ast_member_object(root) : ast_array_access_array(root);
        uint32_t binding = root != NULL
            ? semantic_param_publication_binding(ctx, root) : 0;
        for (size_t p = 0; !formal_backed && caller != NULL &&
                p < ast_func_param_count(caller); p++) {
            FuncParam *param = ast_func_param(caller, p);
            formal_backed = param != NULL && param->mode == PARAM_MODE_REF &&
                binding != 0 &&
                binding == ast_func_param_stable_id(param);
        }
        return caller != NULL && loan_site != NULL && !formal_backed &&
            semantic_loan_copy_landing_carrier_copies(ctx, source);
    }
    for (size_t actual = 0; actual < ast_call_arg_count(escape); actual++) {
        if (ast_call_argument(escape, actual) != source)
            continue;
        ASTNode *callee = semantic_param_publication_direct_callee(ctx,
            (ASTNode *)escape);
        if (callee == NULL &&
            ast_call_semantic_callee_declared_callable(escape) &&
            ast_call_semantic_callee_value_binding_id(escape) == 0) {
            PendingRoutine *published = publication_routine(
                ctx->function_param_publications,
                ast_call_semantic_callee_decl_id(escape));
            if (published != NULL &&
                semantic_param_publication_routine_inputs_current(
                    ctx, published->decl, false))
                callee = published->decl;
        }
        size_t formal = SIZE_MAX;
        bool mapped = semantic_param_publication_formal_for_actual(ctx,
            (ASTNode *)escape, actual, &formal);
        if (callee == NULL || !mapped || formal >= ast_func_param_count(callee))
            return false;
        FuncParam *parameter = ast_func_param(callee, formal);
        if (parameter == NULL)
            return false;
        /* A bodyful ref boundary is still analyzable: the copy-cut owner can
         * follow its returned-loan facts to the exact constructor or element
         * that must be copied.  Only an opaque ref boundary is unrepairable. */
        if (parameter->mode == PARAM_MODE_REF)
            return ast_func_body(callee) != NULL;
        if (parameter->mode == PARAM_MODE_OWN)
            return true;
        if (parameter->mode != PARAM_MODE_DEFAULT)
            return false;
        /* Default aggregate values transfer their complete descriptor at an
         * opaque call boundary.  Their borrowed element can therefore be
         * copied at the inner construction cut.  Scalar String defaults do
         * not establish that ownership transfer and remain fail-closed. */
        SemanticPublishedValue value =
            semantic_param_publication_value(ctx, source);
        return value.ok && !value.meta &&
            semantic_default_aggregate_value(ctx, value.type);
    }
    for (size_t i = 0; i < view->call_count; i++) {
        const PendingRefCall *boundary = &view->calls[i];
        size_t formal = SIZE_MAX;
        if (boundary->call != escape || boundary->site != source)
            continue;
        if (!semantic_param_publication_ref_call_diagnostic_current(ctx,
                boundary, &formal) || boundary->callee == NULL ||
            formal >= ast_func_param_count(boundary->callee))
            return false;
        FuncParam *parameter = ast_func_param(boundary->callee, formal);
        if (parameter == NULL)
            return false;
        if (parameter->mode == PARAM_MODE_REF)
            return ast_func_body(boundary->callee) != NULL;
        if (parameter->mode == PARAM_MODE_OWN)
            return true;
        if (parameter->mode != PARAM_MODE_DEFAULT)
            return false;
        SemanticPublishedValue value =
            semantic_param_publication_value(ctx, source);
        return value.ok && !value.meta &&
            semantic_default_aggregate_value(ctx, value.type);
    }
    return true;
}

bool semantic_consume_param_publication(SemanticContext *ctx, const SemanticParamPublicationView *view)
{
    if (!semantic_param_publication_view_current(ctx, view) ||
        ctx->function_param_publications == NULL ||
        !ctx->function_param_publications->flow_summaries_prepared)
        return false;
    /* Backing frames are complete here and escape diagnostics have not yet
     * consumed them.  The named String owner may therefore select an exact
     * copy boundary without recreating the flow provenance. */
    if (!semantic_indexed_string_named_call_prepare(ctx))
        return false;
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
        semantic_check_param_summary_escapes(row->decl, row->count, row->types,
            row->function_type, ctx, row->normal_exit_closed);
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
    for (size_t c = 0; c < view->default_borrow_call_count; c++) {
        const PendingBorrowedDefaultCall *row =
            &view->default_borrow_calls[c];
        size_t formal = 0;
        if (!semantic_param_publication_default_borrow_call_current(
                ctx, row, &formal)) {
            semantic_error(ctx, row->call,
                "Default aggregate borrow identity is incomplete or stale");
            continue;
        }
        ctx->current_function_decl = row->caller;
        ctx->current_module_path = row->module;
        const PendingRoutine *callee = publication_routine(
            ctx->function_param_publications, row->callee_id);
        /* An aggregate result would make the caller treat borrowed backing as
         * owned. String getters are narrower: the existing String backing and
         * landing owners retain the field provenance and select any required
         * copy. Keep aggregate-return lending fail-closed here. */
        bool string_getter = callee != NULL &&
            type_equals(callee->return_type, TYPE_STRING) &&
            semantic_default_aggregate_formal_lends(
                ctx, row->callee, formal);
        bool admitted = string_getter ||
            (!semantic_default_aggregate_formal_moves(
                 ctx, row->callee, formal) &&
             !semantic_default_aggregate_formal_borrow_handoff(
                 ctx, row->callee, formal));
        if (!admitted)
            semantic_validate_borrowed_escape(
                row->site, row->site, ctx, (Type *)row->type,
                row->borrowed, OWNERSHIP_CONSUMER_HELPER_CALL, NULL,
                row->destination, NULL, false, "default", row->fix);
    }
    for (size_t c = 0; c < view->call_count; c++) {
        const PendingRefCall *row = &view->calls[c];
        size_t formal = 0;
        if (!semantic_param_publication_ref_call_diagnostic_current(ctx, row, &formal)) {
            semantic_error(ctx, row->call, "Ref call diagnostic identity is incomplete or stale");
            continue;
        }
        ctx->current_function_decl = row->caller;
        ctx->current_module_path = row->module;
        const PendingRoutine *callee = publication_routine(ctx->function_param_publications, row->callee_id);
        bool string_loan = type_equals(row->type, TYPE_STRING);
        bool aggregate_loan = !type_is_read_view(row->type) &&
            ast_func_body(row->callee) != NULL && callee != NULL &&
            callee->decl == row->callee &&
            semantic_classify_ownership_type(row->type, ctx) ==
                OWNERSHIP_TYPE_BORROW_TRACKED;
        bool exact_loan = string_loan || aggregate_loan;
        unsigned mask = SLOT_PARAM_SUMMARY_NONE;
        if (exact_loan) {
            if (!formal_escape_effects_fail_closed(
                    ctx->function_param_flow_summaries, row->callee, formal,
                    &mask)) {
                semantic_error(ctx, row->call,
                    "Ref call diagnostic requires current outward flow facts");
                continue;
            }
        } else {
            mask = semantic_callable_param_escape_summary(
                row->callee, formal, ctx);
        }
        /* A routine with a body hands a borrowed input back, if at all, as a
         * loan of it, which this caller proves (results lend `ref` inputs). */
        bool loan = exact_loan;
        bool retaining = loan
            ? ast_func_body(row->callee) == NULL
                || ((semantic_param_summary_has_call_escape(mask) || semantic_param_summary_has_channel_escape(mask))
                    && !semantic_param_publication_value_has_static_string_backing(ctx, row->site))
            : semantic_param_summary_has_any_escape(mask);
        if (type_is_read_view(row->type)) {
            semantic_validate_ref_view_payload_escape(row->call, row->site, ctx,
                row->callee, formal, row->borrowed, OWNERSHIP_CONSUMER_HELPER_CALL, row->destination, row->fix);
            continue;
        }
        ASTNode *loan_escape = NULL;
        bool return_escape = semantic_param_summary_has_return_escape(mask);
        bool return_admitted = !return_escape ||
            function_param_flow_return_loan_admitted(ctx, view, row, &loan_escape);
        bool escaped_loan = loan && !retaining && return_escape &&
            !return_admitted;
        /* The next analysis copies the refused loan where it is made
         * (2026-10-22_ref_call_loan_copied_where_made). */
        bool boundary_repairable = !escaped_loan ||
            escaped_loan_copy_boundary_repairable(ctx, view, loan_escape,
                row->call, row->caller, row->site);
        bool formal_owned_channel_escape = escaped_loan &&
            returned_loan_channel_escape_owned_by_formal_current(ctx,
                row->caller, row->site, loan_escape);
        bool escape_reported = false;
        if (escaped_loan && !formal_owned_channel_escape &&
            (boundary_repairable || !retaining)) {
            ASTNode *diagnostic_site = loan_escape != NULL &&
                (loan_escape->type == AST_RETURN ||
                 loan_escape->type == AST_CHANNEL_SEND)
                ? loan_escape : row->call;
            escape_reported = semantic_validate_borrowed_escape(
                diagnostic_site,
                row->site, ctx, row->type, row->borrowed,
                OWNERSHIP_CONSUMER_HELPER_CALL, NULL, row->destination, NULL,
                true, NULL, row->fix);
        }
        if (escaped_loan && boundary_repairable && escape_reported) {
            FunctionParamFlowLoanEscapes copies = {0};
            bool selected = function_param_flow_ref_call_copy_cut_current(
                ctx, row->call, row->ordinal, &copies);
            for (size_t i = 0; selected && i < copies.count; i++)
                selected = semantic_loan_copy_landing_select_carrier(
                    ctx, copies.at[i]);
            if (!selected)
                selected = semantic_loan_copy_landing_select(ctx, row->call);
            if (!selected)
                semantic_error(ctx, row->call,
                    "Loan copy landing selection could not be recorded");
        }
        if (retaining)
            semantic_validate_borrowed_escape(row->site->line != 0 ? row->site : row->call,
                row->site, ctx, row->type, row->borrowed,
                OWNERSHIP_CONSUMER_HELPER_CALL, NULL, row->destination, NULL, true, NULL, row->fix);
    }
    for (size_t c = 0; c < view->read_loan_count; c++) {
        const PendingReadPayloadLoan *row = &view->read_loans[c];
        ctx->current_function_decl = (ASTNode *)row->caller;
        ctx->current_module_path = row->module;
        ASTNode *escape = NULL;
        if (!function_param_flow_read_payload_no_escape(ctx, view, row, &escape))
            semantic_report_read_payload_escape(escape != NULL ? escape : (ASTNode *)row->call,
                (ASTNode *)row->call, ctx, row->result_type);
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
                                     SemanticContext *ctx,
                                     bool normal_exit_closed)
{
    if (node == NULL || ctx == NULL || (param_count != 0 && param_types == NULL)) {
        return;
    }
    if (semantic_param_publication_pending(ctx)) {
        if (!semantic_param_publication_record_routine(ctx, node, param_count,
                param_types, function_type, normal_exit_closed))
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

        /* A method's `self` has no annotation; its resolved type is still
         * the routine's, and its escapes are checked like any parameter's. */
        bool self_param = param != NULL && param->type == NULL && param->name != NULL &&
            strcmp(param->name, "self") == 0 && param_types[i] != NULL && param_types[i] != TYPE_UNKNOWN;
        if (param == NULL || param->name == NULL || (param->type == NULL && !self_param))
            continue;
        if (publication_array_result_borrowed_formal(function_type,
                param_types[i], param->mode, ctx)) {
            summary_mask = semantic_callable_param_escape_summary(node, i, ctx);
            if (semantic_param_summary_has_return_escape(summary_mask)) {
                bool landing_selected = false;
                bool return_copy_selected = false;
                bool transfer_current = param->mode == PARAM_MODE_DEFAULT
                    && semantic_array_storage_default_formal_result_current(
                        ctx, node, ast_func_param_stable_id(param));
                bool landing_current = transfer_current
                    || (param->mode == PARAM_MODE_DEFAULT
                        && semantic_array_storage_select_default_formal_return_copies(
                            ctx, node, ast_func_param_stable_id(param),
                            &return_copy_selected)
                        && return_copy_selected)
                    || (param->mode == PARAM_MODE_DEFAULT
                        && semantic_formal_string_store_landing_select_return_dependency(
                            ctx, node, ast_func_param_stable_id(param),
                            &landing_selected)
                        && landing_selected);
                if (!landing_current)
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                        PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                        node, "Array return still carries backing or element lifetime from non-own parameter '%s'; inferred transfer proof is unavailable",
                        param->name);
            }
            continue;
        }
        if (param->mode != PARAM_MODE_REF)
            continue;

        ownership_class = semantic_classify_ownership_type(param_types[i], ctx);
        bool string_ref = type_equals(param_types[i], TYPE_STRING);
        bool view_ref = type_is_read_view(param_types[i]);
        if (ownership_class == OWNERSHIP_TYPE_COPY_ONLY && !string_ref && !view_ref)
            continue;

        /* The ordinary flow mask records internal call edges as well as real
         * outward effects.  A ref boundary escapes only when the sealed
         * backing-frame facet says its storage reaches a return, retaining
         * call, or channel.  Reading the coarse CALL bit here would reject a
         * fully local chain merely because it passed through a helper. */
        if (string_ref ||
            (ownership_class == OWNERSHIP_TYPE_BORROW_TRACKED && !view_ref)) {
            if (!formal_escape_effects_fail_closed(
                    ctx->function_param_flow_summaries, node, i,
                    &summary_mask)) {
                semantic_error(ctx, node,
                    "Ref parameter escape check requires current outward flow facts");
                summary_mask = SLOT_PARAM_SUMMARY_ALL;
            }
        } else {
            summary_mask = semantic_callable_param_escape_summary(node, i, ctx);
        }
        type_function_set_param_escape_summary(function_type, i, summary_mask);

        if (string_ref
                ? semantic_param_summary_has_call_escape(summary_mask)
                    || semantic_param_summary_has_channel_escape(summary_mask)
                : semantic_param_summary_has_any_escape(summary_mask)) {
            semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_ESCAPE_REF);
        }
        if (view_ref) {
            if (semantic_param_summary_has_return_escape(summary_mask))
                semantic_validate_ref_view_payload_escape(node, node, ctx, node, i, param->name,
                    OWNERSHIP_CONSUMER_RETURN, NULL, NULL);
            if (semantic_param_summary_has_channel_escape(summary_mask))
                semantic_validate_ref_view_payload_escape(node, node, ctx, node, i, param->name,
                    OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL);
            if (semantic_param_summary_has_call_escape(summary_mask))
                semantic_validate_ref_view_payload_escape(node, node, ctx, node, i, param->name,
                    OWNERSHIP_CONSUMER_HELPER_CALL, ast_declaration_name(node), NULL);
            continue;
        }
        /* A returned borrowed `ref` input is a loan its callers prove,
         * whatever the result type (2026-10-19_results_lend_ref_inputs). A
         * handle or subject is not lent, and a receiver is not a call row,
         * so no caller proves a loan of `self`. */
        if ((self_param || (!string_ref && ownership_class != OWNERSHIP_TYPE_BORROW_TRACKED)) &&
            semantic_param_summary_has_return_escape(summary_mask)) {
            semantic_validate_borrowed_escape(
                node, node, ctx, param_types[i], param->name,
                OWNERSHIP_CONSUMER_RETURN, NULL, NULL, NULL,
                false, NULL, NULL);
        }
        if (!string_ref && semantic_param_summary_has_channel_escape(summary_mask)) {
            semantic_validate_borrowed_escape(
                node, node, ctx, param_types[i], param->name,
                OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
                false, NULL, NULL);
        }
        if (!string_ref && semantic_param_summary_has_call_escape(summary_mask)) {
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

static bool formal_escape_effects_fail_closed(
    FunctionParamFlowSummaryStore *store, ASTNode *decl, size_t formal,
    unsigned *effects)
{
    if (effects != NULL) *effects = SLOT_PARAM_SUMMARY_NONE;
    if (decl == NULL || decl->type != AST_FUNC_DECL ||
        formal >= ast_func_param_count(decl) || effects == NULL)
        return false;
    if (store != NULL && function_param_flow_formal_outward_effects_current(store, decl,
            formal, effects)) return true;
    /* An unsupported or not-yet-sealed exact facet is not evidence of no
     * escape.  Keep the ordinary borrow diagnostic fail closed without
     * reviving the coarse internal-CALL mask as a second authority. */
    *effects = SLOT_PARAM_SUMMARY_ALL;
    return true;
}

static ASTNode *returned_loan_nominal_field_source(
    FunctionParamFlowSummaryStore *store,
    FunctionParamFlowProgramPointIndex *index, ASTNode *caller,
    ASTNode *member, const FunctionParamFlowLoanSpan *span)
{
    if (store == NULL || index == NULL || caller == NULL ||
        member == NULL || member->type != AST_MEMBER_ACCESS ||
        ast_member_name(member) == NULL)
        return NULL;
    ASTNode *object = ast_member_object(member);
    if (object == NULL || object->type != AST_IDENTIFIER)
        return NULL;
    uint32_t binding = semantic_param_publication_binding(store->ctx, object);
    ASTNode *definition = NULL;
    for (size_t i = 0; i < index->local_count; i++) {
        if (binding == 0 || binding != index->local_ids[i])
            continue;
        if (definition != NULL || index->local_definitions[i] == NULL ||
            index->local_definitions[i]->type != AST_LET_DECL ||
            ast_let_is_alias(index->local_definitions[i]))
            return NULL;
        definition = index->local_definitions[i];
    }
    ASTNode *constructor = definition != NULL
        ? ast_let_initializer(definition) : NULL;
    PublishedNode *published = constructor != NULL &&
        constructor->type == AST_CALL
        ? publication_result_current(store->ctx, constructor) : NULL;
    const SemanticNominalResultSnapshot *snapshot = published != NULL
        ? published->nominal_result : NULL;
    if (snapshot == NULL ||
        !semantic_nominal_result_snapshot_current(store->ctx, constructor,
            snapshot) ||
        snapshot->field_count != ast_call_arg_count(constructor) ||
        !function_param_flow_loan_owner_unchanged(store, index->claims,
            caller, binding, NULL, NULL, false, false, false, span))
        return NULL;
    for (size_t i = 0; i < snapshot->field_count; i++)
        if (snapshot->fields[i].field_name != NULL &&
            strcmp(snapshot->fields[i].field_name,
                ast_member_name(member)) == 0)
            return ast_call_argument(constructor, i);
    return NULL;
}

static bool returned_loan_sources_admitted(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowProgramPointIndex *index, const SemanticParamPublicationView *view,
    const PendingRefCall *loan, unsigned outward_effects,
    bool *independent_source)
{
    if (independent_source == NULL) return false;
    *independent_source = true;
    /* A prior landing can replace the exact lent argument with an independent
     * Clone. Its checked result relation is the source-lifetime proof; walking
     * through that Clone to the retired borrowed argument would select a
     * redundant Clone of the callee result in the next round. */
    if (semantic_loan_copy_landing_satisfied(store->ctx, loan->site))
        return true;
    ASTNode **queue = malloc(sizeof(*queue));
    if (queue == NULL) return false;
    queue[0] = loan->site;
    size_t count = 1;
    ASTNode **locals = index->local_definitions;
    uint32_t *local_ids = index->local_ids;
    size_t local_count = index->local_count;
    FunctionParamFlowIndexClaims *claims = index->claims;
    FunctionParamFlowLoanSpan span = function_param_flow_loan_span(
        store->ctx, claims, loan->call, NULL, 0,
        ast_func_body(loan->caller));
    /* This source walk has not yet collected the locals that receive the
     * loan, so it may use only the span's proved-before-call ordering.  A
     * scope or after-last-use answer with an empty carrier set would hide a
     * real mutation after the returned loan (the consumer walk owns that
     * stronger lifetime proof). */
    span.scope = SIZE_MAX;
    span.uses_listed = false;
    bool admitted = true;
    bool local_owned_source = false;
    for (size_t at = 0; at < count && admitted; at++) {
        ASTNode *value = queue[at];
        SemanticPublishedValue published = semantic_param_publication_value(
            store->ctx, value);
        if (!published.ok) { admitted = false; break; }
        /* A local can acquire a ref formal's backing after its declaration.
         * Following only the declaration initializer loses that established
         * relation (for example, a projection filled on one branch).  The
         * backing-frame owner already proves both the current local identity
         * and that it carries exactly one formal, so continue from that
         * formal instead of reconstructing reaching definitions here. */
        size_t backing_formal = SIZE_MAX;
        if (function_param_flow_value_backing_formal_current(store->ctx,
                loan->caller, value, &backing_formal)) {
            *independent_source = false;
            FuncParam *param = backing_formal < ast_func_param_count(loan->caller)
                ? ast_func_param(loan->caller, backing_formal) : NULL;
            unsigned source_effects = SLOT_PARAM_SUMMARY_NONE;
            bool source_ready = param != NULL &&
                function_param_flow_formal_outward_effects_current(store,
                    loan->caller, backing_formal, &source_effects);
            if (param == NULL || param->mode != PARAM_MODE_REF ||
                !source_ready ||
                (source_effects & outward_effects) != outward_effects)
                admitted = false;
            continue;
        }
        if (value->type == AST_IDENTIFIER) {
            const ASTNode *root = NULL;
            if (!function_param_flow_loan_storage_root(store, claims, loan->caller, value, &root) || root == NULL) { admitted = false; break; }
            value = (ASTNode *)root;
        }
        if (semantic_param_publication_value_has_static_string_backing(store->ctx, value)) continue;
        if (semantic_loan_copy_landing_satisfied(store->ctx, value)) continue;
        ASTNode *dependency = NULL;
        if (value->type == AST_MEMBER_ACCESS) {
            dependency = returned_loan_nominal_field_source(store, index,
                loan->caller, value, &span);
            if (dependency == NULL)
                dependency = ast_member_object(value);
        }
        else if (value->type == AST_ARRAY_ACCESS) dependency = ast_array_access_array(value);
        else if (value->type == AST_IDENTIFIER) {
            uint32_t binding = semantic_param_publication_binding(store->ctx, value);
            bool formal = false, by_value_formal = false;
            for (size_t p = 0; p < ast_func_param_count(loan->caller); p++) {
                FuncParam *param = ast_func_param(loan->caller, p);
                if (param == NULL || binding == 0 || binding != ast_func_param_stable_id(param)) continue;
                unsigned source_effects = SLOT_PARAM_SUMMARY_NONE;
                bool source_ready =
                    function_param_flow_formal_outward_effects_current(store,
                        loan->caller, p, &source_effects);
                formal = true;
                /* A loan that stays here may borrow a by-value input this
                 * routine leaves unchanged, checked below as a local is. */
                if (param->mode == PARAM_MODE_DEFAULT && outward_effects == 0 &&
                    source_ready) by_value_formal = true;
                else if (param->mode != PARAM_MODE_REF || !source_ready ||
                    (source_effects & outward_effects) != outward_effects)
                    admitted = false;
            }
            if (formal && !by_value_formal) {
                *independent_source = false;
                continue;
            }
            for (size_t i = 0; !formal && i < local_count; i++) {
                if (binding != 0 && binding == local_ids[i]) dependency = ast_let_initializer(locals[i]);
            }
            if ((dependency != NULL || by_value_formal) &&
                !function_param_flow_loan_owner_unchanged(store, claims,
                    loan->caller, binding, NULL, NULL, by_value_formal, false,
                    false, &span)) admitted = false;
            if (dependency == NULL && !by_value_formal) admitted = false;
            if (by_value_formal) *independent_source = false;
            /* A loan that stays here needs only an owner no write reaches. */
            if (outward_effects == 0) continue;
        } else if (value->type == AST_CALL) {
            if (semantic_indexed_string_independent_result(value,
                    store->ctx)) {
                local_owned_source = true;
                continue;
            }
            bool found = false;
            for (size_t c = 0; c < view->call_count; c++) {
                const PendingRefCall *nested = &view->calls[c];
                if (nested->call != value) continue;
                if (!semantic_param_publication_ref_call_current(store->ctx, nested) || nested->caller != loan->caller) { admitted = false; break; }
                size_t formal = 0;
                if (!semantic_param_publication_formal_for_actual(store->ctx,
                        nested->call, nested->ordinal, &formal)) { admitted = false; break; }
                SlotFunctionLookup lookup = {store->ctx, store->program_root};
                unsigned nested_effects = SLOT_PARAM_SUMMARY_NONE;
                if (!function_param_flow_callee_outward_effects_current(
                        &lookup, nested->call, nested->ordinal,
                        &nested_effects)) {
                    admitted = false;
                    break;
                }
                if ((nested_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0)
                    continue;
                found = true;
                bool seen = false;
                for (size_t q = 0; q < count; q++) if (queue[q] == nested->site) seen = true;
                if (seen) { admitted = false; break; }
                if (count >= store->work_budget || count == SIZE_MAX / sizeof(*queue)) { admitted = false; break; }
                ASTNode **grown = realloc(queue, (count + 1) * sizeof(*queue));
                if (grown == NULL) { admitted = false; break; }
                queue = grown; queue[count++] = nested->site;
            }
            if (!found) admitted = false;
            continue;
        } else { admitted = false; break; }
        if (dependency != NULL && admitted) {
            bool seen = false;
            for (size_t q = 0; q < count; q++) if (queue[q] == dependency) seen = true;
            if (seen || count >= store->work_budget || count == SIZE_MAX / sizeof(*queue)) { admitted = false; break; }
            ASTNode **grown = realloc(queue, (count + 1) * sizeof(*queue));
            if (grown == NULL) { admitted = false; break; }
            queue = grown; queue[count++] = dependency;
        }
    }
    if (admitted && local_owned_source) {
        FunctionParamFlowBackingFact *result =
            function_param_flow_declared_result_frame_current(store,
                loan->call, NULL);
        if (result == NULL) {
            admitted = false;
        } else {
            /* A direct ref-return can transfer this caller-owned source at
             * its final destination.  Once the callee's declared result is
             * itself carried through another call, the outer boundary no
             * longer owns that move; keep the existing copy cut.  The
             * declared-result frame owns this dependency identity. */
            for (size_t i = 0; admitted && i < result->node_count; i++)
                if (result->nodes[i].result_callee_id != 0)
                    admitted = false;
        }
    }
    function_param_flow_loan_span_release(&span);
    free(queue);
    return admitted && !store->failed;
}

static bool sparse_loan_admitted(SemanticContext *ctx,
    const SemanticParamPublicationView *view, ASTNode *caller, ASTNode *seed,
    uint32_t owner_slot_id, uint32_t restore_owner_id,
    const PendingRefCall *return_loan, unsigned allowed_effects,
    ASTNode **escape_site, ASTNode **escape_carrier, const FunctionParamFlowLoanEscapes *copied,
    const FunctionParamFlowResultPath *owner_path,
    const FunctionParamFlowResultPath *seed_path, LoanCarriers *carriers)
{
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index;
    if (store == NULL || store->failed || store->solving ||
        caller == NULL || caller->type != AST_FUNC_DECL ||
        !function_param_flow_program_point_index_build(store, caller, &index) ||
        !function_param_flow_backing_rows_current(store, caller)) return false;
    if (!function_param_flow_program_point_index_build(store, caller, &index)) return false;
    bool private_stores_closed = index->private_local_stores && function_param_flow_no_store_graph(store, caller);
    if (!function_param_flow_program_point_index_build(store, caller, &index)) return false;
    /* One origin and one external-destination flag: neither is a formal ordinal. */
    unsigned effects = 0, outward_effects = 0;
    uint64_t outputs = 0;
    FunctionParamFlowBackingFact fact = {.origin_count = 1, .origin_words = 1,
        .destination_count = 1, .destination_words = 1, .effects = &effects,
        .outward_effects = &outward_effects, .outputs = &outputs,
        .complete = true, .claims = index->claims};
    ReturnedBacking state = {0};
    state.store = store;
    state.lookup = lookup;
    state.decl = caller;
    state.fact = &fact;
    state.private_stores_closed = private_stores_closed;
    state.loan_seed = seed;
    state.loan_owner_slot_id = owner_slot_id;
    state.loan_restore_owner_id = restore_owner_id;
    state.copied = copied;
    if (owner_path != NULL) state.loan_owner_path = *owner_path;
    state.loan_owner_path_current = owner_path != NULL && owner_path->present;
    for (size_t p = 0; p < ast_func_param_count(caller); p++) {
        FuncParam *param = ast_func_param(caller, p);
        FunctionParamFlowBackingBinding *binding = backing_binding(&state, ast_func_param_stable_id(param), true);
        if (binding == NULL) break;
        if (param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF) binding->destinations[0] = 1;
    }
    ASTNode **roots = index->dependency_roots;
    size_t root_count = index->dependency_root_count;
    do {
        if (fact.epoch == SIZE_MAX) { function_param_flow_fail(store, caller, "backing iteration identity overflow"); fact.complete = false; break; }
        fact.epoch++; fact.changed = false;
        for (size_t r = 0; r < root_count && fact.complete; r++) backing_consumers(&state, roots[r]);
    } while (fact.changed && fact.complete);
    if (state.escape_site != NULL) *escape_site = state.escape_site;
    if (escape_carrier != NULL) *escape_carrier = state.escape_carrier;
    /* A returned loan keeps its source storage. Even when the sparse walk
     * finds only the caller's return edge, the source walk must prove static
     * backing, a `ref` formal that the caller also returns, or an unchanged
     * local owner. */
    bool owner_walk = return_loan != NULL;
    bool base_admitted = fact.complete && !fact.pending &&
        function_param_flow_backing_effect_facets_current(&fact) &&
        state.loan_seed_seen && !store->failed &&
        function_param_flow_program_point_index_build(store, caller, &index);
    bool independent_source = false;
    bool source_lifetime_admitted = base_admitted && owner_walk &&
        return_loan != NULL && returned_loan_sources_admitted(store, index,
            view, return_loan, outward_effects, &independent_source);
    const ASTNode *boundary_source = state.escape_carrier != NULL
        ? state.escape_carrier
        : return_loan != NULL ? return_loan->call : NULL;
    uint32_t boundary_builtin = 0;
    bool write_destination = state.escape_site != NULL &&
        state.escape_site->type == AST_CALL &&
        ast_call_semantic_callee_builtin_kind(state.escape_site,
            &boundary_builtin) && boundary_builtin == BUILTIN_WRITE;
    bool inout_destination = false;
    if (state.escape_site != NULL &&
        state.escape_site->type == AST_ASSIGNMENT) {
        const ASTNode *root = ast_assignment_target(state.escape_site);
        while (root != NULL && (root->type == AST_MEMBER_ACCESS ||
                root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS
                ? ast_member_object(root) : ast_array_access_array(root);
        uint32_t binding = root != NULL
            ? semantic_param_publication_binding(ctx, root) : 0;
        for (size_t p = 0; !inout_destination && p <
                ast_func_param_count(caller); p++) {
            FuncParam *param = ast_func_param(caller, p);
            inout_destination = param != NULL &&
                param->mode == PARAM_MODE_MUT_REF && binding != 0 &&
                binding == ast_func_param_stable_id(param);
        }
    }
    bool destination_needs_owner = return_loan != NULL &&
        state.escape_site != NULL &&
        allowed_effects == SLOT_PARAM_SUMMARY_NONE &&
        (inout_destination ||
         (!independent_source &&
          (outward_effects & (SLOT_PARAM_SUMMARY_CALL_ESCAPE |
              SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) != 0 &&
          (state.escape_site->type == AST_CHANNEL_SEND || write_destination ||
           escaped_loan_copy_boundary_repairable(ctx, view,
              state.escape_site, boundary_source, caller,
              return_loan != NULL ? return_loan->site : NULL))));
    /* Source lifetime and destination ownership are separate obligations.
     * A returned loan may still have a live source, yet an aggregate call,
     * inout assignment, or channel destination needs an independent cleanup
     * owner. Refuse only that exact repairable boundary so the landing owner
     * selects its existing copy cut. A static literal or an already checked
     * independent Clone can cross the destination without another copy. */
    bool sources_admitted = source_lifetime_admitted &&
        !destination_needs_owner;
    bool admitted = base_admitted &&
        (((outward_effects & ~allowed_effects) == 0 && !owner_walk) ||
         (return_loan != NULL && sources_admitted));
    for (size_t b = 0; admitted && carriers != NULL && b < fact.binding_count; b++) {
        if (fact.bindings[b].binding == 0 || fact.bindings[b].origins == NULL ||
            (fact.bindings[b].origins[0] & UINT64_C(1)) == 0) continue;
        if (carriers->count == SIZE_MAX / sizeof(*carriers->ids) ||
            carriers->count == SIZE_MAX / sizeof(*carriers->paths)) {
            admitted = false; break;
        }
        uint32_t *grown_ids = realloc(carriers->ids,
            (carriers->count + 1) * sizeof(*grown_ids));
        if (grown_ids == NULL) { admitted = false; break; }
        carriers->ids = grown_ids;
        FunctionParamFlowResultPath *grown_paths = realloc(carriers->paths,
            (carriers->count + 1) * sizeof(*grown_paths));
        if (grown_paths == NULL) { admitted = false; break; }
        carriers->paths = grown_paths;
        carriers->ids[carriers->count] = fact.bindings[b].binding;
        carriers->paths[carriers->count] = (FunctionParamFlowResultPath){0};
        (void)function_param_flow_backing_binding_path_current(store, caller,
            &fact, fact.bindings[b].binding, seed, seed_path,
            &carriers->paths[carriers->count]);
        carriers->count++;
    }
    for (size_t c = 0; admitted && carriers != NULL &&
            c < fact.claims->count; c++) {
        ASTNode *assignment = fact.claims->rows[c].node;
        if (assignment == NULL || assignment->type != AST_ASSIGNMENT ||
            ast_assignment_target(assignment) == NULL) continue;
        bool exact_owner_restore = loan_assignment_restores_owner_path(ctx,
            caller, assignment, restore_owner_id, owner_path);
        if (!exact_owner_restore) continue;
        bool carries = false;
        bool origin_current = function_param_flow_backing_value_origin_current(
            store, &fact, ast_assignment_value(assignment), &carries);
        if (!origin_current || !carries)
            continue;
        if (carriers->preserving_assignment_count ==
                SIZE_MAX / sizeof(*carriers->preserving_assignments)) {
            admitted = false;
            break;
        }
        const ASTNode **grown = realloc(carriers->preserving_assignments,
            (carriers->preserving_assignment_count + 1) * sizeof(*grown));
        if (grown == NULL) {
            admitted = false;
            break;
        }
        carriers->preserving_assignments = grown;
        carriers->preserving_assignments[
            carriers->preserving_assignment_count++] = assignment;
    }
    for (size_t n = 0; n < fact.node_count; n++) {
        free(fact.nodes[n].origins);
        free(fact.nodes[n].origin_path);
    }
    for (size_t b = 0; b < fact.binding_count; b++) {
        free(fact.bindings[b].origins);
        free(fact.bindings[b].destinations);
        free(fact.bindings[b].origin_path);
    }
    free(fact.nodes); free(fact.bindings); free(fact.pending_relations);
    return admitted;
}

bool function_param_flow_return_loan_admitted(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const PendingRefCall *row, ASTNode **escape_site)
{
    if (escape_site == NULL) return false;
    *escape_site = row == NULL ? NULL : row->call;
    if (!semantic_param_publication_view_current(ctx, view) ||
        !semantic_param_publication_ref_call_current(ctx, row)) return false;
    bool registered = false;
    for (size_t c = 0; c < view->call_count; c++) if (&view->calls[c] == row) registered = true;
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    bool dependent_clone = false;
    bool clone_fact_ready = registered &&
        function_param_flow_callee_returned_dependent_clone_current(&lookup,
            row->call, row->ordinal, &dependent_clone);
    if (!registered || !clone_fact_ready || dependent_clone)
        return false;
    if (semantic_loan_copy_landing_ends_loan(ctx, row->caller, row->call))
        return true;

    /* A current literal/static receipt owns process-lifetime backing, but it
     * does not bypass the caller graph's currency proof.  Let the sparse walk
     * validate every consumer and only relax its lifetime effect verdict. */
    unsigned allowed_effects =
        semantic_param_publication_value_has_static_string_backing(
            ctx, row->site)
        ? SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE |
            SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
        : SLOT_PARAM_SUMMARY_NONE;

    /* A store selected in this analysis is already the exact copy boundary
     * for the returned loan.  Replay only those owned store copies here so
     * the call-result consumer does not also select Clone(call) in the same
     * round.  Unsupported stores remain visible and fail closed. */
    FunctionParamFlowLoanEscapes copied = {0};
    ASTNode *escape = NULL, *carrier = NULL;
    for (;;) {
        escape = carrier = NULL;
        if (sparse_loan_admitted(ctx, view, row->caller, row->call, 0, 0,
                row, allowed_effects,
                &escape, &carrier, &copied, NULL, NULL, NULL)) {
            *escape_site = escape;
            return true;
        }
        bool selected_store = carrier != NULL &&
            semantic_formal_store_landing_selects_value(
                ctx, escape, carrier);
        bool fresh = selected_store &&
            copied.count < sizeof(copied.at) / sizeof(copied.at[0]);
        for (size_t i = 0; fresh && i < copied.count; i++)
            fresh = copied.at[i] != carrier;
        if (!fresh) {
            *escape_site = escape != NULL ? escape : row->call;
            return false;
        }
        copied.at[copied.count++] = carrier;
    }
}

/* Records why the lent proof refused `call`'s loan, and shows it under
 * PGY_DEBUG_PIPELINE_STAGE. Returns false, the proof's verdict. */
static bool lent_proof_refused(ASTNode *caller, ASTNode *call,
    FunctionParamFlowLoanFailure *failure, FunctionParamFlowLoanStop stop, const FunctionParamFlowLoanEscapes *copies)
{
    static const char *const names[] = {"not_proved", "uncopyable_exit", "stays_once_copied", "chain_capped", "owner_changed"};
    if (failure != NULL) {
        failure->stop = stop;
        failure->escapes = stop == FUNCTION_PARAM_FLOW_LOAN_UNCOPYABLE_EXIT || stop == FUNCTION_PARAM_FLOW_LOAN_NOT_PROVED
            ? (FunctionParamFlowLoanEscapes){0} : *copies;
    }
    if (getenv("PGY_DEBUG_PIPELINE_STAGE") != NULL) {
        fprintf(stderr, "[semantic loan proof] routine=%s line=%d col=%d stop=%s copies=%zu\n",
            ast_declaration_name(caller) != NULL ? ast_declaration_name(caller) : "<none>",
            call->line, call->column, names[stop], copies->count);
    }
    return false;
}

bool function_param_flow_lent_argument_admitted(SemanticContext *ctx,
    ASTNode *caller, ASTNode *call, const ASTNode *owner_place,
    uint32_t owner, FunctionParamFlowLoanFailure *failure)
{
    SemanticParamPublicationView view;
    if (failure != NULL) *failure = (FunctionParamFlowLoanFailure){0};
    if (ctx == NULL || caller == NULL || call == NULL || owner == 0 ||
        !semantic_param_publication_admitted_view(ctx, &view)) return false;
    /* A loan of a local stays in the routine. One of a formal may be
     * returned when the formal's own summary lends it on in turn. */
    unsigned allowed = 0;
    for (size_t p = 0; p < ast_func_param_count(caller); p++) {
        FuncParam *param = ast_func_param(caller, p);
        if (param != NULL && ast_func_param_stable_id(param) == owner &&
            (param->mode == PARAM_MODE_DEFAULT || param->mode == PARAM_MODE_REF)) {
            unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
            if (function_param_flow_formal_outward_effects_current(ctx->function_param_flow_summaries,
                    caller, p, &outward_effects))
                allowed = outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
        }
    }
    FunctionParamFlowResultPath owner_path = {0};
    bool owner_path_current = owner_place != NULL &&
        function_param_flow_place_path_current(ctx, caller, owner_place,
            owner, &owner_path) && owner_path.present;
    /* A local result can be only a carrier of one current formal's backing.
     * When that formal is itself returned by this routine, forwarding the
     * carrier is the same loan, not a new local lifetime escaping. */
    size_t backing_formal = SIZE_MAX;
    if (allowed == 0 && owner_path_current && owner_path.count != 0 &&
        function_param_flow_value_backing_formal_current(ctx, caller,
            owner_place, &backing_formal)) {
        unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
        if (function_param_flow_formal_outward_effects_current(ctx->function_param_flow_summaries,
                caller, backing_formal, &outward_effects))
            allowed = outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
    }
    /* A next analysis would copy where the loan leaves and could then find
     * it leave at a next place, one per analysis. Each walk here reads the
     * copies before it as made instead, up to a fixed count; a store this
     * analysis selected stores a copy there, made by the store owner. */
    ASTNode *escape = NULL, *carrier = NULL;
    LoanCarriers carriers = {0};
    FunctionParamFlowLoanEscapes copied = {0}, ours = {0};
    FunctionParamFlowResultPath seed_path = {0};
    bool seed_path_current = false;
    if (call->type == AST_CALL && owner_place != NULL) {
        size_t selected = SIZE_MAX;
        for (size_t a = 0; a < ast_call_arg_count(call); a++) {
            if (ast_call_argument(call, a) != owner_place) continue;
            if (selected != SIZE_MAX) { selected = SIZE_MAX; break; }
            selected = a;
        }
        SlotFunctionLookup path_lookup = {ctx,
            ctx->function_param_publications->program};
        FunctionParamFlowSummaryStore *path_store =
            function_param_flow_store_get(&path_lookup);
        seed_path_current = selected != SIZE_MAX && path_store != NULL &&
            function_param_flow_call_result_path_current(path_store, call,
                selected, &seed_path) && seed_path.present;
    }
    while (!sparse_loan_admitted(ctx, &view, caller, call, 0, owner, NULL,
            allowed,
            &escape, &carrier, &copied,
            owner_path_current ? &owner_path : NULL,
            seed_path_current ? &seed_path : NULL, &carriers)) {
        loan_carriers_release(&carriers);
        /* The walk names the exact call through which the value left.  The
         * formal-store owner decides whether that call already lands it. */
        bool stored = carrier != NULL &&
            semantic_formal_store_landing_selects_value(ctx, escape, carrier);
        bool copies = stored || (carrier != NULL && semantic_loan_copy_landing_carrier_copies(ctx, carrier));
        bool next = copies && copied.count < sizeof(copied.at) / sizeof(copied.at[0]);
        for (size_t c = 0; next && c < copied.count; c++) next = copied.at[c] != carrier;
        if (next) {
            copied.at[copied.count++] = carrier;
            if (!stored) ours.at[ours.count++] = carrier;
            escape = carrier = NULL;
            continue;
        }
        /* The chain is kept only when the walks ran out of count; a place no
         * copy can stand at, or one a copy did not help, drops it. */
        bool capped = copies && copied.count == sizeof(copied.at) / sizeof(copied.at[0]);
        return lent_proof_refused(caller, call, failure,
            capped ? FUNCTION_PARAM_FLOW_LOAN_CHAIN_CAPPED : FUNCTION_PARAM_FLOW_LOAN_UNCOPYABLE_EXIT, &ours);
    }
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index;
    if (store == NULL || store->failed || !function_param_flow_program_point_index_build(store, caller, &index)) {
        loan_carriers_release(&carriers);
        return lent_proof_refused(caller, call, failure, FUNCTION_PARAM_FLOW_LOAN_NOT_PROVED, &ours);
    }
    FunctionParamFlowLoanSpan span = function_param_flow_loan_span(ctx, index->claims, call, carriers.ids, carriers.count,
        ast_func_body(caller));
    span.preserving_assignments = carriers.preserving_assignments;
    span.preserving_assignment_count = carriers.preserving_assignment_count;
    /* A local projection that is proved to carry exactly one formal's
     * backing is not a second owner.  Its permitted return is the formal's
     * own loan continuing through the exact result path, so judge it with
     * the same carrier rule used for locals reached after the call. */
    bool owner_is_formal_carrier = backing_formal != SIZE_MAX && allowed != 0 &&
        owner_path_current && owner_path.count != 0;
    bool unchanged = function_param_flow_loan_owner_unchanged(store,
        index->claims, caller, owner, owner_place, NULL, true, false,
        owner_is_formal_carrier, &span);
    /* The loan shares the owner's storage, so a change through a local the
     * loan reached changes the owner. A local of a String, an Option of one
     * or a struct of such fields holds none of it, and a local Array literal
     * that holds parts of the loan has a descriptor of its own: a push or set
     * on it changes nothing it holds. */
    for (size_t k = 0; unchanged && k < carriers.count; k++) {
        bool holds = true, literal = false;
        for (size_t c = 0; c < index->claims->count; c++) {
            const FunctionParamFlowIndexClaim *row = &index->claims->rows[c];
            if (row->id != carriers.ids[k] || row->node == NULL || row->node->type != AST_LET_DECL) continue;
            SemanticPublishedValue value = semantic_param_publication_value(ctx, ast_let_initializer(row->node));
            holds = !value.ok || value.meta || !semantic_ownership_type_holds_no_container(value.type, ctx);
            literal = ast_let_initializer(row->node) != NULL && ast_let_initializer(row->node)->type == AST_ARRAY_LITERAL;
            break;
        }
        unchanged = !holds || function_param_flow_loan_owner_unchanged(store,
            index->claims, caller, carriers.ids[k], NULL,
            carriers.paths[k].present ? &carriers.paths[k] : NULL,
            true, literal, true, &span);
    }
    loan_carriers_release(&carriers);
    function_param_flow_loan_span_release(&span);
    /* The loan stays only once the copies the walks read as made are made.
     * When its owner still changes with them, they are kept too: the move
     * owner makes them only where no copy can stand where the loan is made,
     * and the next analysis judges the owner with every copy in place. */
    if (!unchanged)
        return lent_proof_refused(caller, call, failure, FUNCTION_PARAM_FLOW_LOAN_OWNER_CHANGED, &ours);
    if (copied.count != 0)
        return lent_proof_refused(caller, call, failure, FUNCTION_PARAM_FLOW_LOAN_STAYS_ONCE_COPIED, &ours);
    return true;
}

bool function_param_flow_call_result_stays(SemanticContext *ctx, ASTNode *caller, ASTNode *call)
{
    SemanticParamPublicationView view;
    ASTNode *escape = NULL;
    return ctx != NULL && caller != NULL && call != NULL &&
        semantic_param_publication_admitted_view(ctx, &view) &&
        sparse_loan_admitted(ctx, &view, caller, call, 0, 0, NULL, 0,
            &escape,
            NULL, NULL, NULL, NULL, NULL);
}

bool function_param_flow_call_result_returns_current(
    SemanticContext *ctx, ASTNode *caller, ASTNode *call)
{
    SemanticParamPublicationView view;
    ASTNode *escape = NULL;
    if (ctx == NULL || caller == NULL || call == NULL ||
        !semantic_param_publication_admitted_view(ctx, &view))
        return false;
    if (sparse_loan_admitted(ctx, &view, caller, call, 0, 0, NULL, 0,
            &escape, NULL, NULL, NULL, NULL, NULL))
        return false;
    escape = NULL;
    return sparse_loan_admitted(ctx, &view, caller, call, 0, 0, NULL,
            SLOT_PARAM_SUMMARY_RETURN_ESCAPE,
            &escape,
            NULL, NULL, NULL, NULL, NULL) &&
        escape != NULL && escape->type == AST_RETURN;
}

bool function_param_flow_read_payload_no_escape(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const PendingReadPayloadLoan *row, ASTNode **escape_site)
{
    if (escape_site == NULL) return false;
    *escape_site = row == NULL ? NULL : (ASTNode *)row->call;
    uint32_t owner = 0;
    if (!semantic_param_publication_view_current(ctx, view) ||
        !semantic_param_publication_read_payload_loan_current(ctx, row, &owner)) return false;
    bool registered = false;
    for (size_t c = 0; c < view->read_loan_count; c++) if (&view->read_loans[c] == row) registered = true;
    return registered && sparse_loan_admitted(ctx, view, (ASTNode *)row->caller,
        (ASTNode *)row->call, owner, 0, NULL, 0,
        escape_site, NULL, NULL, NULL, NULL, NULL);
}
