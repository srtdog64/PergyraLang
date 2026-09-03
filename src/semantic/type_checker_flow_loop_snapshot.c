/*
 * Copyright (c) 2026 Pergyra Language Project
 * CFG loop resource snapshot ownership.
 */

#include <stdlib.h>
#include <string.h>

#include "type_checker_flow_internal.h"

static bool
resource_snapshot_count_fits(size_t count)
{
    return count <= SIZE_MAX / sizeof(Symbol *)
        && count <= SIZE_MAX / sizeof(size_t)
        && count <= SIZE_MAX / sizeof(bool)
        && count <= SIZE_MAX / sizeof(PgyFutureLifecycleState)
        && count <= SIZE_MAX / sizeof(uint8_t)
        && count <= SIZE_MAX / sizeof(SlotState)
        && count <= SIZE_MAX / sizeof(QubitSemanticState)
        && count <= SIZE_MAX / sizeof(int32_t);
}

bool
resource_snapshots_equal(const ResourceConsumeSnapshot *a,
                         const ResourceConsumeSnapshot *b)
{
    if (a == NULL || b == NULL)
        return a == b;
    if (!a->valid || !b->valid)
        return false;
    if (a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; i++) {
        if (a->symbol_indices != NULL && b->symbol_indices != NULL
            && a->symbol_indices[i] != (size_t)-1
            && b->symbol_indices[i] != (size_t)-1) {
            if (a->symbol_indices[i] != b->symbol_indices[i])
                return false;
        } else if (a->symbols[i] != b->symbols[i])
            return false;
        if (a->states[i] != b->states[i])
            return false;
        if (a->used_states[i] != b->used_states[i])
            return false;
        if (a->future_states[i] != b->future_states[i])
            return false;
        if (a->access_masks[i] != b->access_masks[i])
            return false;
        if (a->slot_states[i] != b->slot_states[i])
            return false;
        if (a->sem_states[i] != b->sem_states[i])
            return false;
        if (a->pool_ids[i] != b->pool_ids[i])
            return false;
    }
    return true;
}

bool
resource_snapshot_availability_equal(const ResourceConsumeSnapshot *a,
                                     const ResourceConsumeSnapshot *b)
{
    if (a == NULL || b == NULL)
        return a == b;
    if (!a->valid || !b->valid || a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; i++) {
        if (a->symbol_indices != NULL && b->symbol_indices != NULL
            && a->symbol_indices[i] != (size_t)-1
            && b->symbol_indices[i] != (size_t)-1) {
            if (a->symbol_indices[i] != b->symbol_indices[i])
                return false;
        } else if (a->symbols[i] != b->symbols[i]) {
            return false;
        }
        if (a->states[i] != b->states[i]
            || a->future_states[i] != b->future_states[i]
            || a->slot_states[i] != b->slot_states[i]
            || a->sem_states[i] != b->sem_states[i]
            || a->pool_ids[i] != b->pool_ids[i]) {
            return false;
        }
    }
    return true;
}

ResourceConsumeSnapshot
copy_resource_snapshot(const ResourceConsumeSnapshot *src)
{
    ResourceConsumeSnapshot dst = {0};
    if (src == NULL)
        return dst;
    dst.valid = src->valid;
    if (!src->valid)
        return dst;
    if (src->count == 0)
        return dst;
    if (!resource_snapshot_count_fits(src->count)) {
        dst.valid = false;
        return dst;
    }

    dst.symbols = calloc(src->count, sizeof(Symbol *));
    dst.symbol_indices = calloc(src->count, sizeof(size_t));
    dst.states = calloc(src->count, sizeof(bool));
    dst.used_states = calloc(src->count, sizeof(bool));
    dst.future_states = calloc(src->count,
                               sizeof(PgyFutureLifecycleState));
    dst.access_masks = calloc(src->count, sizeof(uint8_t));
    dst.slot_states = calloc(src->count, sizeof(SlotState));
    dst.sem_states = calloc(src->count, sizeof(QubitSemanticState));
    dst.pool_ids = calloc(src->count, sizeof(int32_t));
    if (dst.symbols == NULL || dst.symbol_indices == NULL
        || dst.states == NULL
        || dst.used_states == NULL || dst.future_states == NULL
        || dst.access_masks == NULL
        || dst.slot_states == NULL
        || dst.sem_states == NULL || dst.pool_ids == NULL) {
        destroy_resource_snapshot(&dst);
        dst.valid = false;
        return dst;
    }

    memcpy(dst.symbols, src->symbols, src->count * sizeof(Symbol *));
    memcpy(dst.symbol_indices, src->symbol_indices,
           src->count * sizeof(size_t));
    memcpy(dst.states, src->states, src->count * sizeof(bool));
    memcpy(dst.used_states, src->used_states, src->count * sizeof(bool));
    memcpy(dst.future_states, src->future_states,
           src->count * sizeof(PgyFutureLifecycleState));
    memcpy(dst.access_masks, src->access_masks, src->count * sizeof(uint8_t));
    memcpy(dst.slot_states, src->slot_states, src->count * sizeof(SlotState));
    memcpy(dst.sem_states, src->sem_states,
           src->count * sizeof(QubitSemanticState));
    memcpy(dst.pool_ids, src->pool_ids, src->count * sizeof(int32_t));
    dst.count = src->count;
    dst.capacity = src->count;
    return dst;
}

void
merge_resource_snapshots_or(ResourceConsumeSnapshot *dst,
                            bool *dst_initialized,
                            const ResourceConsumeSnapshot *src)
{
    if (dst == NULL || dst_initialized == NULL || src == NULL)
        return;

    if (!*dst_initialized) {
        ResourceConsumeSnapshot copy = copy_resource_snapshot(src);
        if (!copy.valid || (src->count > 0 && copy.count != src->count)) {
            destroy_resource_snapshot(&copy);
            dst->valid = false;
            *dst_initialized = true;
            return;
        }
        *dst = copy;
        *dst_initialized = true;
        return;
    }

    merge_resource_states_or(dst, src);
}

void
merge_resource_snapshots_parallel(ResourceConsumeSnapshot *dst,
                                  bool *dst_initialized,
                                  const ResourceConsumeSnapshot *src)
{
    if (dst == NULL || dst_initialized == NULL || src == NULL)
        return;
    if (!*dst_initialized) {
        ResourceConsumeSnapshot copy = copy_resource_snapshot(src);
        if (!copy.valid || (src->count > 0 && copy.count != src->count)) {
            destroy_resource_snapshot(&copy);
            dst->valid = false;
            *dst_initialized = true;
            return;
        }
        *dst = copy;
        *dst_initialized = true;
        return;
    }
    merge_resource_states_parallel(dst, src);
}

void
loop_flow_record(LoopFlowState *loop_flow,
                 bool is_break,
                 const ResourceConsumeSnapshot *state)
{
    if (loop_flow == NULL || state == NULL)
        return;

    if (is_break) {
        merge_resource_snapshots_or(&loop_flow->break_states,
                                    &loop_flow->has_break_states,
                                    state);
        return;
    }

    merge_resource_snapshots_or(&loop_flow->continue_states,
                                &loop_flow->has_continue_states,
                                state);
}
