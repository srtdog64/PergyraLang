/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR summary-counter validation owner. Summary counters are compatibility
 * telemetry; EvidenceNode inventory remains the proof source of truth.
 */

#include "air_internal.h"

static bool
air_validate_mir_summary_counter(const AIRProgram *air,
                                 AIREvidenceKind kind,
                                 size_t summary_count,
                                 const char *label,
                                 char **error_message)
{
    size_t node_count;

    if (air == NULL || !air->has_mir_input)
        return true;
    node_count = air_global_evidence_node_count(air, kind);
    if (node_count == 0 || summary_count == node_count)
        return true;
    air_set_invariant_error(error_message,
                            "AIR MIR %s evidence counter does not match evidence nodes; summary=%zu nodes=%zu",
                            label,
                            summary_count,
                            node_count);
    return false;
}

static size_t
air_boundary_evidence_node_count(const AIRProgram *air, AIREvidenceKind kind)
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
air_validate_mir_boundary_summary_counter(const AIRProgram *air,
                                          AIREvidenceKind kind,
                                          size_t summary_count,
                                          const char *label,
                                          char **error_message)
{
    size_t node_count;

    if (air == NULL || !air->has_mir_input)
        return true;
    node_count = air_boundary_evidence_node_count(air, kind);
    if (node_count == 0 || summary_count == node_count)
        return true;
    air_set_invariant_error(error_message,
                            "AIR MIR %s evidence counter does not match evidence nodes; summary=%zu nodes=%zu",
                            label,
                            summary_count,
                            node_count);
    return false;
}

static bool
air_validate_rir_propagation_summary_counter(const AIRProgram *air,
                                             AIREvidenceKind kind,
                                             size_t summary_count,
                                             const char *label,
                                             char **error_message)
{
    size_t fact_count;

    if (air == NULL || !air->has_rir_input)
        return true;
    fact_count = air_global_evidence_fact_count(air, kind);
    if (summary_count == fact_count)
        return true;
    air_set_invariant_error(error_message,
                            "AIR RIR %s propagation evidence counter does not match evidence facts; summary=%zu facts=%zu",
                            label,
                            summary_count,
                            fact_count);
    return false;
}

static bool
air_validate_global_node_summary_counter(const AIRProgram *air,
                                         AIREvidenceKind kind,
                                         size_t summary_count,
                                         const char *label,
                                         char **error_message)
{
    size_t node_count;

    if (air == NULL)
        return true;
    node_count = air_global_evidence_node_count(air, kind);
    if (summary_count == 0)
        return true;
    if (air->strict_evidence && summary_count > 0 && node_count == 0)
        return true;
    if (summary_count == node_count)
        return true;
    air_set_invariant_error(error_message,
                            "AIR %s evidence counter does not match evidence nodes; summary=%zu nodes=%zu",
                            label,
                            summary_count,
                            node_count);
    return false;
}

bool
air_validate_summary_counters(const AIRProgram *air, char **error_message)
{
    return air_validate_mir_summary_counter(
               air,
               AIR_EVIDENCE_MIR_CLEANUP,
               air != NULL ? air->mir_cleanup_evidence_count : 0,
               "cleanup",
               error_message)
        && air_validate_mir_boundary_summary_counter(
               air,
               AIR_EVIDENCE_MIR_PIN_CLEANUP,
               air != NULL ? air->mir_pin_cleanup_evidence_count : 0,
               "pin cleanup",
               error_message)
        && air_validate_mir_summary_counter(
               air,
               AIR_EVIDENCE_MIR_TERMINATOR,
               air != NULL ? air->mir_terminator_evidence_count : 0,
               "terminator",
               error_message)
        && air_validate_mir_summary_counter(
               air,
               AIR_EVIDENCE_MIR_SELECT_RECEIVE,
               air != NULL ? air->mir_select_receive_evidence_count : 0,
               "select receive",
               error_message)
        && air_validate_rir_propagation_summary_counter(
               air,
               AIR_EVIDENCE_RIR_EFFECT_PROPAGATION,
               air != NULL ? air->rir_effect_propagation_evidence_count : 0,
               "effect",
               error_message)
        && air_validate_rir_propagation_summary_counter(
               air,
               AIR_EVIDENCE_RIR_RELATION_PROPAGATION,
               air != NULL ? air->rir_relation_propagation_evidence_count : 0,
               "relation",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_DAG_METADATA,
               air != NULL ? air->dag_metadata_evidence_count : 0,
               "DAG metadata",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_DAG_GENERIC,
               air != NULL ? air->dag_generic_evidence_count : 0,
               "DAG generic",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_DAG_ABILITY,
               air != NULL ? air->dag_ability_evidence_count : 0,
               "DAG ability",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
               air != NULL ? air->observability_schema_evidence_count : 0,
               "observability schema",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
               air != NULL ? air->runtime_frontier_policy_evidence_count : 0,
               "runtime frontier policy",
               error_message);
}
