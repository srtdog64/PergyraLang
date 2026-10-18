#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "array_storage_release_owner.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool function_param_flow_receiver_independent(ReturnedBacking *state, ASTNode *call)
{
    ASTNode *decl = function_param_flow_receiver_readonly_current(state->store, call, true), *receiver = NULL;
    if (decl == NULL || !semantic_param_publication_instance_receiver(state->store->ctx, call, &receiver)) return false;
    SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, receiver);
    size_t at = backing_node(state, call);
    if (at == SIZE_MAX || !value.ok || value.meta) return false;
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    uint32_t id = ast_node_stable_id(receiver), formal = ast_func_param_stable_id(ast_func_param(decl, 0));
    if (id == 0 || formal == 0 || (row->receiver != NULL
        && (row->receiver != receiver || row->receiver_id != id || row->receiver_type != value.type
            || row->receiver_callee_id != ast_node_stable_id(decl) || row->receiver_formal_id != formal))) return false;
    row->receiver = receiver; row->receiver_id = id; row->receiver_type = value.type;
    row->receiver_callee_id = ast_node_stable_id(decl); row->receiver_formal_id = formal;
    return true;
}

/* The frame row of the binding an identifier names. A host field is a
 * field of the routine's `self`, so its storage is the self parameter's. */
static FunctionParamFlowBackingBinding *backing_identifier_row(ReturnedBacking *state, ASTNode *value)
{
    FunctionParamFlowBackingBinding *row = backing_binding(state,
        semantic_param_publication_binding(state->store->ctx, value), false);
    if (row != NULL || !ast_identifier_binding_is_host_field(value)) return row;
    for (size_t p = 0; p < ast_func_param_count(state->decl); p++) {
        FuncParam *param = ast_func_param(state->decl, p);
        if (param != NULL && param->type == NULL && param->name != NULL && strcmp(param->name, "self") == 0)
            return backing_binding(state, ast_func_param_stable_id(param), false);
    }
    return NULL;
}
static void backing_store(ReturnedBacking *state, ASTNode *target, const uint64_t *origins, bool replacement)
{
    /* A stored value that carries none of this frame's origins moves none
     * of its backing, wherever it is stored. */
    if (!backing_any_origin(state, origins)) return;
    FunctionParamFlowBackingBinding *row = backing_binding(state,
        function_param_flow_backing_place(state, target), false);
    if (row == NULL) { backing_effect(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE); state->fact->complete = false; return; }
    if (!replacement) row->destination_read_epoch = state->fact->epoch;
    bool output = false;
    for (size_t w = 0; !replacement && w < state->fact->destination_words; w++) output |= row->destinations[w] != 0;
    for (size_t p = 0; !replacement && p < state->fact->origin_count; p++) if ((origins[p / 64] & (UINT64_C(1) << (p % 64))) != 0)
        backing_merge(state->fact->destination_words, &state->fact->outputs[p * state->fact->destination_words], row->destinations);
    if (output) backing_effect(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
    /* A by-value or own parameter is this call's storage, as a local is: a
     * write through it (an inout argument) keeps the backing in the frame,
     * and a later return or escape of the parameter carries it. A declared
     * result frame keeps its parameters free of origins. */
    bool owned_parameter = false;
    for (size_t p = 0; !row->local && !state->checking_declared_result &&
         p < ast_func_param_count(state->decl); p++) {
        FuncParam *param = ast_func_param(state->decl, p);
        if (param != NULL && ast_func_param_stable_id(param) == row->binding &&
            (param->mode == PARAM_MODE_DEFAULT || param->mode == PARAM_MODE_OWN)) owned_parameter = true;
    }
    if (row->local || replacement || owned_parameter) {
        bool grew = backing_merge(state->fact->origin_words, row->origins, origins);
        state->fact->changed |= grew && row->origin_read_epoch == state->fact->epoch;
    }
    else if (!output) { backing_effect(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE); state->fact->complete = false; }
}
static void backing_destination_value(ReturnedBacking *state, ASTNode *value, uint64_t *destinations)
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
            free(received);
            if (dependent && !function_param_flow_receiver_independent(state, value)) state->fact->complete = false;
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
            if (!semantic_param_publication_nominal_result_complete(state->store->ctx, value) &&
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
                state->fact->complete = false; break;
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
static void backing_destination_alias(ReturnedBacking *state, ASTNode *value, uint32_t id)
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
static const uint64_t *backing_origin(ReturnedBacking *state, ASTNode *node)
{
    if (node == NULL || state->fact->analysis_unsupported) return NULL;
    if (state->checking_declared_result) {
        uint64_t destinations = 0;
        backing_destination_value(state, node, &destinations);
        if (!state->fact->complete || destinations != 0) { state->fact->complete = false; return NULL; }
    }
    if (node != state->loan_seed &&
        (node->type == AST_STRING || node->type == AST_NUMBER || node->type == AST_BOOLEAN)) {
        /* Pure leaves have no input backing; shared claims own their currency. */
        static const uint64_t zero = 0;
        if (state->fact->origin_count != 1 || state->fact->origin_words != 1 ||
            ast_node_stable_id(node) == 0 ||
            !function_param_flow_claim_record(state->store->ctx, state->fact->claims, node)) {
            state->fact->complete = false; return NULL;
        }
        return &zero;
    }
    if (node != state->loan_seed && node->type == AST_IDENTIFIER) {
        /* A leaf needs the current claim and binding, but not a second
         * per-frame expression row. Return the read's one-bit snapshot:
         * later binding growth must not mutate an earlier expression value. */
        static const uint64_t zero = 0, one = 1;
        FunctionParamFlowBackingFact *fact = state->fact;
        SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, node);
        uint32_t binding = semantic_param_publication_binding(state->store->ctx, node);
        if (fact->origin_count != 1 || fact->origin_words != 1 ||
            ast_node_stable_id(node) == 0 ||
            !function_param_flow_claim_record(state->store->ctx, fact->claims, node)) {
            fact->complete = false; return NULL;
        }
        if (semantic_param_publication_slot_token_operand(state->store->ctx, node) ||
            (value.ok && (value.meta || value.origin_ready))) return &zero;
        if (!value.ok || binding == 0) { fact->complete = false; return NULL; }
        FunctionParamFlowBackingBinding *source = backing_identifier_row(state, node);
        if (source == NULL) { fact->complete = false; return NULL; }
        source->origin_read_epoch = fact->epoch;
        uint64_t bits = source->origins[0];
        if ((bits & ~UINT64_C(1)) != 0) { fact->complete = false; return NULL; }
        return bits == 0 ? &zero : &one;
    }
    size_t at = backing_node(state, node);
    if (at == SIZE_MAX || !backing_work(state, at)) return NULL;
    FunctionParamFlowBackingFact *fact = state->fact;
    uint64_t *origins = fact->nodes[at].origins;
    if (fact->nodes[at].origin_epoch == fact->epoch) return origins;
    fact->nodes[at].origin_epoch = fact->epoch;
    if (node == state->loan_seed) { state->loan_seed_seen = true; backing_merge(state->fact->origin_words, origins, &(uint64_t){1}); return origins; }
    if (node->type == AST_CAST) {
        if (!semantic_param_publication_checked_numeric_cast(state->store->ctx, node)) { fact->complete = false; return NULL; }
        (void)backing_origin(state, ast_cast_operand(node));
        return origins;
    }
    if (node->type == AST_CALL) {
        ASTNode *receiver = NULL;
        ASTNode *direct = semantic_param_publication_direct_callee(state->store->ctx, node);
        if ((direct == NULL && ast_call_semantic_callee_declared_callable(node) &&
             !semantic_param_publication_bodyless_selected_call_current(state->store->ctx, node)) ||
            (direct != NULL && !semantic_param_publication_instance_receiver(state->store->ctx, node, &receiver))) {
            fact->complete = false; return NULL;
        }
        if (receiver != NULL) {
            const uint64_t *bits = backing_origin(state, receiver);
            if (bits == NULL || (backing_any_origin(state, bits)
                && !function_param_flow_receiver_independent(state, node))) { fact->complete = false; return NULL; }
        }
    }
    SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, node);
    if (value.ok && (value.meta || value.origin_ready)) return origins;
    if (semantic_param_publication_checked_local_owner_constructor(state->store->ctx, node)) return origins;
    if (node->type == AST_CALL && semantic_typed_map_query_result_independent(state->store->ctx, node)) return origins;
    switch (node->type) {
    case AST_MEMBER_ACCESS:
        if (!semantic_param_publication_checked_unit_enum_selection(state->store->ctx, node)) {
            const uint64_t *bits = backing_origin(state, ast_member_object(node));
            if (bits != NULL) backing_merge(state->fact->origin_words, origins, bits);
        }
        break;
    case AST_ARRAY_ACCESS: { const uint64_t *bits = backing_origin(state, ast_array_access_array(node)); if (bits != NULL) backing_merge(state->fact->origin_words, origins, bits); break; }
    case AST_STRING: case AST_NUMBER: case AST_BOOLEAN: break;
    case AST_CALL:
        if (function_param_flow_backing_unsupported_call(state->store, fact, node)) break;
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            if (!semantic_param_publication_checked_argument(state->store->ctx, node, i)) { fact->complete = false; break; }
            const uint64_t *bits = backing_origin(state, ast_call_argument(node, i));
            if (bits == NULL || !backing_any_origin(state, bits)) continue;
            ASTNode *callee = semantic_param_publication_callee(state->store->ctx, node, i);
            if (callee == NULL) {
                if (function_param_flow_backing_unsupported_call(state->store, fact, node)) break;
                bool depends = true, retains = true;
                bool relation = semantic_param_publication_checked_backing_relation(state->store->ctx, node, i, &depends, &retains);
                if (!relation || depends) backing_merge(state->fact->origin_words, origins, bits);
                if ((!relation || retains) && !semantic_param_publication_argument_borrowed_for_call(state->store->ctx, node, i) &&
                    !semantic_typed_map_query_receiver_read(state->store->ctx, node, i)) fact->complete = false;
            } else {
                unsigned mask = function_param_flow_summary_for_call(&state->lookup, node, i);
                if (!function_param_flow_callee_relation_ready(&state->lookup, node, i))
                    function_param_flow_callee_unfinished(&state->lookup, node, i, fact);
                if ((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) backing_merge(state->fact->origin_words, origins, bits);
            }
        }
        break;
    case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < (node->type == AST_ARRAY_LITERAL ? ast_array_literal_count(node) : ast_tuple_literal_count(node)); i++) {
            const uint64_t *bits = backing_origin(state, node->type == AST_ARRAY_LITERAL ? ast_array_literal_element(node, i) : ast_tuple_literal_element(node, i));
            if (bits != NULL) backing_merge(state->fact->origin_words, origins, bits);
        }
        break;
    default: fact->complete = false; break;
    }
    return origins;
}
void backing_consumers(ReturnedBacking *state, ASTNode *node)
{
    if (node == NULL || state->fact->analysis_unsupported) return;
    bool selector = node->type == AST_MEMBER_ACCESS || node->type == AST_ARRAY_ACCESS;
    bool selector_reachable = true;
    bool explicit_unreachable = state->loan_seed == NULL && selector &&
        semantic_param_publication_execution(state->store->ctx, node, &selector_reachable) && !selector_reachable;
    if (state->loan_seed == NULL && (node->type == AST_BINARY || node->type == AST_UNARY ||
        selector) && !explicit_unreachable) {
        bool empty = false;
        if (state->fact->origin_count != 1 || state->fact->origin_words != 1 ||
            !function_param_flow_consumer_empty_current(state->store->ctx,
                state->fact->claims, node, &empty)) { state->fact->complete = false; return; }
        if (empty) return;
    }
    if (node != state->loan_seed &&
        (node->type == AST_STRING || node->type == AST_NUMBER || node->type == AST_BOOLEAN ||
         node->type == AST_IDENTIFIER)) {
        if (state->fact->origin_count != 1 || state->fact->origin_words != 1 ||
            ast_node_stable_id(node) == 0 ||
            !function_param_flow_claim_record(state->store->ctx, state->fact->claims, node))
            state->fact->complete = false;
        else if (node->type == AST_IDENTIFIER) {
            SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, node);
            if (!value.ok || (!value.meta && semantic_param_publication_binding(state->store->ctx, node) == 0 &&
                !semantic_param_publication_slot_token_operand(state->store->ctx, node)))
                state->fact->complete = false;
        }
        return;
    }
    size_t at = backing_node(state, node);
    if (at == SIZE_MAX || !backing_work(state, at)) return;
    FunctionParamFlowBackingFact *fact = state->fact;
    if (fact->nodes[at].visit_epoch == fact->epoch) return;
    fact->nodes[at].visit_epoch = fact->epoch;
    if (node == state->loan_seed) state->loan_seed_seen = true;
    bool reachable;
    if (semantic_param_publication_execution(state->store->ctx, node, &reachable) && !reachable) return;
    ASTNode *previous_consumer = state->consumer;
    state->consumer = node;
    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++) backing_consumers(state, ast_block_statement(node, i));
        break;
    case AST_DEFER_STMT: backing_consumers(state, ast_defer_body(node)); break;
    case AST_FOR_LOOP:
        backing_consumers(state, ast_for_range_start(node)); backing_consumers(state, ast_for_range_end(node));
        backing_consumers(state, ast_for_iterable(node)); backing_consumers(state, ast_for_body(node)); break;
    case AST_WHILE_LOOP: backing_consumers(state, ast_while_condition(node)); backing_consumers(state, ast_while_body(node)); break;
    case AST_WITH_STMT: backing_consumers(state, ast_with_body(node)); break;
    case AST_BREAK: case AST_CONTINUE: break;
    case AST_LET_DESTRUCTURE: case AST_LET_DECL: {
        SemanticPublishedSlotBirth birth;
        if (semantic_param_publication_owned_slot_binding(state->store->ctx, node, &birth)) {
            FunctionParamFlowBackingBinding *slot = backing_binding(state, birth.slot_binding_id, true);
            if (slot != NULL) slot->local = true;
            if (birth.token_type != NULL && birth.token_binding_id != 0) {
                FunctionParamFlowBackingBinding *token = backing_binding(state, birth.token_binding_id, true);
                if (token != NULL) token->local = true;
            }
            ASTNode *initializer = node->type == AST_LET_DECL ? ast_let_initializer(node) : ast_let_destructure_initializer(node);
            backing_consumers(state, initializer);
            break;
        }
        if (node->type != AST_LET_DECL) { fact->complete = false; break; }
        FunctionParamFlowBackingBinding *local = backing_binding(state, ast_node_stable_id(node), true);
        if (local != NULL) local->local = true;
        if (!semantic_param_publication_channel_binding(state->store->ctx, node) &&
            !semantic_param_publication_empty_map_binding(state->store->ctx, node))
            backing_destination_alias(state, ast_let_initializer(node), ast_node_stable_id(node));
        const uint64_t *bits = backing_origin(state, ast_let_initializer(node));
        local = backing_binding(state, ast_node_stable_id(node), false);
        if (local != NULL && bits != NULL) {
            bool grew = backing_merge(fact->origin_words, local->origins, bits);
            fact->changed |= grew && local->origin_read_epoch == fact->epoch;
        }
        backing_consumers(state, ast_let_initializer(node)); break;
    }
    case AST_ASSIGNMENT: {
        ASTNode *target = ast_assignment_target(node);
        if (target != NULL && target->type == AST_IDENTIFIER)
            backing_destination_alias(state, ast_assignment_value(node), semantic_param_publication_binding(state->store->ctx, target));
        const uint64_t *bits = backing_origin(state, ast_assignment_value(node));
        FunctionParamFlowProgramPointIndex *index;
        bool indexed = function_param_flow_program_point_index_build(state->store, state->decl, &index);
        if (!indexed) fact->complete = false;
        bool replacement = indexed && function_param_flow_local_replacement(state->store->ctx, index, node);
        if (bits != NULL) backing_store(state, target, bits, replacement);
        backing_consumers(state, ast_assignment_value(node)); break;
    }
    case AST_CALL:
        ASTNode *direct = semantic_param_publication_direct_callee(state->store->ctx, node);
        if (direct == NULL && ast_call_semantic_callee_declared_callable(node) &&
            !semantic_param_publication_bodyless_selected_call_current(state->store->ctx, node)) {
            fact->complete = false; break;
        }
        if (direct == NULL && function_param_flow_backing_unsupported_call(state->store, fact, node)) break;
        if (ast_call_semantic_callee_value_binding_id(node) != 0 || ast_call_callee(node) == NULL ||
            (ast_call_callee(node)->type != AST_IDENTIFIER &&
             direct == NULL)) {
            fact->complete = false; break;
        }
        if (direct != NULL) {
            ASTNode *receiver = NULL;
            if (!semantic_param_publication_instance_receiver(state->store->ctx, node, &receiver)) {
                fact->complete = false; break;
            }
            if (receiver != NULL) {
                backing_consumers(state, receiver);
                const uint64_t *bits = backing_origin(state, receiver);
                if (bits == NULL || (backing_any_origin(state, bits)
                    && !function_param_flow_receiver_independent(state, node))) fact->complete = false;
                if (!fact->complete) break;
            }
        }
        bool physical_store = state->private_stores_closed &&
            semantic_param_publication_private_physical_array_mutation(
            state->store->ctx, node, state->decl);
        if (state->private_stores_closed && (physical_store ||
            semantic_param_publication_private_array_push(state->store->ctx, node, state->decl))) {
            /* Physical mutation transfers no backing; String push still does. */
            const uint64_t *bits = physical_store ? NULL : backing_origin(state, ast_call_argument(node, 1));
            if (bits != NULL) backing_store(state, ast_call_argument(node, 0), bits, false);
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                backing_consumers(state, ast_call_argument(node, p));
            break;
        }
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            if (!semantic_param_publication_checked_argument(state->store->ctx, node, i)) { fact->complete = false; break; }
            ASTNode *arg = ast_call_argument(node, i);
            if (i == 1 && semantic_array_storage_independent_physical_hof_result(node, state->store->ctx)) {
                backing_consumers(state, arg); continue;
            }
            const uint64_t *bits = backing_origin(state, arg);
            bool local = semantic_param_publication_argument_borrowed_for_call(state->store->ctx, node, i) ||
                semantic_typed_map_query_receiver_read(state->store->ctx, node, i);
            ASTNode *receiver = ast_call_argument(node, 0);
            uint32_t owner_id = state->loan_seed != NULL ? state->loan_owner_slot_id : state->fact->input_id;
            bool owner_store = i == 1 && owner_id != 0 &&
                semantic_param_publication_checked_slot_write(state->store->ctx, node) &&
                receiver != NULL && receiver->type == AST_IDENTIFIER &&
                semantic_param_publication_binding(state->store->ctx, receiver) == owner_id;
            bool depends = false, retains = true;
            local |= semantic_param_publication_checked_backing_relation(state->store->ctx, node, i, &depends, &retains) && !retains;
            ASTNode *callee = semantic_param_publication_callee(state->store->ctx, node, i);
            size_t formal = 0;
            if (callee != NULL && !semantic_param_publication_formal_for_actual(
                    state->store->ctx, node, i, &formal)) { fact->complete = false; break; }
            bool self = callee == state->decl && !state->decl->is_async_decl && arg->type == AST_IDENTIFIER &&
                ast_func_param(callee, formal)->mode == PARAM_MODE_REF &&
                semantic_param_publication_binding(state->store->ctx, arg) == ast_func_param_stable_id(ast_func_param(callee, formal));
            for (size_t p = 0; self && p < ast_func_param_count(callee); p++) if (ast_func_param(callee, p)->mode == PARAM_MODE_MUT_REF) self = false;
            if (bits != NULL && backing_any_origin(state, bits) && !local && !self && !owner_store) {
                if (callee == NULL) {
                    if (!function_param_flow_backing_unsupported_call(state->store, fact, node))
                        backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                }
                else {
                    FuncParam *param = ast_func_param(callee, formal);
                    unsigned mask = function_param_flow_summary_for_call(&state->lookup, node, i);
                    backing_effect(state, bits, mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE));
                    if (param == NULL || param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_MUT_REF)
                        backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                    if (!function_param_flow_callee_relation_ready(&state->lookup, node, i))
                        function_param_flow_callee_unfinished(&state->lookup, node, i, fact);
                    for (size_t p = 0; p < ast_call_arg_count(node); p++) {
                        size_t output_formal = 0;
                        if (!semantic_param_publication_formal_for_actual(
                                state->store->ctx, node, p, &output_formal)) { fact->complete = false; break; }
                        FuncParam *output = ast_func_param(callee, output_formal);
                        if (output == NULL) { fact->complete = false; break; }
                        if (output->mode != PARAM_MODE_MUT_REF && output->mode != PARAM_MODE_REF) continue;
                        if (output->mode == PARAM_MODE_REF && output->type != NULL && ast_type_name(output->type) != NULL &&
                            strcmp(ast_type_name(output->type), "String") == 0) continue;
                        bool retained = false;
                        if (!function_param_flow_output_dependency_for_call(&state->lookup, node, i, p, &retained))
                            function_param_flow_callee_unfinished(&state->lookup, node, i, fact);
                        else if (retained) backing_store(state, ast_call_argument(node, p), bits, false);
                    }
                }
            }
            backing_consumers(state, arg);
        }
        break;
    case AST_RETURN: {
        if (state->checking_declared_result) {
            state->result_return_seen = true;
            if (ast_return_value(node) == NULL) fact->complete = false;
        }
        const uint64_t *bits = backing_origin(state, ast_return_value(node));
        if (bits != NULL) backing_effect(state, bits, SLOT_PARAM_SUMMARY_RETURN_ESCAPE);
        backing_consumers(state, ast_return_value(node)); break;
    }
    case AST_CHANNEL_SEND: {
        const uint64_t *bits = backing_origin(state, ast_channel_send_value(node));
        if (bits != NULL) backing_effect(state, bits, SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE);
        backing_consumers(state, ast_channel_send_value(node)); break;
    }
    case AST_IF_STMT:
        backing_consumers(state, ast_if_condition(node)); backing_consumers(state, ast_if_then_branch(node));
        backing_consumers(state, ast_if_else_branch(node)); break;
    case AST_BINARY: backing_consumers(state, ast_binary_left(node)); backing_consumers(state, ast_binary_right(node)); break;
    case AST_UNARY: backing_consumers(state, ast_unary_operand(node)); break;
    case AST_CAST:
        if (!semantic_param_publication_checked_numeric_cast(state->store->ctx, node)) fact->complete = false;
        backing_consumers(state, ast_cast_operand(node)); break;
    case AST_MEMBER_ACCESS:
        if (!semantic_param_publication_checked_unit_enum_selection(state->store->ctx, node))
            backing_consumers(state, ast_member_object(node));
        break;
    case AST_ARRAY_ACCESS: backing_consumers(state, ast_array_access_array(node)); backing_consumers(state, ast_array_access_index(node)); break;
    case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < (node->type == AST_ARRAY_LITERAL ? ast_array_literal_count(node) : ast_tuple_literal_count(node)); i++)
            backing_consumers(state, node->type == AST_ARRAY_LITERAL ? ast_array_literal_element(node, i) : ast_tuple_literal_element(node, i));
        break;
    case AST_IDENTIFIER: case AST_NUMBER: case AST_STRING: case AST_BOOLEAN: break;
    default: fact->complete = false; break;
    }
    state->consumer = previous_consumer;
}
