/* Resource-flow snapshot merge owner.
 * Alternative CFG paths require Future lifecycle agreement. Parallel arms all
 * execute, so one arm's retirement contributes to the post-join state. */

#include <stdint.h>

#include "type_checker_flow_internal.h"
#include "type_checker_flow_universe.h"

static PgyFutureLifecycleState
merge_future_state(PgyFutureLifecycleState left,
                   PgyFutureLifecycleState right,
                   bool parallel_join)
{
    if (left == right)
        return left;
    if (left == PGY_FUTURE_LIFECYCLE_DIVERGED
        || right == PGY_FUTURE_LIFECYCLE_DIVERGED) {
        return PGY_FUTURE_LIFECYCLE_DIVERGED;
    }
    if (parallel_join) {
        if (left == PGY_FUTURE_LIFECYCLE_RETIRED
            || right == PGY_FUTURE_LIFECYCLE_RETIRED) {
            return PGY_FUTURE_LIFECYCLE_RETIRED;
        }
        if (left == PGY_FUTURE_LIFECYCLE_LIVE
            || right == PGY_FUTURE_LIFECYCLE_LIVE) {
            return PGY_FUTURE_LIFECYCLE_LIVE;
        }
        return PGY_FUTURE_LIFECYCLE_NONE;
    }
    return PGY_FUTURE_LIFECYCLE_DIVERGED;
}

static void
merge_resource_states(ResourceConsumeSnapshot *dst,
                      const ResourceConsumeSnapshot *src,
                      bool parallel_join)
{
    if (dst == NULL || src == NULL)
        return;
    if (!dst->valid || !src->valid || dst->count != src->count) {
        dst->valid = false;
        return;
    }
    if (dst->slice_permission_count != src->slice_permission_count
        || (dst->slice_permission_count != 0
            && (dst->slice_permissions == NULL || src->slice_permissions == NULL))) {
        dst->valid = false;
        return;
    }
    for (size_t i = 0; i < dst->slice_permission_count; i++) {
        SlicePermissionState *left = &dst->slice_permissions[i];
        const SlicePermissionState *right = &src->slice_permissions[i];
        if (left->symbol == NULL || left->symbol != right->symbol
            || left->declaration_id != right->declaration_id
            || left->symbol->decl_syntax_id != left->declaration_id) {
            dst->valid = false;
            return;
        }
        /* Both paths appended to this live binding's lexical loan union. */
        left->sources = left->symbol->slice_borrow_sources;
    }
    if ((dst->count > 0
            && (dst->symbols == NULL || dst->states == NULL
                || dst->used_states == NULL || dst->future_states == NULL
                || dst->access_masks == NULL
                || dst->slot_states == NULL || dst->sem_states == NULL
                || dst->pool_ids == NULL || dst->array_storage_states == NULL))
        || (src->count > 0
            && (src->symbols == NULL || src->states == NULL
                || src->used_states == NULL || src->future_states == NULL
                || src->access_masks == NULL
                || src->slot_states == NULL || src->sem_states == NULL
                || src->pool_ids == NULL || src->array_storage_states == NULL))) {
        dst->valid = false;
        return;
    }

    for (size_t i = 0; i < dst->count; i++) {
        size_t src_index = SIZE_MAX;
        for (size_t j = 0; j < src->count; j++) {
            bool identity_matches;
            if (src->symbol_indices != NULL && dst->symbol_indices != NULL
                && src->symbol_indices[j] != RESOURCE_FLOW_INDEX_NONE
                && dst->symbol_indices[i] != RESOURCE_FLOW_INDEX_NONE) {
                identity_matches =
                    src->symbol_indices[j] == dst->symbol_indices[i];
            } else {
                identity_matches = src->symbols[j] == dst->symbols[i];
            }
            if (identity_matches) {
                src_index = j;
                break;
            }
        }
        if (src_index == SIZE_MAX) {
            dst->valid = false;
            return;
        }

        dst->states[i] = dst->states[i] || src->states[src_index];
        dst->used_states[i] = dst->used_states[i]
            || src->used_states[src_index];
        dst->future_states[i] = merge_future_state(
            dst->future_states[i], src->future_states[src_index],
            parallel_join);
        dst->access_masks[i] |= src->access_masks[src_index];
        if (src->slot_states[src_index] > dst->slot_states[i])
            dst->slot_states[i] = src->slot_states[src_index];
        if (src->sem_states[src_index] > dst->sem_states[i])
            dst->sem_states[i] = src->sem_states[src_index];
        if (dst->pool_ids[i] < 0)
            dst->pool_ids[i] = src->pool_ids[src_index];
        /* Every live path must own the backing; one escape or handoff blocks it. */
        dst->array_storage_states[i].exclusive &=
            src->array_storage_states[src_index].exclusive;
        dst->array_storage_states[i].escaped |=
            src->array_storage_states[src_index].escaped;
        dst->array_storage_states[i].pending |=
            src->array_storage_states[src_index].pending;
        dst->array_storage_states[i].result_pending |=
            src->array_storage_states[src_index].result_pending;
        ArrayStoragePathState *left = &dst->array_storage_states[i];
        const ArrayStoragePathState *right =
            &src->array_storage_states[src_index];
        if (left->return_elements_present
                != right->return_elements_present) {
            dst->valid = false;
            return;
        }
        if (left->return_elements_present) {
            left->return_elements_owned &= right->return_elements_owned;
            if (left->return_elements_source
                    != right->return_elements_source
                || left->return_elements_source_id
                    != right->return_elements_source_id) {
                left->return_elements_owned = false;
                left->return_elements_source = NULL;
                left->return_elements_source_id = 0;
            }
        }
        ArrayStorageMemberElementPremiseState *left_member =
            &left->member_elements;
        const ArrayStorageMemberElementPremiseState *right_member =
            &right->member_elements;
        if (left_member->present != right_member->present) {
            dst->valid = false;
            return;
        }
        if (left_member->present) {
            if (left_member->source != right_member->source
                || left_member->source_id != right_member->source_id
                || left_member->root_binding_id
                    != right_member->root_binding_id
                || left_member->leaf_field_id
                    != right_member->leaf_field_id
                || left_member->source_birth_serial
                    != right_member->source_birth_serial) {
                dst->valid = false;
                return;
            }
            left_member->current &= right_member->current;
        }
    }
}

void
merge_resource_states_or(ResourceConsumeSnapshot *dst,
                         const ResourceConsumeSnapshot *src)
{
    merge_resource_states(dst, src, false);
}

void
merge_resource_states_parallel(ResourceConsumeSnapshot *dst,
                               const ResourceConsumeSnapshot *src)
{
    merge_resource_states(dst, src, true);
}
