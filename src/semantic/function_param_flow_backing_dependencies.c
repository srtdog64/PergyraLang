#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "array_storage_release_owner.h"
#include "formal_string_store_landing_owner.h"
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
        && (row->receiver_summarized || row->receiver != receiver || row->receiver_id != id || row->receiver_type != value.type
            || row->receiver_callee_id != ast_node_stable_id(decl) || row->receiver_formal_id != formal))) return false;
    row->receiver = receiver; row->receiver_id = id; row->receiver_type = value.type;
    row->receiver_callee_id = ast_node_stable_id(decl); row->receiver_formal_id = formal;
    return true;
}

/* The method's summary for its receiver formal, once that summary is
 * complete: its entry, or SIZE_MAX. */
size_t function_param_flow_receiver_formal_summary(ReturnedBacking *state, ASTNode *decl)
{
    FunctionParamFlowSummaryStore *store = state->store;
    if (decl == NULL) return SIZE_MAX;
    if (!store->validating_current) (void)function_param_flow_summary_demand(&state->lookup, decl, 0);
    size_t index = function_param_flow_find(store, ast_node_stable_id(decl), 0);
    if (store->failed || index == SIZE_MAX) return SIZE_MAX;
    const FunctionParamFlowSummaryEntry *entry = &store->entries[index];
    if (entry->function_decl != decl || entry->analysis_unsupported ||
        (entry->state != FUNCTION_PARAM_FLOW_COMPLETE &&
         !(store->solving && entry->state == FUNCTION_PARAM_FLOW_EVALUATED)) ||
        entry->returned_backing_unproved || entry->returned_backing_pending ||
        !entry->output_dependencies_ready || entry->output_dependencies == NULL ||
        entry->output_count != ast_func_param_count(decl)) return SIZE_MAX;
    return index;
}

/* A receiver the method may change or keep follows the method's summary for
 * its receiver formal. The edge is recorded on the call's row so the currency
 * check revalidates it. Returns that summary's entry, or SIZE_MAX while it is
 * not complete; an unsupported summary makes the frame unsupported. */
size_t backing_receiver_summary(ReturnedBacking *state, ASTNode *call, ASTNode *receiver)
{
    ASTNode *decl = semantic_param_publication_direct_callee(state->store->ctx, call);
    SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, receiver);
    if (decl == NULL || !value.ok || value.meta) return SIZE_MAX;
    size_t index = function_param_flow_receiver_formal_summary(state, decl);
    if (index == SIZE_MAX) { (void)function_param_flow_receiver_unsupported(state, call, decl); return SIZE_MAX; }
    size_t at = backing_node(state, call);
    if (at == SIZE_MAX) return SIZE_MAX;
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    uint32_t id = ast_node_stable_id(receiver), formal = ast_func_param_stable_id(ast_func_param(decl, 0));
    if (id == 0 || formal == 0 || (row->receiver != NULL
        && (!row->receiver_summarized || row->receiver != receiver || row->receiver_id != id ||
            row->receiver_type != value.type || row->receiver_callee_id != ast_node_stable_id(decl) ||
            row->receiver_formal_id != formal))) return SIZE_MAX;
    row->receiver = receiver; row->receiver_id = id; row->receiver_type = value.type;
    row->receiver_callee_id = ast_node_stable_id(decl); row->receiver_formal_id = formal;
    row->receiver_summarized = true;
    return index;
}

/* The frame row of the binding an identifier names. A host field is a
 * field of the routine's `self`, so its storage is the self parameter's. */
FunctionParamFlowBackingBinding *backing_identifier_row(ReturnedBacking *state, ASTNode *value)
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

/* An enum value whose payloads hold no storage, such as a unit variant,
 * carries no backing, whether it is a variable or a variant constant. */
bool backing_storage_free_enum(ReturnedBacking *state, SemanticPublishedValue value)
{
    return value.ok && !value.meta && value.type != NULL && value.type->kind == TYPE_KIND_ENUM &&
        semantic_classify_ownership_type(value.type, state->store->ctx) == OWNERSHIP_TYPE_COPY_ONLY;
}

/* A bare unit variant is bound to its enum's declaration (a payload variant
 * is a constructor), and holds no storage whatever the other variants do. */
static bool backing_unit_variant(ReturnedBacking *state, SemanticPublishedValue value, uint32_t binding)
{
    ASTNode *decl = value.ok && !value.meta && value.type != NULL && value.type->kind == TYPE_KIND_ENUM
        ? semantic_find_enum_decl_by_name(state->store->ctx, value.type->name) : NULL;
    return decl != NULL && decl->type == AST_ENUM_DECL && binding != 0 && ast_node_stable_id(decl) == binding;
}
void backing_store(ReturnedBacking *state, ASTNode *target, const uint64_t *origins, bool replacement,
    ASTNode *carrier)
{
    /* A stored value that carries none of this frame's origins moves none
     * of its backing, wherever it is stored. */
    if (!backing_any_origin(state, origins)) return;
    FunctionParamFlowBackingBinding *row = backing_binding(state,
        function_param_flow_backing_place(state, target), false);
    if (row == NULL) { backing_effect_carried(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier); state->fact->complete = false; return; }
    if (!replacement) row->destination_read_epoch = state->fact->epoch;
    bool output = false;
    for (size_t w = 0; !replacement && w < state->fact->destination_words; w++) output |= row->destinations[w] != 0;
    for (size_t p = 0; !replacement && p < state->fact->origin_count; p++) if ((origins[p / 64] & (UINT64_C(1) << (p % 64))) != 0)
        backing_merge(state->fact->destination_words, &state->fact->outputs[p * state->fact->destination_words], row->destinations);
    if (output) backing_effect_carried(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier);
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
    else if (!output) { backing_effect_carried(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier); state->fact->complete = false; }
}
const uint64_t *backing_origin(ReturnedBacking *state, ASTNode *node)
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
        if (source == NULL && (backing_storage_free_enum(state, value) ||
                backing_unit_variant(state, value, binding))) return &zero;
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
    const uint64_t *receiver_return = NULL;
    if (node->type == AST_CALL) {
        ASTNode *receiver = NULL;
        ASTNode *direct = semantic_param_publication_direct_callee(state->store->ctx, node);
        /* An intent's value is its Bool verdict; the consumer walk applies
         * its effect on the arguments. */
        if (direct == NULL && semantic_param_publication_intent_callee(state->store->ctx, node) != NULL) {
            if (semantic_param_publication_value_is_physical_scalar(state->store->ctx, node)) return origins;
            fact->complete = false; return NULL;
        }
        if ((direct == NULL && ast_call_semantic_callee_declared_callable(node) &&
             !semantic_param_publication_bodyless_selected_call_current(state->store->ctx, node)) ||
            (direct != NULL && !semantic_param_publication_instance_receiver(state->store->ctx, node, &receiver))) {
            fact->complete = false; return NULL;
        }
        if (receiver != NULL) {
            const uint64_t *bits = backing_origin(state, receiver);
            if (bits == NULL) { fact->complete = false; return NULL; }
            if (backing_any_origin(state, bits) && !function_param_flow_receiver_independent(state, node)) {
                size_t entry = backing_receiver_summary(state, node, receiver);
                if (entry == SIZE_MAX) { fact->complete = false; return NULL; }
                if ((state->store->entries[entry].mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) receiver_return = bits;
            }
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
        if (receiver_return != NULL) backing_merge(state->fact->origin_words, origins, receiver_return);
        ASTNode *viewed = semantic_param_publication_slice_view_receiver(state->store->ctx, node);
        if (viewed != NULL) {
            /* A Slice view is the receiver's storage. */
            const uint64_t *bits = backing_origin(state, viewed);
            if (bits == NULL) { fact->complete = false; break; }
            backing_merge(state->fact->origin_words, origins, bits);
            break;
        }
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
/* A match binding names a part of the matched value, so it is a local that
 * carries the value's storage. A unit variant or literal pattern binds
 * nothing; any other pattern shape is not followed. */
static void backing_match_bindings(ReturnedBacking *state, ASTNode *pattern, ASTNode *subject, const uint64_t *bits)
{
    size_t count = 0;
    if (pattern == NULL) return;
    if (pattern->type == AST_CALL) count = ast_call_arg_count(pattern);
    else if (pattern->type == AST_IDENTIFIER) count = 1;
    else if (pattern->type != AST_MEMBER_ACCESS && pattern->type != AST_NUMBER &&
             pattern->type != AST_STRING && pattern->type != AST_BOOLEAN) { state->fact->complete = false; return; }
    for (size_t a = 0; a < count; a++) {
        ASTNode *binding = pattern->type == AST_CALL ? ast_call_argument(pattern, a) : pattern;
        uint32_t id = binding != NULL && binding->type == AST_IDENTIFIER ? ast_node_stable_id(binding) : 0;
        if (pattern->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(binding) != id) return;
        if (id == 0 || ast_identifier_binding_syntax_id(binding) != id) { state->fact->complete = false; return; }
        FunctionParamFlowBackingBinding *row = backing_binding(state, id, true);
        if (row == NULL) { state->fact->complete = false; return; }
        row->local = true;
        backing_destination_alias(state, subject, id);
        row = backing_binding(state, id, false);
        if (row != NULL && bits != NULL) {
            bool grew = backing_merge(state->fact->origin_words, row->origins, bits);
            state->fact->changed |= grew && row->origin_read_epoch == state->fact->epoch;
        }
    }
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
        if (target != NULL && target->type == AST_IDENTIFIER) backing_destination_alias(state, ast_assignment_value(node), semantic_param_publication_binding(state->store->ctx, target));
        const uint64_t *bits = backing_origin(state, ast_assignment_value(node));
        FunctionParamFlowProgramPointIndex *index; bool indexed = function_param_flow_program_point_index_build(state->store, state->decl, &index);
        bool replacement = indexed && function_param_flow_local_replacement(state->store->ctx, index, node);
        bool carried = bits != NULL && state->loan_seed == NULL && (bits[0] & UINT64_C(1)) != 0;
        if (!indexed || (carried && !semantic_formal_store_landing_select_assigned(state->store->ctx, state->decl,
            state->fact->input_id, target, ast_assignment_value(node)))) fact->complete = false;
        if (bits != NULL) backing_store(state, target, bits, replacement, ast_assignment_value(node));
        backing_consumers(state, ast_assignment_value(node)); break;
    }
    case AST_CALL:
        ASTNode *direct = semantic_param_publication_direct_callee(state->store->ctx, node);
        if (direct == NULL && function_param_flow_intent_call(state, node)) break;
        if (direct == NULL && semantic_param_publication_slice_view_receiver(state->store->ctx, node) != NULL) {
            backing_consumers(state, semantic_param_publication_slice_view_receiver(state->store->ctx, node));
            for (size_t p = 0; p < ast_call_arg_count(node); p++) backing_consumers(state, ast_call_argument(node, p));
            break;
        }
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
                if (bits == NULL) fact->complete = false;
                else if (backing_any_origin(state, bits) && !function_param_flow_receiver_independent(state, node)) {
                    /* The receiver's storage escapes or moves into another
                     * argument as the receiver formal's summary says. */
                    size_t entry = backing_receiver_summary(state, node, receiver);
                    FuncParam *self = ast_func_param(direct, 0);
                    if (entry == SIZE_MAX || self == NULL) { fact->complete = false; break; }
                    unsigned mask = state->store->entries[entry].mask;
                    backing_effect(state, bits, mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE));
                    if (self->mode == PARAM_MODE_OWN || self->mode == PARAM_MODE_MUT_REF)
                        backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                    for (size_t p = 0; p < ast_call_arg_count(node) && fact->complete; p++) {
                        size_t formal = 0;
                        if (!semantic_param_publication_formal_for_actual(state->store->ctx, node, p, &formal) ||
                            formal >= state->store->entries[entry].output_count) { fact->complete = false; break; }
                        FuncParam *output = ast_func_param(direct, formal);
                        if (output == NULL) { fact->complete = false; break; }
                        if ((output->mode == PARAM_MODE_REF || output->mode == PARAM_MODE_MUT_REF) &&
                            state->store->entries[entry].output_dependencies[formal])
                            backing_store(state, ast_call_argument(node, p), bits, false, NULL);
                    }
                }
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
            if (bits != NULL) backing_store(state, ast_call_argument(node, 0), bits, false, NULL);
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                backing_consumers(state, ast_call_argument(node, p));
            break;
        }
        size_t stored = 0;
        bool array_store = semantic_param_publication_checked_array_store(state->store->ctx, node, &stored);
        ASTNode *held_root = array_store ? ast_call_argument(node, 0) : NULL;
        while (held_root != NULL && (held_root->type == AST_MEMBER_ACCESS || held_root->type == AST_ARRAY_ACCESS))
            held_root = held_root->type == AST_MEMBER_ACCESS ? ast_member_object(held_root) : ast_array_access_array(held_root);
        uint32_t held_id = held_root == NULL ? 0 : semantic_param_publication_binding(state->store->ctx, held_root);
        FunctionParamFlowBackingBinding *holder = held_id == 0 ? NULL : backing_binding(state, held_id, false);
        bool held = holder != NULL && holder->local, held_output = false;
        for (size_t p = 0; holder != NULL && !held && p < ast_func_param_count(state->decl); p++) {
            FuncParam *param = ast_func_param(state->decl, p);
            held = param != NULL && ast_func_param_stable_id(param) == holder->binding && param->mode != PARAM_MODE_REF;
            held_output = held && param->mode == PARAM_MODE_MUT_REF;
        }
        if (held) {
            /* The pushed or set value lives in the Array afterwards, so its
             * backing goes where that Array goes. The Array is a local or a
             * parameter of this call; an inout one is an output, which the
             * store reports as a call escape. */
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                if (!semantic_param_publication_checked_argument(state->store->ctx, node, p)) fact->complete = false;
            const uint64_t *bits = fact->complete ? backing_origin(state, ast_call_argument(node, stored)) : NULL;
            if (bits == NULL) { fact->complete = false; break; }
            /* The formal's backing an inout output keeps leaves the routine:
             * the next analysis stores a copy instead
             * (2026-10-20_formal_storage_kept_by_output_is_copied). */
            if (held_output && (bits[0] & UINT64_C(1)) != 0)
                semantic_formal_store_landing_select_held(state->store->ctx, node, stored);
            backing_store(state, ast_call_argument(node, 0), bits, false, ast_call_argument(node, stored));
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
                semantic_typed_map_query_receiver_read(state->store->ctx, node, i) ||
                semantic_param_publication_checked_array_pop(state->store->ctx, node);
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
                    /* A copy can stand in for an argument a by-value formal receives;
                     * inout is the caller's place, own and ref take a named variable. */
                    ASTNode *carrier = param != NULL && param->mode == PARAM_MODE_DEFAULT ? arg : NULL;
                    backing_effect_carried(state, bits, mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE), carrier);
                    /* A loan handed to a formal whose storage proof fails changes the
                     * loan's owner: the move owner sees only access paths. */
                    if (param == NULL || param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_MUT_REF ||
                        (state->loan_seed != NULL && !function_param_flow_preserves_array_storage(state->store->ctx, callee, formal)))
                        backing_effect_carried(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE, carrier);
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
                        else if (retained) backing_store(state, ast_call_argument(node, p), bits, false, NULL);
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
        if (bits != NULL) backing_effect_carried(state, bits, SLOT_PARAM_SUMMARY_RETURN_ESCAPE, ast_return_value(node));
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
    case AST_MATCH_STMT: {
        ASTNode *subject = ast_match_subject(node);
        backing_consumers(state, subject);
        const uint64_t *bits = backing_origin(state, subject);
        if (bits == NULL) { fact->complete = false; break; }
        for (size_t c = 0; c < ast_match_case_count(node) && fact->complete; c++) {
            ASTNode *arm = ast_match_case_at(node, c);
            if (arm == NULL || arm->type != AST_MATCH_CASE) { fact->complete = false; break; }
            for (size_t p = 0; p < ast_match_case_pattern_count(arm); p++)
                backing_match_bindings(state, ast_match_case_pattern_at(arm, p), subject, bits);
            backing_consumers(state, ast_match_case_guard(arm));
            backing_consumers(state, ast_match_case_body(arm));
        }
        backing_consumers(state, ast_match_default_body(node));
        break;
    }
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
