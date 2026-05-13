#include <stdint.h>
#include <stdlib.h>

#include "type_checker_resolution_metadata_internal.h"

static size_t
metadata_key_hash(const void *key)
{
    uintptr_t value = (uintptr_t)key;

    value >>= 3;
    value ^= value >> 16;
    value *= (uintptr_t)0x7feb352dU;
    value ^= value >> 15;
    return (size_t)value;
}

static bool
metadata_index_capacity_is_valid(size_t capacity)
{
    return capacity != 0 && (capacity & (capacity - 1)) == 0;
}

static bool
metadata_index_insert_raw(void **keys,
                          size_t *entries,
                          size_t capacity,
                          void *key,
                          size_t entry_index)
{
    size_t mask;
    size_t slot;

    if (keys == NULL || entries == NULL || key == NULL
        || !metadata_index_capacity_is_valid(capacity)) {
        return false;
    }

    mask = capacity - 1;
    slot = metadata_key_hash(key) & mask;
    for (size_t probe = 0; probe < capacity; probe++) {
        void *existing = keys[slot];
        if (existing == NULL || existing == key) {
            keys[slot] = key;
            entries[slot] = entry_index + 1;
            return true;
        }
        slot = (slot + 1) & mask;
    }
    return false;
}

bool
metadata_index_insert(SemanticContext *ctx, void *key, size_t entry_index)
{
    if (ctx == NULL || key == NULL
        || ctx->type_resolution_metadata.index_capacity == 0) {
        return false;
    }
    return metadata_index_insert_raw(
        ctx->type_resolution_metadata.index_keys,
        ctx->type_resolution_metadata.index_entries,
        ctx->type_resolution_metadata.index_capacity,
        key,
        entry_index);
}

static bool
metadata_rebuild_index(SemanticContext *ctx, size_t capacity)
{
    void **keys;
    size_t *entries;

    if (ctx == NULL || !metadata_index_capacity_is_valid(capacity))
        return false;

    keys = calloc(capacity, sizeof(void *));
    entries = calloc(capacity, sizeof(size_t));
    if (keys == NULL || entries == NULL) {
        free(keys);
        free(entries);
        return false;
    }

    for (size_t i = 0; i < ctx->type_resolution_metadata.count; i++) {
        if (!metadata_index_insert_raw(keys,
                                       entries,
                                       capacity,
                                       ctx->type_resolution_metadata.keys[i],
                                       i)) {
            free(keys);
            free(entries);
            return false;
        }
    }

    free(ctx->type_resolution_metadata.index_keys);
    free(ctx->type_resolution_metadata.index_entries);
    ctx->type_resolution_metadata.index_keys = keys;
    ctx->type_resolution_metadata.index_entries = entries;
    ctx->type_resolution_metadata.index_capacity = capacity;
    return true;
}

bool
metadata_ensure_index_capacity(SemanticContext *ctx, size_t next_count)
{
    size_t new_capacity;

    if (ctx == NULL)
        return false;
    if (ctx->type_resolution_metadata.index_capacity != 0
        && !metadata_index_capacity_is_valid(
            ctx->type_resolution_metadata.index_capacity)) {
        return false;
    }
    if (ctx->type_resolution_metadata.index_capacity != 0
        && next_count < ctx->type_resolution_metadata.index_capacity / 2) {
        return true;
    }

    if (ctx->type_resolution_metadata.index_capacity == 0) {
        new_capacity = 256;
    } else {
        if (ctx->type_resolution_metadata.index_capacity > SIZE_MAX / 2)
            return false;
        new_capacity = ctx->type_resolution_metadata.index_capacity * 2;
    }
    while (next_count >= new_capacity / 2) {
        if (new_capacity > SIZE_MAX / 2)
            return false;
        new_capacity *= 2;
    }
    return metadata_rebuild_index(ctx, new_capacity);
}

bool
metadata_lookup_entry_index(SemanticContext *ctx, ASTNode *type_node,
                            size_t *out_index)
{
    size_t mask;
    size_t slot;
    void *key = (void *)type_node;

    if (out_index != NULL)
        *out_index = 0;
    if (ctx == NULL || type_node == NULL)
        return false;
    if (ctx->type_resolution_metadata.count == 0)
        return false;
    if (ctx->type_resolution_metadata.index_capacity == 0
        && !metadata_ensure_index_capacity(ctx,
                                           ctx->type_resolution_metadata.count)) {
        return false;
    }
    if (!metadata_index_capacity_is_valid(
            ctx->type_resolution_metadata.index_capacity)) {
        return false;
    }

    mask = ctx->type_resolution_metadata.index_capacity - 1;
    slot = metadata_key_hash(key) & mask;
    for (size_t probe = 0;
         probe < ctx->type_resolution_metadata.index_capacity;
         probe++) {
        void *existing = ctx->type_resolution_metadata.index_keys[slot];
        if (existing == NULL)
            return false;
        if (existing == key) {
            size_t entry = ctx->type_resolution_metadata.index_entries[slot];
            if (entry == 0)
                return false;
            if (out_index != NULL)
                *out_index = entry - 1;
            return true;
        }
        slot = (slot + 1) & mask;
    }
    return false;
}
