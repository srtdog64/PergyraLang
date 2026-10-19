/* Destination walk of a backing frame: the places a value's backing goes
 * when it is stored, aliased or handed to a call. The origin and consumer
 * walks stay in function_param_flow_backing_dependencies.c. */
#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "array_storage_release_owner.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void backing_destination_value(ReturnedBacking *state, ASTNode *value, uint64_t *destinations)
{
    if (value == NULL || state->fact->analysis_unsupported) return;
    if (value != state->loan_seed && value->type == AST_IDENTIFIER) {
        /* The current binding owns this leaf's destination bits. There is no
         * separate per-frame expression state to allocate or revisit. */
        if (ast_node_stable_id(value) == 0 ||
            !function_param_flow_claim_record(state->store->ctx, state->fact->claims, value)) {
            state->fact->complete = false; return;
        }
        SemanticPublishedValue published = semantic_param_publication_value(state->store->ctx, value);
        if (!published.ok) { state->fact->complete = false; return; }
        if ((state->checking_declared_result &&
            semantic_param_publication_value_is_physical_scalar(state->store->ctx, value)) || published.meta) return;
        FunctionParamFlowBackingBinding *source = backing_identifier_row(state, value);
        if (source == NULL && backing_storage_free_enum(state, published)) return;
        if (source == NULL) { state->fact->complete = false; return; }
        source->destination_read_epoch = state->fact->epoch;
        backing_merge(state->fact->destination_words, destinations, source->destinations);
        return;
    }
    size_t at = backing_node(state, value);
    if (at == SIZE_MAX || !backing_work(state, at)) return;
    SemanticPublishedValue published = semantic_param_publication_value(state->store->ctx, value);
    if (!published.ok) { state->fact->complete = false; return; }
    if (value->type == AST_CALL) {
        ASTNode *receiver = NULL;
        ASTNode *direct = semantic_param_publication_direct_callee(state->store->ctx, value);
        /* An intent's value is its Bool verdict and carries no storage. */
        if (direct == NULL && semantic_param_publication_intent_callee(state->store->ctx, value) != NULL) {
            if (!semantic_param_publication_value_is_physical_scalar(state->store->ctx, value)) state->fact->complete = false;
            return;
        }
        if ((direct == NULL && ast_call_semantic_callee_declared_callable(value) &&
             !semantic_param_publication_bodyless_selected_call_current(state->store->ctx, value)) ||
            (direct != NULL && !semantic_param_publication_instance_receiver(state->store->ctx, value, &receiver))) {
            state->fact->complete = false; return;
        }
        if (receiver != NULL) {
            uint64_t *received = calloc(state->fact->destination_words, sizeof(*received));
            if (received == NULL) { function_param_flow_fail(state->store, value, "receiver destination allocation failed"); state->fact->complete = false; return; }
            backing_destination_value(state, receiver, received);
            bool dependent = false;
            for (size_t w = 0; w < state->fact->destination_words; w++) dependent |= received[w] != 0;
            if (dependent && !function_param_flow_receiver_independent(state, value)) {
                /* A result the method returns from its receiver goes where
                 * the receiver goes. */
                size_t entry = backing_receiver_summary(state, value, receiver);
                if (entry == SIZE_MAX) state->fact->complete = false;
                else if ((state->store->entries[entry].mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0)
                    backing_merge(state->fact->destination_words, destinations, received);
            }
            free(received);
            if (!state->fact->complete) return;
        }
    }
    /* Physical values carry no backing. The result frame still requires its
     * closed store graph and eager consumer traversal, including call inputs. */
    if (state->checking_declared_result &&
        semantic_param_publication_value_is_physical_scalar(state->store->ctx, value)) return;
    if (value->type == AST_ARRAY_ACCESS && !published.meta &&
        semantic_param_publication_value_is_physical_scalar(state->store->ctx, value)) {
        ASTNode *array = ast_array_access_array(value), *index = ast_array_access_index(value);
        if (array != NULL && array->type == AST_IDENTIFIER && index != NULL &&
            (index->type == AST_NUMBER || index->type == AST_IDENTIFIER) &&
            ast_node_stable_id(array) != 0 && ast_node_stable_id(index) != 0 &&
            semantic_param_publication_binding(state->store->ctx, array) != 0 &&
            (index->type != AST_IDENTIFIER ||
                semantic_param_publication_binding(state->store->ctx, index) != 0)) {
            SemanticPublishedValue receiver = semantic_param_publication_value(state->store->ctx, array);
            SemanticPublishedValue offset = semantic_param_publication_value(state->store->ctx, index);
            const Type *element = receiver.ok && !receiver.meta &&
                type_is_constructed_named(receiver.type, "Array") ? type_get_constructed_arg(receiver.type, 0) : NULL;
            if (element != NULL && publication_physical_scalar(element) &&
                type_equals(element, published.type) && offset.ok && !offset.meta &&
                type_equals(offset.type, TYPE_INT) &&
                semantic_param_publication_value_is_physical_scalar(state->store->ctx, index)) {
                /* The element is a value, not an alias to the Array descriptor. */
                if (!function_param_flow_claim_record(state->store->ctx, state->fact->claims, value) ||
                    !function_param_flow_claim_record(state->store->ctx, state->fact->claims, array) ||
                    !function_param_flow_claim_record(state->store->ctx, state->fact->claims, index))
                    state->fact->complete = false;
                return;
            }
        }
    }
    if (published.meta || (!state->checking_declared_result && published.origin_ready && value->type != AST_IDENTIFIER &&
        value->type != AST_MEMBER_ACCESS && value->type != AST_ARRAY_ACCESS)) return;
    if (semantic_param_publication_checked_local_owner_constructor(state->store->ctx, value)) return;
    if (value->type == AST_CALL && semantic_typed_map_query_result_independent(state->store->ctx, value)) return;
    switch (value->type) {
    case AST_STRING: case AST_NUMBER: case AST_BOOLEAN:
        if (!state->checking_declared_result) state->fact->complete = false;
        break;
    case AST_IDENTIFIER: {
        FunctionParamFlowBackingBinding *source = backing_binding(state, semantic_param_publication_binding(state->store->ctx, value), false);
        if (source == NULL) { state->fact->complete = false; return; }
        source->destination_read_epoch = state->fact->epoch;
        backing_merge(state->fact->destination_words, destinations, source->destinations); break;
    }
    case AST_MEMBER_ACCESS:
        if (!semantic_param_publication_checked_unit_enum_selection(state->store->ctx, value))
            backing_destination_value(state, ast_member_object(value), destinations);
        break;
    case AST_ARRAY_ACCESS: backing_destination_value(state, ast_array_access_array(value), destinations); break;
    case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
        for (size_t p = 0; p < (value->type == AST_ARRAY_LITERAL ? ast_array_literal_count(value) : ast_tuple_literal_count(value)); p++)
            backing_destination_value(state, value->type == AST_ARRAY_LITERAL ? ast_array_literal_element(value, p) : ast_tuple_literal_element(value, p), destinations);
        break;
    case AST_CALL:
        if (!semantic_param_publication_checked_call(state->store->ctx, value)) { state->fact->complete = false; break; }
        if (semantic_param_publication_slice_view_receiver(state->store->ctx, value) != NULL) {
            backing_destination_value(state, semantic_param_publication_slice_view_receiver(state->store->ctx, value), destinations);
            break;
        }
        if (function_param_flow_backing_unsupported_call(state->store, state->fact, value)) break;
        if (state->checking_declared_result &&
            semantic_param_publication_direct_callee(state->store->ctx, value) != NULL &&
            !semantic_param_publication_nominal_result_complete(state->store->ctx, value)) {
            size_t count = ast_call_arg_count(value);
            unsigned char *kinds = count == 0 ? NULL : calloc(count, sizeof(*kinds));
            if (count != 0 && kinds == NULL) { state->fact->complete = false; break; }
            for (size_t p = 0; p < count && state->fact->complete; p++) {
                ASTNode *arg = ast_call_argument(value, p);
                if (!semantic_param_publication_checked_argument(state->store->ctx, value, p) ||
                    !semantic_param_publication_value(state->store->ctx, arg).ok) { state->fact->complete = false; break; }
                if (semantic_param_publication_value_is_physical_scalar(state->store->ctx, arg)) {
                    kinds[p] = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_PHYSICAL;
                    continue;
                }
                unsigned mask = function_param_flow_summary_for_call(&state->lookup, value, p);
                if (!function_param_flow_callee_relation_ready(&state->lookup, value, p)) {
                    state->fact->complete = false; break;
                }
                kinds[p] = (mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 ?
                    FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT : FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_INDEPENDENT;
            }
            if (state->fact->complete && !function_param_flow_declared_result_closed(state->store, value, state->fact))
                state->fact->complete = false;
            if (state->fact->complete) {
                FunctionParamFlowBackingNode *edge = &state->fact->nodes[at];
                if (edge->result_argument_kinds == NULL && edge->result_argument_count == 0) {
                    edge->result_argument_kinds = kinds; edge->result_argument_count = count; kinds = NULL;
                } else if (edge->result_argument_count != count || (count != 0 &&
                    memcmp(edge->result_argument_kinds, kinds, count) != 0)) state->fact->complete = false;
                for (size_t p = 0; p < count && state->fact->complete; p++) if (
                    state->fact->nodes[at].result_argument_kinds[p] == FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT)
                    backing_destination_value(state, ast_call_argument(value, p), destinations);
            }
            free(kinds);
            break;
        }
        if (ast_call_arg_count(value) == 0) {
            /* None() holds nothing a destination could keep. */
            if (!semantic_param_publication_checked_none(state->store->ctx, value) &&
                !semantic_param_publication_nominal_result_complete(state->store->ctx, value) &&
                !function_param_flow_declared_result_closed(state->store, value, state->fact)) state->fact->complete = false;
            break;
        }
        ASTNode *body_witness = NULL;
        size_t body_witness_ordinal = SIZE_MAX;
        for (size_t p = 0; p < ast_call_arg_count(value); p++) {
            if (!semantic_param_publication_checked_argument(state->store->ctx, value, p)) { state->fact->complete = false; break; }
            ASTNode *arg = ast_call_argument(value, p);
            ASTNode *callee = semantic_param_publication_callee(state->store->ctx, value, p);
            if (body_witness != NULL && callee == body_witness &&
                semantic_param_publication_value_is_physical_scalar(state->store->ctx, arg)) {
                /* The earlier required frame owns callee-body validation. This
                 * actual has no descriptor destination; C still visits it. */
                if (!function_param_flow_claim_record(state->store->ctx, state->fact->claims, arg))
                    state->fact->complete = false;
                if (!state->fact->complete) break;
                uint32_t call_id = ast_node_stable_id(value), callee_id = ast_node_stable_id(callee);
                if (body_witness_ordinal >= p || call_id == 0 || callee_id == 0 ||
                    !semantic_param_publication_checked_argument(state->store->ctx, value, body_witness_ordinal)) {
                    state->fact->complete = false; break;
                }
                size_t e = 0;
                for (; e < state->fact->pending_relation_count; e++) {
                    FunctionParamFlowPendingRelation *edge = &state->fact->pending_relations[e];
                    if (edge->call == value && edge->call_id == call_id && edge->callee_id == callee_id &&
                        edge->ordinal == body_witness_ordinal) break;
                }
                if (e == state->fact->pending_relation_count) {
                    if (e == SIZE_MAX / sizeof(*state->fact->pending_relations)) {
                        function_param_flow_fail(state->store, value, "backing witness relation size overflow");
                        state->fact->complete = false; break;
                    }
                    FunctionParamFlowPendingRelation *rows = realloc(state->fact->pending_relations,
                        (e + 1) * sizeof(*rows));
                    if (rows == NULL) {
                        function_param_flow_fail(state->store, value, "backing witness relation allocation failed");
                        state->fact->complete = false; break;
                    }
                    state->fact->pending_relations = rows;
                    rows[state->fact->pending_relation_count++] =
                        (FunctionParamFlowPendingRelation){value, call_id, callee_id, body_witness_ordinal};
                }
                if (!state->fact->pending)
                    state->fact->sealed_pending_relation_count = state->fact->pending_relation_count;
                continue;
            }
            if (callee == NULL && type_equals(published.type, TYPE_STRING) &&
                semantic_param_publication_value_is_physical_scalar(state->store->ctx, arg)) {
                /* A physical scalar actual of a String builtin has no
                 * destination. A builtin that births an allocator or a
                 * builder from scalars stays unmodelled: its storage
                 * lifetime has no relation here. */
                if (!function_param_flow_claim_record(state->store->ctx, state->fact->claims, arg)) {
                    state->fact->complete = false; break;
                }
                continue;
            }
            bool depends = false, retains = true;
            bool ready = false, pending_edge = false;
            if (callee != NULL) {
                unsigned mask = function_param_flow_summary_for_call(&state->lookup, value, p);
                ready = function_param_flow_callee_relation_ready(&state->lookup, value, p);
                if (!ready)
                    function_param_flow_callee_unfinished(&state->lookup, value, p, state->fact);
                if (!ready && state->fact->complete && state->fact->pending &&
                    !state->fact->analysis_unsupported) {
                    uint32_t call_id = ast_node_stable_id(value), callee_id = ast_node_stable_id(callee);
                    for (size_t e = 0; e < state->fact->pending_relation_count; e++) {
                        const FunctionParamFlowPendingRelation *edge = &state->fact->pending_relations[e];
                        if (edge->call == value && edge->call_id == call_id &&
                            edge->callee_id == callee_id && edge->ordinal == p) pending_edge = true;
                    }
                }
                depends = (mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0;
            } else if (!semantic_param_publication_checked_backing_relation(state->store->ctx, value, p, &depends, &retains)) {
                /* An argument borrowed only for the call is kept by nothing
                 * after it; with no relation for the result, the result is
                 * taken to depend on it, as the origin walk does. */
                if (!semantic_param_publication_argument_borrowed_for_call(state->store->ctx, value, p)) {
                    state->fact->complete = false; break;
                }
                depends = true;
            }
            if (depends) backing_destination_value(state, arg, destinations);
            if (state->fact->complete && callee != NULL && (ready || pending_edge)) {
                SemanticPublishedValue actual = semantic_param_publication_value(state->store->ctx, arg);
                bool required = false;
                size_t formal = 0;
                if (actual.ok && !publication_physical_scalar(actual.type) &&
                    semantic_param_publication_formal_for_actual(state->store->ctx, value, p, &formal) &&
                    semantic_param_publication_return_backing_required(state->store->ctx, callee, formal, &required) &&
                    required) { body_witness = callee; body_witness_ordinal = p; }
            }
        }
        break;
    default: state->fact->complete = false; break;
    }
}
void backing_destination_alias(ReturnedBacking *state, ASTNode *value, uint32_t id)
{
    if (value == NULL || (value->type != AST_IDENTIFIER && value->type != AST_MEMBER_ACCESS &&
        value->type != AST_ARRAY_ACCESS && value->type != AST_CALL)) return;
    uint64_t *destinations = calloc(state->fact->destination_words, sizeof(*destinations));
    if (destinations == NULL) { function_param_flow_fail(state->store, state->decl, "backing destination allocation failed"); state->fact->complete = false; return; }
    backing_destination_value(state, value, destinations);
    FunctionParamFlowBackingBinding *destination = backing_binding(state, id, true);
    if (destination != NULL) {
        bool grew = backing_merge(state->fact->destination_words, destination->destinations, destinations);
        state->fact->changed |= grew && destination->destination_read_epoch == state->fact->epoch;
    }
    free(destinations);
}
