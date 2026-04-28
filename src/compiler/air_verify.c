/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR global verification owner. This TU is the MIR-entry gate for AIR
 * inventory shape, evidence provenance, and drift diagnostics.
 */

#include "air_internal.h"

#include "../semantic/diag_codes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void
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

static bool
air_sync_conflicts(AIRSyncClass expected, AIRSyncClass actual)
{
    if (expected == AIR_SYNC_UNKNOWN || actual == AIR_SYNC_UNKNOWN)
        return false;
    if (expected == AIR_SYNC_EITHER || actual == AIR_SYNC_EITHER)
        return false;
    return expected != actual;
}

static bool
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
air_boundary_requires_rir_evidence(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return false;
    switch (boundary->kind) {
    case AIR_BOUNDARY_ZONE:
    case AIR_BOUNDARY_WORLD:
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_IO:
    case AIR_BOUNDARY_CHANNEL:
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
        return true;
    case AIR_DRIFT_NONE:
    default:
        return false;
    }
}

static bool
air_append_drift(AIRProgram *air,
                 AIRDriftKind kind,
                 size_t intent_index,
                 size_t boundary_index,
                 const char *message,
                 char **error_message)
{
    char *message_copy = air_strdup_owned(message);
    AIRDrift *next;

    if (message_copy == NULL) {
        air_set_error(error_message, "AIR drift message allocation failed");
        return false;
    }
    next = (AIRDrift *)realloc(air->drifts, sizeof(AIRDrift) * (air->drift_count + 1));
    if (next == NULL) {
        free(message_copy);
        air_set_error(error_message, "AIR drift allocation failed");
        return false;
    }
    air->drifts = next;
    air->drifts[air->drift_count].kind = kind;
    air->drifts[air->drift_count].intent_index = intent_index;
    air->drifts[air->drift_count].boundary_index = boundary_index;
    air->drifts[air->drift_count].message = message_copy;
    air->drift_count++;
    return true;
}

static bool
air_format_authority_names(const AIRBoundaryNode *boundary,
                           char *out,
                           size_t out_size)
{
    size_t used = 0;
    bool emitted = false;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (boundary == NULL || boundary->authority_names == NULL)
        return false;

    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        const char *name = boundary->authority_names[i];
        int written;

        if (name == NULL || name[0] == '\0')
            continue;
        written = snprintf(out + used,
                           out_size - used,
                           "%s%s",
                           emitted ? ", " : "",
                           name);
        if (written < 0)
            return emitted;
        if ((size_t)written >= out_size - used) {
            out[out_size - 1] = '\0';
            return true;
        }
        used += (size_t)written;
        emitted = true;
    }
    return emitted;
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
            && air->boundaries[i].hir_routine_evidence_name == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has HIR evidence without provenance",
                                    i);
            return false;
        }
        if (air->boundaries[i].has_rir_boundary_evidence
            && air->boundaries[i].rir_boundary_evidence_scope == NULL) {
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
            && air->boundaries[i].rir_authority_evidence_name == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR boundary node %zu has RIR authority evidence without provenance",
                                    i);
            return false;
        }
    }
    for (size_t i = 0; i < air->drift_count; i++) {
        if (!air_drift_kind_valid(air->drifts[i].kind)) {
            air_set_invariant_error(error_message, "AIR drift node %zu has invalid kind", i);
            return false;
        }
        if (air->drifts[i].intent_index >= air->intent_count) {
            air_set_invariant_error(error_message,
                                    "AIR drift node %zu references missing intent node %zu",
                                    i,
                                    air->drifts[i].intent_index);
            return false;
        }
        if (air->drifts[i].boundary_index >= air->boundary_count) {
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
    return true;
}

bool
air_verify(AIRProgram *air, char **error_message)
{
    if (!air_validate(air, error_message))
        return false;

    air_clear_drifts(air);

    for (size_t i = 0; i < air->boundary_count; i++) {
        AIRBoundaryNode *boundary = &air->boundaries[i];
        AIRIntentNode *intent = &air->intents[boundary->intent_index];
        if (air_sync_conflicts(intent->sync_class, boundary->sync_class)) {
            if (!air_append_drift(air,
                                  AIR_DRIFT_SYNC_ASYNC_CONFLICT,
                                  boundary->intent_index,
                                  i,
                                  PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT
                                  ": intent sync class conflicts with boundary implementation sync class",
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence
            && air_boundary_requires_rir_evidence(boundary)
            && !boundary->has_rir_boundary_evidence) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR boundary has no matching RIR boundary evidence; implementation boundary '%s' (%s)",
                     boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                     air_boundary_kind_name(boundary->kind));
            if (!air_append_drift(air,
                                  AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                  boundary->intent_index,
                                  i,
                                  message,
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence
            && air_boundary_requires_hir_evidence(boundary)
            && !boundary->has_hir_routine_evidence) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR implementation boundary has no matching HIR CFG evidence; implementation boundary '%s' (%s)",
                     boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                     air_boundary_kind_name(boundary->kind));
            if (!air_append_drift(air,
                                  AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                  boundary->intent_index,
                                  i,
                                  message,
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence
            && boundary->authority_required
            && !boundary->has_rir_authority_evidence) {
            char authority_names[256];
            char message[512];
            const char *drift_message =
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR authority boundary has no matching RIR authority evidence";

            if (air_format_authority_names(boundary,
                                           authority_names,
                                           sizeof(authority_names))) {
                snprintf(message,
                         sizeof(message),
                         PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                         ": AIR authority boundary has no matching RIR authority evidence; expected authority participant(s): %s",
                         authority_names);
                drift_message = message;
            }
            if (!air_append_drift(air,
                                  AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                  boundary->intent_index,
                                  i,
                                  drift_message,
                                  error_message)) {
                return false;
            }
        }
    }
    return true;
}

bool
air_check_drift(AIRProgram *air, char **error_message)
{
    return air_verify(air, error_message);
}
