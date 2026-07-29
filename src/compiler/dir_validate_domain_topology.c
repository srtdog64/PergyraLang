/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * DIR source-declaration topology row validation.
 */

#include "dir_validate_internal.h"

#include <string.h>

#include "../parser/ast_api.h"

bool
dir_domain_topology_is_projection(DIRDomainTopologyKind kind)
{
    return kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH
        || kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH
        || kind == DIR_DOMAIN_TOPOLOGY_PROJECTION_BIND;
}

static bool
dir_domain_topology_slot_matches(const DIRNode *owner,
                                 const char *name,
                                 uint32_t syntax_id,
                                 bool layer)
{
    ASTNode **slots = NULL;
    size_t count = 0;

    if (owner == NULL || owner->ast == NULL || name == NULL
        || syntax_id == 0)
        return false;
    if (layer) {
        if (owner->kind != DIR_NODE_ZONE)
            return false;
        slots = ast_zone_layer_slots(owner->ast, &count);
        for (size_t i = 0; slots != NULL && i < count; i++) {
            if (slots[i] != NULL
                && ast_node_stable_id(slots[i]) == syntax_id
                && ast_zone_layer_slot_name(slots[i]) != NULL
                && strcmp(ast_zone_layer_slot_name(slots[i]), name) == 0) {
                return true;
            }
        }
        return false;
    }

    switch (owner->kind) {
    case DIR_NODE_ZONE:
        slots = ast_zone_slots(owner->ast, &count);
        break;
    case DIR_NODE_RELATION:
        slots = ast_relation_slots(owner->ast, &count);
        break;
    case DIR_NODE_EFFECT:
        slots = ast_effect_slots(owner->ast, &count);
        break;
    default:
        return false;
    }
    for (size_t i = 0; slots != NULL && i < count; i++) {
        if (slots[i] != NULL
            && ast_node_stable_id(slots[i]) == syntax_id
            && ast_domain_slot_name(slots[i]) != NULL
            && strcmp(ast_domain_slot_name(slots[i]), name) == 0) {
            return true;
        }
    }
    return false;
}

static size_t
dir_domain_topology_expected_count(const DIRProgram *dir)
{
    size_t expected = 0;

    if (dir == NULL)
        return 0;
    for (size_t i = 0; i < dir->node_count; i++) {
        const DIRNode *node = &dir->nodes[i];
        size_t count = 0;
        if (node->ast == NULL)
            continue;
        switch (node->kind) {
        case DIR_NODE_ZONE:
            (void)ast_zone_refreshes(node->ast, &count);
            expected += count;
            (void)ast_zone_applies(node->ast, &count);
            expected += count;
            (void)ast_zone_maintained_effects(node->ast, &count);
            expected += count;
            (void)ast_zone_links(node->ast, &count);
            expected += count;
            break;
        case DIR_NODE_RELATION:
            (void)ast_relation_refreshes(node->ast, &count);
            expected += count;
            break;
        case DIR_NODE_EFFECT:
            (void)ast_effect_refreshes(node->ast, &count);
            expected += count;
            break;
        default:
            break;
        }
    }
    return expected;
}

static bool
dir_domain_topology_effect_directive_matches(const DIRNode *owner,
                                             const DIRDomainTopologyRow *row,
                                             bool apply)
{
    ASTNode **directives = NULL;
    size_t count = 0;

    if (owner == NULL || row == NULL || owner->kind != DIR_NODE_ZONE
        || owner->ast == NULL || row->source_syntax_id == 0) {
        return false;
    }
    directives = apply
        ? ast_zone_applies(owner->ast, &count)
        : ast_zone_maintained_effects(owner->ast, &count);
    for (size_t i = 0; directives != NULL && i < count; i++) {
        ASTNode *directive = directives[i];
        const char *layer = ast_zone_effect_slot_name(directive);
        const char *target = ast_zone_effect_target_slot_name(directive);
        const char *participant =
            ast_zone_directive_participant_slot_name(directive);
        bool participant_matches =
            participant == NULL && row->participant_slot_name == NULL;
        if (participant != NULL && row->participant_slot_name != NULL) {
            participant_matches = strcmp(
                participant, row->participant_slot_name) == 0;
        }
        if (directive != NULL
            && ast_node_stable_id(directive) == row->source_syntax_id
            && layer != NULL && row->layer_slot_name != NULL
            && strcmp(layer, row->layer_slot_name) == 0
            && target != NULL && row->target_slot_name != NULL
            && strcmp(target, row->target_slot_name) == 0
            && participant_matches) {
            return true;
        }
    }
    return false;
}

bool
dir_validate_domain_topology(const DIRProgram *dir, char **error_message)
{
    size_t expected;

    if (dir == NULL)
        return false;
    expected = dir_domain_topology_expected_count(dir);
    if (dir->domain_topology_row_count != expected
        || (dir->domain_topology_row_count > 0
            && dir->domain_topology_rows == NULL)) {
        if (error_message != NULL) {
            *error_message = dir_validate_strdup_fmt(
                "DIR domain topology row count %llu does not match source-owned count %llu",
                (unsigned long long)dir->domain_topology_row_count,
                (unsigned long long)expected);
        }
        return false;
    }

    for (size_t i = 0; i < dir->domain_topology_row_count; i++) {
        const DIRDomainTopologyRow *row = &dir->domain_topology_rows[i];
        const DIRNode *owner;
        bool shape_ok = false;

        if (row->owner_node_id >= dir->node_count
            || row->owner_source_syntax_id == 0
            || row->source_syntax_id == 0) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain topology row[%llu] has incomplete stable identity",
                    (unsigned long long)i);
            return false;
        }
        owner = &dir->nodes[row->owner_node_id];
        if (owner->source_syntax_id != row->owner_source_syntax_id
            || (owner->kind != DIR_NODE_ZONE
                && owner->kind != DIR_NODE_RELATION
                && owner->kind != DIR_NODE_EFFECT)) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain topology row[%llu] has owner identity drift",
                    (unsigned long long)i);
            return false;
        }

        if (dir_domain_topology_is_projection(row->kind)) {
            shape_ok = dir_domain_topology_slot_matches(
                    owner, row->projection_slot_name,
                    row->projection_slot_source_syntax_id, false)
                && dir_domain_topology_slot_matches(
                    owner, row->source_slot_name,
                    row->source_slot_source_syntax_id, false)
                && row->layer_slot_name == NULL
                && row->target_slot_name == NULL
                && row->left_slot_name == NULL
                && row->right_slot_name == NULL;
        } else if (row->kind == DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT
                   || row->kind == DIR_DOMAIN_TOPOLOGY_MAINTAIN_EFFECT) {
            shape_ok = owner->kind == DIR_NODE_ZONE
                && dir_domain_topology_effect_directive_matches(
                    owner, row,
                    row->kind == DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT)
                && dir_domain_topology_slot_matches(
                    owner, row->layer_slot_name,
                    row->layer_slot_source_syntax_id, true)
                && dir_domain_topology_slot_matches(
                    owner, row->target_slot_name,
                    row->target_slot_source_syntax_id, false)
                && row->projection_slot_name == NULL
                && row->source_slot_name == NULL
                && row->left_slot_name == NULL
                && row->right_slot_name == NULL;
        } else if (row->kind == DIR_DOMAIN_TOPOLOGY_LINK_RELATION) {
            shape_ok = owner->kind == DIR_NODE_ZONE
                && dir_domain_topology_slot_matches(
                    owner, row->layer_slot_name,
                    row->layer_slot_source_syntax_id, true)
                && dir_domain_topology_slot_matches(
                    owner, row->left_slot_name,
                    row->left_slot_source_syntax_id, false)
                && dir_domain_topology_slot_matches(
                    owner, row->right_slot_name,
                    row->right_slot_source_syntax_id, false)
                && row->projection_slot_name == NULL
                && row->source_slot_name == NULL
                && row->target_slot_name == NULL;
        }
        if (row->participant_slot_name != NULL) {
            shape_ok = shape_ok && dir_domain_topology_slot_matches(
                owner, row->participant_slot_name,
                row->participant_slot_source_syntax_id, false);
        } else if (row->participant_slot_source_syntax_id != 0) {
            shape_ok = false;
        }
        if (!shape_ok) {
            if (error_message != NULL)
                *error_message = dir_validate_strdup_fmt(
                    "DIR domain topology row[%llu] has invalid %s shape",
                    (unsigned long long)i,
                    dir_domain_topology_kind_name(row->kind));
            return false;
        }
        for (size_t j = i + 1; j < dir->domain_topology_row_count; j++) {
            if (dir->domain_topology_rows[j].source_syntax_id
                == row->source_syntax_id) {
                if (error_message != NULL)
                    *error_message = dir_validate_strdup_fmt(
                        "DIR domain topology rows duplicate source identity %u",
                        (unsigned)row->source_syntax_id);
                return false;
            }
        }
    }
    return true;
}
