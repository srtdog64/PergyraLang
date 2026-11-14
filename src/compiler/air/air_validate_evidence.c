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
air_boundary_mark_summary_flag(AIRBoundaryNode *boundary, AIREvidenceKind kind)
{
    if (boundary == NULL)
        return false;
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
        boundary->has_hir_routine_evidence = true;
        return true;
    case AIR_EVIDENCE_HIR_CFG:
        boundary->has_hir_cfg_evidence = true;
        return true;
    case AIR_EVIDENCE_RIR_BOUNDARY:
        boundary->has_rir_boundary_evidence = true;
        return true;
    case AIR_EVIDENCE_RIR_AUTHORITY:
        boundary->has_rir_authority_evidence = true;
        return true;
    default:
        return false;
    }
}

bool
air_evidence_inventory_is_authoritative(const AIRProgram *air)
{
    return air != NULL
        && (air_evidence_node_count(air) > 0
            || air_has_hir_input(air)
            || air_has_rir_input(air)
            || air_has_mir_input(air));
}

static bool
air_evidence_node_matches_scope(const AIREvidenceNode *evidence,
                                size_t boundary_index,
                                AIREvidenceKind kind)
{
    return evidence != NULL
        && air_evidence_node_kind(evidence) == kind
        && air_evidence_node_has_declared_kind_facts(evidence)
        && air_evidence_node_boundary_index_or(evidence, SIZE_MAX)
            == boundary_index;
}

static bool
air_evidence_node_matches_subject(const AIREvidenceNode *evidence,
                                  size_t boundary_index,
                                  AIREvidenceKind kind,
                                  const char *subject_name)
{
    return air_evidence_node_matches_scope(evidence, boundary_index, kind)
        && air_name_matches(
            air_evidence_node_subject_name_or(evidence, NULL),
            subject_name);
}

static bool
air_evidence_node_matches_provider(const AIREvidenceNode *evidence,
                                   size_t boundary_index,
                                   AIREvidenceKind kind,
                                   const char *provider_name)
{
    return air_evidence_node_matches_scope(evidence, boundary_index, kind)
        && air_name_matches(
            air_evidence_node_provider_name_or(evidence, NULL),
            provider_name);
}

const AIREvidenceNode *
air_boundary_evidence_node(const AIRProgram *air,
                           size_t boundary_index,
                           AIREvidenceKind kind)
{
    size_t evidence_count;

    if (air == NULL || boundary_index >= air_boundary_node_count(air))
        return NULL;
    evidence_count = air_evidence_node_count(air);
    for (size_t i = 0; i < evidence_count; i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (air_evidence_node_matches_scope(evidence, boundary_index, kind))
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
    return air_evidence_node_provider_name_or(evidence, "<none>");
}

const char *
air_boundary_evidence_subject(const AIRProgram *air,
                              size_t boundary_index,
                              AIREvidenceKind kind)
{
    const AIREvidenceNode *evidence =
        air_boundary_evidence_node(air, boundary_index, kind);
    return air_evidence_node_subject_name_or(evidence, "<none>");
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
    size_t evidence_count;

    if (air == NULL
        || boundary_index >= air_boundary_node_count(air)
        || air_name_is_empty(subject_name)) {
        return false;
    }
    evidence_count = air_evidence_node_count(air);
    for (size_t i = 0; i < evidence_count; i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (air_evidence_node_matches_subject(evidence,
                                              boundary_index,
                                              kind,
                                              subject_name)) {
            return true;
        }
    }
    return false;
}

bool
air_boundary_has_evidence_kind_provider(const AIRProgram *air,
                                        size_t boundary_index,
                                        AIREvidenceKind kind,
                                        const char *provider_name)
{
    size_t evidence_count;

    if (air == NULL
        || boundary_index >= air_boundary_node_count(air)
        || air_name_is_empty(provider_name)) {
        return false;
    }
    evidence_count = air_evidence_node_count(air);
    for (size_t i = 0; i < evidence_count; i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (air_evidence_node_matches_provider(evidence,
                                               boundary_index,
                                               kind,
                                               provider_name)) {
            return true;
        }
    }
    return false;
}

const AIREvidenceNode *
air_global_evidence_node_provider_subject(const AIRProgram *air,
                                          AIREvidenceKind kind,
                                          const char *provider_name,
                                          const char *subject_name)
{
    if (air == NULL)
        return NULL;
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *node = air_evidence_node_at(air, i);
        if (air_evidence_node_matches_provider(node,
                                               SIZE_MAX,
                                               kind,
                                               provider_name)
            && air_name_matches(
                air_evidence_node_subject_name_or(node, NULL),
                subject_name)) {
            return node;
        }
    }
    return NULL;
}

bool
air_has_global_evidence_provider_subject(const AIRProgram *air,
                                         AIREvidenceKind kind,
                                         const char *provider_name,
                                         const char *subject_name)
{
    return air_global_evidence_node_provider_subject(air,
                                                     kind,
                                                     provider_name,
                                                     subject_name) != NULL;
}

bool
air_has_global_evidence_provider(const AIRProgram *air,
                                 AIREvidenceKind kind,
                                 const char *provider_name)
{
    if (air == NULL || air_name_is_empty(provider_name))
        return false;
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *node = air_evidence_node_at(air, i);
        if (air_evidence_node_matches_provider(node,
                                               SIZE_MAX,
                                               kind,
                                               provider_name)) {
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
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL)
            continue;
        if (air_evidence_node_kind(evidence) == kind
            && air_evidence_node_boundary_index_or(evidence, 0) == SIZE_MAX)
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
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL)
            continue;
        if (air_evidence_node_kind(evidence) == kind
            && air_evidence_node_boundary_index_or(evidence, 0) == SIZE_MAX)
            count += air_evidence_node_fact_count(evidence);
    }
    return count;
}

size_t
air_global_evidence_fallback_count(const AIRProgram *air, AIREvidenceKind kind)
{
    size_t count = 0;

    if (air == NULL || !air_evidence_kind_is_global(kind))
        return 0;
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL)
            continue;
        if (air_evidence_node_kind(evidence) == kind
            && air_evidence_node_boundary_index_or(evidence, 0) == SIZE_MAX)
            count += air_evidence_node_fallback_count(evidence);
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
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL)
            continue;
        if (air_evidence_node_kind(evidence) == kind
            && air_evidence_node_boundary_index_or(evidence, SIZE_MAX)
                != SIZE_MAX)
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

    if (air == NULL || boundary_index >= air_boundary_node_count(air))
        return false;
    if (air_evidence_inventory_is_authoritative(air)) {
        if (air_evidence_node_count(air) > 0)
            return air_boundary_has_evidence_kind(air, boundary_index, kind);
        return false;
    }
    boundary = air_boundary_node_at(air, boundary_index);
    if (boundary == NULL)
        return false;
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
            : air_boundary_first_authority_name_or(boundary, "<authority>");
    }
    for (size_t i = 0;
         i < air_boundary_authority_name_count(boundary);
         i++) {
        const char *name = air_boundary_authority_name_at(boundary, i);
        if (!air_boundary_has_evidence_kind_subject(
                air,
                boundary_index,
                AIR_EVIDENCE_RIR_AUTHORITY,
                name)) {
            return name;
        }
    }
    return air_boundary_authority_name_count(boundary) == 0
        ? "<authority>"
        : NULL;
}

static bool
air_evidence_nodes_duplicate(const AIREvidenceNode *left,
                             const AIREvidenceNode *right)
{
    if (left == NULL || right == NULL)
        return false;
    return air_evidence_node_kind(left) == air_evidence_node_kind(right)
        && air_evidence_node_boundary_index_or(left, SIZE_MAX)
            == air_evidence_node_boundary_index_or(right, SIZE_MAX)
        && air_name_matches(
            air_evidence_node_provider_name_or(left, NULL),
            air_evidence_node_provider_name_or(right, NULL))
        && air_name_matches(
            air_evidence_node_subject_name_or(left, NULL),
            air_evidence_node_subject_name_or(right, NULL));
}

bool
air_validate_evidence_inventory(const AIRProgram *air, char **error_message)
{
    if (air == NULL)
        return false;
    for (size_t i = 0; i < air_evidence_node_count(air); i++) {
        const AIREvidenceNode *evidence = air_evidence_node_at(air, i);
        if (evidence == NULL) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu is missing", i);
            return false;
        }
        AIREvidenceKind kind = air_evidence_node_kind(evidence);
        size_t boundary_index =
            air_evidence_node_boundary_index_or(evidence, SIZE_MAX);
        if (!air_evidence_kind_is_known(kind)) {
            air_set_invariant_error(error_message, "AIR evidence node %zu has invalid kind", i);
            return false;
        }
        if (!air_evidence_node_has_declared_kind_facts(evidence)) {
            air_set_invariant_error(
                error_message,
                "AIR evidence node %zu has typed evidence mismatch; kind=%s provider_kind=%s subject_kind=%s",
                i,
                air_evidence_kind_name(kind),
                air_evidence_provider_kind_name(
                    air_evidence_node_provider_kind(evidence)),
                air_evidence_subject_kind_name(
                    air_evidence_node_subject_kind(evidence)));
            return false;
        }
        if (boundary_index >= air_boundary_node_count(air)
            && !(boundary_index == SIZE_MAX
                 && air_evidence_kind_is_global(kind))) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu references missing boundary node %zu",
                                    i,
                                    boundary_index);
            return false;
        }
        if (air_name_is_empty(
                air_evidence_node_provider_name_or(evidence, NULL))) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu has no provider provenance",
                                    i);
            return false;
        }
        if (air_name_is_empty(
                air_evidence_node_subject_name_or(evidence, NULL))) {
            air_set_invariant_error(error_message,
                                    "AIR evidence node %zu has no subject provenance",
                                    i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const AIREvidenceNode *previous = air_evidence_node_at(air, j);
            if (air_evidence_nodes_duplicate(previous, evidence)) {
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
        for (size_t i = 0; i < air_boundary_node_count(air); i++) {
            if (!air_validate_boundary_summary_inventory(
                    air, i, error_message)) {
                return false;
            }
        }
    }
    return true;
}
