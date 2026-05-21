#ifndef PERGYRA_TYPE_CHECKER_FLOW_RESOURCES_H
#define PERGYRA_TYPE_CHECKER_FLOW_RESOURCES_H

#include <stdbool.h>
#include <stdint.h>

#include "type_checker_internal.h"

typedef struct
{
    Symbol              **symbols;
    bool                 *states;
    bool                 *used_states;
    uint8_t              *access_masks;
    SlotState            *slot_states;
    QubitSemanticState   *sem_states;
    int32_t              *pool_ids;
    size_t                count;
    size_t                capacity;
    bool                  valid;
} ResourceConsumeSnapshot;

ResourceConsumeSnapshot snapshot_resource_states_from_scope(Scope *scope,
                                                            SemanticContext *ctx);
ResourceConsumeSnapshot snapshot_resource_states(SemanticContext *ctx);
void restore_resource_states(const ResourceConsumeSnapshot *snap);
void merge_resource_states_or(ResourceConsumeSnapshot *dst,
                              const ResourceConsumeSnapshot *src);
bool resource_snapshot_has_parallel_conflict(const ResourceConsumeSnapshot *base,
                                             const ResourceConsumeSnapshot *joined,
                                             const ResourceConsumeSnapshot *task,
                                             const Symbol **symbol_out);
bool resource_snapshot_has_parallel_race_risk(const ResourceConsumeSnapshot *base,
                                              const ResourceConsumeSnapshot *joined,
                                              const ResourceConsumeSnapshot *task,
                                              const Symbol **symbol_out);
void destroy_resource_snapshot(ResourceConsumeSnapshot *snap);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_RESOURCES_H */
