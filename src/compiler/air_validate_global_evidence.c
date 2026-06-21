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

static bool
air_validate_mir_global_evidence(const AIREvidenceNode *evidence,
                                 size_t evidence_index,
                                 char **error_message)
{
    AIREvidenceKind kind = air_evidence_node_kind(evidence);
    const char *expected_subject = NULL;
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);
    const char *label = NULL;

    if (kind == AIR_EVIDENCE_MIR_CLEANUP) {
        expected_subject = "cleanup-block";
        label = "cleanup";
    } else if (kind == AIR_EVIDENCE_MIR_TERMINATOR) {
        expected_subject = "cfg-terminator";
        label = "terminator";
    } else if (kind == AIR_EVIDENCE_MIR_SELECT_RECEIVE) {
        expected_subject = "select-receive";
        label = "select receive";
    } else {
        return true;
    }

    if (air_evidence_node_fact_count(evidence) == 0) {
        air_set_invariant_error(error_message,
                                "AIR MIR %s evidence node %zu has no %s facts",
                                label,
                                evidence_index,
                                label);
        return false;
    }
    if (air_evidence_node_fallback_count(evidence) != 0) {
        air_set_invariant_error(error_message,
                                "AIR MIR %s evidence node %zu has fallback %s facts",
                                label,
                                evidence_index,
                                label);
        return false;
    }
    if (!air_name_matches(subject_name, expected_subject)) {
        air_set_invariant_error(error_message,
                                "AIR MIR %s evidence node %zu has invalid %s subject '%s'",
                                label,
                                evidence_index,
                                label,
                                subject_name != NULL ? subject_name : "<null>");
        return false;
    }
    return true;
}

static bool
air_validate_rir_propagation_evidence(const AIREvidenceNode *evidence,
                                      size_t evidence_index,
                                      char **error_message)
{
    AIREvidenceKind kind = air_evidence_node_kind(evidence);

    if (kind != AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        && kind != AIR_EVIDENCE_RIR_RELATION_PROPAGATION) {
        return true;
    }
    if (air_evidence_node_fact_count(evidence) == 0) {
        air_set_invariant_error(error_message,
                                "AIR RIR propagation evidence node %zu has no propagation facts",
                                evidence_index);
        return false;
    }
    if (air_evidence_node_fallback_count(evidence) != 0) {
        air_set_invariant_error(error_message,
                                "AIR RIR propagation evidence node %zu has fallback propagation facts",
                                evidence_index);
        return false;
    }
    return true;
}

static bool
air_validate_dag_global_evidence(const AIREvidenceNode *evidence,
                                 size_t evidence_index,
                                 char **error_message)
{
    AIREvidenceKind kind = air_evidence_node_kind(evidence);
    const char *expected_subject = "metadata-inventory";
    const char *provider_name =
        air_evidence_node_provider_name_or(evidence, NULL);
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);

    if (kind != AIR_EVIDENCE_DAG_METADATA
        && kind != AIR_EVIDENCE_DAG_GENERIC
        && kind != AIR_EVIDENCE_DAG_ABILITY) {
        return true;
    }
    if (kind == AIR_EVIDENCE_DAG_GENERIC)
        expected_subject = "generic-contracts";
    else if (kind == AIR_EVIDENCE_DAG_ABILITY)
        expected_subject = "ability-consumers";
    if (air_evidence_node_fact_count(evidence) == 0
        && air_evidence_node_fallback_count(evidence) == 0) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has no DAG facts",
                                evidence_index);
        return false;
    }
    if (air_evidence_node_fallback_count(evidence) != 0) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has unresolved metadata dead-end facts",
                                evidence_index);
        return false;
    }
    if (!air_name_matches(provider_name, "type-resolution-dag")) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has invalid provider '%s'",
                                evidence_index,
                                provider_name != NULL ? provider_name : "<null>");
        return false;
    }
    if (!air_name_matches(subject_name, expected_subject)) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has invalid subject '%s'",
                                evidence_index,
                                subject_name != NULL ? subject_name : "<null>");
        return false;
    }
    return true;
}

static bool
air_validate_observability_schema_evidence(const AIREvidenceNode *evidence,
                                           size_t evidence_index,
                                           char **error_message)
{
    const char *provider_name =
        air_evidence_node_provider_name_or(evidence, NULL);
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);

    if (air_evidence_node_kind(evidence) != AIR_EVIDENCE_OBSERVABILITY_SCHEMA)
        return true;
    if (air_evidence_node_fact_count(evidence) == 0) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has no schema facts",
                                evidence_index);
        return false;
    }
    if (air_evidence_node_fallback_count(evidence) != 0) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has fallback schema facts",
                                evidence_index);
        return false;
    }
    if (!air_name_matches(provider_name,
                          "runtime-observability-schema")) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has invalid provider '%s'",
                                evidence_index,
                                provider_name != NULL ? provider_name : "<null>");
        return false;
    }
    if (!air_name_matches(subject_name,
                          PGY_OBSERVABILITY_ABI_SCHEMA)) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has invalid subject '%s'",
                                evidence_index,
                                subject_name != NULL ? subject_name : "<null>");
        return false;
    }
    return true;
}

static bool
air_validate_runtime_frontier_policy_evidence(const AIREvidenceNode *evidence,
                                              size_t evidence_index,
                                              char **error_message)
{
    const char *provider_name =
        air_evidence_node_provider_name_or(evidence, NULL);
    const char *subject_name =
        air_evidence_node_subject_name_or(evidence, NULL);
    size_t fact_count = air_evidence_node_fact_count(evidence);

    if (air_evidence_node_kind(evidence) != AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY)
        return true;
    if (fact_count != PGY_FRONTIER_POLICY_FACT_COUNT) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has invalid policy fact count; expected=%zu actual=%zu",
                                evidence_index,
                                (size_t)PGY_FRONTIER_POLICY_FACT_COUNT,
                                fact_count);
        return false;
    }
    if (air_evidence_node_fallback_count(evidence) != 0) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has fallback policy facts",
                                evidence_index);
        return false;
    }
    if (!air_name_matches(provider_name,
                          PGY_FRONTIER_POLICY_SCHEMA)) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has invalid provider '%s'",
                                evidence_index,
                                provider_name != NULL ? provider_name : "<null>");
        return false;
    }
    if (!air_name_matches(subject_name,
                          PGY_FRONTIER_POLICY_SUBJECT)) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has invalid subject '%s'",
                                evidence_index,
                                subject_name != NULL ? subject_name : "<null>");
        return false;
    }
    return true;
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
    return air_validate_mir_global_evidence(evidence, evidence_index, error_message)
        && air_validate_rir_propagation_evidence(evidence, evidence_index, error_message)
        && air_validate_dag_global_evidence(evidence, evidence_index, error_message)
        && air_validate_observability_schema_evidence(evidence,
                                                      evidence_index,
                                                      error_message)
        && air_validate_runtime_frontier_policy_evidence(evidence,
                                                      evidence_index,
                                                      error_message);
}
