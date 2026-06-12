/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for zone declarations.
 */

#include "ast_constructors_internal.h"

const char*
ast_zone_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    return node->data.zone_decl.name;
}

bool
ast_zone_forbids_unsafe(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_DECL)
        return false;
    return node->data.zone_decl.forbids_unsafe;
}

ASTNode**
ast_zone_slots(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.slot_count;
    return node->data.zone_decl.slots;
}

ASTNode**
ast_zone_layer_slots(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.layer_slot_count;
    return node->data.zone_decl.layer_slots;
}

ASTNode**
ast_zone_shared_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.shared_count;
    return node->data.zone_decl.shared_fields;
}

ASTNode**
ast_zone_authorities(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.authority_count;
    return node->data.zone_decl.authorities;
}

ASTNode**
ast_zone_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.method_count;
    return node->data.zone_decl.methods;
}

ASTNode**
ast_zone_refreshes(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.refresh_count;
    return node->data.zone_decl.refreshes;
}

ASTNode**
ast_zone_states(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.state_count;
    return node->data.zone_decl.states;
}

ASTNode**
ast_zone_applies(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.apply_count;
    return node->data.zone_decl.applies;
}

ASTNode**
ast_zone_links(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.link_count;
    return node->data.zone_decl.links;
}

ASTNode**
ast_zone_detaches(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.detach_count;
    return node->data.zone_decl.detaches;
}

ASTNode**
ast_zone_unlinks(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.unlink_count;
    return node->data.zone_decl.unlinks;
}

ASTNode**
ast_zone_maintained_effects(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.maintained_effect_count;
    return node->data.zone_decl.maintained_effects;
}

ASTNode**
ast_zone_maintained_relations(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.maintained_relation_count;
    return node->data.zone_decl.maintained_relations;
}

ASTNode**
ast_zone_maintained_states(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ZONE_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.zone_decl.maintained_state_count;
    return node->data.zone_decl.maintained_states;
}

const char*
ast_zone_refresh_object_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_REFRESH)
        return NULL;
    return node->data.zone_refresh.object_slot_name;
}

const char*
ast_zone_refresh_source_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_REFRESH)
        return NULL;
    return node->data.zone_refresh.source_slot_name;
}

const char*
ast_zone_refresh_participant_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_REFRESH)
        return NULL;
    return node->data.zone_refresh.participant_slot_name;
}

bool
ast_zone_refresh_requires_dto(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_REFRESH
        && node->data.zone_refresh.requires_dto;
}

bool
ast_zone_refresh_derives_target_kind(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_REFRESH
        && node->data.zone_refresh.derive_target_kind;
}

size_t
ast_zone_refresh_field_map_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_REFRESH)
        return 0;
    return node->data.zone_refresh.field_map_count;
}

const char*
ast_zone_refresh_mapped_target_field(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ZONE_REFRESH
        || index >= node->data.zone_refresh.field_map_count) {
        return NULL;
    }
    return node->data.zone_refresh.mapped_target_fields != NULL
        ? node->data.zone_refresh.mapped_target_fields[index]
        : NULL;
}

const char*
ast_zone_refresh_mapped_source_field(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ZONE_REFRESH
        || index >= node->data.zone_refresh.field_map_count) {
        return NULL;
    }
    return node->data.zone_refresh.mapped_source_fields != NULL
        ? node->data.zone_refresh.mapped_source_fields[index]
        : NULL;
}

const char*
ast_zone_authority_subject_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_AUTHORITY)
        return NULL;
    return node->data.zone_authority.subject_slot_name;
}

size_t
ast_zone_authority_ability_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_AUTHORITY)
        return 0;
    return node->data.zone_authority.ability_count;
}

ASTNode**
ast_zone_authority_required_abilities(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_zone_authority_ability_count(node);
    if (node == NULL || node->type != AST_ZONE_AUTHORITY)
        return NULL;
    return node->data.zone_authority.required_abilities;
}

ASTNode*
ast_zone_authority_required_ability(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ZONE_AUTHORITY
        || index >= node->data.zone_authority.ability_count) {
        return NULL;
    }
    return node->data.zone_authority.required_abilities != NULL
        ? node->data.zone_authority.required_abilities[index]
        : NULL;
}

const char*
ast_zone_effect_slot_name(const ASTNode* node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
    case AST_ZONE_APPLY:
        return node->data.zone_apply.effect_slot_name;
    case AST_ZONE_DETACH:
        return node->data.zone_detach.effect_slot_name;
    case AST_ZONE_MAINTAIN_EFFECT:
        return node->data.zone_maintain_effect.effect_slot_name;
    default:
        return NULL;
    }
}

const char*
ast_zone_effect_target_slot_name(const ASTNode* node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
    case AST_ZONE_APPLY:
        return node->data.zone_apply.target_slot_name;
    case AST_ZONE_DETACH:
        return node->data.zone_detach.target_slot_name;
    case AST_ZONE_MAINTAIN_EFFECT:
        return node->data.zone_maintain_effect.target_slot_name;
    default:
        return NULL;
    }
}

const char*
ast_zone_relation_slot_name(const ASTNode* node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
    case AST_ZONE_LINK:
        return node->data.zone_link.relation_slot_name;
    case AST_ZONE_UNLINK:
        return node->data.zone_unlink.relation_slot_name;
    case AST_ZONE_MAINTAIN_RELATION:
        return node->data.zone_maintain_relation.relation_slot_name;
    default:
        return NULL;
    }
}

const char*
ast_zone_relation_left_slot_name(const ASTNode* node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
    case AST_ZONE_LINK:
        return node->data.zone_link.left_slot_name;
    case AST_ZONE_UNLINK:
        return node->data.zone_unlink.left_slot_name;
    case AST_ZONE_MAINTAIN_RELATION:
        return node->data.zone_maintain_relation.left_slot_name;
    default:
        return NULL;
    }
}

const char*
ast_zone_relation_right_slot_name(const ASTNode* node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
    case AST_ZONE_LINK:
        return node->data.zone_link.right_slot_name;
    case AST_ZONE_UNLINK:
        return node->data.zone_unlink.right_slot_name;
    case AST_ZONE_MAINTAIN_RELATION:
        return node->data.zone_maintain_relation.right_slot_name;
    default:
        return NULL;
    }
}

const char*
ast_zone_directive_state_name(const ASTNode* node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
    case AST_ZONE_APPLY:
        return node->data.zone_apply.state_name;
    case AST_ZONE_LINK:
        return node->data.zone_link.state_name;
    case AST_ZONE_DETACH:
        return node->data.zone_detach.state_name;
    case AST_ZONE_UNLINK:
        return node->data.zone_unlink.state_name;
    case AST_ZONE_MAINTAIN_STATE:
        return node->data.zone_maintain_state.state_name;
    default:
        return NULL;
    }
}

const char*
ast_zone_directive_participant_slot_name(const ASTNode* node)
{
    if (node == NULL)
        return NULL;
    switch (node->type) {
    case AST_ZONE_APPLY:
        return node->data.zone_apply.participant_slot_name;
    case AST_ZONE_LINK:
        return node->data.zone_link.participant_slot_name;
    case AST_ZONE_DETACH:
        return node->data.zone_detach.participant_slot_name;
    case AST_ZONE_UNLINK:
        return node->data.zone_unlink.participant_slot_name;
    case AST_ZONE_MAINTAIN_EFFECT:
        return node->data.zone_maintain_effect.participant_slot_name;
    case AST_ZONE_MAINTAIN_RELATION:
        return node->data.zone_maintain_relation.participant_slot_name;
    case AST_ZONE_MAINTAIN_STATE:
        return node->data.zone_maintain_state.participant_slot_name;
    default:
        return NULL;
    }
}
