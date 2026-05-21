/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR evidence-node inventory owner.
 */

#include "air_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    bool present;
    bool boundary_scoped;
    bool has_global_validator;
} AIREvidenceKindMeta;

static const AIREvidenceKindMeta kEvidenceKindMeta[AIR_EVIDENCE_KIND_COUNT] = {
    [AIR_EVIDENCE_HIR_ROUTINE] = { true, true, false },
    [AIR_EVIDENCE_HIR_CFG] = { true, true, false },
    [AIR_EVIDENCE_RIR_BOUNDARY] = { true, true, false },
    [AIR_EVIDENCE_RIR_AUTHORITY] = { true, true, false },
    [AIR_EVIDENCE_MIR_CLEANUP] = { true, false, true },
    [AIR_EVIDENCE_MIR_PIN_CLEANUP] = { true, true, false },
    [AIR_EVIDENCE_MIR_TERMINATOR] = { true, false, true },
    [AIR_EVIDENCE_MIR_SELECT_RECEIVE] = { true, false, true },
    [AIR_EVIDENCE_DAG_METADATA] = { true, false, true },
    [AIR_EVIDENCE_DAG_GENERIC] = { true, false, true },
    [AIR_EVIDENCE_DAG_ABILITY] = { true, false, true },
    [AIR_EVIDENCE_RIR_EFFECT_PROPAGATION] = { true, false, true },
    [AIR_EVIDENCE_RIR_RELATION_PROPAGATION] = { true, false, true },
    [AIR_EVIDENCE_OBSERVABILITY_SCHEMA] = { true, false, true },
    [AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY] = { true, false, true },
};

static const AIREvidenceKindMeta *
air_evidence_kind_meta(AIREvidenceKind kind)
{
    if ((int)kind < 0 || kind >= AIR_EVIDENCE_KIND_COUNT)
        return NULL;
    if (!kEvidenceKindMeta[kind].present)
        return NULL;
    return &kEvidenceKindMeta[kind];
}

bool
air_evidence_kind_is_boundary_scoped(AIREvidenceKind kind)
{
    const AIREvidenceKindMeta *meta = air_evidence_kind_meta(kind);
    return meta != NULL && meta->boundary_scoped;
}

bool
air_evidence_kind_is_known(AIREvidenceKind kind)
{
    return air_evidence_kind_meta(kind) != NULL;
}

bool
air_evidence_kind_has_global_validator(AIREvidenceKind kind)
{
    const AIREvidenceKindMeta *meta = air_evidence_kind_meta(kind);
    return meta != NULL && meta->has_global_validator;
}

bool
air_evidence_inventory_storage_valid(const AIRProgram *air)
{
    return air != NULL
        && (air->evidence_count == 0 || air->evidence_nodes != NULL);
}

size_t
air_evidence_node_count(const AIRProgram *air)
{
    return air != NULL ? air->evidence_count : 0;
}

const AIREvidenceNode *
air_evidence_node_at(const AIRProgram *air, size_t index)
{
    if (air == NULL || index >= air->evidence_count)
        return NULL;
    return &air->evidence_nodes[index];
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
    const char *owned_provider_name = NULL;
    const char *owned_subject_name = NULL;
    size_t owned_name_checkpoint;

    if (air == NULL) {
        air_set_error(error_message, "AIR evidence append requires a program");
        return false;
    }
    if (!air_evidence_kind_is_known(kind)) {
        air_set_error(error_message, "AIR evidence append requires a known evidence kind");
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

    owned_name_checkpoint = air->owned_name_count;
    if (!air_assign_owned_name(air, &owned_provider_name, provider_name)
        || !air_assign_owned_name(air, &owned_subject_name, subject_name)) {
        for (size_t i = owned_name_checkpoint; i < air->owned_name_count; i++) {
            free(air->owned_names[i]);
            air->owned_names[i] = NULL;
        }
        air->owned_name_count = owned_name_checkpoint;
        air_set_error(error_message, "AIR evidence node provenance allocation failed");
        return false;
    }

    node = &air->evidence_nodes[air->evidence_count];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->boundary_index = boundary_index;
    node->fact_count = fact_count;
    node->fallback_count = fallback_count;
    node->provider_name = owned_provider_name;
    node->subject_name = owned_subject_name;
    air->evidence_count++;
    return true;
}
