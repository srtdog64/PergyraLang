/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR invariant validation owner. This TU owns AIR graph shape,
 * evidence shape, and provenance invariants before drift verification.
 */

#include "air_internal.h"

#include "../semantic/diag_codes.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void
air_set_invariant_error(char **error_message, const char *fmt, ...)
{
    char detail[512];
    va_list args;

    va_start(args, fmt);
    vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);
    detail[sizeof(detail) - 1] = '\0';
    air_set_error(error_message,
                  PGY_CODE_AIR_INVARIANT_INVALID ": %s",
                  detail);
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

bool
air_boundary_declares_authority_name(const AIRBoundaryNode *boundary,
                                     const char *authority_name)
{
    if (boundary == NULL || authority_name == NULL)
        return false;
    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        if (air_name_matches(boundary->authority_names[i], authority_name))
            return true;
    }
    return false;
}

bool
air_boundary_requires_rir_evidence(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return false;
    switch (boundary->kind) {
    case AIR_BOUNDARY_ZONE:
    case AIR_BOUNDARY_WORLD:
    case AIR_BOUNDARY_IO:
    case AIR_BOUNDARY_CHANNEL:
        return true;
    case AIR_BOUNDARY_PARALLEL:
        return true;
    case AIR_BOUNDARY_EXECUTION:
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return false;
    }
}

bool
air_boundary_requires_hir_evidence(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return false;
    switch (boundary->kind) {
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_IO:
    case AIR_BOUNDARY_CHANNEL:
    case AIR_BOUNDARY_EXECUTION:
        return true;
    case AIR_BOUNDARY_ZONE:
    case AIR_BOUNDARY_WORLD:
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
    case AIR_DRIFT_DAG_FALLBACK_PRESENT:
        return true;
    case AIR_DRIFT_NONE:
    default:
        return false;
    }
}

static bool
air_drift_kind_is_global(AIRDriftKind kind)
{
    return kind == AIR_DRIFT_EFFECT_PROPAGATION_MISSING
        || kind == AIR_DRIFT_RELATION_PROPAGATION_MISSING
        || kind == AIR_DRIFT_DAG_FALLBACK_PRESENT;
}

bool
air_validate(const AIRProgram *air, char **error_message)
{
    if (air == NULL) {
        air_set_invariant_error(error_message, "AIR validation requires a program");
        return false;
    }
    if (air->intent_count > 0 && air->intents == NULL) {
        air_set_invariant_error(error_message, "AIR has intent count without intent array");
        return false;
    }
    if (air->boundary_count > 0 && air->boundaries == NULL) {
        air_set_invariant_error(error_message, "AIR has boundary count without boundary array");
        return false;
    }
    if (air->drift_count > 0 && air->drifts == NULL) {
        air_set_invariant_error(error_message, "AIR has drift count without drift array");
        return false;
    }
    if (air->evidence_count > 0 && air->evidence_nodes == NULL) {
        air_set_invariant_error(error_message, "AIR has evidence count without evidence array");
        return false;
    }
    for (size_t i = 0; i < air->intent_count; i++) {
        if (air_name_is_empty(air->intents[i].intent_owner)) {
            air_set_invariant_error(error_message, "AIR intent node %zu has no owner name", i);
            return false;
        }
        if (air_name_is_empty(air->intents[i].step_name)) {
            air_set_invariant_error(error_message, "AIR intent node %zu has no step name", i);
            return false;
        }
        if (air->intents[i].sync_class == AIR_SYNC_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR intent node %zu has unknown sync class", i);
            return false;
        }
        if (air->intents[i].failure_class == AIR_FAILURE_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR intent node %zu has unknown failure class", i);
            return false;
        }
    }
    for (size_t i = 0; i < air->boundary_count; i++) {
        if (air->boundaries[i].kind == AIR_BOUNDARY_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has unknown kind", i);
            return false;
        }
        if (air->boundaries[i].intent_index >= air->intent_count) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu references missing intent node %zu",
                                    i,
                                    air->boundaries[i].intent_index);
            return false;
        }
        if (air->boundaries[i].step_index
            != air->intents[air->boundaries[i].intent_index].step_index) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu step index does not match intent node %zu",
                                    i,
                                    air->boundaries[i].intent_index);
            return false;
        }
        if (air_name_is_empty(air->boundaries[i].owner_name)) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has no owner name", i);
            return false;
        }
        if (!air_name_matches(air->boundaries[i].owner_name,
                              air->intents[air->boundaries[i].intent_index].intent_owner)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu owner does not match intent node %zu",
                                    i,
                                    air->boundaries[i].intent_index);
            return false;
        }
        if (air_name_is_empty(air->boundaries[i].source_name)) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has no source name", i);
            return false;
        }
        if (air->boundaries[i].sync_class == AIR_SYNC_UNKNOWN) {
            air_set_invariant_error(error_message, "AIR boundary node %zu has unknown sync class", i);
            return false;
        }
        if (!air_boundary_sync_shape_valid(&air->boundaries[i])) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has invalid sync class %s for %s boundary",
                                    i,
                                    air_sync_class_name(air->boundaries[i].sync_class),
                                    air_boundary_kind_name(air->boundaries[i].kind));
            return false;
        }
        if (air->boundaries[i].authority_name_count > 0
            && air->boundaries[i].authority_names == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has authority count without names",
                                    i);
            return false;
        }
        for (size_t j = 0; j < air->boundaries[i].authority_name_count; j++) {
            if (air->boundaries[i].authority_names[j] == NULL
                || air->boundaries[i].authority_names[j][0] == '\0') {
                air_set_invariant_error(error_message,
                                        "AIR boundary node %zu has empty authority name %zu",
                                        i,
                                        j);
                return false;
            }
        }
        if (air->boundaries[i].authority_required
            && air->boundaries[i].authority_name_count == 0) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu requires authority but has no participant",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_hir_routine_evidence
            && air_name_is_empty(air->boundaries[i].hir_routine_evidence_name)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has HIR evidence without provenance",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_hir_cfg_evidence
            && !air->boundaries[i].has_hir_routine_evidence) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has HIR CFG evidence without routine evidence",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_rir_boundary_evidence
            && air_name_is_empty(air->boundaries[i].rir_boundary_evidence_scope)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has RIR boundary evidence without provenance",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_rir_authority_evidence
            && !air->boundaries[i].has_rir_boundary_evidence) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has RIR authority evidence without boundary evidence",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_rir_authority_evidence
            && !air->boundaries[i].authority_required) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has RIR authority evidence on non-authority boundary",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_rir_authority_evidence
            && air_name_is_empty(air->boundaries[i].rir_authority_evidence_name)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has RIR authority evidence without provenance",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_rir_authority_evidence
            && !air_boundary_declares_authority_name(
                &air->boundaries[i],
                air->boundaries[i].rir_authority_evidence_name)) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has RIR authority evidence for undeclared participant",
                                    i);
            return false;
        }
    }
    for (size_t i = 0; i < air->drift_count; i++) {
        if (!air_drift_kind_valid(air->drifts[i].kind)) {
            air_set_invariant_error(error_message, "AIR drift node %zu has invalid kind", i);
            return false;
        }
        if (air->drifts[i].intent_index >= air->intent_count
            && !(air->drifts[i].intent_index == SIZE_MAX
                 && air_drift_kind_is_global(air->drifts[i].kind))) {
            air_set_invariant_error(error_message,
                                    "AIR drift node %zu references missing intent node %zu",
                                    i,
                                    air->drifts[i].intent_index);
            return false;
        }
        if (air->drifts[i].boundary_index >= air->boundary_count
            && !(air->drifts[i].boundary_index == SIZE_MAX
                 && air_drift_kind_is_global(air->drifts[i].kind))) {
            air_set_invariant_error(error_message,
                                    "AIR drift node %zu references missing boundary node %zu",
                                    i,
                                    air->drifts[i].boundary_index);
            return false;
        }
        if (air_name_is_empty(air->drifts[i].message)) {
            air_set_invariant_error(error_message, "AIR drift node %zu has no message", i);
            return false;
        }
    }
    if (!air_validate_evidence_inventory(air, error_message))
        return false;
    return true;
}
