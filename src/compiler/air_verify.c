/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR global verification owner. This TU is the MIR-entry gate for AIR
 * inventory shape, evidence provenance, and drift diagnostics.
 */

#include "air_internal.h"

#include "../semantic/diag_codes.h"

#include <stdint.h>
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

bool
air_verify(AIRProgram *air, char **error_message)
{
    if (!air_validate(air, error_message))
        return false;

    air_clear_drifts(air);

    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        AIRBoundaryNode *boundary = air_boundary_node_mut_at(air, i);
        const AIRIntentNode *intent;
        if (boundary == NULL)
            continue;
        intent = air_intent_node_at(air, boundary->intent_index);
        if (intent == NULL)
            continue;
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
        if (air_requires_strict_evidence(air)
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
        if (air_requires_strict_evidence(air)
            && air_has_hir_input(air)
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
        if (air_requires_strict_evidence(air)
            && air_boundary_requires_hir_cfg_for_program(air, boundary)
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
        if (air_requires_strict_evidence(air)
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
        if (air_requires_strict_evidence(air)
            && air_has_mir_input(air)
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
    return air_verify_global_evidence_requirements(air, error_message);
}

bool
air_check_drift(AIRProgram *air, char **error_message)
{
    return air_verify(air, error_message);
}
