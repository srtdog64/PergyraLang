#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_metadata_internal.h"
#include "array_storage_deferred_preservation_owner.h"
#include "collection_ownership_fact.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "../common/string_compat.h"
#include "typed_map_query_fact_owner.h"
#include "../common/match_variant_policy.h"

uint32_t function_param_flow_backing_place(ReturnedBacking *state, ASTNode *node)
{
    while (node != NULL && (node->type == AST_MEMBER_ACCESS || node->type == AST_ARRAY_ACCESS))
        node = node->type == AST_MEMBER_ACCESS ? ast_member_object(node) : ast_array_access_array(node);
    uint32_t id = semantic_param_publication_binding(state->store->ctx, node);
    if (id == 0) state->fact->complete = false;
    return id;
}

/* These queries consume an already captured body; they never prepare a new
 * subtree, summary demand or backing iteration after its charging boundary. */
static FunctionParamFlowProgramPointIndex *function_param_flow_current_claim_index(
    SemanticContext *ctx, ASTNode *routine)
{
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (!semantic_param_publication_routine_inputs_current(ctx, routine, true)) return NULL;
    for (size_t i = 0; i < store->program_point_index_count; i++) {
        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
        if (index->function_decl == routine && index->function_id == ast_node_stable_id(routine))
            return function_param_flow_index_roots_current(index) &&
                function_param_flow_index_claims_current(ctx, index) ? index : NULL;
    }
    return NULL;
}

bool function_param_flow_navigation_result_shape(SemanticContext *ctx, ASTNode *call,
    bool *independent_physical_array)
{
    if (independent_physical_array != NULL) *independent_physical_array = false;
    SemanticPublishedValue result = semantic_param_publication_value(ctx, call);
    if (!result.ok || result.meta) return false;
    if (type_equals(result.type, TYPE_VOID) || type_equals(result.type, TYPE_STRING) ||
        publication_physical_scalar(result.type)) return true;
    bool independent = type_is_constructed_named(result.type, "Array") &&
        publication_physical_scalar(type_get_constructed_arg(result.type, 0)) &&
        semantic_array_storage_owned_result_current(ctx, call);
    if (independent_physical_array != NULL) *independent_physical_array = independent;
    return independent;
}

bool function_param_flow_physical_result_header_current(SemanticContext *ctx, ASTNode *routine)
{
    if (ctx == NULL || routine == NULL || routine->type != AST_FUNC_DECL) return false;
    const Type *type = semantic_type_resolution_lookup_resolved_type(ctx, ast_func_return_type(routine));
    return publication_physical_scalar(type) &&
        semantic_param_publication_routine_result_current(ctx, routine, type);
}

/* Standalone consumption requires both physical input currency and the exact
 * owned physical-Array result; a navigation shape predicate is not that proof. */
bool function_param_flow_nonlocal_physical_input_current(SemanticContext *ctx,
    ASTNode *call, size_t actual)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_inputs_ready(ctx) ||
        ctx->function_param_flow_summaries == NULL || ctx->function_param_flow_summaries->failed ||
        ctx->function_param_flow_summaries->solving) return false;
    SemanticPublishedValue result = semantic_param_publication_value(ctx, call);
    if (!semantic_param_publication_checked_call(ctx, call) || !result.ok || result.meta ||
        !type_is_constructed_named(result.type, "Array") ||
        !publication_physical_scalar(type_get_constructed_arg(result.type, 0)) ||
        !semantic_array_storage_owned_result_current(ctx, call)) return false;
    ASTNode *decl = semantic_param_publication_callee(ctx, call, actual);
    size_t ordinal = SIZE_MAX;
    if (decl == NULL || !semantic_param_publication_checked_argument(ctx, call, actual) ||
        !semantic_param_publication_value_is_physical_scalar(ctx, ast_call_argument(call, actual)) ||
        !semantic_param_publication_formal_for_actual(ctx, call, actual, &ordinal)) return false;
    FuncParam *formal = ast_func_param(decl, ordinal);
    if (formal == NULL || (formal->mode != PARAM_MODE_DEFAULT && formal->mode != PARAM_MODE_REF) ||
        !semantic_param_publication_routine_no_output_header_ready(ctx, decl)) return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    for (size_t e = 0; e < store->count; e++) {
        const FunctionParamFlowSummaryEntry *entry = &store->entries[e];
        if (entry->function_decl != decl || entry->param_index != ordinal) continue;
        if (!entry->output_dependencies_ready || entry->output_dependencies == NULL ||
            entry->output_count != ast_func_param_count(decl)) return false;
        for (size_t p = 0; p < entry->output_count; p++)
            if (entry->output_dependencies[p]) return false;
        return entry->function_id == ast_node_stable_id(decl) &&
            entry->state == FUNCTION_PARAM_FLOW_COMPLETE && !entry->analysis_unsupported &&
            !entry->returned_backing_unproved && !entry->returned_backing_pending &&
            (entry->mask & ~(SLOT_PARAM_SUMMARY_READ | SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) == 0 &&
            function_param_flow_output_current(store, e, false) &&
            function_param_flow_no_store_graph(store, decl) &&
            function_param_flow_current_claim_index(ctx, decl) != NULL;
    }
    return false;
}

bool function_param_flow_typed_expression_children_current(SemanticContext *ctx,
    ASTNode *caller, const ASTNode *expression)
{
    if (ctx == NULL || ctx->has_error || expression == NULL || caller == NULL ||
        !semantic_param_publication_inputs_ready(ctx) || ctx->function_param_flow_summaries == NULL ||
        ctx->function_param_flow_summaries->failed || ctx->function_param_flow_summaries->solving) return false;
    FunctionParamFlowProgramPointIndex *index = function_param_flow_current_claim_index(ctx, caller);
    if (index == NULL) return false;
    for (size_t i = 0; i < index->claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &index->claims->rows[i];
        if (row->node == expression) return row->id != 0 && row->id == ast_node_stable_id(expression) &&
            row->snapshot_complete && row->semantic_promoted && row->snapshot_semantic_complete &&
            !row->snapshot_active && row->value_ready && !row->value_meta;
    }
    return false;
}

static bool function_param_flow_claim_returned_only(const FunctionParamFlowIndexClaims *claims, ASTNode *node)
{
    bool returned = false;
    for (size_t r = 0; r < claims->count; r++)
        for (size_t c = 0; c < claims->rows[r].child_count; c++) if (claims->rows[r].children[c] == node) {
            if (claims->rows[r].shape != AST_RETURN || ast_return_value(claims->rows[r].node) != node) return false;
            returned = true;
        }
    return returned;
}

bool function_param_flow_private_string_receiver_replay(SemanticContext *ctx,
    ASTNode *routine, uint32_t binding)
{
    if (ctx == NULL || ctx->has_error || binding == 0 ||
        !semantic_param_publication_inputs_ready(ctx) || ctx->function_param_flow_summaries == NULL ||
        !ctx->function_param_flow_summaries->validating_current ||
        ctx->function_param_flow_summaries->failed || ctx->function_param_flow_summaries->solving) return false;
    FunctionParamFlowProgramPointIndex *index = function_param_flow_current_claim_index(ctx, routine);
    if (index == NULL || !function_param_flow_no_store_graph(ctx->function_param_flow_summaries, routine)) return false;
    index = function_param_flow_current_claim_index(ctx, routine);
    if (index == NULL) return false;
    const FunctionParamFlowIndexClaims *claims = index->claims;
    ASTNode *initializer = NULL;
    for (size_t r = 0; r < claims->count; r++) if (claims->rows[r].id == binding) {
        ASTNode *decl = claims->rows[r].node;
        if (initializer != NULL || decl->type != AST_LET_DECL || ast_let_is_alias(decl)) return false;
        initializer = ast_let_initializer(decl);
    }
    SemanticPublishedValue birth = semantic_param_publication_value(ctx, initializer);
    const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
        ctx, ast_node_stable_id(routine), binding);
    if (initializer == NULL || initializer->type != AST_ARRAY_LITERAL || ast_array_literal_count(initializer) != 0 ||
        !birth.ok || birth.meta || !type_is_constructed_named(birth.type, "Array") ||
        !type_equals(type_get_constructed_arg(birth.type, 0), TYPE_STRING) || fact == NULL ||
        fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL || fact->source_binding_syntax_id != 0 ||
        fact->origin_syntax_id != ast_node_stable_id(initializer) ||
        fact->element_ownership != PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN ||
        fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE || fact->return_elements_source != NULL ||
        fact->return_elements_source_id != 0) return false;
    for (size_t r = 0; r < claims->count; r++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[r];
        ASTNode *node = row->node;
        if (row->shape == AST_ASSIGNMENT) {
            ASTNode *target = ast_assignment_target(node);
            if (target != NULL && target->type == AST_ARRAY_ACCESS &&
                ast_identifier_binding_syntax_id(ast_array_access_array(target)) == binding) return false;
        }
        for (size_t c = 0; c < row->child_count; c++) {
            ASTNode *child = row->children[c];
            if (child == NULL || child->type != AST_IDENTIFIER ||
                ast_identifier_binding_syntax_id(child) != binding) continue;
            if (semantic_param_publication_binding(ctx, child) != binding) return false;
            if (node->type == AST_ARRAY_ACCESS && ast_array_access_array(node) == child) continue;
            if (node->type == AST_RETURN && ast_return_value(node) == child) continue;
            if (node->type == AST_CALL && !row->collection_effect_present &&
                semantic_param_publication_nominal_result_complete(ctx, node) &&
                function_param_flow_claim_returned_only(claims, node)) {
                bool matched = false;
                for (size_t a = 0; a < ast_call_arg_count(node); a++) if (ast_call_argument(node, a) == child) {
                    matched = true;
                    bool depends = false, retains = true;
                    if (!semantic_param_publication_checked_argument(ctx, node, a) ||
                        !semantic_param_publication_checked_result_relation(ctx, node, a, &depends, &retains) ||
                        !depends || retains) return false;
                }
                if (!matched) return false;
                continue;
            }
            if (node->type != AST_CALL || ast_call_argument(node, 0) != child) return false;
            if (ast_call_arg_count(node) == 1 && !row->collection_effect_present &&
                semantic_stdlib_array_argument_preserves_elements(node, 0, ctx)) continue;
            if (!semantic_param_publication_private_array_push(ctx, node, routine) ||
                !row->collection_effect_present || row->collection_effect_kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION ||
                row->collection_effect_receiver != binding || row->collection_effect_source != 0) return false;
        }
    }
    return true;
}

bool function_param_flow_string_result_independent_replay(SemanticContext *ctx, const ASTNode *call)
{
    if (ctx == NULL || ctx->has_error || call == NULL || call->type != AST_CALL ||
        !semantic_param_publication_inputs_ready(ctx) || ctx->function_param_flow_summaries == NULL ||
        !ctx->function_param_flow_summaries->validating_current ||
        ctx->function_param_flow_summaries->failed || ctx->function_param_flow_summaries->solving) return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    const Type *type = NULL;
    ASTNode *decl = semantic_param_publication_declared_result(ctx, call, &type);
    FunctionParamFlowProgramPointIndex *index = decl == NULL ? NULL : function_param_flow_current_claim_index(ctx, decl);
    if (type != TYPE_STRING || index == NULL || index->declared_result == NULL ||
        index->declared_result->epoch == 0 || !index->declared_result->complete ||
        index->declared_result->pending || index->declared_result->building ||
        index->declared_result->analysis_unsupported || !index->declared_result->private_stores_closed ||
        index->declared_result->claims != index->claims ||
        index->declared_result->declared_result_type != TYPE_STRING) return false;
    ASTNode *receiver = NULL;
    if (!semantic_param_publication_instance_receiver(ctx, call, &receiver) ||
        (receiver != NULL && function_param_flow_receiver_readonly_current(store, (ASTNode *)call, false) == NULL)) return false;
    for (size_t a = 0; a < ast_call_arg_count(call); a++) {
        size_t formal = SIZE_MAX, at = SIZE_MAX;
        ASTNode *argument = ast_call_argument(call, a);
        if (!semantic_param_publication_checked_argument(ctx, call, a) ||
            !semantic_param_publication_formal_for_actual(ctx, call, a, &formal)) return false;
        for (size_t e = 0; e < store->count; e++)
            if (store->entries[e].function_decl == decl && store->entries[e].param_index == formal) at = e;
        if (at == SIZE_MAX || store->entries[at].state != FUNCTION_PARAM_FLOW_COMPLETE ||
            store->entries[at].analysis_unsupported || store->entries[at].returned_backing_unproved ||
            store->entries[at].returned_backing_pending ||
            (!semantic_param_publication_value_is_physical_scalar(ctx, argument) &&
             (store->entries[at].mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) ||
            !function_param_flow_output_current(store, at, false)) return false;
    }
    uint32_t source = UINT32_MAX;
    return function_param_flow_string_result_source_current(ctx, call, &source) && source == 0;
}

/* A field-return link is not an independent String or an element cleanup
 * certificate. All admitted leaves must link to the same caller formal. */
bool function_param_flow_string_result_nonlocal_current(SemanticContext *ctx,
    const ASTNode *call, ASTNode *caller, size_t formal_ordinal)
{
    if (ctx == NULL || ctx->has_error || call == NULL || call->type != AST_CALL ||
        caller == NULL || !semantic_param_publication_inputs_ready(ctx) ||
        ctx->function_param_flow_summaries == NULL || ctx->function_param_flow_summaries->failed ||
        ctx->function_param_flow_summaries->solving ||
        !semantic_param_publication_checked_call(ctx, call) ||
        !function_param_flow_typed_expression_children_current(ctx, caller, call)) return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    const Type *result_type = NULL;
    ASTNode *decl = semantic_param_publication_declared_result(ctx, call, &result_type), *receiver = NULL;
    FuncParam *caller_formal = ast_func_param(caller, formal_ordinal);
    uint32_t caller_binding = caller_formal == NULL ? 0 : ast_func_param_stable_id(caller_formal);
    if (decl == NULL || decl == caller || result_type != TYPE_STRING || caller_binding == 0 ||
        semantic_param_publication_direct_callee(ctx, call) != decl ||
        !semantic_param_publication_instance_receiver(ctx, call, &receiver) || receiver != NULL ||
        !function_param_flow_nonlocal_input_current(ctx, caller, formal_ordinal)) return false;
    FunctionParamFlowProgramPointIndex *index = function_param_flow_current_claim_index(ctx, decl);
    if (index == NULL || index->declared_result == NULL || index->declared_result->epoch == 0 ||
        !index->declared_result->complete || index->declared_result->pending ||
        index->declared_result->building || index->declared_result->analysis_unsupported ||
        index->declared_result->claims != index->claims ||
        index->declared_result->declared_result_type != TYPE_STRING) return false;
    const FunctionParamFlowIndexClaims *claims = index->claims;
    size_t reached = 0;
    for (size_t r = 0; r < claims->count; r++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[r];
        if (row->shape != AST_RETURN) continue;
        if (!row->execution_present) return false;
        if (!row->reachable) continue;
        ASTNode *value = ast_return_value(row->node), *returned_caller = NULL, *root = NULL;
        if (!row->semantic_promoted || !row->snapshot_semantic_complete ||
            semantic_param_publication_value(ctx, value).type != TYPE_STRING ||
            !semantic_param_publication_projection_root_current(ctx, value, &returned_caller, &root) ||
            returned_caller != decl || root == NULL) return false;
        uint32_t binding = ast_identifier_binding_syntax_id(root);
        size_t formal = SIZE_MAX;
        for (size_t p = 0; p < ast_func_param_count(decl); p++)
            if (binding != 0 && ast_func_param_stable_id(ast_func_param(decl, p)) == binding) formal = p;
        size_t entry_at = SIZE_MAX;
        for (size_t e = 0; e < store->count; e++)
            if (store->entries[e].function_decl == decl && store->entries[e].param_index == formal)
                entry_at = e;
        if (formal == SIZE_MAX || entry_at == SIZE_MAX) return false;
        const FunctionParamFlowSummaryEntry *entry = &store->entries[entry_at];
        if (!entry->output_dependencies_ready || entry->output_dependencies == NULL ||
            entry->output_count != ast_func_param_count(decl)) return false;
        for (size_t p = 0; p < entry->output_count; p++)
            if (entry->output_dependencies[p]) return false;
        if (entry->state != FUNCTION_PARAM_FLOW_COMPLETE || entry->analysis_unsupported ||
            entry->returned_backing_unproved || entry->returned_backing_pending ||
            (entry->mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0 ||
            (entry->mask & ~(SLOT_PARAM_SUMMARY_READ | SLOT_PARAM_SUMMARY_RETURN_ESCAPE)) != 0 ||
            !function_param_flow_output_current(store, entry_at, false) ||
            !function_param_flow_nonlocal_input_current(ctx, decl, formal)) return false;
        ASTNode *argument = NULL;
        for (size_t a = 0; a < ast_call_arg_count(call); a++) {
            size_t selected = SIZE_MAX;
            if (!semantic_param_publication_checked_argument(ctx, call, a) ||
                !semantic_param_publication_formal_for_actual(ctx, call, a, &selected)) return false;
            if (selected == formal) {
                if (argument != NULL) return false;
                argument = ast_call_argument(call, a);
            }
        }
        ASTNode *actual_caller = NULL, *actual_root = NULL;
        if (argument == NULL || !semantic_param_publication_projection_root_current(ctx,
                argument, &actual_caller, &actual_root) || actual_caller != caller || actual_root == NULL ||
            ast_identifier_binding_syntax_id(actual_root) != caller_binding) return false;
        reached++;
    }
    return reached != 0 && function_param_flow_declared_result_frame_current(store, (ASTNode *)call, NULL) != NULL &&
        function_param_flow_backing_rows_current(store, decl) &&
        function_param_flow_nonlocal_input_current(ctx, caller, formal_ordinal);
}

bool function_param_flow_index_claim_record(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (index->claims == NULL) index->claims = calloc(1, sizeof(*index->claims));
    if (index->claims == NULL) { function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim carrier allocation failed"); return false; }
    return function_param_flow_claim_record(ctx, index->claims, node);
}
bool function_param_flow_claim_record(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node)
{
    return function_param_flow_claim_capture(ctx, claims, node, true);
}
bool function_param_flow_control_expands(ASTNodeType shape)
{
    return shape == AST_BLOCK || shape == AST_IF_STMT || shape == AST_WHILE_LOOP ||
        shape == AST_FOR_LOOP || shape == AST_WITH_STMT || shape == AST_DEFER_STMT;
}
static bool function_param_flow_claim_snapshot(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node, bool eager,
    FunctionParamFlowControlProjection *projection, size_t root, bool project, bool body_roots,
    size_t *claim_out)
{
    if (node == NULL) { if (claim_out != NULL) *claim_out = SIZE_MAX; return !body_roots; }
    if (node->type == AST_TYPE || node->type == AST_CHANNEL_TYPE || node->type == AST_FUTURE_TYPE ||
        node->type == AST_FUNC_DECL || node->type == AST_TYPE_ALIAS || node->type == AST_EVENT_HANDLER_TYPE) eager = false;
    if (!function_param_flow_claim_capture(ctx, claims, node, eager)) return false;
    size_t at = 0;
    while (at < claims->count && claims->rows[at].node != node) at++;
    if (at == claims->count || claims->rows[at].snapshot_active ||
        (project && claims->rows[at].control_projection_active)) {
        claims->failed = true; return false;
    }
    if (claim_out != NULL) *claim_out = at;
    bool snapshot_done = claims->rows[at].snapshot_complete &&
        (!eager || claims->rows[at].snapshot_semantic_complete);
    bool project_needed = project && !claims->rows[at].control_projected;
    if (snapshot_done && !project_needed && !body_roots) return true;
    if (!snapshot_done) claims->rows[at].snapshot_active = true;
    if (project_needed) {
        if (projection == NULL || root >= projection->root_count ||
            projection->projected_claim_count == SIZE_MAX) { claims->failed = true; return false; }
        claims->rows[at].control_projection_active = true;
        claims->rows[at].control_root_ordinal = root;
        if (!function_param_flow_control_expands(node->type) &&
            node->type != AST_BREAK && node->type != AST_CONTINUE) {
            if (!function_param_flow_control_append(ctx, projection, node, at, root)) {
                claims->failed = true; return false;
            }
            claims->rows[at].control_step_ordinal = projection->step_count - 1;
        }
    }
    bool descendants_eager = eager;
    if (eager && claims->rows[at].execution_present && !claims->rows[at].reachable) descendants_eager = false;
    if (node->type == AST_LAMBDA_EXPR || node->type == AST_ASYNC_BLOCK || node->type == AST_SPAWN_EXPR ||
        node->type == AST_EVENT_SUBSCRIBE || node->type == AST_EVENT_UNSUBSCRIBE || node->type == AST_EVENT_INVOKE)
        descendants_eager = false;
    size_t children = claims->rows[at].child_count;
    if (body_roots && (projection == NULL || node->type != AST_BLOCK ||
        children != projection->root_count)) { claims->failed = true; return false; }
    bool visit_children = !snapshot_done || body_roots ||
        (project_needed && descendants_eager && function_param_flow_control_expands(node->type));
    for (size_t c = 0; c < children && visit_children; c++) {
        ASTNode *child = claims->rows[at].children[c];
        if (child == NULL && (node->type == AST_ARRAY_LITERAL || node->type == AST_TUPLE_LITERAL ||
            node->type == AST_SET_LITERAL || node->type == AST_MAP_LITERAL ||
            (node->type == AST_TRANSACTION_BLOCK && c != 0) ||
            (node->type == AST_EVENT_HANDLER_TYPE && c != 0))) {
            claims->failed = true; return false;
        }
        /* The function body wrapper is not a C root; each statement starts
         * with its own publication/currentness, regardless of body metadata. */
        bool child_eager = body_roots || descendants_eager;
        if (node->type == AST_MEMBER_ACCESS && c == 0 &&
            claims->rows[at].unit_enum_selection) child_eager = false;
        if (node->type == AST_CALL && c > ast_call_arg_count(node)) child_eager = false;
        if ((node->type == AST_LET_DECL && c == 1) || (node->type == AST_WITH_STMT && c == 0) ||
            node->type == AST_CONTEXT_ACCESS) child_eager = false;
        if (node->type == AST_MATCH_CASE && (c == 0 || c >= 3) &&
            ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_UNKNOWN) child_eager = false;
        bool child_project = body_roots || (project_needed && descendants_eager &&
            function_param_flow_control_expands(node->type) &&
            (node->type != AST_WITH_STMT || c == 1));
        if (body_roots) projection->root_offsets[c] = projection->step_count;
        if (body_roots && (child == NULL || ast_node_stable_id(child) == 0)) { claims->failed = true; return false; }
        size_t child_at = SIZE_MAX;
        if ((!snapshot_done || child_project) && !function_param_flow_claim_snapshot(ctx, claims, child,
            child_eager, projection, body_roots ? c : root, child_project, false, &child_at)) {
            claims->failed = true; return false;
        }
        if (child != NULL && child_at != SIZE_MAX) claims->rows[at].child_claim_ordinals[c] = child_at;
        if (child_project && child != NULL && (child_at == SIZE_MAX || child_at >= claims->count)) {
            claims->failed = true; return false;
        }
        if (project_needed && child_project && child != NULL) {
            if (claims->rows[at].control_outgoing_count == SIZE_MAX ||
                claims->rows[child_at].control_incoming_count == SIZE_MAX) { claims->failed = true; return false; }
            claims->rows[at].control_outgoing_count++;
            claims->rows[child_at].control_incoming_count++;
        }
        if (body_roots) {
            if (claims->rows[child_at].control_incoming_count == SIZE_MAX) { claims->failed = true; return false; }
            claims->rows[child_at].control_incoming_count++;
            projection->root_claim_ordinals[c] = child_at;
        }
        if (body_roots) projection->root_offsets[c + 1] = projection->step_count;
    }
    if (!snapshot_done) {
        claims->rows[at].snapshot_active = false;
        claims->rows[at].snapshot_complete = true;
        claims->rows[at].snapshot_semantic_complete |= eager;
    }
    claims->rows[at].consumer_empty =
        function_param_flow_consumer_empty_derive(ctx, claims, at);
    if (project_needed) {
        claims->rows[at].control_projection_active = false;
        claims->rows[at].control_projected = true;
        projection->projected_claim_count++;
    }
    return true;
}
bool function_param_flow_index_claims_snapshot(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (ctx == NULL || index == NULL || node == NULL || node != index->dependency_body || ast_node_stable_id(node) != index->dependency_body_id) return false;
    if (index->claims == NULL) index->claims = calloc(1, sizeof(*index->claims));
    if (index->control_projection == NULL) index->control_projection = calloc(1, sizeof(*index->control_projection));
    if (index->claims == NULL || index->control_projection == NULL ||
        index->dependency_root_count == SIZE_MAX ||
        index->dependency_root_count + 1 > SIZE_MAX / sizeof(size_t)) return false;
    FunctionParamFlowControlProjection *projection = index->control_projection;
    if (projection->ready || projection->root_offsets != NULL) return false;
    projection->root_count = index->dependency_root_count;
    projection->root_offsets = calloc(projection->root_count + 1, sizeof(*projection->root_offsets));
    projection->root_claim_ordinals = projection->root_count == 0 ? NULL :
        calloc(projection->root_count, sizeof(*projection->root_claim_ordinals));
    if (projection->root_offsets == NULL ||
        (projection->root_count != 0 && projection->root_claim_ordinals == NULL)) return false;
    bool body_roots = node != NULL && node->type == AST_BLOCK;
    size_t body_claim = SIZE_MAX;
    bool ok = function_param_flow_claim_snapshot(ctx, index->claims, node, true,
        projection, 0, !body_roots, body_roots, &body_claim);
    if (!body_roots && ok) {
        if (body_claim == SIZE_MAX || index->claims->rows[body_claim].control_incoming_count == SIZE_MAX) return false;
        index->claims->rows[body_claim].control_incoming_count++;
        projection->root_claim_ordinals[0] = body_claim;
        projection->root_offsets[1] = projection->step_count;
    }
    projection->ready = ok;
    return ok;
}
bool function_param_flow_syntax_claims_snapshot(SemanticContext *ctx,
    FunctionParamFlowIndexClaims **claims, ASTNode *node)
{
    if (ctx == NULL || claims == NULL || node == NULL) return false;
    if (*claims == NULL) *claims = calloc(1, sizeof(**claims));
    return *claims != NULL && function_param_flow_claim_snapshot(ctx, *claims, node, false,
        NULL, SIZE_MAX, false, false, NULL);
}

static bool function_param_flow_claim_current_failure(
    const FunctionParamFlowIndexClaim *row, size_t ordinal,
    const char *reason)
{
    if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
        fprintf(stderr,
            "pgy: function-param-flow-claim-stale row=%zu shape=%d"
            " id=%u line=%u column=%u reason=%s\n",
            ordinal, row != NULL ? (int)row->shape : -1,
            row != NULL ? row->id : 0,
            row != NULL && row->node != NULL ? row->node->line : 0,
            row != NULL && row->node != NULL ? row->node->column : 0,
            reason);
    }
    return false;
}

bool function_param_flow_claims_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, bool semantic)
{
    if (claims == NULL || claims->failed || claims->count == 0 || claims->rows == NULL) return false;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        bool supported = false;
        size_t children = row->node == NULL ? 0 : function_param_flow_claim_child_count(row->node, &supported);
        if (row->node == NULL || (row->child_count != 0 && (row->children == NULL || row->child_ids == NULL)) ||
            row->shape != row->node->type || row->id != ast_node_stable_id(row->node) ||
            !supported || row->child_count != children)
            return function_param_flow_claim_current_failure(row, i,
                "syntax identity or child count changed");
        if (semantic && row->snapshot_complete && row->child_count != 0 &&
            row->child_claim_ordinals == NULL)
            return function_param_flow_claim_current_failure(row, i,
                "sealed child ordinals are missing");
        for (size_t c = 0; c < row->child_count; c++) {
            if (row->children[c] != function_param_flow_claim_child(row->node, c) ||
                row->child_ids[c] != ast_node_stable_id(row->children[c])) return false;
            if (semantic && row->snapshot_complete) {
                size_t child = row->child_claim_ordinals[c];
                if (row->children[c] == NULL ? child != SIZE_MAX :
                    child >= claims->count || claims->rows[child].node != row->children[c] ||
                    claims->rows[child].id != row->child_ids[c] ||
                    !claims->rows[child].snapshot_complete || claims->rows[child].snapshot_active)
                    return function_param_flow_claim_current_failure(row, i,
                        "child identity or snapshot changed");
            }
        }
        if (row->shape == AST_IDENTIFIER && row->binding != ast_identifier_binding_syntax_id(row->node))
            return function_param_flow_claim_current_failure(row, i, "identifier binding changed");
        if (row->shape == AST_LET_DECL && row->local_alias != ast_let_is_alias(row->node))
            return function_param_flow_claim_current_failure(row, i, "local alias classification changed");
        if (row->shape == AST_MATCH_CASE && row->match_family != ast_match_case_semantic_subject_family(row->node))
            return function_param_flow_claim_current_failure(row, i, "match family changed");
        if (row->shape == AST_BINARY && row->syntax_operator != (int)row->node->data.binary.op.type)
            return function_param_flow_claim_current_failure(row, i, "binary operator changed");
        if (row->shape == AST_UNARY && row->syntax_operator != (int)row->node->data.unary.op.type)
            return function_param_flow_claim_current_failure(row, i, "unary operator changed");
        if (row->shape == AST_PARALLEL_BLOCK && row->parallel_join != ast_parallel_is_join_form(row->node)) return false;
        if (row->shape == AST_BOOLEAN && row->syntax_literal_flags != (unsigned)ast_boolean_value(row->node)) return false;
        if (row->shape == AST_NUMBER && (row->syntax_number != ast_number_value(row->node) ||
            row->syntax_long != ast_number_exact_long_value(row->node) || row->syntax_literal_flags !=
            (unsigned)(ast_number_is_long(row->node) | (ast_number_is_float(row->node) << 1) | (ast_number_is_duration(row->node) << 2)))) return false;
        const char *syntax_name = row->shape == AST_IDENTIFIER ? ast_identifier_name(row->node) :
            row->shape == AST_MEMBER_ACCESS ? ast_member_name(row->node) :
            row->shape == AST_TYPE ? ast_type_name(row->node) : row->shape == AST_STRING ? ast_string_value(row->node) : NULL;
        if ((syntax_name == NULL) != (row->syntax_name == NULL) ||
            (syntax_name != NULL && strcmp(syntax_name, row->syntax_name) != 0))
            return function_param_flow_claim_current_failure(row, i, "syntax name changed");
        if (row->shape == AST_TYPE || row->shape == AST_CALL) {
            GenericParams *args = row->shape == AST_TYPE ? ast_type_generic_args(row->node) : ast_call_generic_args(row->node);
            size_t count = args == NULL ? 0 : args->count;
            if (count != row->generic_name_count || (count != 0 && row->generic_names == NULL))
                return function_param_flow_claim_current_failure(row, i, "generic arity changed");
            for (size_t p = 0; p < count; p++)
                if (row->generic_names[p] == NULL || strcmp(row->generic_names[p], args->params[p]->name) != 0)
                    return function_param_flow_claim_current_failure(row, i, "generic argument changed");
        }
        if (row->shape == AST_CAST || row->shape == AST_TYPE_TEST) {
            const char *target = row->shape == AST_CAST ? ast_cast_target_type(row->node) : ast_type_test_target_type(row->node);
            if ((target == NULL) != (row->cast_target == NULL) ||
                (target != NULL && strcmp(target, row->cast_target) != 0))
                return function_param_flow_claim_current_failure(row, i, "cast target changed");
        }
        if (row->readfile_result && !function_param_flow_readfile_claim_current(NULL, claims, row))
            return function_param_flow_claim_current_failure(row, i, "ReadFile syntax receipt changed");
        if (!semantic || !row->semantic_promoted) continue;
        bool reachable = false;
        bool execution_present = semantic_param_publication_execution(ctx, row->node, &reachable);
        if (row->execution_present != execution_present ||
            (execution_present && row->reachable != reachable))
            return function_param_flow_claim_current_failure(row, i, "execution reachability changed");
        if (row->unit_enum_selection != (row->shape == AST_MEMBER_ACCESS &&
            semantic_param_publication_checked_unit_enum_selection(ctx, row->node)))
            return function_param_flow_claim_current_failure(row, i, "unit enum classification changed");
        if (execution_present && !reachable) continue;
        if (row->shape == AST_LET_DECL && row->channel_binding != semantic_param_publication_channel_binding(ctx, row->node)) return false;
        if (row->shape == AST_LET_DECL && row->empty_map_binding != semantic_param_publication_empty_map_binding(ctx, row->node)) return false;
        if (row->shape == AST_LET_DECL || row->shape == AST_LET_DESTRUCTURE) {
            SemanticPublishedSlotBirth birth;
            if (row->owned_slot_binding != semantic_param_publication_owned_slot_binding(ctx, row->node, &birth)) return false;
        }
        SemanticPublishedValue value = semantic_param_publication_value(ctx, row->node);
        if (row->value_ready != value.ok || row->value_meta != (value.ok && value.meta) ||
            row->value_origin_ready != (value.ok && value.origin_ready) || row->value_type != value.type)
            return function_param_flow_claim_current_failure(row, i, "published value changed");
        if (row->physical_value != semantic_param_publication_value_is_physical_scalar(ctx, row->node))
            return function_param_flow_claim_current_failure(row, i, "physical scalar classification changed");
        if (row->slot_token_operand != semantic_param_publication_slot_token_operand(ctx, row->node))
            return function_param_flow_claim_current_failure(row, i, "slot-token classification changed");
        if (row->shape == AST_ARRAY_LITERAL) {
            /* Replay the same current typed premise as the no-output selector.
             * Child count/identity above seal every element evaluation. */
            if (!value.ok || value.meta)
                return function_param_flow_claim_current_failure(row, i, "array literal value is no longer concrete");
        }
        if (row->shape == AST_CALL) {
            if (row->readfile_result != semantic_param_publication_checked_readfile_independent(ctx, row->node) ||
                (row->readfile_result && !function_param_flow_readfile_claim_current(ctx, claims, row)))
                return function_param_flow_claim_current_failure(row, i, "ReadFile semantic receipt changed");
            uint32_t kind = 0, receiver = 0, source = 0;
            bool present = ast_call_semantic_collection_effect(row->node, &kind, &receiver, &source);
            if (present != row->collection_effect_present || kind != row->collection_effect_kind ||
                receiver != row->collection_effect_receiver || source != row->collection_effect_source)
                return function_param_flow_claim_current_failure(row, i, "collection effect changed");
            if (row->slot_write != semantic_param_publication_checked_slot_write(ctx, row->node))
                return function_param_flow_claim_current_failure(row, i, "slot write classification changed");
            if (row->local_owner_constructor != semantic_param_publication_checked_local_owner_constructor(ctx, row->node))
                return function_param_flow_claim_current_failure(row, i, "local owner constructor changed");
            if (!semantic_param_publication_checked_call(ctx, row->node))
                return function_param_flow_claim_current_failure(row, i, "call receipt is missing");
            if (row->private_array_push != semantic_param_publication_private_array_push(ctx, row->node, NULL))
                return function_param_flow_claim_current_failure(row, i, "private push classification changed");
            bool depends = false, retains = false;
            bool relation = semantic_param_publication_checked_result_relation(ctx, row->node, 0, &depends, &retains);
            if (row->result_relation != relation || (relation &&
                (row->result_depends != depends || row->external_retains != retains)))
                return function_param_flow_claim_current_failure(row, i, "result relation changed");
            bool backing = semantic_param_publication_checked_backing_relation(ctx, row->node, 0, &depends, &retains);
            if (row->backing_relation != backing || (backing &&
                (row->backing_depends != depends || row->backing_external_retains != retains)))
                return function_param_flow_claim_current_failure(row, i, "backing relation changed");
            if ((relation || backing) && (row->result_type_name == NULL ||
                ast_call_semantic_value_type_name(row->node) == NULL ||
                strcmp(row->result_type_name, ast_call_semantic_value_type_name(row->node)) != 0))
                return function_param_flow_claim_current_failure(row, i, "result type projection changed");
        }
        if (row->shape == AST_CAST) {
            const char *target = ast_cast_target_type(row->node);
            if ((target == NULL) != (row->cast_target == NULL) ||
                (target != NULL && strcmp(target, row->cast_target) != 0) ||
                row->numeric_cast != semantic_param_publication_checked_numeric_cast(ctx, row->node)) return false;
        }
        if (row->shape == AST_ASSIGNMENT && ast_assignment_target(row->node) != NULL &&
            ast_assignment_target(row->node)->type == AST_IDENTIFIER &&
            row->binding != semantic_param_publication_binding(ctx, ast_assignment_target(row->node))) return false;
        if (row->shape == AST_ASSIGNMENT && row->assignment_replacement !=
            semantic_param_publication_assignment_replacement(ctx, row->node)) return false;
    }
    return true;
}
bool function_param_flow_syntax_claims_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims)
{
    if (claims == NULL || claims->failed || claims->rows == NULL) return false;
    for (size_t i = 0; i < claims->count; i++)
        if (!claims->rows[i].snapshot_complete || claims->rows[i].snapshot_active) return false;
    return function_param_flow_claims_current(ctx, claims, false);
}
/* Each synchronous validation owns its reached declaration set. Shared DAG
 * descendants and cycles replay once here, never across separate queries. */
void function_param_flow_claims_destroy(FunctionParamFlowIndexClaims *claims)
{
    if (claims == NULL) return;
    for (size_t c = 0; c < claims->count; c++) {
        free(claims->rows[c].children); free(claims->rows[c].child_ids);
        free(claims->rows[c].child_claim_ordinals);
        free(claims->rows[c].cast_target); free(claims->rows[c].syntax_name);
        for (size_t p = 0; claims->rows[c].generic_names != NULL && p < claims->rows[c].generic_name_count; p++)
            free(claims->rows[c].generic_names[p]);
        free(claims->rows[c].generic_names);
        free(claims->rows[c].result_type_name);
    }
    free(claims->rows); free(claims);
}
