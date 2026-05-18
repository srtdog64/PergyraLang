/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR evidence inventory validation owner.
 */

#include "air_internal.h"

bool
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

bool
air_evidence_inventory_is_authoritative(const AIRProgram *air)
{
    return air != NULL
        && (air->evidence_count > 0
            || air->has_hir_input
            || air->has_rir_input
            || air->has_mir_input);
}

const AIREvidenceNode *
air_boundary_evidence_node(const AIRProgram *air,
                           size_t boundary_index,
                           AIREvidenceKind kind)
{
    if (air == NULL || boundary_index >= air->boundary_count)
        return NULL;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind && evidence->boundary_index == boundary_index)
            return evidence;
    }
    return NULL;
}

const char *
air_boundary_evidence_provider(const AIRProgram *air,
                               size_t boundary_index,
                               AIREvidenceKind kind)
{
    const AIREvidenceNode *evidence =
        air_boundary_evidence_node(air, boundary_index, kind);
    return evidence != NULL && evidence->provider_name != NULL
        ? evidence->provider_name
        : "<none>";
}

const char *
air_boundary_evidence_subject(const AIRProgram *air,
                              size_t boundary_index,
                              AIREvidenceKind kind)
{
    const AIREvidenceNode *evidence =
        air_boundary_evidence_node(air, boundary_index, kind);
    return evidence != NULL && evidence->subject_name != NULL
        ? evidence->subject_name
        : "<none>";
}

bool
air_boundary_has_evidence_kind(const AIRProgram *air,
                               size_t boundary_index,
                               AIREvidenceKind kind)
{
    return air_boundary_evidence_node(air, boundary_index, kind) != NULL;
}

bool
air_boundary_has_evidence_kind_subject(const AIRProgram *air,
                                       size_t boundary_index,
                                       AIREvidenceKind kind,
                                       const char *subject_name)
{
    if (air == NULL
        || boundary_index >= air->boundary_count
        || air_name_is_empty(subject_name)) {
        return false;
    }
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind
            && evidence->boundary_index == boundary_index
            && air_name_matches(evidence->subject_name, subject_name)) {
            return true;
        }
    }
    return false;
}

size_t
air_global_evidence_node_count(const AIRProgram *air, AIREvidenceKind kind)
{
    size_t count = 0;

    if (air == NULL || !air_evidence_kind_is_global(kind))
        return 0;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind && evidence->boundary_index == SIZE_MAX)
            count++;
    }
    return count;
}

size_t
air_global_evidence_fact_count(const AIRProgram *air, AIREvidenceKind kind)
{
    size_t count = 0;

    if (air == NULL || !air_evidence_kind_is_global(kind))
        return 0;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind && evidence->boundary_index == SIZE_MAX)
            count += evidence->fact_count;
    }
    return count;
}

size_t
air_global_evidence_fallback_count(const AIRProgram *air, AIREvidenceKind kind)
{
    size_t count = 0;

    if (air == NULL || !air_evidence_kind_is_global(kind))
        return 0;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (evidence->kind == kind && evidence->boundary_index == SIZE_MAX)
            count += evidence->fallback_count;
    }
    return count;
}

bool
air_global_has_evidence_kind(const AIRProgram *air, AIREvidenceKind kind)
{
    return air_global_evidence_node_count(air, kind) > 0;
}

size_t
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

bool
air_boundary_has_evidence(const AIRProgram *air,
                          size_t boundary_index,
                          AIREvidenceKind kind)
{
    const AIRBoundaryNode *boundary;

    if (air == NULL || boundary_index >= air->boundary_count)
        return false;
    if (air_evidence_inventory_is_authoritative(air)) {
        if (air->evidence_count > 0)
            return air_boundary_has_evidence_kind(air, boundary_index, kind);
        return false;
    }
    boundary = &air->boundaries[boundary_index];
    return air_boundary_has_summary_flag(boundary, kind);
}

const char *
air_boundary_missing_authority_evidence(const AIRProgram *air,
                                        const AIRBoundaryNode *boundary,
                                        size_t boundary_index)
{
    if (air == NULL || boundary == NULL || !boundary->authority_required)
        return NULL;
    if (!air_evidence_inventory_is_authoritative(air)) {
        return air_boundary_has_evidence(air, boundary_index,
                                         AIR_EVIDENCE_RIR_AUTHORITY)
            ? NULL
            : (boundary->authority_name_count > 0
                ? boundary->authority_names[0]
                : "<authority>");
    }
    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        const char *name = boundary->authority_names[i];
        if (!air_boundary_has_evidence_kind_subject(
                air,
                boundary_index,
                AIR_EVIDENCE_RIR_AUTHORITY,
                name)) {
            return name;
        }
    }
    return boundary->authority_name_count == 0 ? "<authority>" : NULL;
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

bool
air_validate_evidence_inventory(const AIRProgram *air, char **error_message)
{
    if (air == NULL)
        return false;
    for (size_t i = 0; i < air->evidence_count; i++) {
        const AIREvidenceNode *evidence = &air->evidence_nodes[i];
        if (!air_evidence_kind_is_known(evidence->kind)) {
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
    if (!air_validate_summary_counters(air, error_message))
        return false;
    if (air_evidence_inventory_is_authoritative(air)) {
        for (size_t i = 0; i < air->boundary_count; i++) {
            if (!air_validate_boundary_summary_inventory(
                    air, i, error_message)) {
                return false;
            }
        }
    }
    return true;
}
