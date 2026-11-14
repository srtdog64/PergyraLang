/* The summary store's entry table: one entry per (routine, formal),
 * found through an open-addressed hash of their stable identities. */

#include <stdint.h>
#include <stdlib.h>

#include "function_param_flow_summary_internal.h"

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

size_t
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


size_t
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
