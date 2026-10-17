#include <stdlib.h>

#include "type_checker_flow_resources.h"
#include "type_checker_flow_universe.h"

void
restore_resource_states(const ResourceConsumeSnapshot *snap)
{
    if (snap == NULL || !snap->valid)
        return;
    bool missing_array_state = snap->count > 0 && snap->array_storage_states == NULL;
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->symbols[i] != NULL) {
            snap->symbols[i]->is_consumed = snap->states[i];
            snap->symbols[i]->is_used = snap->used_states[i];
            snap->symbols[i]->future_lifecycle_state = snap->future_states[i];
            snap->symbols[i]->slot_flow_access_mask = snap->access_masks[i];
            snap->symbols[i]->slot_info.state = snap->slot_states[i];
            snap->symbols[i]->qubit_info.semantic_state = snap->sem_states[i];
            snap->symbols[i]->qubit_info.entangle_pool_id = snap->pool_ids[i];
            if (type_is_constructed_named(snap->symbols[i]->type, "Array")) {
                /* A damaged snapshot cannot restore private storage authority. */
                snap->symbols[i]->has_exclusive_array_storage =
                    !missing_array_state && snap->array_storage_states[i].exclusive;
                snap->symbols[i]->has_escaped_array_storage =
                    missing_array_state || snap->array_storage_states[i].escaped;
                snap->symbols[i]->has_pending_inout_preservation =
                    missing_array_state || snap->array_storage_states[i].pending;
                snap->symbols[i]->has_pending_array_result =
                    missing_array_state || snap->array_storage_states[i].result_pending;
            }
        }
    }
}

void
restore_resource_states_for_context(const ResourceConsumeSnapshot *snap,
                                    SemanticContext *ctx)
{
    if (snap == NULL || !snap->valid)
        return;
    bool missing_array_state = snap->count > 0 && snap->array_storage_states == NULL;
    for (size_t i = 0; i < snap->count; i++) {
        Symbol *symbol = snap->symbols[i];
        if (ctx != NULL && ctx->resource_flow_universe != NULL
            && snap->symbol_indices != NULL
            && snap->symbol_indices[i] != RESOURCE_FLOW_INDEX_NONE) {
            symbol = resource_flow_universe_symbol(ctx,
                                                   snap->symbol_indices[i]);
        }
        if (symbol != NULL) {
            symbol->is_consumed = snap->states[i];
            symbol->is_used = snap->used_states[i];
            symbol->future_lifecycle_state = snap->future_states[i];
            symbol->slot_flow_access_mask = snap->access_masks[i];
            symbol->slot_info.state = snap->slot_states[i];
            symbol->qubit_info.semantic_state = snap->sem_states[i];
            symbol->qubit_info.entangle_pool_id = snap->pool_ids[i];
            if (type_is_constructed_named(symbol->type, "Array")) {
                symbol->has_exclusive_array_storage =
                    !missing_array_state && snap->array_storage_states[i].exclusive;
                symbol->has_escaped_array_storage =
                    missing_array_state || snap->array_storage_states[i].escaped;
                symbol->has_pending_inout_preservation =
                    missing_array_state || snap->array_storage_states[i].pending;
                symbol->has_pending_array_result =
                    missing_array_state || snap->array_storage_states[i].result_pending;
            }
        }
    }
}

void
destroy_resource_snapshot(ResourceConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    free(snap->symbols);
    free(snap->symbol_indices);
    free(snap->states);
    free(snap->used_states);
    free(snap->future_states);
    free(snap->access_masks);
    free(snap->slot_states);
    free(snap->sem_states);
    free(snap->pool_ids);
    free(snap->array_storage_states);
    snap->symbols = NULL;
    snap->symbol_indices = NULL;
    snap->states = NULL;
    snap->used_states = NULL;
    snap->future_states = NULL;
    snap->access_masks = NULL;
    snap->slot_states = NULL;
    snap->sem_states = NULL;
    snap->pool_ids = NULL;
    snap->array_storage_states = NULL;
    snap->count = 0;
    snap->capacity = 0;
    snap->valid = false;
}
