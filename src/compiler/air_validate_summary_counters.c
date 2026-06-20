/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR summary-counter validation owner. Summary counters are compatibility
 * telemetry; EvidenceNode inventory remains the proof source of truth.
 */

#include "air_internal.h"

#include <stdint.h>

size_t
air_evidence_summary_count(const AIRProgram *air, AIREvidenceKind kind)
{
    if (air == NULL)
        return 0;
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
        return air->hir_routine_evidence_count;
    case AIR_EVIDENCE_HIR_CFG:
        return air->hir_cfg_evidence_count;
    case AIR_EVIDENCE_RIR_BOUNDARY:
        return air->rir_boundary_evidence_count;
    case AIR_EVIDENCE_RIR_AUTHORITY:
        return air->rir_authority_evidence_count;
    case AIR_EVIDENCE_MIR_CLEANUP:
        return air->mir_cleanup_evidence_count;
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
        return air->mir_pin_cleanup_evidence_count;
    case AIR_EVIDENCE_MIR_TERMINATOR:
        return air->mir_terminator_evidence_count;
    case AIR_EVIDENCE_MIR_SELECT_RECEIVE:
        return air->mir_select_receive_evidence_count;
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
        return air->rir_effect_propagation_evidence_count;
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
        return air->rir_relation_propagation_evidence_count;
    case AIR_EVIDENCE_DAG_METADATA:
        return air->dag_metadata_evidence_count;
    case AIR_EVIDENCE_DAG_GENERIC:
        return air->dag_generic_evidence_count;
    case AIR_EVIDENCE_DAG_ABILITY:
        return air->dag_ability_evidence_count;
    case AIR_EVIDENCE_OBSERVABILITY_SCHEMA:
        return air->observability_schema_evidence_count;
    case AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY:
        return air->runtime_frontier_policy_evidence_count;
    default:
        return 0;
    }
}

bool
air_increment_evidence_summary_count(AIRProgram *air, AIREvidenceKind kind)
{
    size_t *slot = NULL;

    if (air == NULL)
        return false;
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
        slot = &air->hir_routine_evidence_count;
        break;
    case AIR_EVIDENCE_HIR_CFG:
        slot = &air->hir_cfg_evidence_count;
        break;
    case AIR_EVIDENCE_RIR_BOUNDARY:
        slot = &air->rir_boundary_evidence_count;
        break;
    case AIR_EVIDENCE_RIR_AUTHORITY:
        slot = &air->rir_authority_evidence_count;
        break;
    case AIR_EVIDENCE_MIR_CLEANUP:
        slot = &air->mir_cleanup_evidence_count;
        break;
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
        slot = &air->mir_pin_cleanup_evidence_count;
        break;
    case AIR_EVIDENCE_MIR_TERMINATOR:
        slot = &air->mir_terminator_evidence_count;
        break;
    case AIR_EVIDENCE_MIR_SELECT_RECEIVE:
        slot = &air->mir_select_receive_evidence_count;
        break;
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
        slot = &air->rir_effect_propagation_evidence_count;
        break;
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
        slot = &air->rir_relation_propagation_evidence_count;
        break;
    case AIR_EVIDENCE_DAG_METADATA:
        slot = &air->dag_metadata_evidence_count;
        break;
    case AIR_EVIDENCE_DAG_GENERIC:
        slot = &air->dag_generic_evidence_count;
        break;
    case AIR_EVIDENCE_DAG_ABILITY:
        slot = &air->dag_ability_evidence_count;
        break;
    case AIR_EVIDENCE_OBSERVABILITY_SCHEMA:
        slot = &air->observability_schema_evidence_count;
        break;
    case AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY:
        slot = &air->runtime_frontier_policy_evidence_count;
        break;
    default:
        return false;
    }
    if (*slot == SIZE_MAX)
        return false;
    (*slot)++;
    return true;
}

size_t
air_evidence_required_count(const AIRProgram *air, AIREvidenceKind kind)
{
    if (air == NULL)
        return 0;
    switch (kind) {
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
        return air->rir_effect_propagation_required_count;
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
        return air->rir_relation_propagation_required_count;
    default:
        return 0;
    }
}

bool
air_increment_evidence_required_count(AIRProgram *air, AIREvidenceKind kind)
{
    size_t *slot = NULL;

    if (air == NULL)
        return false;
    switch (kind) {
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
        slot = &air->rir_effect_propagation_required_count;
        break;
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
        slot = &air->rir_relation_propagation_required_count;
        break;
    default:
        return false;
    }
    if (*slot == SIZE_MAX)
        return false;
    (*slot)++;
    return true;
}

static bool
air_validate_mir_summary_counter(const AIRProgram *air,
                                 AIREvidenceKind kind,
                                 size_t summary_count,
                                 const char *label,
                                 char **error_message)
{
    size_t node_count;

    if (air == NULL || !air_has_mir_input(air))
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

static bool
air_validate_mir_boundary_summary_counter(const AIRProgram *air,
                                          AIREvidenceKind kind,
                                          size_t summary_count,
                                          const char *label,
                                          char **error_message)
{
    size_t node_count;

    if (air == NULL || !air_has_mir_input(air))
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
air_validate_rir_boundary_summary_counter(const AIRProgram *air,
                                          AIREvidenceKind kind,
                                          size_t summary_count,
                                          const char *label,
                                          char **error_message)
{
    size_t node_count;

    if (air == NULL || !air_has_rir_input(air))
        return true;
    node_count = air_boundary_evidence_node_count(air, kind);
    if (node_count == 0 || summary_count == node_count)
        return true;
    air_set_invariant_error(error_message,
                            "AIR RIR %s evidence counter does not match evidence nodes; summary=%zu nodes=%zu",
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

    if (air == NULL || !air_has_rir_input(air))
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
    if (air_requires_strict_evidence(air) && summary_count > 0 && node_count == 0)
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
    return air_validate_rir_boundary_summary_counter(
               air,
               AIR_EVIDENCE_RIR_BOUNDARY,
               air_evidence_summary_count(air, AIR_EVIDENCE_RIR_BOUNDARY),
               "boundary",
               error_message)
        && air_validate_rir_boundary_summary_counter(
               air,
               AIR_EVIDENCE_RIR_AUTHORITY,
               air_evidence_summary_count(air, AIR_EVIDENCE_RIR_AUTHORITY),
               "authority",
               error_message)
        && air_validate_mir_summary_counter(
               air,
               AIR_EVIDENCE_MIR_CLEANUP,
               air_evidence_summary_count(air, AIR_EVIDENCE_MIR_CLEANUP),
               "cleanup",
               error_message)
        && air_validate_mir_boundary_summary_counter(
               air,
               AIR_EVIDENCE_MIR_PIN_CLEANUP,
               air_evidence_summary_count(air, AIR_EVIDENCE_MIR_PIN_CLEANUP),
               "pin cleanup",
               error_message)
        && air_validate_mir_summary_counter(
               air,
               AIR_EVIDENCE_MIR_TERMINATOR,
               air_evidence_summary_count(air, AIR_EVIDENCE_MIR_TERMINATOR),
               "terminator",
               error_message)
        && air_validate_mir_summary_counter(
               air,
               AIR_EVIDENCE_MIR_SELECT_RECEIVE,
               air_evidence_summary_count(air, AIR_EVIDENCE_MIR_SELECT_RECEIVE),
               "select receive",
               error_message)
        && air_validate_rir_propagation_summary_counter(
               air,
               AIR_EVIDENCE_RIR_EFFECT_PROPAGATION,
               air_evidence_summary_count(air, AIR_EVIDENCE_RIR_EFFECT_PROPAGATION),
               "effect",
               error_message)
        && air_validate_rir_propagation_summary_counter(
               air,
               AIR_EVIDENCE_RIR_RELATION_PROPAGATION,
               air_evidence_summary_count(air, AIR_EVIDENCE_RIR_RELATION_PROPAGATION),
               "relation",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_DAG_METADATA,
               air_evidence_summary_count(air, AIR_EVIDENCE_DAG_METADATA),
               "DAG metadata",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_DAG_GENERIC,
               air_evidence_summary_count(air, AIR_EVIDENCE_DAG_GENERIC),
               "DAG generic",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_DAG_ABILITY,
               air_evidence_summary_count(air, AIR_EVIDENCE_DAG_ABILITY),
               "DAG ability",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_OBSERVABILITY_SCHEMA,
               air_evidence_summary_count(air, AIR_EVIDENCE_OBSERVABILITY_SCHEMA),
               "observability schema",
               error_message)
        && air_validate_global_node_summary_counter(
               air,
               AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY,
               air_evidence_summary_count(air, AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY),
               "runtime frontier policy",
               error_message);
}
