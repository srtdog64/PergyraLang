/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * DIR runtime role/projection fact validation against topology owners.
 */

#include "dir_validate_internal.h"

#include <string.h>

#include "../common/string_compat.h"

static bool
dir_domain_runtime_text_present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static const DIRNode *
dir_domain_runtime_find_node_by_source(const DIRProgram *dir,
                                       uint32_t source_syntax_id)
{
    if (dir == NULL || source_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].source_syntax_id == source_syntax_id)
            return &dir->nodes[i];
    }
    return NULL;
}

static const DIRNode *
dir_domain_runtime_find_slot(const DIRProgram *dir,
                             uint32_t owner_syntax_id,
                             uint32_t slot_syntax_id)
{
    const DIRNode *slot = dir_domain_runtime_find_node_by_source(
        dir, slot_syntax_id);
    if (slot == NULL || slot->owner_source_syntax_id != owner_syntax_id
        || (slot->kind != DIR_NODE_ZONE_SLOT
            && slot->kind != DIR_NODE_PROJECTION_SLOT)) {
        return NULL;
    }
    return slot;
}

static bool
dir_domain_runtime_slot_type_matches(const DIRProgram *dir,
                                     const DIRNode *slot,
                                     const char *field_name,
                                     const char *type_name,
                                     uint32_t type_decl_syntax_id)
{
    if (dir == NULL || slot == NULL
        || !dir_domain_runtime_text_present(field_name)
        || !dir_domain_runtime_text_present(type_name)) {
        return false;
    }
    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->from_node_id != slot->id
            || (edge->kind != DIR_EDGE_ZONE_SLOT_TYPE
                && edge->kind != DIR_EDGE_PROJECTION_SLOT_TYPE)
            || edge->label == NULL || strcmp(edge->label, field_name) != 0
            || edge->target_name == NULL
            || strcmp(edge->target_name, type_name) != 0) {
            continue;
        }
        if (type_decl_syntax_id == 0)
            return true;
        return edge->to_node_id < dir->node_count
            && dir->nodes[edge->to_node_id].source_syntax_id
                == type_decl_syntax_id;
    }
    return false;
}

static const DIRNode *
dir_domain_runtime_layer_type(const DIRProgram *dir,
                              const DIRDomainTopologyRow *row)
{
    if (dir == NULL || row == NULL || row->owner_node_id >= dir->node_count
        || !dir_domain_runtime_text_present(row->layer_slot_name)) {
        return NULL;
    }
    for (size_t i = 0; i < dir->edge_count; i++) {
        const DIREdge *edge = &dir->edges[i];
        if (edge->kind == DIR_EDGE_ZONE_LAYER_TYPE
            && edge->from_node_id == row->owner_node_id
            && edge->label != NULL
            && strcmp(edge->label, row->layer_slot_name) == 0
            && edge->to_node_id < dir->node_count) {
            return &dir->nodes[edge->to_node_id];
        }
    }
    return NULL;
}

static const PgyDomainParticipantRoleFact *
dir_domain_runtime_find_role(
    const DIRProgram *dir,
    uint32_t owner_syntax_id,
    PgyDomainParticipantRole role)
{
    if (dir == NULL || owner_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < dir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &dir->domain_participant_role_facts[i];
        if (fact->owner_syntax_id == owner_syntax_id
            && fact->role == role) {
            return fact;
        }
    }
    return NULL;
}

static PgyDomainProjectionOperation
dir_domain_runtime_projection_operation(DIRDomainTopologyKind kind)
{
    if (kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH)
        return PGY_DOMAIN_PROJECTION_PUBLISH;
    if (kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND)
        return PGY_DOMAIN_PROJECTION_BIND;
    return PGY_DOMAIN_PROJECTION_REFRESH;
}

static const DIRDomainTopologyRow *
dir_domain_runtime_find_projection_row(
    const DIRProgram *dir,
    const PgyDomainProjectionMemberAssignmentFact *fact)
{
    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        const DIRDomainTopologyRow *row = &dir->domain_topology_rows[i];
        if (dir_domain_topology_is_projection(row->kind)
            && row->owner_source_syntax_id == fact->owner_syntax_id
            && row->source_syntax_id == fact->directive_syntax_id
            && row->projection_slot_source_syntax_id
                == fact->projection_slot_syntax_id
            && row->source_slot_source_syntax_id
                == fact->source_slot_syntax_id
            && row->projection_slot_name != NULL
            && strcmp(row->projection_slot_name,
                      fact->projection_slot_name) == 0
            && row->source_slot_name != NULL
            && strcmp(row->source_slot_name, fact->source_slot_name) == 0
            && dir_domain_runtime_projection_operation(row->kind)
                == fact->operation) {
            return row;
        }
    }
    return NULL;
}

static bool
dir_validate_domain_runtime_role_facts(const DIRProgram *dir,
                                       char **error_message)
{
    for (size_t i = 0; i < dir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &dir->domain_participant_role_facts[i];
        const DIRNode *owner;
        const DIRNode *field;
        DIRNodeKind expected_owner_kind;

        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != dir->source_program_syntax_id
            || fact->owner_syntax_id == 0 || fact->field_syntax_id == 0
            || (unsigned)fact->role
                > (unsigned)PGY_DOMAIN_PARTICIPANT_RELATION_TARGET
            || !dir_domain_runtime_text_present(fact->owner_name)
            || !dir_domain_runtime_text_present(fact->field_name)
            || !dir_domain_runtime_text_present(fact->field_type_name)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain participant-role fact[%llu] has incomplete exact identity, name, or type",
                    (unsigned long long)i);
            return false;
        }
        expected_owner_kind =
            fact->role == PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER
                ? DIR_NODE_EFFECT : DIR_NODE_RELATION;
        owner = dir_domain_runtime_find_node_by_source(
            dir, fact->owner_syntax_id);
        field = dir_domain_runtime_find_slot(
            dir, fact->owner_syntax_id, fact->field_syntax_id);
        if (owner == NULL || owner->kind != expected_owner_kind
            || owner->name == NULL
            || strcmp(owner->name, fact->owner_name) != 0
            || field == NULL
            || !dir_domain_runtime_slot_type_matches(
                dir, field, fact->field_name, fact->field_type_name, 0)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain participant-role fact[%llu] does not match its exact owner/field declaration",
                    (unsigned long long)i);
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainParticipantRoleFact *prior =
                &dir->domain_participant_role_facts[j];
            if ((prior->program_syntax_id == fact->program_syntax_id
                 && prior->owner_syntax_id == fact->owner_syntax_id
                 && prior->role == fact->role)
                || prior->field_syntax_id == fact->field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "DIR domain participant-role facts duplicate stable identity");
                return false;
            }
        }
    }
    return true;
}

static bool
dir_validate_domain_runtime_projection_facts(const DIRProgram *dir,
                                             char **error_message)
{
    for (size_t i = 0;
         i < dir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &dir->domain_projection_member_assignment_facts[i];
        const DIRDomainTopologyRow *row;
        const DIRNode *owner;
        const DIRNode *projection_slot;
        const DIRNode *source_slot;
        const DIRNode *target_decl;

        if (fact->program_syntax_id == 0
            || fact->program_syntax_id != dir->source_program_syntax_id
            || fact->owner_syntax_id == 0
            || fact->directive_syntax_id == 0
            || fact->projection_slot_syntax_id == 0
            || fact->source_slot_syntax_id == 0
            || fact->target_decl_syntax_id == 0
            || fact->target_field_syntax_id == 0
            || fact->source_decl_syntax_id == 0
            || (unsigned)fact->operation
                > (unsigned)PGY_DOMAIN_PROJECTION_BIND
            || !dir_domain_runtime_text_present(fact->owner_name)
            || !dir_domain_runtime_text_present(fact->projection_slot_name)
            || !dir_domain_runtime_text_present(fact->source_slot_name)
            || !dir_domain_runtime_text_present(fact->target_field_name)
            || !dir_domain_runtime_text_present(
                fact->target_field_type_name)
            || !dir_domain_runtime_text_present(fact->source_path)
            || !dir_domain_runtime_text_present(fact->source_leaf_type_name)
            || fact->source_path_segment_count == 0
            || fact->source_path_segments == NULL) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain projection assignment[%llu] has incomplete exact identity, name, type, or path",
                    (unsigned long long)i);
            return false;
        }
        for (size_t s = 0; s < fact->source_path_segment_count; s++) {
            const PgyDomainProjectionPathSegmentFact *segment =
                &fact->source_path_segments[s];
            if (segment->field_syntax_id == 0
                || !dir_domain_runtime_text_present(segment->field_name)
                || !dir_domain_runtime_text_present(
                    segment->field_type_name)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR domain projection assignment[%llu] has an incomplete source-path segment",
                        (unsigned long long)i);
                return false;
            }
        }
        if (strcmp(fact->source_path_segments[
                       fact->source_path_segment_count - 1]
                       .field_type_name,
                   fact->source_leaf_type_name) != 0) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain projection assignment[%llu] has exact type drift",
                    (unsigned long long)i);
            return false;
        }

        row = dir_domain_runtime_find_projection_row(dir, fact);
        owner = dir_domain_runtime_find_node_by_source(
            dir, fact->owner_syntax_id);
        projection_slot = dir_domain_runtime_find_slot(
            dir, fact->owner_syntax_id,
            fact->projection_slot_syntax_id);
        source_slot = dir_domain_runtime_find_slot(
            dir, fact->owner_syntax_id, fact->source_slot_syntax_id);
        target_decl = dir_domain_runtime_find_node_by_source(
            dir, fact->target_decl_syntax_id);
        if (row == NULL || owner == NULL || owner->name == NULL
            || strcmp(owner->name, fact->owner_name) != 0
            || projection_slot == NULL
            || projection_slot->kind != DIR_NODE_PROJECTION_SLOT
            || source_slot == NULL || target_decl == NULL
            || target_decl->name == NULL
            || !dir_domain_runtime_slot_type_matches(
                dir, projection_slot, fact->projection_slot_name,
                target_decl->name,
                fact->target_decl_syntax_id)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain projection assignment[%llu] does not match its topology owner/target declaration",
                    (unsigned long long)i);
            return false;
        }
        {
            const DIRNode *source_decl =
                dir_domain_runtime_find_node_by_source(
                    dir, fact->source_decl_syntax_id);
            if (source_decl == NULL || source_decl->name == NULL
                || !dir_domain_runtime_slot_type_matches(
                    dir, source_slot, fact->source_slot_name,
                    source_decl->name, fact->source_decl_syntax_id)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR domain projection assignment[%llu] does not match its exact source declaration",
                        (unsigned long long)i);
                return false;
            }
        }
        for (size_t j = 0; j < i; j++) {
            const PgyDomainProjectionMemberAssignmentFact *prior =
                &dir->domain_projection_member_assignment_facts[j];
            if (prior->program_syntax_id == fact->program_syntax_id
                && prior->owner_syntax_id == fact->owner_syntax_id
                && prior->directive_syntax_id == fact->directive_syntax_id
                && prior->projection_slot_syntax_id
                    == fact->projection_slot_syntax_id
                && prior->target_field_syntax_id
                    == fact->target_field_syntax_id) {
                if (error_message != NULL)
                    *error_message = pergyra_strdup(
                        "DIR domain projection assignments duplicate stable member identity");
                return false;
            }
        }
    }
    return true;
}

static bool
dir_validate_domain_runtime_topology_coverage(const DIRProgram *dir,
                                              char **error_message)
{
    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        const DIRDomainTopologyRow *row = &dir->domain_topology_rows[i];
        if (dir_domain_topology_is_projection(row->kind)) {
            bool found = false;
            for (size_t j = 0;
                 j < dir->domain_projection_member_assignment_fact_count;
                 j++) {
                const PgyDomainProjectionMemberAssignmentFact *fact =
                    &dir->domain_projection_member_assignment_facts[j];
                if (fact->owner_syntax_id == row->owner_source_syntax_id
                    && fact->directive_syntax_id == row->source_syntax_id) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR projection topology directive %u has no semantic member-assignment facts",
                        (unsigned)row->source_syntax_id);
                return false;
            }
            continue;
        }

        const DIRNode *layer_type =
            dir_domain_runtime_layer_type(dir, row);
        if (layer_type == NULL) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR layer topology directive %u has no exact layer type",
                    (unsigned)row->source_syntax_id);
            return false;
        }
        if (row->kind == DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT
            || row->kind == DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT) {
            const PgyDomainParticipantRoleFact *bearer =
                dir_domain_runtime_find_role(
                    dir, layer_type->source_syntax_id,
                    PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER);
            const DIRNode *target_slot = dir_domain_runtime_find_slot(
                dir, row->owner_source_syntax_id,
                row->target_slot_source_syntax_id);
            if (layer_type->kind != DIR_NODE_EFFECT || bearer == NULL
                || target_slot == NULL
                || !dir_domain_runtime_slot_type_matches(
                    dir, target_slot, row->target_slot_name,
                    bearer->field_type_name, 0)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR effect topology directive %u has no exact semantic bearer role",
                        (unsigned)row->source_syntax_id);
                return false;
            }
        } else if (row->kind == DIR_DOMAIN_TOPOLOGY_LINK_RELATION) {
            const PgyDomainParticipantRoleFact *source =
                dir_domain_runtime_find_role(
                    dir, layer_type->source_syntax_id,
                    PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE);
            const PgyDomainParticipantRoleFact *target =
                dir_domain_runtime_find_role(
                    dir, layer_type->source_syntax_id,
                    PGY_DOMAIN_PARTICIPANT_RELATION_TARGET);
            const DIRNode *left_slot = dir_domain_runtime_find_slot(
                dir, row->owner_source_syntax_id,
                row->left_slot_source_syntax_id);
            const DIRNode *right_slot = dir_domain_runtime_find_slot(
                dir, row->owner_source_syntax_id,
                row->right_slot_source_syntax_id);
            if (layer_type->kind != DIR_NODE_RELATION || source == NULL
                || target == NULL || left_slot == NULL || right_slot == NULL
                || !dir_domain_runtime_slot_type_matches(
                    dir, left_slot, row->left_slot_name,
                    source->field_type_name, 0)
                || !dir_domain_runtime_slot_type_matches(
                    dir, right_slot, row->right_slot_name,
                    target->field_type_name, 0)) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR relation topology directive %u has no exact semantic source/target roles",
                        (unsigned)row->source_syntax_id);
                return false;
            }
        }
    }
    return true;
}

bool
dir_validate_domain_runtime_facts(const DIRProgram *dir,
                                  char **error_message)
{
    if (!dir->has_domain_runtime_facts) {
        if (dir->domain_participant_role_facts != NULL
            || dir->domain_participant_role_fact_count != 0
            || dir->domain_projection_member_assignment_facts != NULL
            || dir->domain_projection_member_assignment_fact_count != 0) {
            if (error_message != NULL)
                *error_message = pergyra_strdup(
                    "DIR domain runtime storage exists without HIR projection marker");
            return false;
        }
        /* Compatibility-only legacy entry points (dir_lower and the LSP's
         * syntax probe) do not claim a semantic snapshot.  The production
         * dir_lower_with_hir_* path sets the marker even for an empty table,
         * so topology there still fails closed through coverage below. */
        return true;
    }
    if ((dir->domain_participant_role_fact_count == 0
         && dir->domain_participant_role_facts != NULL)
        || (dir->domain_participant_role_fact_count != 0
            && dir->domain_participant_role_facts == NULL)
        || (dir->domain_projection_member_assignment_fact_count == 0
            && dir->domain_projection_member_assignment_facts != NULL)
        || (dir->domain_projection_member_assignment_fact_count != 0
            && dir->domain_projection_member_assignment_facts == NULL)) {
        if (error_message != NULL)
            *error_message = pergyra_strdup(
                "DIR domain runtime semantic snapshot has incomplete storage");
        return false;
    }
    return dir_validate_domain_runtime_role_facts(dir, error_message)
        && dir_validate_domain_runtime_projection_facts(dir, error_message)
        && dir_validate_domain_runtime_topology_coverage(dir, error_message);
}
