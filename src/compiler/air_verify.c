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
#include <string.h>

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

static bool
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
air_boundary_requires_hir_routine_evidence(const AIRBoundaryNode *boundary)
{
    if (boundary == NULL)
        return false;
    switch (boundary->kind) {
    case AIR_BOUNDARY_ZONE:
    case AIR_BOUNDARY_WORLD:
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_IO:
    case AIR_BOUNDARY_CHANNEL:
    case AIR_BOUNDARY_EXECUTION:
        return true;
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

static bool
air_evidence_kind_valid(AIREvidenceKind kind)
{
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
    case AIR_EVIDENCE_HIR_CFG:
    case AIR_EVIDENCE_RIR_BOUNDARY:
    case AIR_EVIDENCE_RIR_AUTHORITY:
    case AIR_EVIDENCE_MIR_CLEANUP:
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
    case AIR_EVIDENCE_DAG_GENERIC:
    case AIR_EVIDENCE_DAG_ABILITY:
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
        return true;
    }
    return false;
}

static bool
air_evidence_kind_is_global(AIREvidenceKind kind)
{
    return kind == AIR_EVIDENCE_DAG_GENERIC
        || kind == AIR_EVIDENCE_DAG_ABILITY
        || kind == AIR_EVIDENCE_MIR_CLEANUP
        || kind == AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        || kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION;
}

static bool
air_boundary_has_evidence_kind(const AIRProgram *air,
                               size_t boundary_index,
                               AIREvidenceKind kind)
{
    if (air == NULL || boundary_index >= air->boundary_count)
        return false;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind && evidence->boundary_index == boundary_index)
            return true;
    }
    return false;
}

static bool
air_boundary_requires_mir_pin_cleanup_evidence(const AIRBoundaryNode *boundary);

static bool
air_evidence_node_matches_boundary_shape(const AIRProgram *air,
                                         size_t evidence_index,
                                         char **error_message)
{
    const AIREvidenceNode *evidence;
    const AIRBoundaryNode *boundary;

    if (air == NULL || evidence_index >= air->evidence_count)
        return false;

    evidence = &air->evidence_nodes[evidence_index];
    if (air_evidence_kind_is_global(evidence->kind)) {
        if (evidence->boundary_index != SIZE_MAX) {
            air_set_invariant_error(error_message,
                                    "AIR global evidence node %zu is attached to boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    }

    if (evidence->boundary_index >= air->boundary_count) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu references missing boundary node %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }

    boundary = &air->boundaries[evidence->boundary_index];
    switch (evidence->kind) {
    case AIR_EVIDENCE_HIR_CFG:
        if (!air_boundary_has_evidence_kind(air,
                                            evidence->boundary_index,
                                            AIR_EVIDENCE_HIR_ROUTINE)) {
            air_set_invariant_error(error_message,
                                    "AIR HIR CFG evidence node %zu has no HIR routine evidence for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_RIR_BOUNDARY:
        if (!air_boundary_requires_rir_evidence(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR boundary evidence node %zu is attached to non-RIR boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_RIR_AUTHORITY:
        if (!boundary->authority_required) {
            air_set_invariant_error(error_message,
                                    "AIR RIR authority evidence node %zu is attached to non-authority boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (!air_boundary_has_evidence_kind(air,
                                            evidence->boundary_index,
                                            AIR_EVIDENCE_RIR_BOUNDARY)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR authority evidence node %zu has no RIR boundary evidence for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (!air_boundary_declares_authority_name(boundary, evidence->subject_name)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR authority evidence node %zu has undeclared authority subject for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
        if (!air_boundary_requires_mir_pin_cleanup_evidence(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR MIR pin cleanup evidence node %zu is attached to non-pin boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_HIR_ROUTINE:
        return true;
    case AIR_EVIDENCE_DAG_GENERIC:
    case AIR_EVIDENCE_DAG_ABILITY:
    case AIR_EVIDENCE_MIR_CLEANUP:
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
        return true;
    }

    return false;
}

static bool
air_boundary_has_authoritative_evidence(const AIRProgram *air,
                                        const AIRBoundaryNode *boundary,
                                        size_t boundary_index,
                                        AIREvidenceKind kind,
                                        bool legacy_flag)
{
    /*
     * Compatibility fixtures may still construct AIRBoundaryNode booleans by
     * hand. Once an evidence inventory exists, the inventory is the authority
     * and the booleans are only cached summaries for dumps/legacy consumers.
     */
    (void)boundary;
    if (air != NULL && air->evidence_count > 0)
        return air_boundary_has_evidence_kind(air, boundary_index, kind);
    return legacy_flag;
}

static bool
air_boundary_requires_mir_pin_cleanup_evidence(const AIRBoundaryNode *boundary)
{
    return boundary != NULL
        && boundary->kind == AIR_BOUNDARY_EXECUTION
        && air_name_matches(boundary->source_name, "pin");
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
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (!air_evidence_kind_valid(evidence->kind)) {
            air_set_invariant_error(error_message, "AIR evidence node %zu has invalid kind", i);
            return false;
        }
        if (evidence->boundary_index >= air->boundary_count
            && !(evidence->boundary_index == SIZE_MAX
                 && air_evidence_kind_is_global(evidence->kind))) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu references missing boundary node %zu",
                                    i,
                                    evidence->boundary_index);
            return false;
        }
        if (air_name_is_empty(evidence->provider_name)) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu has no provider provenance",
                                    i);
            return false;
        }
        if (air_name_is_empty(evidence->subject_name)) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu has no subject provenance",
                                    i);
            return false;
        }
        if (!air_evidence_node_matches_boundary_shape(air, i, error_message))
            return false;
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
            && !air_boundary_has_authoritative_evidence(
                air, boundary, i, AIR_EVIDENCE_RIR_BOUNDARY,
                boundary->has_rir_boundary_evidence)) {
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
            && air->has_hir_input
            && air_boundary_requires_hir_routine_evidence(boundary)
            && !air_boundary_has_authoritative_evidence(
                air, boundary, i, AIR_EVIDENCE_HIR_ROUTINE,
                boundary->has_hir_routine_evidence)) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR boundary has no matching HIR routine evidence; implementation boundary '%s' (%s)",
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
            && !air_boundary_has_authoritative_evidence(
                air, boundary, i, AIR_EVIDENCE_HIR_CFG,
                boundary->has_hir_cfg_evidence)) {
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
            && !air_boundary_has_authoritative_evidence(
                air, boundary, i, AIR_EVIDENCE_RIR_AUTHORITY,
                boundary->has_rir_authority_evidence)) {
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
        if (air->strict_evidence
            && air->has_mir_input
            && air_boundary_requires_mir_pin_cleanup_evidence(boundary)
            && !air_boundary_has_authoritative_evidence(
                air, boundary, i, AIR_EVIDENCE_MIR_PIN_CLEANUP,
                false)) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR pin boundary has no matching MIR pin cleanup evidence; implementation boundary '%s' (%s)",
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
    }
    if (air->strict_evidence
        && air->rir_effect_propagation_required_count
            > air->rir_effect_propagation_evidence_count) {
        char message[256];
        snprintf(message,
                 sizeof(message),
                 PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                 ": AIR effect propagation has RIR op without effect resource/state evidence; required=%zu evidence=%zu",
                 air->rir_effect_propagation_required_count,
                 air->rir_effect_propagation_evidence_count);
        if (!air_append_drift(air,
                              AIR_DRIFT_EFFECT_PROPAGATION_MISSING,
                              SIZE_MAX,
                              SIZE_MAX,
                              message,
                              error_message)) {
            return false;
        }
    }
    if (air->strict_evidence
        && air->rir_relation_propagation_required_count
            > air->rir_relation_propagation_evidence_count) {
        char message[256];
        snprintf(message,
                 sizeof(message),
                 PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                 ": AIR relation propagation has RIR op without relation resource/state evidence; required=%zu evidence=%zu",
                 air->rir_relation_propagation_required_count,
                 air->rir_relation_propagation_evidence_count);
        if (!air_append_drift(air,
                              AIR_DRIFT_RELATION_PROPAGATION_MISSING,
                              SIZE_MAX,
                              SIZE_MAX,
                              message,
                              error_message)) {
            return false;
        }
    }
    if (air->strict_evidence) {
        for (size_t i = 0; i < air->evidence_count; i++) {
            const AIREvidenceNode *evidence = &air->evidence_nodes[i];
            if ((evidence->kind == AIR_EVIDENCE_DAG_GENERIC
                 || evidence->kind == AIR_EVIDENCE_DAG_ABILITY)
                && evidence->fallback_count > 0) {
                char message[256];
                snprintf(message,
                         sizeof(message),
                         PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                         ": AIR DAG evidence contains metadata materializer fallback; kind=%s fallback_count=%zu",
                         air_evidence_kind_name(evidence->kind),
                         evidence->fallback_count);
                if (!air_append_drift(air,
                                      AIR_DRIFT_DAG_FALLBACK_PRESENT,
                                      SIZE_MAX,
                                      SIZE_MAX,
                                      message,
                                      error_message)) {
                    return false;
                }
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
