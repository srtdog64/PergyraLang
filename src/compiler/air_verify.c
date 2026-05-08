/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR global verification owner. This TU is the MIR-entry gate for AIR
 * inventory shape, evidence provenance, and drift diagnostics.
 */

#include "air_internal.h"

#include "../semantic/diag_codes.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool
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

    if (message_copy == NULL) {
        air_set_error(error_message, "AIR drift message allocation failed");
        return false;
    }
    if (air->drift_count >= air->drift_capacity) {
        AIRDrift *next;
        size_t new_capacity = air->drift_capacity;
        if (!air_next_capacity(&new_capacity, 8, sizeof(AIRDrift))) {
            free(message_copy);
            air_set_error(error_message, "AIR drift allocation failed");
            return false;
        }
        next = (AIRDrift *)realloc(air->drifts,
                                   sizeof(AIRDrift) * new_capacity);
        if (next == NULL) {
            free(message_copy);
            air_set_error(error_message, "AIR drift allocation failed");
            return false;
        }
        air->drifts = next;
        air->drift_capacity = new_capacity;
    }
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

static void
air_format_boundary_provenance(const AIRIntentNode *intent,
                               const AIRBoundaryNode *boundary,
                               char *out,
                               size_t out_size)
{
    const char *source_provenance;
    const char *who_provenance;
    const char *authority_provenance;

    if (out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if (intent == NULL || boundary == NULL)
        return;

    if (boundary->source_from_intent_default && boundary->source_from_transfer)
        source_provenance = "intent-default+transfer";
    else if (boundary->source_from_intent_default)
        source_provenance = "intent-default";
    else if (boundary->source_from_action)
        source_provenance = "action-inherited";
    else if (boundary->source_from_transfer)
        source_provenance = "transfer";
    else
        source_provenance = "explicit";
    if (intent->who_from_intent_default)
        who_provenance = "intent-default";
    else if (intent->who_from_on_receiver)
        who_provenance = "on-receiver";
    else if (intent->who_from_single_participant)
        who_provenance = "single-participant";
    else
        who_provenance = "explicit";
    if (boundary->authority_from_zone)
        authority_provenance = "zone-derived";
    else if (boundary->authority_from_action)
        authority_provenance = "action-inherited";
    else
        authority_provenance = boundary->authority_required ? "explicit" : "none";
    snprintf(out,
             out_size,
             "; owner=%s step=%s boundary_source=%s source_provenance=%s who_provenance=%s authority_provenance=%s",
             intent->intent_owner != NULL ? intent->intent_owner : "<intent>",
             intent->step_name != NULL ? intent->step_name : "<step>",
             boundary->source_name != NULL ? boundary->source_name : "<boundary>",
             source_provenance,
             who_provenance,
             authority_provenance);
    out[out_size - 1] = '\0';
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
        char provenance[256];
        air_format_boundary_provenance(intent, boundary, provenance, sizeof(provenance));
        if (air_sync_conflicts(intent->sync_class, boundary->sync_class)) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT
                     ": intent sync class conflicts with boundary implementation sync class%s",
                     provenance);
            if (!air_append_drift(air,
                                  AIR_DRIFT_SYNC_ASYNC_CONFLICT,
                                  boundary->intent_index,
                                  i,
                                  message,
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence
            && air_boundary_requires_rir_evidence(boundary)
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_RIR_BOUNDARY)) {
            char message[768];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR boundary has no matching RIR boundary evidence; implementation boundary '%s' (%s)%s. "
                     "Reason: strict AIR requires lowered boundary evidence before abstraction-boundary verification. "
                     "Fix: preserve the RIR boundary evidence node for this implementation boundary.",
                     boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                     air_boundary_kind_name(boundary->kind),
                     provenance);
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
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_HIR_ROUTINE)) {
            char message[768];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR boundary has no matching HIR routine evidence; implementation boundary '%s' (%s)%s. "
                     "Reason: strict AIR requires a CFG-capable HIR routine owner for this boundary. "
                     "Fix: attach the HIR routine evidence node before AIR verification.",
                     boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                     air_boundary_kind_name(boundary->kind),
                     provenance);
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
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_HIR_CFG)) {
            char message[768];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR implementation boundary has no matching HIR CFG evidence; implementation boundary '%s' (%s)%s. "
                     "Reason: strict AIR requires body control-flow evidence for boundary safety. "
                     "Fix: lower this body through the HIR CFG path and attach AIR_EVIDENCE_HIR_CFG.",
                     boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                     air_boundary_kind_name(boundary->kind),
                     provenance);
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
            && boundary->authority_required) {
            char authority_names[256];
            char message[768];
            const char *missing_authority =
                air_boundary_missing_authority_evidence(air, boundary, i);
            const char *drift_message =
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR authority boundary has no matching RIR authority evidence. "
                "Reason: strict AIR requires authority checks to be backed by RIR authority evidence. "
                "Fix: attach AIR_EVIDENCE_RIR_AUTHORITY for this authority boundary.";

            if (missing_authority != NULL
                && air_format_authority_names(boundary,
                                              authority_names,
                                              sizeof(authority_names))) {
                snprintf(message,
                         sizeof(message),
                         PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                         ": AIR authority boundary has no matching RIR authority evidence; expected authority participant(s): %s%s. "
                         "Reason: strict AIR requires authority checks to be backed by RIR authority evidence. "
                         "Fix: attach AIR_EVIDENCE_RIR_AUTHORITY for each expected authority participant.",
                         authority_names,
                         provenance);
                drift_message = message;
            }
            if (missing_authority != NULL
                && !air_name_is_empty(missing_authority)
                && !air_name_matches(missing_authority, "<authority>")) {
                snprintf(message,
                         sizeof(message),
                         PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                         ": AIR authority boundary is missing RIR authority evidence for participant '%s'; expected authority participant(s): %s%s. "
                         "Reason: strict AIR requires every authorized participant to be backed by RIR authority evidence. "
                         "Fix: attach AIR_EVIDENCE_RIR_AUTHORITY for the missing authority participant.",
                         missing_authority,
                         air_format_authority_names(boundary,
                                                    authority_names,
                                                    sizeof(authority_names))
                            ? authority_names
                            : "<unknown>",
                         provenance);
                drift_message = message;
            }
            if (missing_authority != NULL
                && !air_append_drift(air,
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
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_MIR_PIN_CLEANUP)) {
            char message[768];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR pin boundary has no matching MIR pin cleanup evidence; implementation boundary '%s' (%s)%s. "
                     "Reason: strict AIR requires pin boundaries to prove all exits run unpin cleanup. "
                     "Fix: emit the MIR pin cleanup edge and attach AIR_EVIDENCE_MIR_PIN_CLEANUP.",
                     boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                     air_boundary_kind_name(boundary->kind),
                     provenance);
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
        && air->has_mir_input
        && air->boundary_count > 0
        && air->mir_terminator_evidence_count == 0) {
        const char *message =
            PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
            ": AIR MIR input has no CFG terminator evidence. "
            "Reason: strict AIR requires MIR branch/return terminator provenance before abstraction-boundary verification. "
            "Fix: preserve MIR source_terminator_kind facts and attach AIR_EVIDENCE_MIR_TERMINATOR.";
        if (!air_append_drift(air,
                              AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                              SIZE_MAX,
                              SIZE_MAX,
                              message,
                              error_message)) {
            return false;
        }
    }
    if (air->strict_evidence
        && air->rir_effect_propagation_required_count
            > air->rir_effect_propagation_evidence_count) {
        char message[512];
        snprintf(message,
                 sizeof(message),
                 PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                 ": AIR effect propagation has RIR op without effect resource/state evidence; required=%zu evidence=%zu. "
                 "Reason: strict AIR requires every effect propagation op to carry resource/state evidence. "
                 "Fix: attach AIR_EVIDENCE_RIR_EFFECT_PROPAGATION for the missing propagation op.",
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
        char message[512];
        snprintf(message,
                 sizeof(message),
                 PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                 ": AIR relation propagation has RIR op without relation resource/state evidence; required=%zu evidence=%zu. "
                 "Reason: strict AIR requires every relation propagation op to carry resource/state evidence. "
                 "Fix: attach AIR_EVIDENCE_RIR_RELATION_PROPAGATION for the missing propagation op.",
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
            if ((evidence->kind == AIR_EVIDENCE_DAG_METADATA
                 || evidence->kind == AIR_EVIDENCE_DAG_GENERIC
                 || evidence->kind == AIR_EVIDENCE_DAG_ABILITY)
                && evidence->fallback_count > 0) {
                char message[768];
                snprintf(message,
                         sizeof(message),
                         PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                         ": AIR DAG evidence contains unresolved metadata dead-end; kind=%s fallback_count=%zu. "
                         "Reason: strict AIR requires graph-backed type evidence before abstraction-boundary verification. "
                         "Fix: resolve the type-resolution metadata dead-end or add the missing DAG evidence node.",
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
