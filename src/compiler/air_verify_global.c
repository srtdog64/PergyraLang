#include "air_internal.h"

#include "../semantic/diag_codes.h"

typedef enum
{
    AIR_STRICT_COUNTER_GLOBAL,
    AIR_STRICT_COUNTER_BOUNDARY
} AIRStrictCounterScope;

typedef struct
{
    AIREvidenceKind kind;
    AIRStrictCounterScope scope;
    AIRDriftKind drift_kind;
    const char *diagnostic_subject;
    const char *reason;
    const char *fix;
} AIRStrictCounterRequirement;

typedef struct
{
    AIREvidenceKind kind;
    const char *missing_subject;
    const char *reason;
    const char *fix;
} AIRStrictGlobalEvidenceRequirement;

static bool
air_strict_counter_requirement_has_node(
    const AIRProgram *air,
    const AIRStrictCounterRequirement *requirement)
{
    if (air == NULL || requirement == NULL)
        return false;
    if (requirement->scope == AIR_STRICT_COUNTER_BOUNDARY) {
        return air_boundary_evidence_node_count(air, requirement->kind) > 0;
    }
    return air_global_has_evidence_kind(air, requirement->kind);
}

static bool
air_strict_require_counter_evidence_node(
    AIRProgram *air,
    const AIRStrictCounterRequirement *requirement,
    char **error_message)
{
    size_t counter;

    if (air == NULL || requirement == NULL)
        return true;
    counter = air_evidence_summary_count(air, requirement->kind);
    if (!air_requires_strict_evidence(air) || counter == 0
        || air_strict_counter_requirement_has_node(air, requirement)) {
        return true;
    }
    return air_append_driftf(air, requirement->drift_kind,
                             SIZE_MAX, SIZE_MAX, error_message,
                             PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                             ": AIR %s evidence counter has no matching %sevidence node; kind=%s counter=%zu. "
                             "Reason: %s "
                             "Fix: %s",
                             requirement->diagnostic_subject,
                             requirement->scope == AIR_STRICT_COUNTER_BOUNDARY
                                 ? "boundary "
                                 : "",
                             air_evidence_kind_name(requirement->kind),
                             counter,
                             requirement->reason,
                             requirement->fix);
}

static bool
air_strict_require_counter_proofs(
    AIRProgram *air,
    const AIRStrictCounterRequirement *requirements,
    size_t requirement_count,
    char **error_message)
{
    for (size_t i = 0; i < requirement_count; i++) {
        if (!air_strict_require_counter_evidence_node(
                air,
                &requirements[i],
                error_message)) {
            return false;
        }
    }
    return true;
}

static bool
air_verify_mir_global_requirements(AIRProgram *air, char **error_message)
{
    static const AIRStrictCounterRequirement kMirCounterRequirements[] = {
        {
            AIR_EVIDENCE_MIR_CLEANUP,
            AIR_STRICT_COUNTER_GLOBAL,
            AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
            "MIR",
            "strict AIR treats MIR summary counters as observability only; cleanup, terminator, and select receive safety must be carried by EvidenceNode.",
            "attach the missing MIR evidence node or remove the stale counter-only summary.",
        },
        {
            AIR_EVIDENCE_MIR_PIN_CLEANUP,
            AIR_STRICT_COUNTER_BOUNDARY,
            AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
            "MIR",
            "strict AIR treats MIR pin-cleanup summaries as observability only; pin cleanup safety must be carried by boundary-scoped EvidenceNode.",
            "attach the missing MIR pin-cleanup evidence node or remove the stale counter-only summary.",
        },
        {
            AIR_EVIDENCE_MIR_TERMINATOR,
            AIR_STRICT_COUNTER_GLOBAL,
            AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
            "MIR",
            "strict AIR treats MIR summary counters as observability only; cleanup, terminator, and select receive safety must be carried by EvidenceNode.",
            "attach the missing MIR evidence node or remove the stale counter-only summary.",
        },
        {
            AIR_EVIDENCE_MIR_SELECT_RECEIVE,
            AIR_STRICT_COUNTER_GLOBAL,
            AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
            "MIR",
            "strict AIR treats MIR summary counters as observability only; cleanup, terminator, and select receive safety must be carried by EvidenceNode.",
            "attach the missing MIR evidence node or remove the stale counter-only summary.",
        },
    };

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
    return air_strict_require_counter_proofs(
        air,
        kMirCounterRequirements,
        sizeof(kMirCounterRequirements) / sizeof(kMirCounterRequirements[0]),
        error_message);
}

static bool
air_verify_runtime_global_requirements(AIRProgram *air, char **error_message)
{
    static const AIRStrictGlobalEvidenceRequirement kRuntimeGlobalRequirements[] = {
        {
            AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
            "runtime observability schema",
            "strict AIR requires observability ABI schema evidence before abstraction-boundary verification.",
            "attach AIR_EVIDENCE_OBSERVABILITY_SCHEMA from the runtime observability schema.",
        },
        {
            AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
            "runtime frontier policy",
            "strict AIR requires runtime frontier policy evidence before world/zone/projection frontier verification.",
            "attach AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY from the runtime frontier policy schema.",
        },
    };

    if (!air_requires_strict_evidence(air))
        return true;
    for (size_t i = 0;
         i < sizeof(kRuntimeGlobalRequirements)
             / sizeof(kRuntimeGlobalRequirements[0]);
         i++) {
        const AIRStrictGlobalEvidenceRequirement *requirement =
            &kRuntimeGlobalRequirements[i];
        if (air_global_has_evidence_kind(air, requirement->kind))
            continue;
        if (!air_append_driftf(
                air,
                AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                SIZE_MAX,
                SIZE_MAX,
                error_message,
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR has no %s evidence. "
                "Reason: %s "
                "Fix: %s",
                requirement->missing_subject,
                requirement->reason,
                requirement->fix)) {
            return false;
        }
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

static const char *
air_rir_propagation_label(AIREvidenceKind kind)
{
    return kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION
        ? "relation"
        : "effect";
}

static AIRDriftKind
air_rir_propagation_drift_kind(AIREvidenceKind kind)
{
    return kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION
        ? AIR_DRIFT_RELATION_PROPAGATION_MISSING
        : AIR_DRIFT_EFFECT_PROPAGATION_MISSING;
}

static bool
air_rir_propagation_requirement_seen_before(const AIRProgram *air,
                                            size_t requirement_index)
{
    const AIRPropagationRequirement *needle =
        air_propagation_requirement_at(air, requirement_index);

    if (needle == NULL)
        return false;
    for (size_t i = 0; i < requirement_index; i++) {
        const AIRPropagationRequirement *prior =
            air_propagation_requirement_at(air, i);
        if (prior != NULL
            && prior->kind == needle->kind
            && air_name_matches(prior->provider_name, needle->provider_name)
            && air_name_matches(prior->subject_name, needle->subject_name)) {
            return true;
        }
    }
    return false;
}

static bool
air_verify_rir_propagation_requirement_key(
    AIRProgram *air,
    const AIRPropagationRequirement *requirement,
    char **error_message)
{
    const AIREvidenceNode *evidence;
    size_t required_count;
    size_t evidence_count;
    const char *label;

    if (air == NULL || requirement == NULL)
        return true;
    label = air_rir_propagation_label(requirement->kind);
    required_count = air_propagation_requirement_key_count(
        air,
        requirement->kind,
        requirement->provider_name,
        requirement->subject_name);
    evidence = air_global_evidence_node_provider_subject(
        air,
        requirement->kind,
        requirement->provider_name,
        requirement->subject_name);
    evidence_count = air_evidence_node_fact_count(evidence);
    if (evidence_count >= required_count)
        return true;
    return air_append_driftf(
        air,
        air_rir_propagation_drift_kind(requirement->kind),
        SIZE_MAX,
        SIZE_MAX,
        error_message,
        PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
        ": AIR %s propagation requirement has no matching RIR evidence key; provider=%s subject=%s required=%zu evidence=%zu. "
        "Reason: strict AIR requires propagation evidence to match the required provider/subject key, not just the aggregate counter. "
        "Fix: attach AIR_EVIDENCE_RIR_%s_PROPAGATION for this provider/subject or remove the stale requirement.",
        label,
        requirement->provider_name,
        requirement->subject_name,
        required_count,
        evidence_count,
        requirement->kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION
            ? "RELATION"
            : "EFFECT");
}

static bool
air_verify_rir_propagation_requirement_keys(AIRProgram *air,
                                            char **error_message)
{
    size_t requirement_count;

    if (!air_requires_strict_evidence(air))
        return true;
    requirement_count = air_propagation_requirement_count(air);
    if ((air_evidence_required_count(air, AIR_EVIDENCE_RIR_EFFECT_PROPAGATION)
            > 0
         || air_evidence_required_count(air, AIR_EVIDENCE_RIR_RELATION_PROPAGATION)
            > 0)
        && requirement_count == 0) {
        if (!air_append_drift(
                air,
                AIR_DRIFT_EFFECT_PROPAGATION_MISSING,
                SIZE_MAX,
                SIZE_MAX,
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR RIR propagation has required counters without provider/subject requirement keys. "
                "Reason: strict AIR treats aggregate propagation counters as observability only. "
                "Fix: preserve AIRPropagationRequirement keys from RIR propagation ops.",
                error_message)) {
            return false;
        }
    }
    for (size_t i = 0; i < requirement_count; i++) {
        const AIRPropagationRequirement *requirement =
            air_propagation_requirement_at(air, i);
        if (air_rir_propagation_requirement_seen_before(air, i))
            continue;
        if (!air_verify_rir_propagation_requirement_key(air,
                                                        requirement,
                                                        error_message)) {
            return false;
        }
    }
    return true;
}

static bool
air_verify_dag_global_requirements(AIRProgram *air, char **error_message)
{
    static const AIRStrictCounterRequirement kDagCounterRequirements[] = {
        {
            AIR_EVIDENCE_DAG_METADATA,
            AIR_STRICT_COUNTER_GLOBAL,
            AIR_DRIFT_DAG_DEAD_END_PRESENT,
            "DAG",
            "strict AIR treats DAG summary counters as observability only; graph-backed type evidence must be carried by EvidenceNode.",
            "attach the missing DAG evidence node or remove the stale counter-only summary.",
        },
        {
            AIR_EVIDENCE_DAG_GENERIC,
            AIR_STRICT_COUNTER_GLOBAL,
            AIR_DRIFT_DAG_DEAD_END_PRESENT,
            "DAG",
            "strict AIR treats DAG summary counters as observability only; graph-backed type evidence must be carried by EvidenceNode.",
            "attach the missing DAG evidence node or remove the stale counter-only summary.",
        },
        {
            AIR_EVIDENCE_DAG_ABILITY,
            AIR_STRICT_COUNTER_GLOBAL,
            AIR_DRIFT_DAG_DEAD_END_PRESENT,
            "DAG",
            "strict AIR treats DAG summary counters as observability only; graph-backed type evidence must be carried by EvidenceNode.",
            "attach the missing DAG evidence node or remove the stale counter-only summary.",
        },
    };

    if (!air_requires_strict_evidence(air))
        return true;
    if (!air_strict_require_counter_proofs(
            air,
            kDagCounterRequirements,
            sizeof(kDagCounterRequirements) / sizeof(kDagCounterRequirements[0]),
            error_message)) {
        return false;
    }
    for (size_t i = 0;
         i < sizeof(kDagCounterRequirements) / sizeof(kDagCounterRequirements[0]);
         i++) {
        AIREvidenceKind kind = kDagCounterRequirements[i].kind;
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
        && air_verify_rir_propagation_requirement_keys(air, error_message)
        && air_verify_dag_global_requirements(air, error_message);
}
