/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * AIR global evidence validation owner.
 */

#include "air_internal.h"
#include "../runtime/pgy_runtime_observability_schema.h"
#include "../runtime/pgy_frontier_policy.h"

bool
air_evidence_kind_is_global(AIREvidenceKind kind)
{
    return air_evidence_kind_is_known(kind)
        && !air_evidence_kind_is_boundary_scoped(kind);
}

static bool
air_global_evidence_kind_has_validator(AIREvidenceKind kind)
{
    return air_evidence_kind_has_global_validator(kind);
}

typedef enum
{
    AIR_GLOBAL_SHAPE_MIR,
    AIR_GLOBAL_SHAPE_RIR_PROPAGATION,
    AIR_GLOBAL_SHAPE_DAG,
    AIR_GLOBAL_SHAPE_OBSERVABILITY_SCHEMA,
    AIR_GLOBAL_SHAPE_RUNTIME_FRONTIER_POLICY
} AIRGlobalEvidenceShapeClass;

typedef struct
{
    AIREvidenceKind kind;
    AIRGlobalEvidenceShapeClass shape_class;
    const char *label;
    const char *expected_provider;
    const char *expected_subject;
    size_t exact_fact_count;
    bool exact_fact_count_required;
} AIRGlobalEvidencePolicy;

static const AIRGlobalEvidencePolicy kGlobalEvidencePolicies[] = {
    {
        AIR_EVIDENCE_MIR_CLEANUP,
        AIR_GLOBAL_SHAPE_MIR,
        "cleanup",
        NULL,
        "cleanup-block",
        0,
        false,
    },
    {
        AIR_EVIDENCE_MIR_TERMINATOR,
        AIR_GLOBAL_SHAPE_MIR,
        "terminator",
        NULL,
        "cfg-terminator",
        0,
        false,
    },
    {
        AIR_EVIDENCE_MIR_SELECT_RECEIVE,
        AIR_GLOBAL_SHAPE_MIR,
        "select receive",
        NULL,
        "select-receive",
        0,
        false,
    },
    {
        AIR_EVIDENCE_RIR_EFFECT_PROPAGATION,
        AIR_GLOBAL_SHAPE_RIR_PROPAGATION,
        "propagation",
        NULL,
        NULL,
        0,
        false,
    },
    {
        AIR_EVIDENCE_RIR_RELATION_PROPAGATION,
        AIR_GLOBAL_SHAPE_RIR_PROPAGATION,
        "propagation",
        NULL,
        NULL,
        0,
        false,
    },
    {
        AIR_EVIDENCE_DAG_METADATA,
        AIR_GLOBAL_SHAPE_DAG,
        "DAG",
        "type-resolution-dag",
        "metadata-inventory",
        0,
        false,
    },
    {
        AIR_EVIDENCE_DAG_GENERIC,
        AIR_GLOBAL_SHAPE_DAG,
        "DAG",
        "type-resolution-dag",
        "generic-contracts",
        0,
        false,
    },
    {
        AIR_EVIDENCE_DAG_ABILITY,
        AIR_GLOBAL_SHAPE_DAG,
        "DAG",
        "type-resolution-dag",
        "ability-consumers",
        0,
        false,
    },
    {
        AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
        AIR_GLOBAL_SHAPE_OBSERVABILITY_SCHEMA,
        "schema",
        "runtime-observability-schema",
        PGY_OBSERVABILITY_ABI_SCHEMA,
        0,
        false,
    },
    {
        AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
        AIR_GLOBAL_SHAPE_RUNTIME_FRONTIER_POLICY,
        "policy",
        PGY_FRONTIER_POLICY_SCHEMA,
        PGY_FRONTIER_POLICY_SUBJECT,
        PGY_FRONTIER_POLICY_FACT_COUNT,
        true,
    },
};

static const AIRGlobalEvidencePolicy *
air_global_evidence_policy_for_kind(AIREvidenceKind kind)
{
    for (size_t i = 0; i < sizeof(kGlobalEvidencePolicies)
             / sizeof(kGlobalEvidencePolicies[0]); i++) {
        if (kGlobalEvidencePolicies[i].kind == kind)
            return &kGlobalEvidencePolicies[i];
    }
    return NULL;
}

static bool
air_validate_global_evidence_fact_shape(
    const AIRGlobalEvidencePolicy *policy,
    const AIREvidenceNode *evidence,
    size_t evidence_index,
    char **error_message)
{
    size_t fact_count = air_evidence_node_fact_count(evidence);

    if (policy->exact_fact_count_required) {
        if (fact_count != policy->exact_fact_count) {
            air_set_invariant_error(
                error_message,
                "AIR runtime frontier policy evidence node %zu has invalid policy fact count; expected=%zu actual=%zu",
                evidence_index,
                policy->exact_fact_count,
                fact_count);
            return false;
        }
        return true;
    }

    if (fact_count > 0)
        return true;

    switch (policy->shape_class) {
    case AIR_GLOBAL_SHAPE_MIR:
        air_set_invariant_error(error_message,
                                "AIR MIR %s evidence node %zu has no %s facts",
                                policy->label,
                                evidence_index,
                                policy->label);
        return false;
    case AIR_GLOBAL_SHAPE_RIR_PROPAGATION:
        air_set_invariant_error(
            error_message,
            "AIR RIR propagation evidence node %zu has no propagation facts",
            evidence_index);
        return false;
    case AIR_GLOBAL_SHAPE_DAG:
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has no DAG facts",
                                evidence_index);
        return false;
    case AIR_GLOBAL_SHAPE_OBSERVABILITY_SCHEMA:
        air_set_invariant_error(
            error_message,
            "AIR observability schema evidence node %zu has no schema facts",
            evidence_index);
        return false;
    case AIR_GLOBAL_SHAPE_RUNTIME_FRONTIER_POLICY:
        break;
    }
    return true;
}

static bool
air_validate_global_evidence_fallback_shape(
    const AIRGlobalEvidencePolicy *policy,
    const AIREvidenceNode *evidence,
    size_t evidence_index,
    char **error_message)
{
    if (air_evidence_node_fallback_count(evidence) == 0)
        return true;

    switch (policy->shape_class) {
    case AIR_GLOBAL_SHAPE_MIR:
        air_set_invariant_error(
            error_message,
            "AIR MIR %s evidence node %zu has fallback %s facts",
            policy->label,
            evidence_index,
            policy->label);
        return false;
    case AIR_GLOBAL_SHAPE_RIR_PROPAGATION:
        air_set_invariant_error(
            error_message,
            "AIR RIR propagation evidence node %zu has fallback propagation facts",
            evidence_index);
        return false;
    case AIR_GLOBAL_SHAPE_DAG:
        air_set_invariant_error(
            error_message,
            "AIR DAG evidence node %zu has unresolved metadata dead-end facts",
            evidence_index);
        return false;
    case AIR_GLOBAL_SHAPE_OBSERVABILITY_SCHEMA:
        air_set_invariant_error(
            error_message,
            "AIR observability schema evidence node %zu has fallback schema facts",
            evidence_index);
        return false;
    case AIR_GLOBAL_SHAPE_RUNTIME_FRONTIER_POLICY:
        air_set_invariant_error(
            error_message,
            "AIR runtime frontier policy evidence node %zu has fallback policy facts",
            evidence_index);
        return false;
    }
    return true;
}

static bool
air_validate_global_evidence_name_shape(
    const AIRGlobalEvidencePolicy *policy,
    const AIREvidenceNode *evidence,
    size_t evidence_index,
    char **error_message)
{
    const char *provider_name =
        air_evidence_node_provider_name_or(evidence, NULL);
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);

    if (policy->expected_provider != NULL
        && !air_name_matches(provider_name, policy->expected_provider)) {
        if (policy->shape_class == AIR_GLOBAL_SHAPE_DAG) {
            air_set_invariant_error(
                error_message,
                "AIR DAG evidence node %zu has invalid provider '%s'",
                evidence_index,
                provider_name != NULL ? provider_name : "<null>");
        } else if (policy->shape_class
                   == AIR_GLOBAL_SHAPE_OBSERVABILITY_SCHEMA) {
            air_set_invariant_error(
                error_message,
                "AIR observability schema evidence node %zu has invalid provider '%s'",
                evidence_index,
                provider_name != NULL ? provider_name : "<null>");
        } else {
            air_set_invariant_error(
                error_message,
                "AIR runtime frontier policy evidence node %zu has invalid provider '%s'",
                evidence_index,
                provider_name != NULL ? provider_name : "<null>");
        }
        return false;
    }

    if (policy->expected_subject != NULL
        && !air_name_matches(subject_name, policy->expected_subject)) {
        if (policy->shape_class == AIR_GLOBAL_SHAPE_MIR) {
            air_set_invariant_error(
                error_message,
                "AIR MIR %s evidence node %zu has invalid %s subject '%s'",
                policy->label,
                evidence_index,
                policy->label,
                subject_name != NULL ? subject_name : "<null>");
        } else if (policy->shape_class == AIR_GLOBAL_SHAPE_DAG) {
            air_set_invariant_error(
                error_message,
                "AIR DAG evidence node %zu has invalid subject '%s'",
                evidence_index,
                subject_name != NULL ? subject_name : "<null>");
        } else if (policy->shape_class
                   == AIR_GLOBAL_SHAPE_OBSERVABILITY_SCHEMA) {
            air_set_invariant_error(
                error_message,
                "AIR observability schema evidence node %zu has invalid subject '%s'",
                evidence_index,
                subject_name != NULL ? subject_name : "<null>");
        } else {
            air_set_invariant_error(
                error_message,
                "AIR runtime frontier policy evidence node %zu has invalid subject '%s'",
                evidence_index,
                subject_name != NULL ? subject_name : "<null>");
        }
        return false;
    }
    return true;
}

static bool
air_validate_global_evidence_against_policy(const AIREvidenceNode *evidence,
                                            size_t evidence_index,
                                            char **error_message)
{
    const AIRGlobalEvidencePolicy *policy =
        air_global_evidence_policy_for_kind(air_evidence_node_kind(evidence));

    if (policy == NULL)
        return true;
    return air_validate_global_evidence_fact_shape(policy,
                                                   evidence,
                                                   evidence_index,
                                                   error_message)
        && air_validate_global_evidence_fallback_shape(policy,
                                                       evidence,
                                                       evidence_index,
                                                       error_message)
        && air_validate_global_evidence_name_shape(policy,
                                                   evidence,
                                                   evidence_index,
                                                   error_message);
}

bool
air_validate_global_evidence_node(const AIREvidenceNode *evidence,
                                  size_t evidence_index,
                                  char **error_message)
{
    AIREvidenceKind kind;

    if (evidence == NULL)
        return false;
    kind = air_evidence_node_kind(evidence);
    if (!air_evidence_node_has_declared_kind_facts(evidence)) {
        air_set_invariant_error(
            error_message,
            "AIR global evidence node %zu has typed evidence mismatch for kind '%s'",
            evidence_index,
            air_evidence_kind_name(kind));
        return false;
    }
    if (!air_global_evidence_kind_has_validator(kind)) {
        air_set_invariant_error(error_message,
                                "AIR global evidence node %zu has no global validator for kind '%s'",
                                evidence_index,
                                air_evidence_kind_name(kind));
        return false;
    }
    return air_validate_global_evidence_against_policy(evidence,
                                                       evidence_index,
                                                       error_message);
}
