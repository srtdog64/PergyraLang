#include "air_internal.h"

#include "../semantic/diag_codes.h"

static bool
air_strict_require_dag_evidence_node(AIRProgram *air,
                                     AIREvidenceKind kind,
                                     size_t counter,
                                     char **error_message)
{
    if (!air_requires_strict_evidence(air) || counter == 0
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
    if (!air_requires_strict_evidence(air) || counter == 0
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

static bool
air_strict_require_mir_boundary_evidence_node(AIRProgram *air,
                                              AIREvidenceKind kind,
                                              size_t counter,
                                              char **error_message)
{
    if (!air_requires_strict_evidence(air) || counter == 0
        || air_boundary_evidence_node_count(air, kind) > 0) {
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

static bool
air_verify_mir_global_requirements(AIRProgram *air, char **error_message)
{
    if (air_requires_strict_evidence(air)
        && air_has_mir_input(air)
        && air_boundary_node_count(air) > 0
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
    if (!air_requires_strict_evidence(air))
        return true;
    return air_strict_require_mir_evidence_node(
            air, AIR_EVIDENCE_MIR_CLEANUP,
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_CLEANUP),
            error_message)
        && air_strict_require_mir_boundary_evidence_node(
            air, AIR_EVIDENCE_MIR_PIN_CLEANUP,
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_PIN_CLEANUP),
            error_message)
        && air_strict_require_mir_evidence_node(
            air, AIR_EVIDENCE_MIR_TERMINATOR,
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_TERMINATOR),
            error_message)
        && air_strict_require_mir_evidence_node(
            air, AIR_EVIDENCE_MIR_SELECT_RECEIVE,
            air_evidence_summary_count(air, AIR_EVIDENCE_MIR_SELECT_RECEIVE),
            error_message);
}

static bool
air_verify_runtime_global_requirements(AIRProgram *air, char **error_message)
{
    if (!air_requires_strict_evidence(air))
        return true;
    if (!air_global_has_evidence_kind(air,
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
    if (!air_global_has_evidence_kind(air,
                                      AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY)) {
        const char *message =
            PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
            ": AIR has no runtime frontier policy evidence. "
            "Reason: strict AIR requires runtime frontier policy evidence before world/zone/projection frontier verification. "
            "Fix: attach AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY from the runtime frontier policy schema.";
        return air_append_drift(air,
                                AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                SIZE_MAX,
                                SIZE_MAX,
                                message,
                                error_message);
    }
    return true;
}

static bool
air_verify_rir_propagation_requirements(AIRProgram *air,
                                        char **error_message)
{
    size_t evidence_count;

    if (!air_requires_strict_evidence(air))
        return true;
    evidence_count = air_global_evidence_fact_count(
        air,
        AIR_EVIDENCE_RIR_EFFECT_PROPAGATION);
    if (air_evidence_required_count(
            air,
            AIR_EVIDENCE_RIR_EFFECT_PROPAGATION) > evidence_count
        && !air_append_driftf(
                air,
                AIR_DRIFT_EFFECT_PROPAGATION_MISSING,
                SIZE_MAX,
                SIZE_MAX,
                error_message,
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR effect propagation has RIR op without effect resource/state evidence; required=%zu evidence=%zu. "
                "Reason: strict AIR requires every effect propagation op to carry resource/state evidence. "
                "Fix: attach AIR_EVIDENCE_RIR_EFFECT_PROPAGATION for the missing propagation op.",
                air_evidence_required_count(
                    air,
                    AIR_EVIDENCE_RIR_EFFECT_PROPAGATION),
                evidence_count)) {
        return false;
    }

    evidence_count = air_global_evidence_fact_count(
        air,
        AIR_EVIDENCE_RIR_RELATION_PROPAGATION);
    if (air_evidence_required_count(
            air,
            AIR_EVIDENCE_RIR_RELATION_PROPAGATION) > evidence_count) {
        return air_append_driftf(
            air,
            AIR_DRIFT_RELATION_PROPAGATION_MISSING,
            SIZE_MAX,
            SIZE_MAX,
            error_message,
            PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
            ": AIR relation propagation has RIR op without relation resource/state evidence; required=%zu evidence=%zu. "
            "Reason: strict AIR requires every relation propagation op to carry resource/state evidence. "
            "Fix: attach AIR_EVIDENCE_RIR_RELATION_PROPAGATION for the missing propagation op.",
            air_evidence_required_count(
                air,
                AIR_EVIDENCE_RIR_RELATION_PROPAGATION),
            evidence_count);
    }
    return true;
}

static bool
air_verify_dag_global_requirements(AIRProgram *air, char **error_message)
{
    const AIREvidenceKind dag_kinds[] = {
        AIR_EVIDENCE_DAG_METADATA,
        AIR_EVIDENCE_DAG_GENERIC,
        AIR_EVIDENCE_DAG_ABILITY,
    };

    if (!air_requires_strict_evidence(air))
        return true;
    if (!air_strict_require_dag_evidence_node(
            air, AIR_EVIDENCE_DAG_METADATA,
            air_evidence_summary_count(air, AIR_EVIDENCE_DAG_METADATA),
            error_message)
        || !air_strict_require_dag_evidence_node(
            air, AIR_EVIDENCE_DAG_GENERIC,
            air_evidence_summary_count(air, AIR_EVIDENCE_DAG_GENERIC),
            error_message)
        || !air_strict_require_dag_evidence_node(
            air, AIR_EVIDENCE_DAG_ABILITY,
            air_evidence_summary_count(air, AIR_EVIDENCE_DAG_ABILITY),
            error_message)) {
        return false;
    }
    for (size_t i = 0; i < sizeof(dag_kinds) / sizeof(dag_kinds[0]); i++) {
        AIREvidenceKind kind = dag_kinds[i];
        size_t fallback_count = air_global_evidence_fallback_count(air, kind);

        if (fallback_count > 0) {
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
                    air_evidence_kind_name(kind),
                    fallback_count)) {
                return false;
            }
        }
    }
    return true;
}

bool
air_verify_global_evidence_requirements(AIRProgram *air,
                                        char **error_message)
{
    return air_verify_mir_global_requirements(air, error_message)
        && air_verify_runtime_global_requirements(air, error_message)
        && air_verify_rir_propagation_requirements(air, error_message)
        && air_verify_dag_global_requirements(air, error_message);
}
