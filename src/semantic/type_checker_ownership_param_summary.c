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
#include "function_param_flow_summary_internal.h"
#include "typed_map_query_fact_owner.h"
#include "loan_copy_landing_owner.h"
#include "diag_codes.h"
#include <stdlib.h>
#include <string.h>

/* The bindings a loan's backing reached. */
typedef struct {
    uint32_t *ids;
    size_t count;
} LoanCarriers;

static bool sparse_loan_admitted(SemanticContext *ctx,
    const SemanticParamPublicationView *view, ASTNode *caller, ASTNode *seed,
    uint32_t owner_slot_id, const PendingRefCall *return_loan, unsigned allowed_effects,
    ASTNode **escape_site, ASTNode **escape_carrier, LoanCarriers *carriers);

static bool array_result_borrowed_formal(Type *function_type, const Type *formal_type,
    ParamMode mode, SemanticContext *ctx)
{
    const Type *result_type = function_type == NULL ? NULL : type_function_return_type(function_type);
    return type_is_constructed_named(result_type, "Array") &&
        (mode == PARAM_MODE_DEFAULT || mode == PARAM_MODE_MUT_REF) &&
        formal_type != NULL && formal_type != TYPE_UNKNOWN &&
        !publication_physical_scalar(formal_type) &&
        semantic_classify_ownership_type(formal_type, ctx) == OWNERSHIP_TYPE_BORROW_TRACKED;
}

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
            if (param != NULL && (param->mode == PARAM_MODE_REF ||
                array_result_borrowed_formal(row->function_type, row->types[p], param->mode, ctx)))
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
    for (size_t c = 0; c < view->call_count; c++) {
        const PendingRefCall *row = &view->calls[c];
        size_t formal = 0;
        if (!semantic_param_publication_ref_call_diagnostic_current(ctx, row, &formal)) {
            semantic_error(ctx, row->call, "Ref call diagnostic identity is incomplete or stale");
            continue;
        }
        unsigned mask = semantic_callable_param_escape_summary(row->callee, formal, ctx);
        ctx->current_function_decl = row->caller;
        ctx->current_module_path = row->module;
        const PendingRoutine *callee = publication_routine(ctx->function_param_publications, row->callee_id);
        /* A routine with a body hands a borrowed input back, if at all, as a
         * loan of it, which this caller proves (results lend `ref` inputs). */
        bool loan = type_equals(row->type, TYPE_STRING) ||
            (ast_func_body(row->callee) != NULL && callee != NULL && callee->decl == row->callee &&
             semantic_classify_ownership_type(row->type, ctx) == OWNERSHIP_TYPE_BORROW_TRACKED);
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
        bool escaped_loan = loan && !retaining &&
            semantic_param_summary_has_return_escape(mask) &&
            !function_param_flow_return_loan_admitted(ctx, view, row, &loan_escape);
        /* The next analysis copies the refused loan where it is made
         * (2026-10-22_ref_call_loan_copied_where_made). */
        if (escaped_loan && semantic_validate_borrowed_escape(loan_escape != NULL ? loan_escape : row->call,
                row->site, ctx, row->type, row->borrowed,
                OWNERSHIP_CONSUMER_HELPER_CALL, NULL, row->destination, NULL, true, NULL, row->fix) &&
            !semantic_loan_copy_landing_select(ctx, row->call))
            semantic_error(ctx, row->call, "Loan copy landing selection could not be recorded");
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
        if (array_result_borrowed_formal(function_type, param_types[i], param->mode, ctx)) {
            summary_mask = semantic_callable_param_escape_summary(node, i, ctx);
            if (semantic_param_summary_has_return_escape(summary_mask))
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                    node, "Array return still carries backing or element lifetime from non-own parameter '%s'; inferred transfer proof is unavailable",
                    param->name);
            continue;
        }
        if (param->mode != PARAM_MODE_REF)
            continue;

        ownership_class = semantic_classify_ownership_type(param_types[i], ctx);
        bool string_ref = type_equals(param_types[i], TYPE_STRING);
        bool view_ref = type_is_read_view(param_types[i]);
        if (ownership_class == OWNERSHIP_TYPE_COPY_ONLY && !string_ref && !view_ref)
            continue;

        summary_mask = semantic_callable_param_escape_summary(node, i, ctx);
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

static bool returned_loan_sources_admitted(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowProgramPointIndex *index, const SemanticParamPublicationView *view,
    const PendingRefCall *loan, unsigned effects)
{
    ASTNode **queue = malloc(sizeof(*queue));
    if (queue == NULL) return false;
    queue[0] = loan->site;
    size_t count = 1;
    ASTNode **locals = index->local_definitions;
    uint32_t *local_ids = index->local_ids;
    size_t local_count = index->local_count;
    FunctionParamFlowIndexClaims *claims = index->claims;
    bool admitted = true;
    for (size_t at = 0; at < count && admitted; at++) {
        ASTNode *value = queue[at];
        if (!semantic_param_publication_value(store->ctx, value).ok) { admitted = false; break; }
        if (value->type == AST_IDENTIFIER) {
            const ASTNode *root = NULL;
            if (!function_param_flow_loan_storage_root(store, claims, loan->caller, value, &root) || root == NULL) { admitted = false; break; }
            value = (ASTNode *)root;
        }
        if (semantic_param_publication_value_has_static_string_backing(store->ctx, value)) continue;
        ASTNode *dependency = NULL;
        if (value->type == AST_MEMBER_ACCESS) dependency = ast_member_object(value);
        else if (value->type == AST_ARRAY_ACCESS) dependency = ast_array_access_array(value);
        else if (value->type == AST_IDENTIFIER) {
            uint32_t binding = semantic_param_publication_binding(store->ctx, value);
            bool formal = false, by_value_formal = false;
            for (size_t p = 0; p < ast_func_param_count(loan->caller); p++) {
                FuncParam *param = ast_func_param(loan->caller, p);
                if (param == NULL || binding == 0 || binding != ast_func_param_stable_id(param)) continue;
                unsigned mask = function_param_flow_summary_for_param(store->ctx, loan->caller, p);
                formal = true;
                /* A loan that stays here may borrow a by-value input this
                 * routine leaves unchanged, checked below as a local is. */
                if (param->mode == PARAM_MODE_DEFAULT && effects == 0 && !store->failed) by_value_formal = true;
                else if (param->mode != PARAM_MODE_REF || (mask & effects) != effects || store->failed) admitted = false;
            }
            if (formal && !by_value_formal) continue;
            for (size_t i = 0; !formal && i < local_count; i++) {
                if (binding != 0 && binding == local_ids[i]) dependency = ast_let_initializer(locals[i]);
            }
            if ((dependency != NULL || by_value_formal) &&
                !function_param_flow_loan_owner_unchanged(store, claims, loan->caller, binding, by_value_formal, false, false, NULL)) admitted = false;
            if (dependency == NULL && !by_value_formal) admitted = false;
            /* A loan that stays here needs only an owner no write reaches. */
            if (effects == 0) continue;
        } else if (value->type == AST_CALL) {
            bool found = false;
            for (size_t c = 0; c < view->call_count; c++) {
                const PendingRefCall *nested = &view->calls[c];
                if (nested->call != value) continue;
                if (!semantic_param_publication_ref_call_current(store->ctx, nested) || nested->caller != loan->caller) { admitted = false; break; }
                size_t formal = 0;
                if (!semantic_param_publication_formal_for_actual(store->ctx,
                        nested->call, nested->ordinal, &formal)) { admitted = false; break; }
                unsigned mask = function_param_flow_summary_for_param(store->ctx, nested->callee, formal);
                if ((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0) continue;
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
    free(queue);
    return admitted && !store->failed;
}

static bool sparse_loan_admitted(SemanticContext *ctx,
    const SemanticParamPublicationView *view, ASTNode *caller, ASTNode *seed,
    uint32_t owner_slot_id, const PendingRefCall *return_loan, unsigned allowed_effects,
    ASTNode **escape_site, ASTNode **escape_carrier, LoanCarriers *carriers)
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
    unsigned effects = 0;
    uint64_t outputs = 0;
    FunctionParamFlowBackingFact fact = {.origin_count = 1, .origin_words = 1,
        .destination_count = 1, .destination_words = 1, .effects = &effects,
        .outputs = &outputs, .complete = true, .claims = index->claims};
    ReturnedBacking state = {.store = store, .lookup = lookup, .decl = caller,
        .fact = &fact, .private_stores_closed = private_stores_closed, .loan_seed = seed,
        .loan_owner_slot_id = owner_slot_id};
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
    /* A String lent by a borrowed aggregate is that aggregate's storage, so
     * even a loan used only here needs an owner this routine leaves
     * unchanged: the source walk proves a `ref` formal or an unmutated local. */
    bool owner_walk = return_loan != NULL && !type_equals(return_loan->type, TYPE_STRING);
    bool admitted = fact.complete && !fact.pending && state.loan_seed_seen && !store->failed &&
        function_param_flow_program_point_index_build(store, caller, &index) &&
        (((effects & ~allowed_effects) == 0 && !owner_walk) ||
         (return_loan != NULL && returned_loan_sources_admitted(store, index, view, return_loan, effects)));
    for (size_t b = 0; admitted && carriers != NULL && b < fact.binding_count; b++) {
        if (fact.bindings[b].binding == 0 || fact.bindings[b].origins == NULL ||
            (fact.bindings[b].origins[0] & UINT64_C(1)) == 0) continue;
        uint32_t *grown = realloc(carriers->ids, (carriers->count + 1) * sizeof(*grown));
        if (grown == NULL) { admitted = false; break; }
        carriers->ids = grown;
        carriers->ids[carriers->count++] = fact.bindings[b].binding;
    }
    for (size_t n = 0; n < fact.node_count; n++) free(fact.nodes[n].origins);
    for (size_t b = 0; b < fact.binding_count; b++) { free(fact.bindings[b].origins); free(fact.bindings[b].destinations); }
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
    if (!registered || (function_param_flow_summary_for_call(&lookup, row->call, row->ordinal) & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0 ||
        !function_param_flow_callee_relation_ready(&lookup, row->call, row->ordinal)) return false;
    return semantic_loan_copy_landing_ends_loan(ctx, row->caller, row->call) ||
        sparse_loan_admitted(ctx, view, row->caller, row->call, 0, row, 0, escape_site, NULL, NULL);
}

bool function_param_flow_lent_argument_admitted(SemanticContext *ctx,
    ASTNode *caller, ASTNode *call, uint32_t owner, FunctionParamFlowLoanFailure *failure)
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
            (param->mode == PARAM_MODE_DEFAULT || param->mode == PARAM_MODE_REF))
            allowed = function_param_flow_summary_for_param(ctx, caller, p) & SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
    }
    ASTNode *escape = NULL, *carrier = NULL;
    LoanCarriers carriers = {0};
    if (!sparse_loan_admitted(ctx, &view, caller, call, 0, NULL, allowed, &escape, &carrier, &carriers)) {
        if (failure != NULL) failure->escape_carrier = carrier;
        free(carriers.ids);
        return false;
    }
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index;
    if (store == NULL || store->failed || !function_param_flow_program_point_index_build(store, caller, &index)) {
        free(carriers.ids);
        return false;
    }
    FunctionParamFlowLoanSpan span = function_param_flow_loan_span(ctx, index->claims, call, carriers.ids, carriers.count,
        ast_func_body(caller));
    bool unchanged = function_param_flow_loan_owner_unchanged(store, index->claims, caller, owner, true, false, false, &span);
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
        unchanged = !holds || function_param_flow_loan_owner_unchanged(store, index->claims, caller, carriers.ids[k], true, literal, true, &span);
    }
    free(carriers.ids);
    function_param_flow_loan_span_release(&span);
    if (!unchanged && failure != NULL) failure->owner_changed = true;
    return unchanged;
}

bool function_param_flow_call_result_stays(SemanticContext *ctx, ASTNode *caller, ASTNode *call)
{
    SemanticParamPublicationView view;
    ASTNode *escape = NULL;
    return ctx != NULL && caller != NULL && call != NULL &&
        semantic_param_publication_admitted_view(ctx, &view) &&
        sparse_loan_admitted(ctx, &view, caller, call, 0, NULL, 0, &escape, NULL, NULL);
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
        (ASTNode *)row->call, owner, NULL, 0, escape_site, NULL, NULL);
}
