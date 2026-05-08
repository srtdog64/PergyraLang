/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR evidence-node inventory owner.
 */

#include "air_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool
air_evidence_kind_is_boundary_scoped(AIREvidenceKind kind)
{
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
    case AIR_EVIDENCE_HIR_CFG:
    case AIR_EVIDENCE_RIR_BOUNDARY:
    case AIR_EVIDENCE_RIR_AUTHORITY:
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
        return true;
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
        return false;
    }
    return false;
}

bool
air_evidence_kind_is_known(AIREvidenceKind kind)
{
    switch (kind) {
    case AIR_EVIDENCE_HIR_ROUTINE:
    case AIR_EVIDENCE_HIR_CFG:
    case AIR_EVIDENCE_RIR_BOUNDARY:
    case AIR_EVIDENCE_RIR_AUTHORITY:
    case AIR_EVIDENCE_MIR_PIN_CLEANUP:
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
    }
    return false;
}

static bool
air_evidence_node_merge_counts(AIREvidenceNode *node,
                               AIREvidenceKind kind,
                               size_t fact_count,
                               size_t fallback_count,
                               char **error_message)
{
    if (node == NULL)
        return false;
    if (air_evidence_kind_is_boundary_scoped(kind)) {
        if (fact_count != 1 || fallback_count != 0) {
            air_set_error(error_message,
                          "AIR boundary evidence duplicate has invalid counts");
            return false;
        }
        return true;
    }
    if (fact_count > SIZE_MAX - node->fact_count
        || fallback_count > SIZE_MAX - node->fallback_count) {
        air_set_error(error_message, "AIR evidence node count overflow");
        return false;
    }
    node->fact_count += fact_count;
    node->fallback_count += fallback_count;
    return true;
}

bool
air_append_evidence_node(AIRProgram *air,
                         AIREvidenceKind kind,
                         size_t boundary_index,
                         const char *provider_name,
                         const char *subject_name,
                         char **error_message)
{
    return air_append_evidence_node_ex(air,
                                       kind,
                                       boundary_index,
                                       provider_name,
                                       subject_name,
                                       1,
                                       0,
                                       error_message);
}

bool
air_append_evidence_node_ex(AIRProgram *air,
                            AIREvidenceKind kind,
                            size_t boundary_index,
                            const char *provider_name,
                            const char *subject_name,
                            size_t fact_count,
                            size_t fallback_count,
                            char **error_message)
{
    AIREvidenceNode *node;

    if (air == NULL) {
        air_set_error(error_message, "AIR evidence append requires a program");
        return false;
    }
    for (size_t i = 0; i < air->evidence_count; i++) {
        node = &air->evidence_nodes[i];
        if (node->kind == kind
            && node->boundary_index == boundary_index
            && air_name_matches(node->provider_name, provider_name)
            && air_name_matches(node->subject_name, subject_name)) {
            return air_evidence_node_merge_counts(node,
                                                  kind,
                                                  fact_count,
                                                  fallback_count,
                                                  error_message);
        }
    }
    if (air->evidence_count >= air->evidence_capacity) {
        AIREvidenceNode *next;
        size_t new_capacity = air->evidence_capacity;
        if (!air_next_capacity(&new_capacity, 16, sizeof(AIREvidenceNode))) {
            air_set_error(error_message, "AIR evidence node allocation failed");
            return false;
        }
        next = (AIREvidenceNode *)realloc(air->evidence_nodes,
                                          new_capacity * sizeof(AIREvidenceNode));
        if (next == NULL) {
            air_set_error(error_message, "AIR evidence node allocation failed");
            return false;
        }
        air->evidence_nodes = next;
        air->evidence_capacity = new_capacity;
    }
    if (air->evidence_nodes == NULL) {
        air_set_error(error_message, "AIR evidence node allocation failed");
        return false;
    }

    node = &air->evidence_nodes[air->evidence_count];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->boundary_index = boundary_index;
    node->fact_count = fact_count;
    node->fallback_count = fallback_count;
    if (!air_assign_owned_name(air, &node->provider_name, provider_name)
        || !air_assign_owned_name(air, &node->subject_name, subject_name)) {
        air_set_error(error_message, "AIR evidence node provenance allocation failed");
        return false;
    }
    air->evidence_count++;
    return true;
}
