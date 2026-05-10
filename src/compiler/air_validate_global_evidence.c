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
    switch (kind) {
    case AIR_EVIDENCE_MIR_CLEANUP:
    case AIR_EVIDENCE_MIR_TERMINATOR:
    case AIR_EVIDENCE_MIR_SELECT_RECEIVE:
    case AIR_EVIDENCE_DAG_METADATA:
    case AIR_EVIDENCE_DAG_GENERIC:
    case AIR_EVIDENCE_DAG_ABILITY:
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
    case AIR_EVIDENCE_OBSERVABILITY_SCHEMA:
    case AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY:
        return true;
    default:
        return false;
    }
}

static bool
air_validate_mir_global_evidence(const AIREvidenceNode *evidence,
                                 size_t evidence_index,
                                 char **error_message)
{
    const char *expected_subject = NULL;
    const char *label = NULL;

    if (evidence->kind == AIR_EVIDENCE_MIR_CLEANUP) {
        expected_subject = "cleanup-block";
        label = "cleanup";
    } else if (evidence->kind == AIR_EVIDENCE_MIR_TERMINATOR) {
        expected_subject = "cfg-terminator";
        label = "terminator";
    } else if (evidence->kind == AIR_EVIDENCE_MIR_SELECT_RECEIVE) {
        expected_subject = "select-receive";
        label = "select receive";
    } else {
        return true;
    }

    if (evidence->fact_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR MIR %s evidence node %zu has no %s facts",
                                label,
                                evidence_index,
                                label);
        return false;
    }
    if (evidence->fallback_count != 0) {
        air_set_invariant_error(error_message,
                                "AIR MIR %s evidence node %zu has fallback %s facts",
                                label,
                                evidence_index,
                                label);
        return false;
    }
    if (!air_name_matches(evidence->subject_name, expected_subject)) {
        air_set_invariant_error(error_message,
                                "AIR MIR %s evidence node %zu has invalid %s subject '%s'",
                                label,
                                evidence_index,
                                label,
                                evidence->subject_name != NULL
                                    ? evidence->subject_name
                                    : "<null>");
        return false;
    }
    return true;
}

static bool
air_validate_rir_propagation_evidence(const AIREvidenceNode *evidence,
                                      size_t evidence_index,
                                      char **error_message)
{
    if (evidence->kind != AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        && evidence->kind != AIR_EVIDENCE_RIR_RELATION_PROPAGATION) {
        return true;
    }
    if (evidence->fact_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR RIR propagation evidence node %zu has no propagation facts",
                                evidence_index);
        return false;
    }
    if (evidence->fallback_count != 0) {
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
    const char *expected_subject = "metadata-inventory";

    if (evidence->kind != AIR_EVIDENCE_DAG_METADATA
        && evidence->kind != AIR_EVIDENCE_DAG_GENERIC
        && evidence->kind != AIR_EVIDENCE_DAG_ABILITY) {
        return true;
    }
    if (evidence->kind == AIR_EVIDENCE_DAG_GENERIC)
        expected_subject = "generic-contracts";
    else if (evidence->kind == AIR_EVIDENCE_DAG_ABILITY)
        expected_subject = "ability-consumers";
    if (evidence->fact_count == 0 && evidence->fallback_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has no DAG facts",
                                evidence_index);
        return false;
    }
    if (evidence->fallback_count != 0) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has fallback DAG facts",
                                evidence_index);
        return false;
    }
    if (!air_name_matches(evidence->provider_name, "type-resolution-dag")) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has invalid provider '%s'",
                                evidence_index,
                                evidence->provider_name != NULL
                                    ? evidence->provider_name
                                    : "<null>");
        return false;
    }
    if (!air_name_matches(evidence->subject_name, expected_subject)) {
        air_set_invariant_error(error_message,
                                "AIR DAG evidence node %zu has invalid subject '%s'",
                                evidence_index,
                                evidence->subject_name != NULL
                                    ? evidence->subject_name
                                    : "<null>");
        return false;
    }
    return true;
}

static bool
air_validate_observability_schema_evidence(const AIREvidenceNode *evidence,
                                           size_t evidence_index,
                                           char **error_message)
{
    if (evidence->kind != AIR_EVIDENCE_OBSERVABILITY_SCHEMA)
        return true;
    if (evidence->fact_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has no schema facts",
                                evidence_index);
        return false;
    }
    if (evidence->fallback_count != 0) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has fallback schema facts",
                                evidence_index);
        return false;
    }
    if (!air_name_matches(evidence->provider_name,
                          "runtime-observability-schema")) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has invalid provider '%s'",
                                evidence_index,
                                evidence->provider_name != NULL
                                    ? evidence->provider_name
                                    : "<null>");
        return false;
    }
    if (!air_name_matches(evidence->subject_name,
                          PGY_OBSERVABILITY_ABI_SCHEMA)) {
        air_set_invariant_error(error_message,
                                "AIR observability schema evidence node %zu has invalid subject '%s'",
                                evidence_index,
                                evidence->subject_name != NULL
                                    ? evidence->subject_name
                                    : "<null>");
        return false;
    }
    return true;
}

static bool
air_validate_runtime_frontier_policy_evidence(const AIREvidenceNode *evidence,
                                              size_t evidence_index,
                                              char **error_message)
{
    if (evidence->kind != AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY)
        return true;
    if (evidence->fact_count != PGY_FRONTIER_POLICY_FACT_COUNT) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has invalid policy fact count",
                                evidence_index);
        return false;
    }
    if (evidence->fallback_count != 0) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has fallback policy facts",
                                evidence_index);
        return false;
    }
    if (!air_name_matches(evidence->provider_name,
                          PGY_FRONTIER_POLICY_SCHEMA)) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has invalid provider '%s'",
                                evidence_index,
                                evidence->provider_name != NULL
                                    ? evidence->provider_name
                                    : "<null>");
        return false;
    }
    if (!air_name_matches(evidence->subject_name,
                          PGY_FRONTIER_POLICY_SUBJECT)) {
        air_set_invariant_error(error_message,
                                "AIR runtime frontier policy evidence node %zu has invalid subject '%s'",
                                evidence_index,
                                evidence->subject_name != NULL
                                    ? evidence->subject_name
                                    : "<null>");
        return false;
    }
    return true;
}

bool
air_validate_global_evidence_node(const AIREvidenceNode *evidence,
                                  size_t evidence_index,
                                  char **error_message)
{
    if (evidence == NULL)
        return false;
    if (!air_global_evidence_kind_has_validator(evidence->kind)) {
        air_set_invariant_error(error_message,
                                "AIR global evidence node %zu has no global validator for kind '%s'",
                                evidence_index,
                                air_evidence_kind_name(evidence->kind));
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
