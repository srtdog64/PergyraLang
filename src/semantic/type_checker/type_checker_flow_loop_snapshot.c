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
        && count <= SIZE_MAX / sizeof(int32_t)
        && count <= SIZE_MAX / sizeof(ArrayStoragePathState);
}

static bool
member_element_states_equal(
    const ArrayStorageMemberElementPremiseState *left,
    const ArrayStorageMemberElementPremiseState *right)
{
    return left->present == right->present
        && left->current == right->current
        && left->source == right->source
        && left->source_id == right->source_id
        && left->root_binding_id == right->root_binding_id
        && left->leaf_field_id == right->leaf_field_id
        && left->source_birth_serial == right->source_birth_serial;
}

static bool
slice_permission_states_equal(const ResourceConsumeSnapshot *a,
                              const ResourceConsumeSnapshot *b)
{
    if (a->slice_permission_count != b->slice_permission_count)
        return false;
    if (a->slice_permission_count != 0
        && (a->slice_permissions == NULL || b->slice_permissions == NULL))
        return false;
    for (size_t i = 0; i < a->slice_permission_count; i++) {
        const SlicePermissionState *left = &a->slice_permissions[i];
        const SlicePermissionState *right = &b->slice_permissions[i];
        if (left->symbol != right->symbol
            || left->declaration_id != right->declaration_id
            || left->sources != right->sources)
            return false;
    }
    return true;
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
    if (!slice_permission_states_equal(a, b))
        return false;
    if (a->count > 0 && (a->array_storage_states == NULL
                         || b->array_storage_states == NULL))
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
        if (a->array_storage_states[i].exclusive != b->array_storage_states[i].exclusive
            || a->array_storage_states[i].escaped != b->array_storage_states[i].escaped
            || a->array_storage_states[i].pending != b->array_storage_states[i].pending
            || a->array_storage_states[i].result_pending != b->array_storage_states[i].result_pending
            || a->array_storage_states[i].return_elements_present
                != b->array_storage_states[i].return_elements_present
            || a->array_storage_states[i].return_elements_owned
                != b->array_storage_states[i].return_elements_owned
            || a->array_storage_states[i].return_elements_source
                != b->array_storage_states[i].return_elements_source
            || a->array_storage_states[i].return_elements_source_id
                != b->array_storage_states[i].return_elements_source_id
            || !member_element_states_equal(
                &a->array_storage_states[i].member_elements,
                &b->array_storage_states[i].member_elements))
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
    if (!slice_permission_states_equal(a, b))
        return false;
    if (a->count > 0 && (a->array_storage_states == NULL
                         || b->array_storage_states == NULL))
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
            || a->pool_ids[i] != b->pool_ids[i]
            || a->array_storage_states[i].exclusive != b->array_storage_states[i].exclusive
            || a->array_storage_states[i].escaped != b->array_storage_states[i].escaped
            || a->array_storage_states[i].pending != b->array_storage_states[i].pending
            || a->array_storage_states[i].result_pending != b->array_storage_states[i].result_pending
            || a->array_storage_states[i].return_elements_present
                != b->array_storage_states[i].return_elements_present
            || a->array_storage_states[i].return_elements_owned
                != b->array_storage_states[i].return_elements_owned
            || a->array_storage_states[i].return_elements_source
                != b->array_storage_states[i].return_elements_source
            || a->array_storage_states[i].return_elements_source_id
                != b->array_storage_states[i].return_elements_source_id
            || !member_element_states_equal(
                &a->array_storage_states[i].member_elements,
                &b->array_storage_states[i].member_elements)) {
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
    dst.context = src->context;
    if (!src->valid)
        return dst;
    if (src->slice_permission_count != 0) {
        if (src->slice_permissions == NULL
            || src->slice_permission_count > SIZE_MAX / sizeof(SlicePermissionState)) {
            dst.valid = false;
            return dst;
        }
        dst.slice_permissions = malloc(src->slice_permission_count
            * sizeof(SlicePermissionState));
        if (dst.slice_permissions == NULL) {
            dst.valid = false;
            return dst;
        }
        memcpy(dst.slice_permissions, src->slice_permissions,
            src->slice_permission_count * sizeof(SlicePermissionState));
        dst.slice_permission_count = src->slice_permission_count;
    }
    if (src->count == 0)
        return dst;
    if (!resource_snapshot_count_fits(src->count)
        || src->array_storage_states == NULL) {
        destroy_resource_snapshot(&dst);
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
    dst.array_storage_states = calloc(src->count,
                                      sizeof(ArrayStoragePathState));
    if (dst.symbols == NULL || dst.symbol_indices == NULL
        || dst.states == NULL
        || dst.used_states == NULL || dst.future_states == NULL
        || dst.access_masks == NULL
        || dst.slot_states == NULL
        || dst.sem_states == NULL || dst.pool_ids == NULL
        || dst.array_storage_states == NULL) {
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
    memcpy(dst.array_storage_states, src->array_storage_states,
           src->count * sizeof(ArrayStoragePathState));
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
