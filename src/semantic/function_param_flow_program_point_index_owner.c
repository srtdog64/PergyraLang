#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "typed_map_query_fact_owner.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
function_param_flow_identifier_matches(const char *candidate, void *userdata)
{
    const char *name = userdata;
    return candidate != NULL && name != NULL && strcmp(candidate, name) == 0;
}

static bool
function_param_flow_has_identifier_ref(const ASTNode *node, const char *name)
{
    return node != NULL && name != NULL
        && ast_contains_identifier_ref(
            node, function_param_flow_identifier_matches, (void *)name);
}

static size_t
function_param_flow_program_point_index_find(
    const FunctionParamFlowSummaryStore *store,
    uint32_t function_id)
{
    if (store == NULL || function_id == 0)
        return SIZE_MAX;
    for (size_t i = 0; i < store->program_point_index_count; i++) {
        if (store->program_point_indexes[i].function_id == function_id)
            return i;
    }
    return SIZE_MAX;
}

static bool
function_param_flow_program_point_index_reserve(
    FunctionParamFlowSummaryStore *store,
    size_t minimum)
{
    FunctionParamFlowProgramPointIndex *grown;
    size_t capacity;

    if (store == NULL)
        return false;
    if (minimum <= store->program_point_index_capacity)
        return true;
    capacity = store->program_point_index_capacity == 0
        ? 8 : store->program_point_index_capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(store->program_point_indexes,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    memset(grown + store->program_point_index_capacity, 0,
           (capacity - store->program_point_index_capacity) * sizeof(*grown));
    store->program_point_indexes = grown;
    store->program_point_index_capacity = capacity;
    return true;
}

static bool
function_param_flow_program_point_append(
    FunctionParamFlowProgramPointIndex *index,
    size_t param_index,
    ASTNode *root)
{
    ASTNode **grown;
    size_t capacity;

    if (index == NULL || root == NULL || param_index >= index->param_count)
        return false;
    if (index->root_counts[param_index]
            == index->root_capacities[param_index]) {
        capacity = index->root_capacities[param_index] == 0
            ? 8 : index->root_capacities[param_index];
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
        if (capacity > SIZE_MAX / sizeof(*grown))
            return false;
        grown = realloc(index->roots_by_param[param_index],
                        capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        index->roots_by_param[param_index] = grown;
        index->root_capacities[param_index] = capacity;
    }
    index->roots_by_param[param_index][index->root_counts[param_index]++] = root;
    index->relevant_root_count++;
    return true;
}

bool
function_param_flow_program_point_index_build(
    FunctionParamFlowSummaryStore *store,
    ASTNode *function_decl,
    FunctionParamFlowProgramPointIndex **index_out)
{
    FunctionParamFlowProgramPointIndex *index;
    ASTNode *body;
    uint32_t function_id;
    size_t existing;
    size_t param_count;

    if (store == NULL || function_decl == NULL || index_out == NULL)
        return false;
    function_id = ast_node_stable_id(function_decl);
    if (function_id == 0)
        return false;
    existing = function_param_flow_program_point_index_find(
        store, function_id);
    if (existing != SIZE_MAX) {
        *index_out = &store->program_point_indexes[existing];
        return (*index_out)->function_decl == function_decl && (*index_out)->dependency_roots_ready &&
            function_param_flow_index_roots_current(*index_out) &&
            function_param_flow_index_claims_current(store->ctx, *index_out);
    }

    param_count = ast_func_param_count(function_decl);
    if (!function_param_flow_program_point_index_reserve(
            store, store->program_point_index_count + 1)
        || (param_count > SIZE_MAX / sizeof(ASTNode **))
        || (param_count > SIZE_MAX / sizeof(size_t))) {
        return false;
    }

    existing = store->program_point_index_count++;
    index = &store->program_point_indexes[existing];
    memset(index, 0, sizeof(*index));
    index->function_id = function_id;
    index->function_decl = function_decl;
    index->param_count = param_count;
    if (param_count > 0) {
        index->roots_by_param = calloc(param_count, sizeof(ASTNode **));
        index->root_counts = calloc(param_count, sizeof(size_t));
        index->root_capacities = calloc(param_count, sizeof(size_t));
        if (index->roots_by_param == NULL
            || index->root_counts == NULL
            || index->root_capacities == NULL) {
            function_param_flow_program_point_index_destroy(index);
            store->program_point_index_count--;
            return false;
        }
    }

    body = ast_func_body(function_decl);
    index->dependency_body = body;
    index->dependency_body_id = ast_node_stable_id(body);
    if (body == NULL) {
        index->dependency_roots_ready = true;
        *index_out = index;
        return true;
    }

    if (body->type == AST_BLOCK) {
        index->dependency_root_count = ast_block_statement_count(body);
        if (index->dependency_root_count > SIZE_MAX / sizeof(*index->dependency_roots)) return false;
        index->dependency_roots = calloc(index->dependency_root_count, sizeof(*index->dependency_roots));
        index->dependency_root_ids = calloc(index->dependency_root_count, sizeof(*index->dependency_root_ids));
        if (index->dependency_root_count != 0 && (index->dependency_roots == NULL || index->dependency_root_ids == NULL)) return false;
        for (size_t s = 0; s < ast_block_statement_count(body); s++) {
            ASTNode *statement = ast_block_statement(body, s);
            index->dependency_roots[s] = statement;
            index->dependency_root_ids[s] = ast_node_stable_id(statement);
            index->statement_count++;
            store->indexed_statement_visits++;
            for (size_t p = 0; p < param_count; p++) {
                FuncParam *param = ast_func_param(function_decl, p);
                if (param == NULL || param->name == NULL
                    || !function_param_flow_has_identifier_ref(
                        statement, param->name))
                    continue;
                if (!function_param_flow_program_point_append(
                        index, p, statement)) {
                    store->indexed_program_points -=
                        index->relevant_root_count;
                    function_param_flow_program_point_index_destroy(index);
                    store->program_point_index_count--;
                    return false;
                }
                store->indexed_program_points++;
            }
        }
    } else {
        index->dependency_roots = malloc(sizeof(*index->dependency_roots));
        index->dependency_root_ids = malloc(sizeof(*index->dependency_root_ids));
        if (index->dependency_roots == NULL || index->dependency_root_ids == NULL) return false;
        index->dependency_roots[0] = body;
        index->dependency_root_ids[0] = ast_node_stable_id(body);
        index->dependency_root_count = 1;
        index->statement_count = 1;
        store->indexed_statement_visits++;
        for (size_t p = 0; p < param_count; p++) {
            FuncParam *param = ast_func_param(function_decl, p);
            if (param == NULL || param->name == NULL
                || !function_param_flow_has_identifier_ref(body, param->name))
                continue;
            if (!function_param_flow_program_point_append(index, p, body)) {
                store->indexed_program_points -= index->relevant_root_count;
                function_param_flow_program_point_index_destroy(index);
                store->program_point_index_count--;
                return false;
            }
            store->indexed_program_points++;
        }
    }

    index->mutable_output_required = function_param_flow_mutable_output_required(store->ctx, function_decl, index, body);
    index->dependency_roots_ready = true;
    *index_out = index;
    return true;
}

typedef struct {
    FunctionParamFlowSummaryStore *store;
    SlotFunctionLookup lookup;
    ASTNode *decl;
    FunctionParamFlowBackingFact *fact;
    bool private_stores_closed;
    ASTNode *loan_seed, *consumer, *escape_site;
    size_t loan_work;
    bool loan_seed_seen;
} ReturnedBacking;
/* Only binding origins/destinations feed a later traversal. Expression caches
 * flow child-to-parent in this traversal; final outputs are not origin inputs.
 * Their growth must not independently schedule another whole-body traversal. */
static bool backing_merge(ReturnedBacking *state, uint64_t *into, const uint64_t *from)
{
    bool grew = false;
    for (size_t w = 0; w < state->fact->words; w++) {
        uint64_t next = into[w] | from[w];
        grew |= next != into[w]; into[w] = next;
    }
    return grew;
}
static void backing_effect(ReturnedBacking *state, const uint64_t *origins, unsigned effect)
{
    if (state->loan_seed != NULL && origins[0] != 0 && effect != 0 && state->escape_site == NULL)
        state->escape_site = state->consumer;
    for (size_t p = 0; p < state->fact->count; p++)
        if ((origins[p / 64] & (UINT64_C(1) << (p % 64))) != 0) state->fact->effects[p] |= effect;
}
static bool backing_any_origin(const ReturnedBacking *state, const uint64_t *origins)
{
    for (size_t w = 0; w < state->fact->words; w++) if (origins[w] != 0) return true;
    return false;
}
static FunctionParamFlowBackingBinding *backing_binding(ReturnedBacking *state, uint32_t id, bool insert)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (id == 0) { fact->complete = false; return NULL; }
    for (size_t i = 0; i < fact->binding_count; i++) if (fact->bindings[i].binding == id) return &fact->bindings[i];
    if (!insert) return NULL;
    if (fact->binding_count == SIZE_MAX / sizeof(*fact->bindings)) { function_param_flow_fail(state->store, state->decl, "backing binding size overflow"); fact->complete = false; return NULL; }
    FunctionParamFlowBackingBinding *rows = realloc(fact->bindings, (fact->binding_count + 1) * sizeof(*rows));
    if (rows == NULL) { function_param_flow_fail(state->store, state->decl, "backing binding allocation failed"); fact->complete = false; return NULL; }
    fact->bindings = rows;
    FunctionParamFlowBackingBinding *row = &rows[fact->binding_count];
    *row = (FunctionParamFlowBackingBinding){.binding = id};
    row->origins = calloc(fact->words, sizeof(uint64_t));
    row->destinations = calloc(fact->words, sizeof(uint64_t));
    fact->binding_count++;
    if (row->origins == NULL || row->destinations == NULL) { function_param_flow_fail(state->store, state->decl, "backing binding bitset allocation failed"); fact->complete = false; return NULL; }
    return row;
}
static size_t backing_node(ReturnedBacking *state, ASTNode *node)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (node == NULL || ast_node_stable_id(node) == 0) { fact->complete = false; return SIZE_MAX; }
    if (!function_param_flow_claim_record(state->store->ctx, fact->claims, node)) { fact->complete = false; return SIZE_MAX; }
    for (size_t i = 0; i < fact->node_count; i++) if (fact->nodes[i].node == node) return i;
    if (fact->node_count == SIZE_MAX / sizeof(*fact->nodes)) { function_param_flow_fail(state->store, state->decl, "backing node size overflow"); fact->complete = false; return SIZE_MAX; }
    FunctionParamFlowBackingNode *rows = realloc(fact->nodes, (fact->node_count + 1) * sizeof(*rows));
    if (rows == NULL) { function_param_flow_fail(state->store, state->decl, "backing node allocation failed"); fact->complete = false; return SIZE_MAX; }
    fact->nodes = rows;
    size_t at = fact->node_count++;
    rows[at] = (FunctionParamFlowBackingNode){.node = node, .id = ast_node_stable_id(node), .shape = node->type};
    rows[at].origins = calloc(fact->words, sizeof(uint64_t));
    if (rows[at].origins == NULL) { function_param_flow_fail(state->store, state->decl, "backing origin allocation failed"); fact->complete = false; return SIZE_MAX; }
    return at;
}
static bool backing_work(ReturnedBacking *state, size_t at)
{
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    if (row->work_epoch == state->fact->epoch) return true;
    size_t *work = state->loan_seed != NULL ? &state->loan_work : &state->store->work_units;
    if (*work >= state->store->work_budget) { function_param_flow_fail(state->store, state->decl, "recursive summary work budget exceeded"); state->fact->complete = false; return false; }
    (*work)++; row->work_epoch = state->fact->epoch; return true;
}
static uint32_t backing_place(ReturnedBacking *state, ASTNode *node)
{
    while (node != NULL && (node->type == AST_MEMBER_ACCESS || node->type == AST_ARRAY_ACCESS))
        node = node->type == AST_MEMBER_ACCESS ? ast_member_object(node) : ast_array_access_array(node);
    uint32_t id = semantic_param_publication_binding(state->store->ctx, node);
    if (id == 0) state->fact->complete = false;
    return id;
}
static void backing_store(ReturnedBacking *state, ASTNode *target, const uint64_t *origins)
{
    FunctionParamFlowBackingBinding *row = backing_binding(state, backing_place(state, target), false);
    if (row == NULL) { backing_effect(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE); state->fact->complete = false; return; }
    bool output = false;
    for (size_t w = 0; w < state->fact->words; w++) output |= row->destinations[w] != 0;
    for (size_t p = 0; p < state->fact->count; p++) if ((origins[p / 64] & (UINT64_C(1) << (p % 64))) != 0)
        backing_merge(state, &state->fact->outputs[p * state->fact->words], row->destinations);
    if (output) backing_effect(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
    else if (row->local) state->fact->changed |= backing_merge(state, row->origins, origins);
    else { backing_effect(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE); state->fact->complete = false; }
}
static void backing_destination_alias(ReturnedBacking *state, ASTNode *value, uint32_t id)
{
    if (value == NULL || (value->type != AST_IDENTIFIER && value->type != AST_MEMBER_ACCESS && value->type != AST_ARRAY_ACCESS)) return;
    FunctionParamFlowBackingBinding *source = backing_binding(state, backing_place(state, value), false);
    if (source == NULL) return;
    uint64_t *destinations = source->destinations;
    FunctionParamFlowBackingBinding *destination = backing_binding(state, id, true);
    if (destination != NULL) state->fact->changed |= backing_merge(state, destination->destinations, destinations);
}
static const uint64_t *backing_origin(ReturnedBacking *state, ASTNode *node)
{
    if (node == NULL) return NULL;
    size_t at = backing_node(state, node);
    if (at == SIZE_MAX || !backing_work(state, at)) return NULL;
    FunctionParamFlowBackingFact *fact = state->fact;
    uint64_t *origins = fact->nodes[at].origins;
    if (fact->nodes[at].origin_epoch == fact->epoch) return origins;
    fact->nodes[at].origin_epoch = fact->epoch;
    if (node == state->loan_seed) { state->loan_seed_seen = true; backing_merge(state, origins, &(uint64_t){1}); return origins; }
    if (node->type == AST_CAST) {
        if (!semantic_param_publication_checked_numeric_cast(state->store->ctx, node)) { fact->complete = false; return NULL; }
        (void)backing_origin(state, ast_cast_operand(node));
        return origins;
    }
    SemanticPublishedValue value = semantic_param_publication_value(state->store->ctx, node);
    if (value.ok && (value.meta || value.origin_ready)) return origins;
    if (node->type == AST_CALL && semantic_typed_map_query_result_independent(state->store->ctx, node)) return origins;
    switch (node->type) {
    case AST_IDENTIFIER: {
        FunctionParamFlowBackingBinding *row = backing_binding(state, semantic_param_publication_binding(state->store->ctx, node), false);
        if (row != NULL) backing_merge(state, origins, row->origins);
        else fact->complete = false;
        break;
    }
    case AST_MEMBER_ACCESS: { const uint64_t *bits = backing_origin(state, ast_member_object(node)); if (bits != NULL) backing_merge(state, origins, bits); break; }
    case AST_ARRAY_ACCESS: { const uint64_t *bits = backing_origin(state, ast_array_access_array(node)); if (bits != NULL) backing_merge(state, origins, bits); break; }
    case AST_STRING: case AST_NUMBER: case AST_BOOLEAN: break;
    case AST_CALL:
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            if (!semantic_param_publication_checked_argument(state->store->ctx, node, i)) { fact->complete = false; break; }
            const uint64_t *bits = backing_origin(state, ast_call_argument(node, i));
            if (bits == NULL || !backing_any_origin(state, bits)) continue;
            ASTNode *callee = semantic_param_publication_callee(state->store->ctx, node, i);
            if (callee == NULL) {
                bool depends = true, retains = true;
                bool relation = semantic_param_publication_checked_result_relation(state->store->ctx, node, i, &depends, &retains);
                if (!relation || depends) backing_merge(state, origins, bits);
                if ((!relation || retains) && !semantic_param_publication_argument_borrowed_for_call(state->store->ctx, node, i) &&
                    !semantic_typed_map_query_receiver_read(state->store->ctx, node, i)) fact->complete = false;
            } else {
                unsigned mask = function_param_flow_summary_for_call(&state->lookup, node, i);
                if (!function_param_flow_callee_relation_ready(&state->lookup, node, i))
                    function_param_flow_callee_unfinished(&state->lookup, node, i, &fact->pending, &fact->complete);
                if ((mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) backing_merge(state, origins, bits);
            }
        }
        break;
    case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < (node->type == AST_ARRAY_LITERAL ? ast_array_literal_count(node) : ast_tuple_literal_count(node)); i++) {
            const uint64_t *bits = backing_origin(state, node->type == AST_ARRAY_LITERAL ? ast_array_literal_element(node, i) : ast_tuple_literal_element(node, i));
            if (bits != NULL) backing_merge(state, origins, bits);
        }
        break;
    default: fact->complete = false; break;
    }
    return origins;
}
static void backing_consumers(ReturnedBacking *state, ASTNode *node)
{
    if (node == NULL) return;
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
    case AST_FOR_LOOP:
        backing_consumers(state, ast_for_range_start(node)); backing_consumers(state, ast_for_range_end(node));
        backing_consumers(state, ast_for_iterable(node)); backing_consumers(state, ast_for_body(node)); break;
    case AST_WHILE_LOOP: backing_consumers(state, ast_while_condition(node)); backing_consumers(state, ast_while_body(node)); break;
    case AST_WITH_STMT: backing_consumers(state, ast_with_body(node)); break;
    case AST_BREAK: case AST_CONTINUE: break;
    case AST_LET_DECL: {
        FunctionParamFlowBackingBinding *local = backing_binding(state, ast_node_stable_id(node), true);
        if (local != NULL) local->local = true;
        backing_destination_alias(state, ast_let_initializer(node), ast_node_stable_id(node));
        const uint64_t *bits = backing_origin(state, ast_let_initializer(node));
        local = backing_binding(state, ast_node_stable_id(node), false);
        if (local != NULL && bits != NULL) fact->changed |= backing_merge(state, local->origins, bits);
        backing_consumers(state, ast_let_initializer(node)); break;
    }
    case AST_ASSIGNMENT: {
        ASTNode *target = ast_assignment_target(node);
        if (target != NULL && target->type == AST_IDENTIFIER)
            backing_destination_alias(state, ast_assignment_value(node), semantic_param_publication_binding(state->store->ctx, target));
        const uint64_t *bits = backing_origin(state, ast_assignment_value(node));
        if (bits != NULL) backing_store(state, target, bits);
        backing_consumers(state, ast_assignment_value(node)); break;
    }
    case AST_CALL:
        if (ast_call_semantic_callee_value_binding_id(node) != 0 || ast_call_callee(node) == NULL ||
            ast_call_callee(node)->type != AST_IDENTIFIER) { fact->complete = false; break; }
        if (state->private_stores_closed &&
            semantic_param_publication_private_array_push(state->store->ctx, node, state->decl)) {
            /* Store the inserted backing, not a blanket non-retention claim.
             * Subsequent return/wrapper/output/channel operations read it. */
            const uint64_t *bits = backing_origin(state, ast_call_argument(node, 1));
            if (bits != NULL) backing_store(state, ast_call_argument(node, 0), bits);
            backing_consumers(state, ast_call_argument(node, 0));
            backing_consumers(state, ast_call_argument(node, 1));
            break;
        }
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            if (!semantic_param_publication_checked_argument(state->store->ctx, node, i)) { fact->complete = false; break; }
            ASTNode *arg = ast_call_argument(node, i);
            const uint64_t *bits = backing_origin(state, arg);
            bool local = semantic_param_publication_argument_borrowed_for_call(state->store->ctx, node, i) ||
                semantic_typed_map_query_receiver_read(state->store->ctx, node, i);
            bool depends = false, retains = true;
            local |= semantic_param_publication_checked_result_relation(state->store->ctx, node, i, &depends, &retains) && !retains;
            ASTNode *callee = semantic_param_publication_callee(state->store->ctx, node, i);
            bool self = callee == state->decl && !state->decl->is_async_decl && arg->type == AST_IDENTIFIER &&
                ast_func_param(callee, i)->mode == PARAM_MODE_REF &&
                semantic_param_publication_binding(state->store->ctx, arg) == ast_func_param_stable_id(ast_func_param(callee, i));
            for (size_t p = 0; self && p < ast_func_param_count(callee); p++) if (ast_func_param(callee, p)->mode == PARAM_MODE_MUT_REF) self = false;
            if (bits != NULL && backing_any_origin(state, bits) && !local && !self) {
                if (callee == NULL) backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                else {
                    FuncParam *param = ast_func_param(callee, i);
                    unsigned mask = function_param_flow_summary_for_call(&state->lookup, node, i);
                    backing_effect(state, bits, mask & (SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE));
                    if (param == NULL || param->mode == PARAM_MODE_OWN || param->mode == PARAM_MODE_MUT_REF)
                        backing_effect(state, bits, SLOT_PARAM_SUMMARY_CALL_ESCAPE);
                    if (!function_param_flow_callee_relation_ready(&state->lookup, node, i))
                        function_param_flow_callee_unfinished(&state->lookup, node, i, &fact->pending, &fact->complete);
                    for (size_t p = 0; p < ast_func_param_count(callee); p++) {
                        FuncParam *output = ast_func_param(callee, p);
                        if (output == NULL) { fact->complete = false; break; }
                        if (output->mode != PARAM_MODE_MUT_REF && output->mode != PARAM_MODE_REF) continue;
                        if (output->mode == PARAM_MODE_REF && output->type != NULL && ast_type_name(output->type) != NULL &&
                            strcmp(ast_type_name(output->type), "String") == 0) continue;
                        bool retained = false;
                        if (!function_param_flow_output_dependency_for_call(&state->lookup, node, i, p, &retained))
                            function_param_flow_callee_unfinished(&state->lookup, node, i, &fact->pending, &fact->complete);
                        else if (retained) backing_store(state, ast_call_argument(node, p), bits);
                    }
                }
            }
            backing_consumers(state, arg);
        }
        break;
    case AST_RETURN: {
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
    case AST_MEMBER_ACCESS: backing_consumers(state, ast_member_object(node)); break;
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
bool function_param_flow_index_roots_current(const FunctionParamFlowProgramPointIndex *index)
{
    ASTNode *body = ast_func_body(index->function_decl);
    if (body == NULL || body != index->dependency_body || ast_node_stable_id(body) != index->dependency_body_id ||
        (body->type == AST_BLOCK ? ast_block_statement_count(body) : 1) != index->dependency_root_count) return false;
    for (size_t r = 0; r < index->dependency_root_count; r++)
        if (index->dependency_root_ids[r] == 0 ||
            ast_node_stable_id(index->dependency_roots[r]) != index->dependency_root_ids[r] ||
            index->dependency_roots[r] != (body->type == AST_BLOCK ? ast_block_statement(body, r) : body)) return false;
    return true;
}

bool function_param_flow_returned_backing_effects(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, unsigned *effects, bool *outputs, size_t output_count, bool *pending,
    bool private_stores_closed)
{
    if (store == NULL || store->failed || effects == NULL || pending == NULL || decl == NULL || outputs == NULL ||
        output_count != ast_func_param_count(decl) || ordinal >= output_count ||
        !semantic_param_publication_inputs_ready(store->ctx)) return false;
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index) ||
        !function_param_flow_index_roots_current(index)) return false;
    if (index->backing == NULL) {
        if (output_count > SIZE_MAX - 63) { function_param_flow_fail(store, decl, "backing width overflow"); return false; }
        size_t words = (output_count + 63) / 64;
        if (words == 0 || words > SIZE_MAX / sizeof(uint64_t) ||
            output_count > SIZE_MAX / (words * sizeof(uint64_t)) || output_count > SIZE_MAX / sizeof(unsigned)) { function_param_flow_fail(store, decl, "backing matrix size overflow"); return false; }
        index->backing = calloc(1, sizeof(*index->backing));
        if (index->backing == NULL) { function_param_flow_fail(store, decl, "backing fact allocation failed"); return false; }
        index->backing->count = output_count; index->backing->words = words;
        index->backing->claims = index->claims;
        index->backing->effects = calloc(output_count, sizeof(unsigned));
        index->backing->outputs = calloc(output_count * words, sizeof(uint64_t));
        if (index->backing->effects == NULL || index->backing->outputs == NULL) { function_param_flow_fail(store, decl, "backing matrix allocation failed"); return false; }
        ReturnedBacking seed = {.store = store, .decl = decl, .fact = index->backing};
        for (size_t p = 0; p < output_count; p++) {
            FuncParam *param = ast_func_param(decl, p);
            FunctionParamFlowBackingBinding *row = backing_binding(&seed, ast_func_param_stable_id(param), true);
            if (row == NULL) return false;
            row->origins[p / 64] |= UINT64_C(1) << (p % 64);
            if (param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF)
                row->destinations[p / 64] |= UINT64_C(1) << (p % 64);
        }
    }
    /* Heap-owned fact/root rows survive recursive index and entry growth. */
    FunctionParamFlowBackingFact *fact = index->backing;
    /* Effects are monotone within one admission mode. Never reuse a precise
     * private-store evaluation for a query lacking its fresh graph witness,
     * nor pretend a previously conservative CALL mask can be subtracted. */
    if (fact->epoch != 0 && fact->private_stores_closed != private_stores_closed) return false;
    fact->private_stores_closed = private_stores_closed;
    ASTNode **roots = index->dependency_roots;
    size_t count = index->dependency_root_count;
    ReturnedBacking state = {.store = store, .lookup = {store->ctx, store->program_root}, .decl = decl,
        .fact = fact, .private_stores_closed = private_stores_closed};
    if (!function_param_flow_backing_rows_current(store, decl)) return false;
    if (fact->building) { *pending = true; return false; }
    if (fact->epoch == 0 || (fact->pending && fact->evaluation_pass != store->fixed_point_passes + 1)) {
        fact->building = true; fact->complete = true; fact->pending = false;
        fact->evaluation_pass = store->fixed_point_passes + 1;
        do {
            if (fact->epoch == SIZE_MAX) { function_param_flow_fail(store, decl, "backing iteration identity overflow"); fact->complete = false; break; }
            fact->epoch++; fact->changed = false;
            for (size_t r = 0; r < count && fact->complete; r++) backing_consumers(&state, roots[r]);
        } while (fact->changed && fact->complete);
        fact->building = false;
    }
    *effects = fact->effects[ordinal];
    for (size_t p = 0; p < output_count; p++)
        outputs[p] = (fact->outputs[ordinal * fact->words + p / 64] & (UINT64_C(1) << (p % 64))) != 0;
    *pending = fact->pending && fact->complete;
    return fact->complete && !fact->pending;
}

static bool returned_loan_storage_root(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowIndexClaims *claims, ASTNode *caller, const ASTNode *place, const ASTNode **root)
{
    if (claims == NULL || !slot_call_argument_storage_root(place, root)) return false;
    for (size_t step = 0; *root != NULL && step <= claims->count; step++) {
        uint32_t binding = semantic_param_publication_binding(store->ctx, *root);
        if (binding == 0) return false;
        FunctionParamFlowIndexClaim *declaration = NULL;
        for (size_t c = 0; c < claims->count; c++)
            if (claims->rows[c].id == binding) { declaration = &claims->rows[c]; break; }
        if (declaration == NULL) {
            for (size_t p = 0; p < ast_func_param_count(caller); p++)
                if (binding == ast_func_param_stable_id(ast_func_param(caller, p))) return true;
            return false;
        }
        if (declaration->shape != AST_LET_DECL || declaration->node == NULL ||
            declaration->node->type != AST_LET_DECL || ast_node_stable_id(declaration->node) != binding ||
            declaration->local_alias != ast_let_is_alias(declaration->node)) return false;
        if (!declaration->local_alias) return true;
        ASTNode *initializer = ast_let_initializer(declaration->node);
        if (initializer == NULL || declaration->child_count != 2 || declaration->children[0] != initializer ||
            declaration->child_ids[0] == 0 || declaration->child_ids[0] != ast_node_stable_id(initializer) ||
            !semantic_param_publication_value(store->ctx, initializer).ok ||
            !slot_call_argument_storage_root(initializer, root) || *root == NULL) return false;
    }
    return *root == NULL;
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
            if (!returned_loan_storage_root(store, claims, loan->caller, value, &root) || root == NULL) { admitted = false; break; }
            value = (ASTNode *)root;
        }
        if (semantic_param_publication_value_has_static_string_backing(store->ctx, value)) continue;
        ASTNode *dependency = NULL;
        if (value->type == AST_MEMBER_ACCESS) dependency = ast_member_object(value);
        else if (value->type == AST_ARRAY_ACCESS) dependency = ast_array_access_array(value);
        else if (value->type == AST_IDENTIFIER) {
            uint32_t binding = semantic_param_publication_binding(store->ctx, value);
            bool formal = false;
            for (size_t p = 0; p < ast_func_param_count(loan->caller); p++) {
                FuncParam *param = ast_func_param(loan->caller, p);
                if (param == NULL || binding == 0 || binding != ast_func_param_stable_id(param)) continue;
                unsigned mask = function_param_flow_summary_for_param(store->ctx, loan->caller, p);
                formal = true;
                if (param->mode != PARAM_MODE_REF || (mask & effects) != effects || store->failed) admitted = false;
            }
            if (formal) continue;
            for (size_t i = 0; i < local_count; i++) {
                if (binding != 0 && binding == local_ids[i]) dependency = ast_let_initializer(locals[i]);
            }
            for (size_t c = 0; dependency != NULL && c < claims->count; c++) {
                ASTNode *node = claims->rows[c].node;
                if (node->type == AST_ASSIGNMENT) {
                    const ASTNode *target = NULL;
                    if (!returned_loan_storage_root(store, claims, loan->caller, ast_assignment_target(node), &target) ||
                        (target != NULL && semantic_param_publication_binding(store->ctx, target) == binding)) {
                        admitted = false; break;
                    }
                }
                if (node->type == AST_CALL) for (size_t p = 0; p < ast_call_arg_count(node); p++) {
                    if (!semantic_param_publication_checked_argument(store->ctx, node, p)) { admitted = false; break; }
                    if (semantic_param_publication_string_argument_read_only(store->ctx, node, p) ||
                        semantic_typed_map_query_receiver_read(store->ctx, node, p)) continue;
                    const ASTNode *root = NULL;
                    if (!returned_loan_storage_root(store, claims, loan->caller, ast_call_argument(node, p), &root)) { admitted = false; break; }
                    if (root == NULL || semantic_param_publication_binding(store->ctx, root) != binding) continue;
                    ASTNode *callee = semantic_param_publication_callee(store->ctx, node, p);
                    if (callee != NULL) {
                        if (ast_func_param(callee, p)->mode == PARAM_MODE_MUT_REF ||
                            (function_param_flow_summary_for_call(&(SlotFunctionLookup){store->ctx, store->program_root}, node, p) & SLOT_PARAM_SUMMARY_WRITE) != 0) admitted = false;
                    } else admitted = false;
                }
            }
            if (dependency == NULL) admitted = false;
        } else if (value->type == AST_CALL) {
            bool found = false;
            for (size_t c = 0; c < view->call_count; c++) {
                const PendingRefCall *nested = &view->calls[c];
                if (nested->call != value) continue;
                if (!semantic_param_publication_ref_call_current(store->ctx, nested) || nested->caller != loan->caller) { admitted = false; break; }
                unsigned mask = function_param_flow_summary_for_param(store->ctx, nested->callee, nested->ordinal);
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

bool function_param_flow_return_loan_admitted(SemanticContext *ctx,
    const SemanticParamPublicationView *view, const PendingRefCall *row, ASTNode **escape_site)
{
    if (escape_site == NULL) return false;
    *escape_site = row == NULL ? NULL : row->call;
    if (!semantic_param_publication_view_current(ctx, view) ||
        !semantic_param_publication_ref_call_current(ctx, row)) return false;
    bool registered = false;
    for (size_t c = 0; c < view->call_count; c++) if (&view->calls[c] == row) registered = true;
    if (!registered) return false;
    SlotFunctionLookup lookup = {ctx, ctx->function_param_publications->program};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index;
    if (store == NULL || store->failed || store->solving ||
        (function_param_flow_summary_for_call(&lookup, row->call, row->ordinal) & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0 ||
        !function_param_flow_callee_relation_ready(&lookup, row->call, row->ordinal) ||
        !function_param_flow_program_point_index_build(store, row->caller, &index) ||
        !function_param_flow_backing_rows_current(store, row->caller)) return false;
    if (!function_param_flow_program_point_index_build(store, row->caller, &index)) return false;
    bool private_stores_closed = index->private_local_stores && function_param_flow_no_store_graph(store, row->caller);
    if (!function_param_flow_program_point_index_build(store, row->caller, &index)) return false;
    /* One origin and one external-destination flag: neither is a formal ordinal. */
    unsigned effects = 0;
    uint64_t outputs = 0;
    FunctionParamFlowBackingFact fact = {.count = 1, .words = 1, .effects = &effects,
        .outputs = &outputs, .complete = true, .claims = index->claims};
    ReturnedBacking state = {.store = store, .lookup = lookup, .decl = row->caller,
        .fact = &fact, .private_stores_closed = private_stores_closed, .loan_seed = row->call};
    for (size_t p = 0; p < ast_func_param_count(row->caller); p++) {
        FuncParam *param = ast_func_param(row->caller, p);
        FunctionParamFlowBackingBinding *binding = backing_binding(&state, ast_func_param_stable_id(param), true);
        if (binding == NULL) break;
        if (param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF) binding->destinations[0] = 1;
    }
    ASTNode **roots = index->dependency_roots;
    size_t root_count = index->dependency_root_count;
    do {
        fact.epoch++; fact.changed = false;
        for (size_t r = 0; r < root_count && fact.complete; r++) backing_consumers(&state, roots[r]);
    } while (fact.changed && fact.complete);
    if (state.escape_site != NULL) *escape_site = state.escape_site;
    bool admitted = fact.complete && !fact.pending && state.loan_seed_seen && !store->failed &&
        function_param_flow_program_point_index_build(store, row->caller, &index) &&
        (effects == 0 || returned_loan_sources_admitted(store, index, view, row, effects));
    for (size_t n = 0; n < fact.node_count; n++) free(fact.nodes[n].origins);
    for (size_t b = 0; b < fact.binding_count; b++) { free(fact.bindings[b].origins); free(fact.bindings[b].destinations); }
    free(fact.nodes); free(fact.bindings);
    return admitted;
}

bool function_param_flow_no_store_call_append(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *call)
{
    ASTNode *callee = semantic_param_publication_direct_callee(ctx, call);
    if (callee == NULL || ast_call_arg_count(call) != ast_func_param_count(callee) ||
        !semantic_param_publication_routine_no_output_header_ready(ctx, callee)) return false;
    for (size_t p = 0; p < ast_call_arg_count(call); p++)
        if (!semantic_param_publication_value(ctx, ast_call_argument(call, p)).ok) return false;
    size_t count = index->no_store_call_count;
    for (size_t e = 0; e < count; e++) if (index->no_store_calls[e] == call) return true;
    if (count == SIZE_MAX / sizeof(ASTNode *)) return false;
    ASTNode **calls = realloc(index->no_store_calls, (count + 1) * sizeof(*calls));
    if (calls == NULL) return false;
    index->no_store_calls = calls;
    calls[count] = call;
    index->no_store_call_count++;
    return true;
}

static bool function_param_flow_no_store_member_admitted(
    const FunctionParamFlowSummaryStore *store, size_t ordinal)
{
    if (ordinal >= store->program_point_index_count) return false;
    const FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[ordinal];
    size_t position = index->admitted_no_store_position;
    return store->solving && store->demand_epoch != 0 &&
        index->admitted_no_store_epoch == store->demand_epoch &&
        position != 0 && position <= store->admitted_no_store_count &&
        store->admitted_no_store_indexes[position - 1] == ordinal;
}
/* The union is one admitted set in this demand, not copied per root. Replaying
 * it checks every premise, including edges into the set. An epoch alone is
 * never permission; current typed facts and exact membership must still join. */
static bool function_param_flow_no_store_admitted_current(FunctionParamFlowSummaryStore *store)
{
    for (size_t m = 0; m < store->admitted_no_store_count; m++) {
        size_t ordinal = store->admitted_no_store_indexes[m];
        if (!function_param_flow_no_store_member_admitted(store, ordinal)) return false;
        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[ordinal];
        ASTNode *decl = index->function_decl;
        if (index->function_id != ast_node_stable_id(decl) ||
            index->failed_no_store_epoch == store->demand_epoch ||
            !semantic_param_publication_routine_no_output_header_ready(store->ctx, decl) ||
            !function_param_flow_program_point_index_build(store, decl, &index) ||
            !index->dependency_roots_ready || index->mutable_output_required) return false;
        for (size_t e = 0; e < index->no_store_call_count; e++) {
            ASTNode *call = index->no_store_calls[e];
            ASTNode *callee = semantic_param_publication_direct_callee(store->ctx, call);
            if (callee == NULL || ast_call_arg_count(call) != ast_func_param_count(callee)) return false;
            for (size_t p = 0; p < ast_call_arg_count(call); p++)
                if (!semantic_param_publication_checked_argument(store->ctx, call, p) ||
                    !semantic_param_publication_value(store->ctx, ast_call_argument(call, p)).ok) return false;
            size_t target = function_param_flow_program_point_index_find(store, ast_node_stable_id(callee));
            if (!function_param_flow_no_store_member_admitted(store, target) ||
                store->program_point_indexes[target].function_decl != callee) return false;
        }
    }
    return true;
}
/* The queue is owned by this closure, not an early parameter mask or an index
 * pointer retained across growable table allocation. Cycles close only after
 * every reached header and local store predicate has been checked. */
bool function_param_flow_no_store_graph(FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
    if (store == NULL || decl == NULL || store->failed) return false;
    uint32_t declaration_id = ast_node_stable_id(decl);
    /* Repeating a refusal cannot grant permission. Its only consumer is the
     * unchanged conservative backing path. Positive set reuse replays every
     * current premise; out-of-solver queries reconstruct the graph. No borrowed
     * index pointer survives closure construction. */
    if (store->solving && store->demand_epoch != 0)
        for (size_t i = 0; i < store->program_point_index_count; i++) {
            FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
            if (index->function_decl == decl && index->function_id == declaration_id &&
                index->failed_no_store_epoch == store->demand_epoch) return false;
        }
    bool admitted_current = store->solving && store->demand_epoch != 0 &&
        store->admitted_no_store_count != 0;
    if (admitted_current) {
        if (!function_param_flow_no_store_admitted_current(store)) return false;
        size_t ordinal = function_param_flow_program_point_index_find(store, declaration_id);
        if (function_param_flow_no_store_member_admitted(store, ordinal) &&
            store->program_point_indexes[ordinal].function_decl == decl) return true;
    }
    ASTNode **queue = malloc(sizeof(*queue));
    if (queue == NULL) return false;
    queue[0] = decl;
    size_t count = 1;
    bool closed = true;
    for (size_t at = 0; at < count && closed; at++) {
        if (store->work_units >= store->work_budget) { closed = false; break; }
        store->work_units++;
        FunctionParamFlowProgramPointIndex *index;
        if (!semantic_param_publication_routine_no_output_header_ready(store->ctx, queue[at]) ||
            !function_param_flow_program_point_index_build(store, queue[at], &index) ||
            !index->dependency_roots_ready || index->mutable_output_required) { closed = false; break; }
        if (!function_param_flow_index_roots_current(index)) { closed = false; break; }
        for (size_t e = 0; e < index->no_store_call_count; e++) {
            ASTNode *call = index->no_store_calls[e];
            ASTNode *callee = semantic_param_publication_direct_callee(store->ctx, call);
            if (callee == NULL || ast_call_arg_count(call) != ast_func_param_count(callee)) { closed = false; break; }
            for (size_t p = 0; p < ast_call_arg_count(call); p++)
                if (!semantic_param_publication_value(store->ctx, ast_call_argument(call, p)).ok) closed = false;
            if (!closed) break;
            if (admitted_current) {
                size_t ordinal = function_param_flow_program_point_index_find(store, ast_node_stable_id(callee));
                if (function_param_flow_no_store_member_admitted(store, ordinal) &&
                    store->program_point_indexes[ordinal].function_decl == callee) continue;
            }
            size_t found = 0;
            while (found < count && queue[found] != callee) found++;
            if (found != count) continue;
            if (count == SIZE_MAX / sizeof(*queue)) { closed = false; break; }
            ASTNode **grown = realloc(queue, (count + 1) * sizeof(*queue));
            if (grown == NULL) { closed = false; break; }
            queue = grown;
            queue[count++] = callee;
        }
    }
    if (closed && store->solving && store->demand_epoch != 0) {
        size_t prior = store->admitted_no_store_count;
        if (count > SIZE_MAX - prior || prior + count > SIZE_MAX / sizeof(size_t)) closed = false;
        else {
            size_t *members = realloc(store->admitted_no_store_indexes, (prior + count) * sizeof(*members));
            if (members == NULL) closed = false;
            else {
                store->admitted_no_store_indexes = members;
                for (size_t m = 0; m < count; m++) {
                    size_t ordinal = function_param_flow_program_point_index_find(store, ast_node_stable_id(queue[m]));
                    if (ordinal == SIZE_MAX || store->program_point_indexes[ordinal].function_decl != queue[m]) { closed = false; break; }
                    members[prior + m] = ordinal;
                }
                if (closed) {
                    store->admitted_no_store_count = prior + count;
                    for (size_t m = prior; m < prior + count; m++) {
                        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[members[m]];
                        index->admitted_no_store_epoch = store->demand_epoch;
                        index->admitted_no_store_position = m + 1;
                    }
                }
            }
        }
    }
    free(queue);
    if (!closed && store->solving && store->demand_epoch != 0)
        for (size_t i = 0; i < store->program_point_index_count; i++) {
            FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
            if (index->function_decl == decl && index->function_id == declaration_id) {
                index->failed_no_store_epoch = store->demand_epoch;
                break;
            }
        }
    return closed;
}
