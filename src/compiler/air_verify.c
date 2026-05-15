/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR global verification owner. This TU is the MIR-entry gate for AIR
 * inventory shape, evidence provenance, and drift diagnostics.
 */

#include "air_internal.h"

#include "../semantic/diag_codes.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
air_append_driftf(AIRProgram *air,
                  AIRDriftKind kind,
                  size_t intent_index,
                  size_t boundary_index,
                  char **error_message,
                  const char *fmt,
                  ...)
{
    va_list args;
    char *message;
    bool ok;

    va_start(args, fmt);
    message = air_vformat_owned(fmt, args);
    va_end(args);
    if (message == NULL) {
        air_set_error(error_message, "AIR drift message formatting failed");
        return false;
    }

    ok = air_append_drift(air, kind, intent_index, boundary_index,
                          message, error_message);
    free(message);
    return ok;
}

static bool
air_strict_require_dag_evidence_node(AIRProgram *air,
                                     AIREvidenceKind kind,
                                     size_t counter,
                                     char **error_message)
{
    if (!air->strict_evidence || counter == 0
        || air_global_has_evidence_kind(air, kind))
        return true;
    return air_append_driftf(air, AIR_DRIFT_DAG_DEAD_END_PRESENT,
                             SIZE_MAX, SIZE_MAX, error_message,
        PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
        ": AIR DAG evidence counter has no matching evidence node; kind=%s counter=%zu. "
        "Reason: strict AIR treats DAG summary counters as observability only; graph-backed type evidence must be carried by EvidenceNode. "
        "Fix: attach the missing DAG evidence node or remove the stale counter-only summary.",
        air_evidence_kind_name(kind),
        counter);
}

static bool
air_strict_require_mir_evidence_node(AIRProgram *air,
                                     AIREvidenceKind kind,
                                     size_t counter,
                                     char **error_message)
{
    if (!air->strict_evidence || counter == 0
        || air_global_has_evidence_kind(air, kind))
        return true;
    return air_append_driftf(air, AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                             SIZE_MAX, SIZE_MAX, error_message,
        PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
        ": AIR MIR evidence counter has no matching evidence node; kind=%s counter=%zu. "
        "Reason: strict AIR treats MIR summary counters as observability only; cleanup, terminator, and select receive safety must be carried by EvidenceNode. "
        "Fix: attach the missing MIR evidence node or remove the stale counter-only summary.",
        air_evidence_kind_name(kind),
        counter);
}

static size_t
air_boundary_evidence_node_count_for_kind(const AIRProgram *air,
                                          AIREvidenceKind kind)
{
    size_t count = 0;

    if (air == NULL || !air_evidence_kind_is_boundary_scoped(kind))
        return 0;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind && evidence->boundary_index != SIZE_MAX)
            count++;
    }
    return count;
}

static bool
air_strict_require_mir_boundary_evidence_node(AIRProgram *air,
                                              AIREvidenceKind kind,
                                              size_t counter,
                                              char **error_message)
{
    if (!air->strict_evidence || counter == 0
        || air_boundary_evidence_node_count_for_kind(air, kind) > 0) {
        return true;
    }
    return air_append_driftf(air, AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                             SIZE_MAX, SIZE_MAX, error_message,
        PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
        ": AIR MIR evidence counter has no matching boundary evidence node; kind=%s counter=%zu. "
        "Reason: strict AIR treats MIR pin-cleanup summaries as observability only; pin cleanup safety must be carried by boundary-scoped EvidenceNode. "
        "Fix: attach the missing MIR pin-cleanup evidence node or remove the stale counter-only summary.",
        air_evidence_kind_name(kind),
        counter);
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
        char *provenance = air_format_boundary_provenance_owned(intent, boundary);
        if (provenance == NULL) {
            air_set_error(error_message, "AIR boundary provenance formatting failed");
            return false;
        }
        if (air_sync_conflicts(intent->sync_class, boundary->sync_class)) {
            if (!air_append_driftf(
                    air,
                    AIR_DRIFT_SYNC_ASYNC_CONFLICT,
                    boundary->intent_index,
                    i,
                    error_message,
                    PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT
                    ": intent sync class conflicts with boundary implementation sync class%s",
                    provenance)) {
                free(provenance);
                return false;
            }
        }
        if (air->strict_evidence
            && air_boundary_requires_rir_evidence(boundary)
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_RIR_BOUNDARY)) {
            if (!air_append_driftf(
                    air,
                    AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                    boundary->intent_index,
                    i,
                    error_message,
                    PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                    ": AIR boundary has no matching RIR boundary evidence; implementation boundary '%s' (%s)%s. "
                    "Reason: strict AIR requires lowered boundary evidence before abstraction-boundary verification. "
                    "Fix: preserve the RIR boundary evidence node for this implementation boundary.",
                    boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                    air_boundary_kind_name(boundary->kind),
                    provenance)) {
                free(provenance);
                return false;
            }
        }
        if (air->strict_evidence
            && air->has_hir_input
            && air_boundary_requires_hir_routine_evidence(boundary)
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_HIR_ROUTINE)) {
            if (!air_append_driftf(
                    air,
                    AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                    boundary->intent_index,
                    i,
                    error_message,
                    PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                    ": AIR boundary has no matching HIR routine evidence; implementation boundary '%s' (%s)%s. "
                    "Reason: strict AIR requires a CFG-capable HIR routine owner for this boundary. "
                    "Fix: attach the HIR routine evidence node before AIR verification.",
                    boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                    air_boundary_kind_name(boundary->kind),
                    provenance)) {
                free(provenance);
                return false;
            }
        }
        if (air->strict_evidence
            && air_boundary_requires_hir_evidence(boundary)
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_HIR_CFG)) {
            if (!air_append_driftf(
                    air,
                    AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                    boundary->intent_index,
                    i,
                    error_message,
                    PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                    ": AIR implementation boundary has no matching HIR CFG evidence; implementation boundary '%s' (%s)%s. "
                    "Reason: strict AIR requires body control-flow evidence for boundary safety. "
                    "Fix: lower this body through the HIR CFG path and attach AIR_EVIDENCE_HIR_CFG.",
                    boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                    air_boundary_kind_name(boundary->kind),
                    provenance)) {
                free(provenance);
                return false;
            }
        }
        if (air->strict_evidence
            && boundary->authority_required) {
            const char *missing_authority =
                air_boundary_missing_authority_evidence(air, boundary, i);
            char *authority_names = NULL;

            if (missing_authority != NULL) {
                authority_names = air_format_authority_names_owned(boundary);
                if (!air_name_is_empty(missing_authority)
                    && !air_name_matches(missing_authority, "<authority>")) {
                    if (!air_append_driftf(
                            air,
                            AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                            boundary->intent_index,
                            i,
                            error_message,
                            PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                            ": AIR authority boundary is missing RIR authority evidence for participant '%s'; expected authority participant(s): %s%s. "
                            "Reason: strict AIR requires every authorized participant to be backed by RIR authority evidence. "
                            "Fix: attach AIR_EVIDENCE_RIR_AUTHORITY for the missing authority participant.",
                            missing_authority,
                            authority_names != NULL ? authority_names : "<unknown>",
                            provenance)) {
                        free(authority_names);
                        free(provenance);
                        return false;
                    }
                } else if (authority_names != NULL) {
                    if (!air_append_driftf(
                            air,
                            AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                            boundary->intent_index,
                            i,
                            error_message,
                            PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                            ": AIR authority boundary has no matching RIR authority evidence; expected authority participant(s): %s%s. "
                            "Reason: strict AIR requires authority checks to be backed by RIR authority evidence. "
                            "Fix: attach AIR_EVIDENCE_RIR_AUTHORITY for each expected authority participant.",
                            authority_names,
                            provenance)) {
                        free(authority_names);
                        free(provenance);
                        return false;
                    }
                } else if (!air_append_drift(
                               air,
                               AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                               boundary->intent_index,
                               i,
                               PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                               ": AIR authority boundary has no matching RIR authority evidence. "
                               "Reason: strict AIR requires authority checks to be backed by RIR authority evidence. "
                               "Fix: attach AIR_EVIDENCE_RIR_AUTHORITY for this authority boundary.",
                               error_message)) {
                    free(provenance);
                    return false;
                }
                free(authority_names);
            }
        }
        if (air->strict_evidence
            && air->has_mir_input
            && air_boundary_requires_mir_pin_cleanup_evidence(boundary)
            && !air_boundary_has_evidence(air, i, AIR_EVIDENCE_MIR_PIN_CLEANUP)) {
            if (!air_append_driftf(
                    air,
                    AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                    boundary->intent_index,
                    i,
                    error_message,
                    PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                    ": AIR pin boundary has no matching MIR pin cleanup evidence; implementation boundary '%s' (%s)%s. "
                    "Reason: strict AIR requires pin boundaries to prove all exits run unpin cleanup. "
                    "Fix: emit the MIR pin cleanup edge and attach AIR_EVIDENCE_MIR_PIN_CLEANUP.",
                    boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                    air_boundary_kind_name(boundary->kind),
                    provenance)) {
                free(provenance);
                return false;
            }
        }
        free(provenance);
    }
    if (air->strict_evidence
        && air->has_mir_input
        && air->boundary_count > 0
        && !air_global_has_evidence_kind(air, AIR_EVIDENCE_MIR_TERMINATOR)) {
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
    if (air->strict_evidence) {
        if (!air_strict_require_mir_evidence_node(
                air,
                AIR_EVIDENCE_MIR_CLEANUP,
                air->mir_cleanup_evidence_count,
                error_message)
            || !air_strict_require_mir_boundary_evidence_node(
                air,
                AIR_EVIDENCE_MIR_PIN_CLEANUP,
                air->mir_pin_cleanup_evidence_count,
                error_message)
            || !air_strict_require_mir_evidence_node(
                air,
                AIR_EVIDENCE_MIR_TERMINATOR,
                air->mir_terminator_evidence_count,
                error_message)
            || !air_strict_require_mir_evidence_node(
                air,
                AIR_EVIDENCE_MIR_SELECT_RECEIVE,
                air->mir_select_receive_evidence_count,
                error_message)) {
            return false;
        }
    }
    if (air->strict_evidence
        && !air_global_has_evidence_kind(air,
                                         AIR_EVIDENCE_OBSERVABILITY_SCHEMA)) {
        const char *message =
            PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
            ": AIR has no runtime observability schema evidence. "
            "Reason: strict AIR requires observability ABI schema evidence before abstraction-boundary verification. "
            "Fix: attach AIR_EVIDENCE_OBSERVABILITY_SCHEMA from the runtime observability schema.";
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
        && !air_global_has_evidence_kind(
            air,
            AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY)) {
        const char *message =
            PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
            ": AIR has no runtime frontier policy evidence. "
            "Reason: strict AIR requires runtime frontier policy evidence before world/zone/projection frontier verification. "
            "Fix: attach AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY from the runtime frontier policy schema.";
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
            > air_global_evidence_fact_count(
                air,
                AIR_EVIDENCE_RIR_EFFECT_PROPAGATION)) {
        size_t evidence_count = air_global_evidence_fact_count(
            air,
            AIR_EVIDENCE_RIR_EFFECT_PROPAGATION);
        if (!air_append_driftf(
                air,
                AIR_DRIFT_EFFECT_PROPAGATION_MISSING,
                SIZE_MAX,
                SIZE_MAX,
                error_message,
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR effect propagation has RIR op without effect resource/state evidence; required=%zu evidence=%zu. "
                "Reason: strict AIR requires every effect propagation op to carry resource/state evidence. "
                "Fix: attach AIR_EVIDENCE_RIR_EFFECT_PROPAGATION for the missing propagation op.",
                air->rir_effect_propagation_required_count,
                evidence_count)) {
            return false;
        }
    }
    if (air->strict_evidence
        && air->rir_relation_propagation_required_count
            > air_global_evidence_fact_count(
                air,
                AIR_EVIDENCE_RIR_RELATION_PROPAGATION)) {
        size_t evidence_count = air_global_evidence_fact_count(
            air,
            AIR_EVIDENCE_RIR_RELATION_PROPAGATION);
        if (!air_append_driftf(
                air,
                AIR_DRIFT_RELATION_PROPAGATION_MISSING,
                SIZE_MAX,
                SIZE_MAX,
                error_message,
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR relation propagation has RIR op without relation resource/state evidence; required=%zu evidence=%zu. "
                "Reason: strict AIR requires every relation propagation op to carry resource/state evidence. "
                "Fix: attach AIR_EVIDENCE_RIR_RELATION_PROPAGATION for the missing propagation op.",
                air->rir_relation_propagation_required_count,
                evidence_count)) {
            return false;
        }
    }
    if (air->strict_evidence) {
        if (!air_strict_require_dag_evidence_node(
                air,
                AIR_EVIDENCE_DAG_METADATA,
                air->dag_metadata_evidence_count,
                error_message)
            || !air_strict_require_dag_evidence_node(
                air,
                AIR_EVIDENCE_DAG_GENERIC,
                air->dag_generic_evidence_count,
                error_message)
            || !air_strict_require_dag_evidence_node(
                air,
                AIR_EVIDENCE_DAG_ABILITY,
                air->dag_ability_evidence_count,
                error_message)) {
            return false;
        }
        for (size_t i = 0; i < air->evidence_count; i++) {
            const AIREvidenceNode *evidence = &air->evidence_nodes[i];
            if ((evidence->kind == AIR_EVIDENCE_DAG_METADATA
                 || evidence->kind == AIR_EVIDENCE_DAG_GENERIC
                 || evidence->kind == AIR_EVIDENCE_DAG_ABILITY)
                && evidence->fallback_count > 0) {
                if (!air_append_driftf(
                        air,
                        AIR_DRIFT_DAG_DEAD_END_PRESENT,
                        SIZE_MAX,
                        SIZE_MAX,
                        error_message,
                        PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                        ": AIR DAG evidence contains unresolved metadata dead-end; kind=%s unresolved_count=%zu. "
                        "Reason: strict AIR requires graph-backed type evidence before abstraction-boundary verification. "
                        "Fix: resolve the type-resolution metadata dead-end or add the missing DAG evidence node.",
                        air_evidence_kind_name(evidence->kind),
                        evidence->fallback_count)) {
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
