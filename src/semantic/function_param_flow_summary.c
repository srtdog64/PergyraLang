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

#define FUNCTION_PARAM_FLOW_WORK_BUDGET 4096u

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

static bool
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
        return true;
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
    if (body == NULL) {
        *index_out = index;
        return true;
    }

    if (body->type == AST_BLOCK) {
        for (size_t s = 0; s < ast_block_statement_count(body); s++) {
            ASTNode *statement = ast_block_statement(body, s);
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

    *index_out = index;
    return true;
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
    store->entries[index].function_id = function_id;
    store->entries[index].param_index = param_index;
    store->entries[index].function_decl = function_decl;
    store->entries[index].mask = SLOT_PARAM_SUMMARY_NONE;
    store->entries[index].state = FUNCTION_PARAM_FLOW_UNSEEN;

    slot = function_param_flow_key_hash(function_id, param_index)
        & (store->hash_capacity - 1);
    while (store->hash[slot] != 0)
        slot = (slot + 1) & (store->hash_capacity - 1);
    store->hash[slot] = index + 1;
    return index;
}

static void
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
    SlotSummaryOrigin origin;
    SlotFunctionLookup lookup;
    unsigned candidate;
    unsigned previous;

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
            "parameter program-point index allocation failed");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }
    roots = program_points->roots_by_param[store->entries[index].param_index];
    root_count = program_points->root_counts[store->entries[index].param_index];
    candidate = slot_param_summary_in_program_points(
        roots, root_count, param->name, &lookup, 0, &origin);

    /* Recursive demands may grow the entries array, so reacquire by index. */
    previous = store->entries[index].mask;
    candidate |= previous;
    if (candidate != previous) {
        store->entries[index].mask = candidate;
        store->changed = true;
    }
    store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
    store->body_evaluations++;
    return store->entries[index].mask;
}

static FunctionParamFlowSummaryStore *
function_param_flow_store_get(const SlotFunctionLookup *lookup)
{
    FunctionParamFlowSummaryStore *store;
    SemanticContext *ctx;

    if (lookup == NULL || lookup->ctx == NULL || lookup->program_root == NULL)
        return NULL;
    ctx = lookup->ctx;
    if (ctx->function_param_flow_summaries != NULL)
        return ctx->function_param_flow_summaries;
    store = calloc(1, sizeof(*store));
    if (store == NULL) {
        semantic_error(ctx, lookup->program_root,
            "Function parameter flow summary store allocation failed");
        return NULL;
    }
    store->ctx = ctx;
    store->program_root = lookup->program_root;
    store->work_budget = FUNCTION_PARAM_FLOW_WORK_BUDGET;
    ctx->function_param_flow_summaries = store;
    return store;
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

    store->solving = true;
    store->active_had_recursion = false;

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
        if (passes > (store->count - store->active_start + 1) * 6 + 1) {
            function_param_flow_fail(store, function_decl,
                "recursive summary fixed point did not converge");
            for (size_t i = store->active_start; i < store->count; i++)
                store->entries[i].mask = SLOT_PARAM_SUMMARY_ALL;
            break;
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
