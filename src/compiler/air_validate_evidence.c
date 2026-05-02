/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR evidence inventory validation owner.
 */

#include "air_internal.h"
#include "../runtime/pgy_runtime_observability_schema.h"

static bool
air_evidence_kind_valid(AIREvidenceKind kind)
{
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
    case AIR_EVIDENCE_HIR_CFG:
    case AIR_EVIDENCE_RIR_BOUNDARY:
    case AIR_EVIDENCE_RIR_AUTHORITY:
    case AIR_EVIDENCE_MIR_CLEANUP:
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
    case AIR_EVIDENCE_DAG_METADATA:
    case AIR_EVIDENCE_DAG_GENERIC:
    case AIR_EVIDENCE_DAG_ABILITY:
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
    case AIR_EVIDENCE_OBSERVABILITY_SCHEMA:
        return true;
    }
    return false;
}

static bool
air_evidence_kind_is_global(AIREvidenceKind kind)
{
    return kind == AIR_EVIDENCE_DAG_GENERIC
        || kind == AIR_EVIDENCE_DAG_METADATA
        || kind == AIR_EVIDENCE_DAG_ABILITY
        || kind == AIR_EVIDENCE_MIR_CLEANUP
        || kind == AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        || kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION
        || kind == AIR_EVIDENCE_OBSERVABILITY_SCHEMA;
}

static bool
air_boundary_has_evidence_kind_provider(const AIRProgram *air,
                                        size_t boundary_index,
                                        AIREvidenceKind kind,
                                        const char *provider_name)
{
    if (air == NULL || boundary_index >= air->boundary_count)
        return false;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind
            && evidence->boundary_index == boundary_index
            && air_name_matches(evidence->provider_name, provider_name)) {
            return true;
        }
    }
    return false;
}

static bool
air_evidence_node_matches_boundary_shape(const AIRProgram *air,
                                         size_t evidence_index,
                                         char **error_message)
{
    const AIREvidenceNode *evidence;
    const AIRBoundaryNode *boundary;

    if (air == NULL || evidence_index >= air->evidence_count)
        return false;

    evidence = &air->evidence_nodes[evidence_index];
    if (air_evidence_kind_is_global(evidence->kind)) {
        if (evidence->boundary_index != SIZE_MAX) {
            air_set_invariant_error(error_message,
                                    "AIR global evidence node %zu is attached to boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (evidence->kind == AIR_EVIDENCE_MIR_CLEANUP) {
            if (evidence->fact_count == 0) {
                air_set_invariant_error(error_message,
                                        "AIR MIR cleanup evidence node %zu has no cleanup facts",
                                        evidence_index);
                return false;
            }
            if (evidence->fallback_count != 0) {
                air_set_invariant_error(error_message,
                                        "AIR MIR cleanup evidence node %zu has fallback cleanup facts",
                                        evidence_index);
                return false;
            }
            if (!air_name_matches(evidence->subject_name, "cleanup-block")) {
                air_set_invariant_error(error_message,
                                        "AIR MIR cleanup evidence node %zu has invalid cleanup subject '%s'",
                                        evidence_index,
                                        evidence->subject_name != NULL
                                            ? evidence->subject_name
                                            : "<null>");
                return false;
            }
        }
        if (evidence->kind == AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
            || evidence->kind == AIR_EVIDENCE_RIR_RELATION_PROPAGATION) {
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
        }
        if (evidence->kind == AIR_EVIDENCE_DAG_METADATA
            || evidence->kind == AIR_EVIDENCE_DAG_GENERIC
            || evidence->kind == AIR_EVIDENCE_DAG_ABILITY) {
            const char *expected_subject = "metadata-inventory";
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
        }
        if (evidence->kind == AIR_EVIDENCE_OBSERVABILITY_SCHEMA) {
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
        }
        return true;
    }

    if (evidence->boundary_index >= air->boundary_count) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu references missing boundary node %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }

    boundary = &air->boundaries[evidence->boundary_index];
    if (evidence->fact_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has no evidence facts",
                                evidence_index);
        return false;
    }
    if (evidence->fallback_count != 0) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has fallback evidence facts",
                                evidence_index);
        return false;
    }
    switch (evidence->kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
        if (!air_name_matches(evidence->subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR HIR routine evidence node %zu has subject/source mismatch for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_HIR_CFG:
        if (!air_name_matches(evidence->subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR HIR CFG evidence node %zu has subject/source mismatch for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (!air_boundary_has_evidence_kind_provider(air,
                                                     evidence->boundary_index,
                                                     AIR_EVIDENCE_HIR_ROUTINE,
                                                     evidence->provider_name)) {
            air_set_invariant_error(error_message,
                                    "AIR HIR CFG evidence node %zu has no matching HIR routine evidence for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_RIR_BOUNDARY:
        if (!air_boundary_requires_rir_evidence(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR boundary evidence node %zu is attached to non-RIR boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (!air_name_matches(evidence->subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR boundary evidence node %zu has subject/source mismatch for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_RIR_AUTHORITY:
        if (!boundary->authority_required) {
            air_set_invariant_error(error_message,
                                    "AIR RIR authority evidence node %zu is attached to non-authority boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (!air_boundary_has_evidence_kind_provider(air,
                                                     evidence->boundary_index,
                                                     AIR_EVIDENCE_RIR_BOUNDARY,
                                                     evidence->provider_name)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR authority evidence node %zu has no matching RIR boundary evidence for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (!air_boundary_declares_authority_name(boundary, evidence->subject_name)) {
            air_set_invariant_error(error_message,
                                    "AIR RIR authority evidence node %zu has undeclared authority subject for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
        if (!air_boundary_requires_mir_pin_cleanup_evidence(boundary)) {
            air_set_invariant_error(error_message,
                                    "AIR MIR pin cleanup evidence node %zu is attached to non-pin boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        if (air_name_matches(evidence->subject_name, boundary->source_name)) {
            air_set_invariant_error(error_message,
                                    "AIR MIR pin cleanup evidence node %zu has no slot anchor subject for boundary %zu",
                                    evidence_index,
                                    evidence->boundary_index);
            return false;
        }
        return true;
    case AIR_EVIDENCE_DAG_GENERIC:
    case AIR_EVIDENCE_DAG_METADATA:
    case AIR_EVIDENCE_DAG_ABILITY:
    case AIR_EVIDENCE_MIR_CLEANUP:
    case AIR_EVIDENCE_RIR_EFFECT_PROPAGATION:
    case AIR_EVIDENCE_RIR_RELATION_PROPAGATION:
    case AIR_EVIDENCE_OBSERVABILITY_SCHEMA:
        return true;
    }

    return false;
}

bool
air_validate_evidence_inventory(const AIRProgram *air, char **error_message)
{
    if (air == NULL)
        return false;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (!air_evidence_kind_valid(evidence->kind)) {
            air_set_invariant_error(error_message, "AIR evidence node %zu has invalid kind", i);
            return false;
        }
        if (evidence->boundary_index >= air->boundary_count
            && !(evidence->boundary_index == SIZE_MAX
                 && air_evidence_kind_is_global(evidence->kind))) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu references missing boundary node %zu",
                                    i,
                                    evidence->boundary_index);
            return false;
        }
        if (air_name_is_empty(evidence->provider_name)) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu has no provider provenance",
                                    i);
            return false;
        }
        if (air_name_is_empty(evidence->subject_name)) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu has no subject provenance",
                                    i);
            return false;
        }
        if (!air_evidence_node_matches_boundary_shape(air, i, error_message))
            return false;
    }
    return true;
}
