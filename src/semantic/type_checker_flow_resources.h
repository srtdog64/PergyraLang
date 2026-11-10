#ifndef PERGYRA_TYPE_CHECKER_FLOW_RESOURCES_H
#define PERGYRA_TYPE_CHECKER_FLOW_RESOURCES_H

#include <stdbool.h>
#include <stdint.h>

#include "type_checker_internal.h"
#include "array_storage_element_lifetime_owner.h"

/* Path-local Array authority; the pending-call sequence ledger is separate. */
typedef struct
{
    bool exclusive;
    bool escaped;
    bool pending;
    bool result_pending;
    bool return_elements_present;
    bool return_elements_owned;
    const ASTNode *return_elements_source;
    uint32_t return_elements_source_id;
    ArrayStorageMemberElementPremiseState member_elements;
} ArrayStoragePathState;

typedef struct {
    Symbol *symbol;
    uint32_t declaration_id;
    bool invalidated;
} DeferredStringInvalidationState;

/* Origin lists grow monotonically during one scope's check. The head is an
 * identity token only; a snapshot never owns or dereferences the list. */
typedef struct {
    Symbol *symbol;
    uint32_t declaration_id;
    const SliceBorrowSource *sources;
} SlicePermissionState;

typedef struct
{
    Symbol              **symbols;
    size_t               *symbol_indices;
    bool                 *states;
    bool                 *used_states;
    PgyFutureLifecycleState *future_states;
    uint8_t              *access_masks;
    SlotState            *slot_states;
    QubitSemanticState   *sem_states;
    int32_t              *pool_ids;
    ArrayStoragePathState *array_storage_states;
    size_t                count;
    size_t                capacity;
    bool                  valid;
    SemanticContext       *context;
    DeferredStringInvalidationState *deferred_strings;
    size_t deferred_string_count, deferred_string_capacity;
    bool deferred_strings_captured;
    SlicePermissionState *slice_permissions;
    size_t slice_permission_count;
} ResourceConsumeSnapshot;

ResourceConsumeSnapshot snapshot_resource_states_from_scope(Scope *scope,
                                                            SemanticContext *ctx);
ResourceConsumeSnapshot snapshot_resource_states(SemanticContext *ctx);
bool resource_snapshot_capture_deferred_strings(ResourceConsumeSnapshot *snap,
    SemanticContext *ctx);
bool resource_snapshot_restore_deferred_strings(const ResourceConsumeSnapshot *snap,
    SemanticContext *ctx);
void resource_snapshot_join_deferred_strings(const ResourceConsumeSnapshot *snap, bool *joined);
void restore_resource_states(const ResourceConsumeSnapshot *snap);
void restore_resource_states_for_context(const ResourceConsumeSnapshot *snap,
                                         SemanticContext *ctx);
void merge_resource_states_or(ResourceConsumeSnapshot *dst,
                              const ResourceConsumeSnapshot *src);
void merge_resource_states_parallel(ResourceConsumeSnapshot *dst,
                                    const ResourceConsumeSnapshot *src);
bool resource_snapshot_has_parallel_conflict(const ResourceConsumeSnapshot *base,
                                             const ResourceConsumeSnapshot *joined,
                                             const ResourceConsumeSnapshot *task,
                                             const Symbol **symbol_out);
bool resource_snapshot_has_parallel_race_risk(const ResourceConsumeSnapshot *base,
                                              const ResourceConsumeSnapshot *joined,
                                              const ResourceConsumeSnapshot *task,
                                              const Symbol **symbol_out);
void resource_snapshot_record_parallel_boundary_witness(
    const ResourceConsumeSnapshot *base,
    const ResourceConsumeSnapshot *joined,
    const ResourceConsumeSnapshot *task,
    SemanticContext *ctx);
void destroy_resource_snapshot(ResourceConsumeSnapshot *snap);

#endif /* PERGYRA_TYPE_CHECKER_FLOW_RESOURCES_H */
