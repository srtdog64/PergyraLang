/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR evidence inventory validation owner.
 */

#include "air_internal.h"

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
    case AIR_EVIDENCE_MIR_TERMINATOR:
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
air_boundary_has_summary_flag(const AIRBoundaryNode *boundary,
                              AIREvidenceKind kind)
{
    if (boundary == NULL)
        return false;
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
        return boundary->has_hir_routine_evidence;
    case AIR_EVIDENCE_HIR_CFG:
        return boundary->has_hir_cfg_evidence;
    case AIR_EVIDENCE_RIR_BOUNDARY:
        return boundary->has_rir_boundary_evidence;
    case AIR_EVIDENCE_RIR_AUTHORITY:
        return boundary->has_rir_authority_evidence;
    default:
        return false;
    }
}

static bool
air_program_requires_summary_flag_for_evidence(const AIRProgram *air,
                                               AIREvidenceKind kind)
{
    if (air == NULL)
        return false;
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
    case AIR_EVIDENCE_HIR_CFG:
        return air->has_hir_input;
    case AIR_EVIDENCE_RIR_BOUNDARY:
    case AIR_EVIDENCE_RIR_AUTHORITY:
        return air->has_rir_input;
    default:
        return false;
    }
}

typedef struct
{
    AIREvidenceKind kind;
    const char     *label;
} AIRBoundaryEvidenceSummaryRule;

static const AIRBoundaryEvidenceSummaryRule kBoundaryEvidenceSummaryRules[] = {
    { AIR_EVIDENCE_HIR_ROUTINE, "HIR routine evidence" },
    { AIR_EVIDENCE_HIR_CFG, "HIR CFG evidence" },
    { AIR_EVIDENCE_RIR_BOUNDARY, "RIR boundary evidence" },
    { AIR_EVIDENCE_RIR_AUTHORITY, "RIR authority evidence" },
};

static bool
air_inventory_has_boundary_evidence_kind(const AIRProgram *air,
                                         size_t boundary_index,
                                         AIREvidenceKind kind)
{
    if (air == NULL || boundary_index >= air->boundary_count)
        return false;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind && evidence->boundary_index == boundary_index)
            return true;
    }
    return false;
}

static bool
air_evidence_nodes_duplicate(const AIREvidenceNode *left,
                             const AIREvidenceNode *right)
{
    if (left == NULL || right == NULL)
        return false;
    return left->kind == right->kind
        && left->boundary_index == right->boundary_index
        && air_name_matches(left->provider_name, right->provider_name)
        && air_name_matches(left->subject_name, right->subject_name);
}

static bool
air_validate_boundary_summary_evidence(const AIRProgram *air,
                                       size_t boundary_index,
                                       AIREvidenceKind kind,
                                       const char *label,
                                       char **error_message)
{
    const AIRBoundaryNode *boundary;

    if (air == NULL || boundary_index >= air->boundary_count)
        return false;
    boundary = &air->boundaries[boundary_index];
    if (!air_boundary_has_summary_flag(boundary, kind))
        return true;
    if (air_inventory_has_boundary_evidence_kind(air, boundary_index, kind))
        return true;
    air_set_invariant_error(error_message,
                            "AIR boundary node %zu has %s summary without evidence node",
                            boundary_index,
                            label);
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
        return air_validate_global_evidence_node(evidence,
                                                 evidence_index,
                                                 error_message);
    }

    if (evidence->boundary_index >= air->boundary_count) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu references missing boundary node %zu",
                                evidence_index,
                                evidence->boundary_index);
        return false;
    }

    boundary = &air->boundaries[evidence->boundary_index];
    if (air_program_requires_summary_flag_for_evidence(air, evidence->kind)
        && !air_boundary_has_summary_flag(boundary, evidence->kind)) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has no matching boundary summary flag",
                                evidence_index);
        return false;
    }
    if (evidence->fact_count == 0) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu has no evidence facts",
                                evidence_index);
        return false;
    }
    if (evidence->fact_count != 1) {
        air_set_invariant_error(error_message,
                                "AIR boundary evidence node %zu must carry exactly one boundary fact",
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
    case AIR_EVIDENCE_MIR_TERMINATOR:
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
        for (size_t j = 0; j < i; j++) {
            if (air_evidence_nodes_duplicate(&air->evidence_nodes[j], evidence)) {
                air_set_invariant_error(error_message,
                                        "AIR evidence node %zu duplicates evidence node %zu",
                                        i,
                                        j);
                return false;
            }
        }
        if (!air_evidence_node_matches_boundary_shape(air, i, error_message))
            return false;
    }
    if (air->evidence_count > 0) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            for (size_t j = 0;
                 j < sizeof(kBoundaryEvidenceSummaryRules)
                    / sizeof(kBoundaryEvidenceSummaryRules[0]);
                 j++) {
                const AIRBoundaryEvidenceSummaryRule *rule =
                    &kBoundaryEvidenceSummaryRules[j];
                if (!air_validate_boundary_summary_evidence(
                        air, i, rule->kind, rule->label, error_message)) {
                    return false;
                }
            }
        }
    }
    return true;
}
