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
} ReturnedBacking;
static void backing_merge(ReturnedBacking *state, uint64_t *into, const uint64_t *from)
{
    for (size_t w = 0; w < state->fact->words; w++) {
        uint64_t next = into[w] | from[w];
        state->fact->changed |= next != into[w]; into[w] = next;
    }
}
static void backing_effect(ReturnedBacking *state, const uint64_t *origins, unsigned effect)
{
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
    if (state->store->work_units >= state->store->work_budget) { function_param_flow_fail(state->store, state->decl, "recursive summary work budget exceeded"); state->fact->complete = false; return false; }
    state->store->work_units++; row->work_epoch = state->fact->epoch; return true;
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
    else if (row->local) backing_merge(state, row->origins, origins);
    else { backing_effect(state, origins, SLOT_PARAM_SUMMARY_CALL_ESCAPE); state->fact->complete = false; }
}
static void backing_destination_alias(ReturnedBacking *state, ASTNode *value, uint32_t id)
{
    if (value == NULL || (value->type != AST_IDENTIFIER && value->type != AST_MEMBER_ACCESS && value->type != AST_ARRAY_ACCESS)) return;
    FunctionParamFlowBackingBinding *source = backing_binding(state, backing_place(state, value), false);
    if (source == NULL) return;
    uint64_t *destinations = source->destinations;
    FunctionParamFlowBackingBinding *destination = backing_binding(state, id, true);
    if (destination != NULL) backing_merge(state, destination->destinations, destinations);
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
    bool reachable;
    if (semantic_param_publication_execution(state->store->ctx, node, &reachable) && !reachable) return;
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
        if (local != NULL && bits != NULL) backing_merge(state, local->origins, bits);
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
            for (size_t p = 0; self && p < fact->count; p++) if (ast_func_param(callee, p)->mode == PARAM_MODE_MUT_REF) self = false;
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
    ASTNode *decl, size_t ordinal, unsigned *effects, bool *outputs, size_t output_count, bool *pending)
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
    ASTNode **roots = index->dependency_roots;
    size_t count = index->dependency_root_count;
    ReturnedBacking state = {.store = store, .lookup = {store->ctx, store->program_root}, .decl = decl, .fact = fact};
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

/* The queue is owned by this closure, not an early parameter mask or an index
 * pointer retained across growable table allocation. Cycles close only after
 * every reached header and local store predicate has been checked. */
bool function_param_flow_no_store_graph(FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
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
    free(queue);
    return closed;
}
