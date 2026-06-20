/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR invariant validation owner. This TU owns AIR graph shape,
 * evidence shape, and provenance invariants before drift verification.
 */

#include "air_internal.h"

#include "../semantic/diag_codes.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void
air_set_invariant_error(char **error_message, const char *fmt, ...)
{
    va_list args;
    char *detail;

    va_start(args, fmt);
    detail = air_vformat_owned(fmt, args);
    va_end(args);
    if (detail == NULL) {
        air_set_error(error_message,
                      PGY_CODE_AIR_INVARIANT_INVALID
                      ": AIR invariant detail formatting failed");
        return;
    }
    air_set_error(error_message,
                  PGY_CODE_AIR_INVARIANT_INVALID ": %s",
                  detail);
    free(detail);
}

bool
air_name_is_empty(const char *name)
{
    return name == NULL || name[0] == '\0';
}

static bool
air_boundary_sync_shape_valid(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return false;
    switch (boundary->kind) {
    case AIR_BOUNDARY_WORLD:
        return boundary->sync_class == AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_CHANNEL:
        return boundary->sync_class == AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_IO:
        return boundary->sync_class == AIR_SYNC_EITHER;
    case AIR_BOUNDARY_EXECUTION:
        return boundary->sync_class == AIR_SYNC_SYNC;
    case AIR_BOUNDARY_ZONE:
        return boundary->sync_class == AIR_SYNC_SYNC
            || boundary->sync_class == AIR_SYNC_ASYNC
            || boundary->sync_class == AIR_SYNC_EITHER;
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return false;
    }
}

static bool
air_drift_kind_valid(AIRDriftKind kind)
{
    switch (kind) {
    case AIR_DRIFT_SYNC_ASYNC_CONFLICT:
    case AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING:
    case AIR_DRIFT_EFFECT_PROPAGATION_MISSING:
    case AIR_DRIFT_RELATION_PROPAGATION_MISSING:
    case AIR_DRIFT_DAG_DEAD_END_PRESENT:
        return true;
    case AIR_DRIFT_NONE:
    default:
        return false;
    }
}

static bool
air_drift_kind_is_global(AIRDriftKind kind)
{
    return kind == AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING
        || kind == AIR_DRIFT_EFFECT_PROPAGATION_MISSING
        || kind == AIR_DRIFT_RELATION_PROPAGATION_MISSING
        || kind == AIR_DRIFT_DAG_DEAD_END_PRESENT;
}

bool
air_validate(const AIRProgram *air, char **error_message)
{
    if (air == NULL) {
        air_set_invariant_error(error_message, "AIR validation requires a program");
        return false;
    }
    if (!air_intent_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has intent count without intent array");
        return false;
    }
    if (!air_boundary_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has boundary count without boundary array");
        return false;
    }
    if (!air_drift_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has drift count without drift array");
        return false;
    }
    if (!air_evidence_inventory_storage_valid(air)) {
        air_set_invariant_error(error_message, "AIR has evidence count without evidence array");
        return false;
    }
    for (size_t i = 0; i < air_intent_node_count(air); i++) {
        const AIRIntentNode *intent = air_intent_node_at(air, i);
        if (intent == NULL) {
            air_set_invariant_error(error_message, "AIR intent node %zu is missing", i);
            return false;
        }
        if (air_name_is_empty(intent->intent_owner)) {
            air_set_invariant_error(error_message, "AIR intent node %zu has no owner name", i);
            return false;
        }
        if (air_name_is_empty(intent->step_name)) {
            air_set_invariant_error(error_message, "AIR intent node %zu has no step name", i);
            return false;
        }
        if (intent->sync_class == AIR_SYNC_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR intent node %zu has unknown sync class", i);
            return false;
        }
        if (intent->failure_class == AIR_FAILURE_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR intent node %zu has unknown failure class", i);
            return false;
        }
    }
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        const AIRBoundaryNode *boundary = air_boundary_node_at(air, i);
        const AIRIntentNode *intent;
        if (boundary == NULL) {
            air_set_invariant_error(error_message, "AIR boundary node %zu is missing", i);
            return false;
        }
        if (boundary->kind == AIR_BOUNDARY_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has unknown kind", i);
            return false;
        }
        if (boundary->intent_index >= air_intent_node_count(air)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu references missing intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        intent = air_intent_node_at(air, boundary->intent_index);
        if (intent == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu references missing intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        if (boundary->step_index != intent->step_index) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu step index does not match intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        if (air_name_is_empty(boundary->owner_name)) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has no owner name", i);
            return false;
        }
        if (!air_name_matches(boundary->owner_name, intent->intent_owner)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu owner does not match intent node %zu",
                                    i,
                                    boundary->intent_index);
            return false;
        }
        if (air_name_is_empty(boundary->source_name)) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has no source name", i);
            return false;
        }
        if (boundary->sync_class == AIR_SYNC_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has unknown sync class", i);
            return false;
        }
        if (!air_boundary_sync_shape_valid(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has invalid sync class %s for %s boundary",
                                    i,
                                    air_sync_class_name(boundary->sync_class),
                                    air_boundary_kind_name(boundary->kind));
            return false;
        }
        if (boundary->kind == AIR_BOUNDARY_WORLD
            && !boundary->source_from_transfer) {
            air_set_invariant_error(error_message,
                                    "AIR world boundary node %zu has no transfer provenance",
                                    i);
            return false;
        }
        if (!air_boundary_authority_storage_valid(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has authority count without names",
                                    i);
            return false;
        }
        for (size_t j = 0;
             j < air_boundary_authority_name_count(boundary);
             j++) {
            const char *authority_name =
                air_boundary_authority_name_at(boundary, j);
            if (air_name_is_empty(authority_name)) {
                air_set_invariant_error(error_message,
                                        "AIR boundary node %zu has empty authority name %zu",
                                        i,
                                        j);
                return false;
            }
        }
        if (boundary->authority_required
            && air_boundary_authority_name_count(boundary) == 0) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu requires authority but has no participant",
                                    i);
            return false;
        }
        if (!boundary->authority_required
            && air_boundary_authority_name_count(boundary) > 0) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has authority participants but does not require authority",
                                    i);
            return false;
        }
        if (boundary->authority_from_zone && !boundary->authority_required) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has legacy zone-authority field without authority",
                                    i);
            return false;
        }
        if (boundary->authority_from_action && !boundary->authority_required) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has action-inherited authority without authority",
                                    i);
            return false;
        }
        if (boundary->authority_from_zone
            && boundary->kind != AIR_BOUNDARY_ZONE
            && boundary->kind != AIR_BOUNDARY_WORLD) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has legacy zone-authority field on non-zone/world boundary",
                                    i);
            return false;
        }
        if (!air_validate_boundary_summary_shape(air, i, error_message))
            return false;
    }
    for (size_t i = 0; i < air_drift_count(air); i++) {
        const AIRDrift *drift = air_drift_at(air, i);
        if (drift == NULL) {
            air_set_invariant_error(error_message, "AIR drift node %zu is missing", i);
            return false;
        }
        if (!air_drift_kind_valid(drift->kind)) {
            air_set_invariant_error(error_message, "AIR drift node %zu has invalid kind", i);
            return false;
        }
        if (drift->intent_index >= air_intent_node_count(air)
            && !(drift->intent_index == SIZE_MAX
                 && air_drift_kind_is_global(drift->kind))) {
            air_set_invariant_error(error_message,
                                    "AIR drift node %zu references missing intent node %zu",
                                    i,
                                    drift->intent_index);
            return false;
        }
        if (drift->boundary_index >= air_boundary_node_count(air)
            && !(drift->boundary_index == SIZE_MAX
                 && air_drift_kind_is_global(drift->kind))) {
            air_set_invariant_error(error_message,
                                    "AIR drift node %zu references missing boundary node %zu",
                                    i,
                                    drift->boundary_index);
            return false;
        }
        if (air_name_is_empty(drift->message)) {
            air_set_invariant_error(error_message, "AIR drift node %zu has no message", i);
            return false;
        }
    }
    if (!air_validate_evidence_inventory(air, error_message))
        return false;
    return true;
}
