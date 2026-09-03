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
    if ((dst->count > 0
            && (dst->symbols == NULL || dst->states == NULL
                || dst->used_states == NULL || dst->future_states == NULL
                || dst->access_masks == NULL
                || dst->slot_states == NULL || dst->sem_states == NULL
                || dst->pool_ids == NULL))
        || (src->count > 0
            && (src->symbols == NULL || src->states == NULL
                || src->used_states == NULL || src->future_states == NULL
                || src->access_masks == NULL
                || src->slot_states == NULL || src->sem_states == NULL
                || src->pool_ids == NULL))) {
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
