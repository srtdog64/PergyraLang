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
    AIREvidenceKindScope scope;
    bool has_global_validator;
    AIREvidenceProviderKind provider_kind;
    AIREvidenceSubjectKind subject_kind;
} AIREvidenceKindMeta;

static const AIREvidenceKindMeta kEvidenceKindMeta[AIR_EVIDENCE_KIND_COUNT] = {
    [AIR_EVIDENCE_HIR_ROUTINE] = {
        true, AIR_EVIDENCE_SCOPE_BOUNDARY, false,
        AIR_EVIDENCE_PROVIDER_HIR, AIR_EVIDENCE_SUBJECT_ROUTINE },
    [AIR_EVIDENCE_HIR_CFG] = {
        true, AIR_EVIDENCE_SCOPE_BOUNDARY, false,
        AIR_EVIDENCE_PROVIDER_HIR, AIR_EVIDENCE_SUBJECT_CFG },
    [AIR_EVIDENCE_RIR_BOUNDARY] = {
        true, AIR_EVIDENCE_SCOPE_BOUNDARY, false,
        AIR_EVIDENCE_PROVIDER_RIR, AIR_EVIDENCE_SUBJECT_BOUNDARY },
    [AIR_EVIDENCE_RIR_AUTHORITY] = {
        true, AIR_EVIDENCE_SCOPE_BOUNDARY, false,
        AIR_EVIDENCE_PROVIDER_RIR, AIR_EVIDENCE_SUBJECT_AUTHORITY },
    [AIR_EVIDENCE_MIR_CLEANUP] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_MIR, AIR_EVIDENCE_SUBJECT_CLEANUP },
    [AIR_EVIDENCE_MIR_PIN_CLEANUP] = {
        true, AIR_EVIDENCE_SCOPE_BOUNDARY, false,
        AIR_EVIDENCE_PROVIDER_MIR, AIR_EVIDENCE_SUBJECT_PIN_CLEANUP },
    [AIR_EVIDENCE_MIR_TERMINATOR] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_MIR, AIR_EVIDENCE_SUBJECT_TERMINATOR },
    [AIR_EVIDENCE_MIR_SELECT_RECEIVE] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_MIR, AIR_EVIDENCE_SUBJECT_SELECT_RECEIVE },
    [AIR_EVIDENCE_DAG_METADATA] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_DAG, AIR_EVIDENCE_SUBJECT_METADATA },
    [AIR_EVIDENCE_DAG_GENERIC] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_DAG, AIR_EVIDENCE_SUBJECT_GENERIC },
    [AIR_EVIDENCE_DAG_ABILITY] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_DAG, AIR_EVIDENCE_SUBJECT_ABILITY },
    [AIR_EVIDENCE_RIR_EFFECT_PROPAGATION] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_RIR, AIR_EVIDENCE_SUBJECT_EFFECT_PROPAGATION },
    [AIR_EVIDENCE_RIR_RELATION_PROPAGATION] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_RIR, AIR_EVIDENCE_SUBJECT_RELATION_PROPAGATION },
    [AIR_EVIDENCE_OBSERVABILITY_SCHEMA] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_RUNTIME, AIR_EVIDENCE_SUBJECT_OBSERVABILITY_SCHEMA },
    [AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY] = {
        true, AIR_EVIDENCE_SCOPE_GLOBAL, true,
        AIR_EVIDENCE_PROVIDER_RUNTIME, AIR_EVIDENCE_SUBJECT_FRONTIER_POLICY },
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
    return meta != NULL && meta->scope == AIR_EVIDENCE_SCOPE_BOUNDARY;
}

AIREvidenceKindScope
air_evidence_kind_scope(AIREvidenceKind kind)
{
    const AIREvidenceKindMeta *meta = air_evidence_kind_meta(kind);
    return meta != NULL ? meta->scope : AIR_EVIDENCE_SCOPE_UNKNOWN;
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

AIREvidenceProviderKind
air_evidence_kind_provider_kind(AIREvidenceKind kind)
{
    const AIREvidenceKindMeta *meta = air_evidence_kind_meta(kind);
    return meta != NULL ? meta->provider_kind : AIR_EVIDENCE_PROVIDER_UNKNOWN;
}

AIREvidenceSubjectKind
air_evidence_kind_subject_kind(AIREvidenceKind kind)
{
    const AIREvidenceKindMeta *meta = air_evidence_kind_meta(kind);
    return meta != NULL ? meta->subject_kind : AIR_EVIDENCE_SUBJECT_UNKNOWN;
}

static bool
air_evidence_kind_scope_matches_boundary_index(AIREvidenceKind kind,
                                               size_t boundary_index,
                                               char **error_message)
{
    switch (air_evidence_kind_scope(kind)) {
    case AIR_EVIDENCE_SCOPE_BOUNDARY:
        if (boundary_index == SIZE_MAX) {
            air_set_error(error_message,
                          "AIR boundary evidence append requires boundary evidence kind to carry a boundary index");
            return false;
        }
        return true;
    case AIR_EVIDENCE_SCOPE_GLOBAL:
        if (boundary_index != SIZE_MAX) {
            air_set_error(error_message,
                          "AIR global evidence append must not carry a boundary index");
            return false;
        }
        return true;
    case AIR_EVIDENCE_SCOPE_UNKNOWN:
        break;
    }
    air_set_error(error_message, "AIR evidence append requires a known evidence kind scope");
    return false;
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

AIREvidenceKind
air_evidence_node_kind(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->kind : AIR_EVIDENCE_KIND_COUNT;
}

AIREvidenceProviderKind
air_evidence_node_provider_kind(const AIREvidenceNode *evidence)
{
    return evidence != NULL
        ? evidence->provider_kind
        : AIR_EVIDENCE_PROVIDER_UNKNOWN;
}

AIREvidenceSubjectKind
air_evidence_node_subject_kind(const AIREvidenceNode *evidence)
{
    return evidence != NULL
        ? evidence->subject_kind
        : AIR_EVIDENCE_SUBJECT_UNKNOWN;
}

bool
air_evidence_node_has_declared_kind_facts(const AIREvidenceNode *evidence)
{
    AIREvidenceKind kind = air_evidence_node_kind(evidence);
    return evidence != NULL
        && air_evidence_node_provider_kind(evidence)
            == air_evidence_kind_provider_kind(kind)
        && air_evidence_node_subject_kind(evidence)
            == air_evidence_kind_subject_kind(kind);
}

size_t
air_evidence_node_boundary_index_or(const AIREvidenceNode *evidence,
                                    size_t fallback)
{
    return evidence != NULL ? evidence->boundary_index : fallback;
}

const char *
air_evidence_node_provider_name_or(const AIREvidenceNode *evidence,
                                   const char *fallback)
{
    return evidence != NULL && evidence->provider_name != NULL
        ? evidence->provider_name
        : fallback;
}

const char *
air_evidence_node_subject_name_or(const AIREvidenceNode *evidence,
                                  const char *fallback)
{
    return evidence != NULL && evidence->subject_name != NULL
        ? evidence->subject_name
        : fallback;
}

bool
air_evidence_node_has_boundary_shape(const AIREvidenceNode *evidence)
{
    return evidence != NULL && evidence->has_boundary_shape;
}

AIRBoundaryKind
air_evidence_node_boundary_kind_or(const AIREvidenceNode *evidence,
                                   AIRBoundaryKind fallback)
{
    return air_evidence_node_has_boundary_shape(evidence)
        ? evidence->boundary_kind
        : fallback;
}

const char *
air_evidence_node_boundary_owner_name_or(const AIREvidenceNode *evidence,
                                         const char *fallback)
{
    return air_evidence_node_has_boundary_shape(evidence)
        && evidence->boundary_owner_name != NULL
        ? evidence->boundary_owner_name
        : fallback;
}

const char *
air_evidence_node_boundary_source_name_or(const AIREvidenceNode *evidence,
                                          const char *fallback)
{
    return air_evidence_node_has_boundary_shape(evidence)
        && evidence->boundary_source_name != NULL
        ? evidence->boundary_source_name
        : fallback;
}

size_t
air_evidence_node_fact_count(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->fact_count : 0;
}

size_t
air_evidence_node_fallback_count(const AIREvidenceNode *evidence)
{
    return evidence != NULL ? evidence->fallback_count : 0;
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
    if (!air_evidence_node_has_declared_kind_facts(node)) {
        air_set_error(error_message,
                      "AIR evidence duplicate has typed evidence mismatch");
        return false;
    }
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
    const AIRBoundaryNode *boundary_shape = NULL;
    const char *owned_provider_name = NULL;
    const char *owned_subject_name = NULL;
    const char *owned_boundary_owner_name = NULL;
    const char *owned_boundary_source_name = NULL;
    size_t owned_name_checkpoint;

    if (air == NULL) {
        air_set_error(error_message, "AIR evidence append requires a program");
        return false;
    }
    if (!air_evidence_kind_is_known(kind)) {
        air_set_error(error_message, "AIR evidence append requires a known evidence kind");
        return false;
    }
    if (!air_evidence_kind_scope_matches_boundary_index(kind,
                                                       boundary_index,
                                                       error_message)) {
        return false;
    }
    if (air_name_is_empty(provider_name) || air_name_is_empty(subject_name)) {
        air_set_error(error_message,
                      "AIR evidence append requires non-empty provider and subject provenance");
        return false;
    }
    if (fact_count == 0 && fallback_count == 0) {
        air_set_error(error_message,
                      "AIR evidence append requires at least one fact or fallback fact");
        return false;
    }
    if (boundary_index != SIZE_MAX) {
        boundary_shape = air_boundary_node_at(air, boundary_index);
        if (boundary_shape == NULL) {
            air_set_error(error_message,
                          "AIR boundary evidence append references missing boundary");
            return false;
        }
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
        || !air_assign_owned_name(air, &owned_subject_name, subject_name)
        || (boundary_shape != NULL
            && (!air_assign_owned_name(air,
                                       &owned_boundary_owner_name,
                                       boundary_shape->owner_name)
                || !air_assign_owned_name(air,
                                          &owned_boundary_source_name,
                                          boundary_shape->source_name)))) {
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
    node->provider_kind = air_evidence_kind_provider_kind(kind);
    node->subject_kind = air_evidence_kind_subject_kind(kind);
    node->boundary_index = boundary_index;
    node->fact_count = fact_count;
    node->fallback_count = fallback_count;
    node->provider_name = owned_provider_name;
    node->subject_name = owned_subject_name;
    if (boundary_shape != NULL) {
        node->has_boundary_shape = true;
        node->boundary_kind = boundary_shape->kind;
        node->boundary_owner_name = owned_boundary_owner_name;
        node->boundary_source_name = owned_boundary_source_name;
    }
    air->evidence_count++;
    return true;
}
