/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Demanded function-parameter resource/escape summary owner.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_analysis.h"
#include "slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "type_checker_internal.h"
#include "function_param_publication_owner.h"
#include "typed_map_query_fact_owner.h"



static size_t
function_param_flow_key_hash(uint32_t function_id, size_t param_index)
{
    size_t h = (size_t)function_id * (size_t)2654435761u;
    h ^= param_index + (size_t)0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    return h;
}

static bool
function_param_flow_rebuild_hash(FunctionParamFlowSummaryStore *store,
                                 size_t minimum_entries)
{
    size_t capacity = 16;
    size_t *hash;

    if (store == NULL)
        return false;
    if (minimum_entries > SIZE_MAX / 2)
        return false;
    while (capacity < minimum_entries * 2) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (store->hash != NULL && store->hash_capacity >= capacity)
        return true;

    hash = calloc(capacity, sizeof(size_t));
    if (hash == NULL)
        return false;

    for (size_t i = 0; i < store->count; i++) {
        size_t slot = function_param_flow_key_hash(
            store->entries[i].function_id,
            store->entries[i].param_index) & (capacity - 1);
        while (hash[slot] != 0)
            slot = (slot + 1) & (capacity - 1);
        hash[slot] = i + 1;
    }

    free(store->hash);
    store->hash = hash;
    store->hash_capacity = capacity;
    return true;
}

static size_t
function_param_flow_find(const FunctionParamFlowSummaryStore *store,
                         uint32_t function_id,
                         size_t param_index)
{
    size_t slot;

    if (store == NULL || store->hash == NULL || store->hash_capacity == 0)
        return SIZE_MAX;
    slot = function_param_flow_key_hash(function_id, param_index)
        & (store->hash_capacity - 1);
    for (size_t probe = 0; probe < store->hash_capacity; probe++) {
        size_t encoded = store->hash[slot];
        size_t index;

        if (encoded == 0)
            return SIZE_MAX;
        index = encoded - 1;
        if (store->entries[index].function_id == function_id
            && store->entries[index].param_index == param_index) {
            return index;
        }
        slot = (slot + 1) & (store->hash_capacity - 1);
    }
    return SIZE_MAX;
}


static bool
function_param_flow_reserve_entries(FunctionParamFlowSummaryStore *store,
                                    size_t minimum)
{
    FunctionParamFlowSummaryEntry *entries;
    size_t capacity;

    if (store == NULL)
        return false;
    if (minimum <= store->capacity)
        return true;
    capacity = store->capacity == 0 ? 16 : store->capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*entries))
        return false;
    entries = realloc(store->entries, capacity * sizeof(*entries));
    if (entries == NULL)
        return false;
    store->entries = entries;
    store->capacity = capacity;
    return true;
}

static size_t
function_param_flow_add(FunctionParamFlowSummaryStore *store,
                        ASTNode *function_decl,
                        size_t param_index)
{
    uint32_t function_id = ast_node_stable_id(function_decl);
    size_t index;
    size_t slot;

    if (store == NULL || function_decl == NULL || function_id == 0
        || param_index >= ast_func_param_count(function_decl)) {
        return SIZE_MAX;
    }
    index = function_param_flow_find(store, function_id, param_index);
    if (index != SIZE_MAX)
        return index;
    if (!function_param_flow_reserve_entries(store, store->count + 1)
        || !function_param_flow_rebuild_hash(store, store->count + 1)) {
        return SIZE_MAX;
    }

    index = store->count++;
    store->entries[index] = (FunctionParamFlowSummaryEntry){
        .function_id = function_id, .param_index = param_index,
        .function_decl = function_decl, .state = FUNCTION_PARAM_FLOW_UNSEEN};

    slot = function_param_flow_key_hash(function_id, param_index)
        & (store->hash_capacity - 1);
    while (store->hash[slot] != 0)
        slot = (slot + 1) & (store->hash_capacity - 1);
    store->hash[slot] = index + 1;
    return index;
}

void
function_param_flow_fail(FunctionParamFlowSummaryStore *store,
                         ASTNode *site,
                         const char *reason)
{
    if (store == NULL || store->failed)
        return;
    store->failed = true;
    semantic_error(store->ctx, site,
        "Function parameter flow summary failed closed: %s",
        reason != NULL ? reason : "unknown owner failure");
}

static bool function_param_flow_local_replacement(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *assignment)
{
    if (!semantic_param_publication_assignment_replacement(ctx, assignment)) return false;
    ASTNode *node = ast_assignment_target(assignment);
    if (!function_param_flow_index_claim_record(ctx, index, node)) return false;
    uint32_t binding = semantic_param_publication_binding(ctx, node);
    if (binding == 0 || node->type != AST_IDENTIFIER) return false;
    for (size_t i = 0; i < index->local_count; i++)
        if (binding == index->local_ids[i] && !ast_let_is_alias(index->local_definitions[i]) &&
            binding == ast_node_stable_id(index->local_definitions[i])) return true;
    return false;
}
static bool function_param_flow_local_record(FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (ast_let_is_alias(node)) return true;
    size_t count = index->local_count;
    if (ast_node_stable_id(node) == 0 || count == SIZE_MAX / sizeof(ASTNode *)) return false;
    ASTNode **definitions = realloc(index->local_definitions, (count + 1) * sizeof(*definitions));
    if (definitions == NULL) return false;
    index->local_definitions = definitions;
    uint32_t *ids = realloc(index->local_ids, (count + 1) * sizeof(*ids));
    if (ids == NULL) return false;
    index->local_ids = ids;
    definitions[count] = node;
    ids[count] = ast_node_stable_id(node);
    index->local_count++;
    return true;
}
bool function_param_flow_mutable_output_required(SemanticContext *ctx, ASTNode *decl, FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (node == NULL) return false;
    if (!function_param_flow_index_claim_record(ctx, index, node)) return true;
    bool reachable;
    if (semantic_param_publication_execution(ctx, node, &reachable) && !reachable) return false;
    switch (node->type) {
    case AST_LET_DECL:
        return !function_param_flow_local_record(index, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_let_initializer(node));
    case AST_RETURN: return function_param_flow_mutable_output_required(ctx, decl, index, ast_return_value(node));
    case AST_CALL:
        if (semantic_param_publication_private_array_push(ctx, node, decl)) {
            index->private_local_stores = true;
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                if (!semantic_param_publication_checked_argument(ctx, node, p) ||
                    function_param_flow_mutable_output_required(ctx, decl, index, ast_call_argument(node, p))) return true;
            return false;
        }
        if (!semantic_param_publication_value_call_target(ctx, node) &&
            !function_param_flow_no_store_call_append(ctx, index, node)) return true;
        if (ast_call_arg_count(node) == 0)
            return !function_param_flow_no_store_call_append(ctx, index, node);
        for (size_t p = 0; p < ast_call_arg_count(node); p++) {
            if (!semantic_param_publication_checked_argument(ctx, node, p)) return true;
            bool depends = false, retains = true;
            bool relation = semantic_param_publication_checked_result_relation(ctx, node, p, &depends, &retains);
            /* Physical scalars carry no backing into an output. Their argument
             * evaluation is still traversed below, including nested stores. */
            if (!semantic_param_publication_value_is_physical_scalar(ctx, ast_call_argument(node, p)) &&
                !(relation && !retains) && !semantic_param_publication_argument_borrowed_for_call(ctx, node, p) &&
                !semantic_typed_map_query_receiver_read(ctx, node, p) &&
                !function_param_flow_no_store_call_append(ctx, index, node)) return true;
            if (function_param_flow_mutable_output_required(ctx, decl, index, ast_call_argument(node, p))) return true;
        }
        return false;
    case AST_ASSIGNMENT:
        return !function_param_flow_local_replacement(ctx, index, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_assignment_value(node));
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            if (function_param_flow_mutable_output_required(ctx, decl, index, ast_block_statement(node, i))) return true;
        return false;
    case AST_IF_STMT:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_if_condition(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_if_then_branch(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_if_else_branch(node));
    case AST_FOR_LOOP:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_for_range_start(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_range_end(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_iterable(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_body(node));
    case AST_WHILE_LOOP:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_while_condition(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_while_body(node));
    case AST_WITH_STMT: return function_param_flow_mutable_output_required(ctx, decl, index, ast_with_body(node));
    case AST_IDENTIFIER: case AST_STRING: case AST_NUMBER: case AST_BOOLEAN: case AST_BREAK: case AST_CONTINUE: return false;
    case AST_BINARY:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_binary_left(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_binary_right(node));
    case AST_UNARY: return function_param_flow_mutable_output_required(ctx, decl, index, ast_unary_operand(node));
    case AST_CAST:
        return !semantic_param_publication_checked_numeric_cast(ctx, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_cast_operand(node));
    case AST_MEMBER_ACCESS: return function_param_flow_mutable_output_required(ctx, decl, index, ast_member_object(node));
    case AST_ARRAY_ACCESS:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_array_access_array(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_array_access_index(node));
    case AST_ARRAY_LITERAL: {
        /* No elements means no evaluation or store edge. The checked value
         * publication, rather than literal syntax, admits this narrow case;
         * it grants no returned-backing or cleanup permission. */
        SemanticPublishedValue value = semantic_param_publication_value(ctx, node);
        return ast_array_literal_count(node) != 0 || !value.ok || value.meta;
    }
    default: return true;
    }
}

static unsigned function_param_flow_evaluate(
    FunctionParamFlowSummaryStore *store,
    size_t index);

static unsigned
function_param_flow_demand_index(FunctionParamFlowSummaryStore *store,
                                 ASTNode *function_decl,
                                 size_t param_index)
{
    size_t index = function_param_flow_add(store, function_decl, param_index);
    FunctionParamFlowSummaryState state;

    if (index == SIZE_MAX) {
        function_param_flow_fail(store, function_decl,
            ast_node_stable_id(function_decl) == 0
                ? "function stable identity is missing"
                : "summary hash allocation failed");
        return SLOT_PARAM_SUMMARY_ALL;
    }

    state = store->entries[index].state;
    if (state == FUNCTION_PARAM_FLOW_COMPLETE) {
        store->cache_hits++;
        return store->entries[index].mask;
    }
    if (state == FUNCTION_PARAM_FLOW_COMPUTING) {
        store->active_had_recursion = true;
        store->recursion_hits++;
        return store->entries[index].mask;
    }
    if (state == FUNCTION_PARAM_FLOW_EVALUATED)
        return store->entries[index].mask;
    return function_param_flow_evaluate(store, index);
}

static unsigned
function_param_flow_evaluate(FunctionParamFlowSummaryStore *store,
                             size_t index)
{
    ASTNode *function_decl;
    FuncParam *param;
    FunctionParamFlowProgramPointIndex *program_points;
    ASTNode *const *roots;
    size_t root_count;
    SlotSummaryOrigin origin = {0};
    SlotFunctionLookup lookup;
    unsigned candidate;
    unsigned previous;
    bool storage_unproved;

    if (store == NULL || index >= store->count)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->failed)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->entries[index].state == FUNCTION_PARAM_FLOW_COMPUTING) {
        store->active_had_recursion = true;
        store->recursion_hits++;
        return store->entries[index].mask;
    }
    if (store->entries[index].state == FUNCTION_PARAM_FLOW_EVALUATED
        || store->entries[index].state == FUNCTION_PARAM_FLOW_COMPLETE) {
        return store->entries[index].mask;
    }

    if (store->work_units >= store->work_budget) {
        function_param_flow_fail(store, store->entries[index].function_decl,
            "recursive summary work budget exceeded");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }
    store->work_units++;

    function_decl = store->entries[index].function_decl;
    param = ast_func_param(function_decl, store->entries[index].param_index);
    if (param == NULL || param->name == NULL
        || ast_func_body(function_decl) == NULL) {
        function_param_flow_fail(store, function_decl,
            "function parameter or body identity is missing");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }

    store->entries[index].state = FUNCTION_PARAM_FLOW_COMPUTING;
    origin.function_decl = function_decl;
    origin.param_index = store->entries[index].param_index;
    origin.param_name = param->name;
    lookup.ctx = store->ctx;
    lookup.program_root = store->program_root;
    if (!function_param_flow_program_point_index_build(
            store, function_decl, &program_points)
        || program_points == NULL
        || store->entries[index].param_index >= program_points->param_count) {
        function_param_flow_fail(store, function_decl,
            "parameter program-point index is unavailable or stale");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }
    roots = program_points->roots_by_param[store->entries[index].param_index];
    root_count = program_points->root_counts[store->entries[index].param_index];
    bool private_stores = program_points->private_local_stores;
    bool graph_required = private_stores || program_points->no_store_call_count != 0;
    bool graph_closed = !program_points->mutable_output_required &&
        (!graph_required || function_param_flow_no_store_graph(store, function_decl));
    bool actual_store = !graph_closed;
    if (private_stores && graph_closed) {
        origin.private_stores_owner = function_decl;
        origin.private_stores_owner_id = ast_node_stable_id(function_decl);
    }
    candidate = slot_param_summary_in_program_points(
        roots, root_count, param->name, &lookup, &origin);
    storage_unproved = function_param_array_storage_unproved_in_program_points(
        roots, root_count, &origin, &lookup);
    bool scalar_return = (candidate & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 &&
        semantic_param_publication_routine_return_dependency_is_scalar(store->ctx, function_decl);
    bool anchored = false;
    bool source_kind_ready = semantic_param_publication_parameter_is_anchored_handle(
        store->ctx, function_decl, origin.param_index, &anchored);
    bool mutable_output = false;
    size_t output_count = ast_func_param_count(function_decl);
    for (size_t p = 0; p < output_count; p++) {
        FuncParam *output = ast_func_param(function_decl, p);
        if (output == NULL) mutable_output = true;
        else if (output->mode == PARAM_MODE_MUT_REF) mutable_output = true;
    }
    mutable_output |= actual_store;
    if ((scalar_return || mutable_output || (private_stores && graph_closed)) && !source_kind_ready) {
        function_param_flow_fail(store, function_decl, "typed parameter source kind is missing or stale");
        return SLOT_PARAM_SUMMARY_ALL;
    }
    if (!anchored && (scalar_return || mutable_output || (private_stores && graph_closed))) {
        unsigned backing_effects = SLOT_PARAM_SUMMARY_NONE;
        bool pending = false;
        bool *outputs = calloc(output_count, sizeof(*outputs));
        if (outputs == NULL) { function_param_flow_fail(store, function_decl, "output relation allocation failed"); return SLOT_PARAM_SUMMARY_ALL; }
        bool complete = function_param_flow_returned_backing_effects(store, function_decl,
            origin.param_index, &backing_effects, outputs, output_count, &pending, private_stores && graph_closed);
        if (complete && scalar_return) candidate = (candidate & ~SLOT_PARAM_SUMMARY_RETURN_ESCAPE) | backing_effects;
        else candidate |= backing_effects & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE);
        if (!complete && private_stores && graph_closed) candidate |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
        if (!complete && mutable_output && (!pending || store->settling_pending)) candidate |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
        if (store->entries[index].output_dependencies_ready != complete ||
            store->entries[index].returned_backing_pending != pending) store->changed = true;
        store->entries[index].returned_backing_pending = pending;
        store->entries[index].returned_backing_unproved = !complete;
        if (store->entries[index].output_dependencies == NULL) {
            store->entries[index].output_dependencies = outputs;
            store->entries[index].output_count = output_count;
            outputs = NULL;
            if (complete) store->changed = true;
        } else if (outputs != NULL) {
            for (size_t p = 0; p < output_count; p++) if (outputs[p] &&
                !store->entries[index].output_dependencies[p]) {
                store->entries[index].output_dependencies[p] = true;
                store->changed = true;
            }
        }
        store->entries[index].output_dependencies_ready = complete;
        free(outputs);
    } else if (source_kind_ready && !anchored &&
        semantic_param_publication_routine_no_output_header_ready(store->ctx, function_decl)) {
        /* The owned exhaustive selector, not a zero mask, proves no output
           store. Ordinary RETURN dependence remains in candidate unchanged. */
        if (store->entries[index].output_dependencies == NULL) {
            store->entries[index].output_dependencies = calloc(output_count, sizeof(bool));
            if (store->entries[index].output_dependencies == NULL) { function_param_flow_fail(store, function_decl, "empty output relation allocation failed"); return SLOT_PARAM_SUMMARY_ALL; }
            store->entries[index].output_count = output_count;
        }
        store->entries[index].output_dependencies_ready = store->entries[index].output_dependencies != NULL;
    }

    /* Recursive demands may grow the entries array, so reacquire by index. */
    previous = store->entries[index].mask;
    candidate |= previous;
    if (candidate != previous) {
        store->entries[index].mask = candidate;
        store->changed = true;
    }
    if (storage_unproved && !store->entries[index].array_storage_unproved) {
        store->entries[index].array_storage_unproved = true;
        store->changed = true;
    }
    store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
    store->body_evaluations++;
    return store->entries[index].mask;
}

unsigned
function_param_flow_summary_demand(const SlotFunctionLookup *lookup,
                                   ASTNode *function_decl,
                                   size_t param_index)
{
    FunctionParamFlowSummaryStore *store;
    uint32_t function_id;
    size_t existing;
    size_t root_index;
    size_t passes = 0;

    if (lookup == NULL || lookup->ctx == NULL || function_decl == NULL
        || function_decl->type != AST_FUNC_DECL
        || param_index >= ast_func_param_count(function_decl)) {
        return SLOT_PARAM_SUMMARY_ALL;
    }
    if (!semantic_param_publication_inputs_ready(lookup->ctx) ||
        !semantic_param_publication_routine_inputs_current(lookup->ctx, function_decl, true)) {
        semantic_error(lookup->ctx, function_decl, "Parameter flow demand requires sealed typed inputs");
        return SLOT_PARAM_SUMMARY_ALL;
    }
    store = function_param_flow_store_get(lookup);
    if (store == NULL)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->failed)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->program_root != lookup->program_root) {
        function_param_flow_fail(store, function_decl,
            "summary program owner changed during semantic analysis");
        return SLOT_PARAM_SUMMARY_ALL;
    }

    function_id = ast_node_stable_id(function_decl);
    existing = function_param_flow_find(store, function_id, param_index);
    if (existing != SIZE_MAX) {
        FunctionParamFlowProgramPointIndex *cached;
        if (store->entries[existing].state == FUNCTION_PARAM_FLOW_COMPLETE &&
            !function_param_flow_program_point_index_build(store, function_decl, &cached)) return SLOT_PARAM_SUMMARY_ALL;
        if (store->solving)
            return function_param_flow_demand_index(
                store, function_decl, param_index);
        if (store->entries[existing].state == FUNCTION_PARAM_FLOW_COMPLETE) {
            store->cache_hits++;
            return store->entries[existing].mask;
        }
    }

    if (store->solving)
        return function_param_flow_demand_index(store, function_decl,
                                                param_index);

    store->active_start = store->count;
    root_index = function_param_flow_add(store, function_decl, param_index);
    if (root_index == SIZE_MAX) {
        function_param_flow_fail(store, function_decl,
            function_id == 0
                ? "function stable identity is missing"
                : "summary hash allocation failed");
        return SLOT_PARAM_SUMMARY_ALL;
    }

    if (store->demand_epoch == SIZE_MAX) {
        function_param_flow_fail(store, function_decl, "summary demand identity overflow");
        return SLOT_PARAM_SUMMARY_ALL;
    }
    store->demand_epoch++;
    store->admitted_no_store_count = 0;
    store->solving = true;
    store->active_had_recursion = false;
    store->settling_pending = false;
    /* Bound this demand closure, not the lifetime of the compilation. Completed
     * unrelated summaries remain cached; recursive revisits in this episode
     * still share the same cap. Body-evaluation telemetry stays cumulative. */
    store->work_units = 0;

    do {
        store->changed = false;
        for (size_t i = store->active_start; i < store->count; i++)
            store->entries[i].state = FUNCTION_PARAM_FLOW_UNSEEN;
        for (size_t i = store->active_start; i < store->count; i++)
            (void)function_param_flow_evaluate(store, i);
        passes++;
        store->fixed_point_passes++;
        if (store->failed)
            break;
        if (passes > (store->count - store->active_start + 1) * 7 + 1) {
            function_param_flow_fail(store, function_decl,
                "recursive summary fixed point did not converge");
            for (size_t i = store->active_start; i < store->count; i++)
                store->entries[i].mask = SLOT_PARAM_SUMMARY_ALL;
            break;
        }
        if (!store->changed && store->active_had_recursion && !store->settling_pending)
            for (size_t i = store->active_start; i < store->count; i++)
                if (store->entries[i].returned_backing_pending) {
                    store->settling_pending = true; store->changed = true; break;
                }
    } while (store->active_had_recursion && store->changed);

    if (store->failed) {
        for (size_t i = store->active_start; i < store->count; i++)
            store->entries[i].mask = SLOT_PARAM_SUMMARY_ALL;
    }
    for (size_t i = store->active_start; i < store->count; i++)
        store->entries[i].state = FUNCTION_PARAM_FLOW_COMPLETE;
    store->solving = false;
    if (store->failed)
        return SLOT_PARAM_SUMMARY_ALL;
    return store->entries[root_index].mask;
}

unsigned
function_param_flow_summary_for_param(SemanticContext *ctx,
                                      ASTNode *function_decl,
                                      size_t param_index)
{
    SlotFunctionLookup lookup = {ctx, ctx != NULL ? ctx->program_root : NULL};

    return function_param_flow_summary_demand(&lookup, function_decl,
                                              param_index);
}

unsigned function_param_flow_summary_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t ordinal)
{
    if (lookup == NULL) return SLOT_PARAM_SUMMARY_ALL;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    return decl == NULL ? SLOT_PARAM_SUMMARY_ALL :
        function_param_flow_summary_demand(lookup, decl, ordinal);
}

bool function_param_flow_output_dependency_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t source_ordinal, size_t output_ordinal, bool *dependent)
{
    if (lookup == NULL || dependent == NULL ||
        !semantic_param_publication_checked_argument(lookup->ctx, call, output_ordinal) ||
        !semantic_param_publication_value(lookup->ctx, ast_call_argument(call, source_ordinal)).ok ||
        !semantic_param_publication_value(lookup->ctx, ast_call_argument(call, output_ordinal)).ok) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, source_ordinal);
    if (decl == NULL || output_ordinal >= ast_func_param_count(decl)) return false;
    size_t ordinals[2] = {source_ordinal, output_ordinal};
    for (size_t i = 0; i < 2; i++) {
        ASTNode *root = ast_call_argument(call, ordinals[i]);
        while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
        if (root == NULL || !semantic_param_publication_value(lookup->ctx, root).ok) return false;
    }
    (void)function_param_flow_summary_demand(lookup, decl, source_ordinal);
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return false;
    size_t index = function_param_flow_find(store, ast_node_stable_id(decl), source_ordinal);
    if (index == SIZE_MAX || store->entries[index].state == FUNCTION_PARAM_FLOW_COMPUTING ||
        !store->entries[index].output_dependencies_ready ||
        output_ordinal >= store->entries[index].output_count) return false;
    if (!function_param_flow_backing_rows_current(store, decl)) return false;
    *dependent = store->entries[index].output_dependencies[output_ordinal];
    return true;
}

bool
function_param_flow_preserves_array_storage(SemanticContext *ctx,
                                           ASTNode *function_decl,
                                           size_t param_index)
{
    FunctionParamFlowSummaryStore *store;
    size_t index;
    FuncParam *param = ast_func_param(function_decl, param_index);

    if (ctx == NULL || function_decl == NULL || param == NULL
        || ast_func_body(function_decl) == NULL || function_decl->is_async_decl
        || param->mode == PARAM_MODE_OWN || param->type == NULL
        || ast_type_name(param->type) == NULL
        || strcmp(ast_type_name(param->type), "Array") != 0)
        return false;
    (void)function_param_flow_summary_for_param(ctx, function_decl, param_index);
    store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed)
        return false;
    index = function_param_flow_find(store, ast_node_stable_id(function_decl), param_index);
    return index != SIZE_MAX && !store->entries[index].array_storage_unproved;
}
