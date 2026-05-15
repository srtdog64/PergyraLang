/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for domain declarations.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>

const char*
ast_roster_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_SYSTEMIC_SLOT)
        return NULL;
    return node->data.roster_slot.slot_name;
}

const char*
ast_roster_slot_party_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_SYSTEMIC_SLOT)
        return NULL;
    return node->data.roster_slot.party_type;
}

bool
ast_roster_slot_replace_party_type(ASTNode* node, const char* party_type)
{
    char *copy;

    if (node == NULL || node->type != AST_SYSTEMIC_SLOT)
        return false;

    copy = party_type != NULL ? pergyra_strdup(party_type) : NULL;
    if (party_type != NULL && copy == NULL)
        return false;

    free(node->data.roster_slot.party_type);
    node->data.roster_slot.party_type = copy;
    return true;
}

const char*
ast_world_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    return node->data.world_decl.name;
}

ASTNode**
ast_world_rosters(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.roster_count;
    return node->data.world_decl.rosters;
}

ASTNode**
ast_world_zones(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.zone_count;
    return node->data.world_decl.zones;
}

const char*
ast_world_roster_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WORLD_SYSTEMIC)
        return NULL;
    return node->data.world_roster.slot_name;
}

const char*
ast_world_roster_type_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WORLD_SYSTEMIC)
        return NULL;
    return node->data.world_roster.roster_type;
}

ASTNode*
ast_world_roster_initializer(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WORLD_SYSTEMIC)
        return NULL;
    return node->data.world_roster.initializer;
}

bool
ast_world_roster_replace_type_name(ASTNode* node, const char* type_name)
{
    char *owned_type_name;

    if (node == NULL || node->type != AST_WORLD_SYSTEMIC || type_name == NULL)
        return false;

    owned_type_name = pergyra_strdup(type_name);
    if (owned_type_name == NULL)
        return false;

    free(node->data.world_roster.roster_type);
    node->data.world_roster.roster_type = owned_type_name;
    return true;
}

const char*
ast_world_zone_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WORLD_ZONE)
        return NULL;
    return node->data.world_zone.slot_name;
}

const char*
ast_world_zone_type_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WORLD_ZONE)
        return NULL;
    return node->data.world_zone.zone_type;
}

ASTNode*
ast_world_zone_initializer(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WORLD_ZONE)
        return NULL;
    return node->data.world_zone.initializer;
}

ASTNode**
ast_world_shared_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.shared_count;
    return node->data.world_decl.shared_fields;
}

ASTNode**
ast_world_states(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.state_count;
    return node->data.world_decl.states;
}

ASTNode**
ast_world_activations(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.activate_count;
    return node->data.world_decl.activations;
}

ASTNode**
ast_world_deactivations(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.deactivate_count;
    return node->data.world_decl.deactivations;
}

ASTNode**
ast_world_maintained_zones(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.maintained_zone_count;
    return node->data.world_decl.maintained_zones;
}

ASTNode**
ast_world_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_WORLD_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.world_decl.method_count;
    return node->data.world_decl.methods;
}

const char*
ast_relation_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_RELATION_DECL)
        return NULL;
    return node->data.relation_decl.name;
}

RelationEndpointKind
ast_relation_between_left_kind(const ASTNode* node)
{
    if (node == NULL || node->type != AST_RELATION_DECL)
        return RELATION_ENDPOINT_NAMED;
    return node->data.relation_decl.between_left_kind;
}

RelationEndpointKind
ast_relation_between_right_kind(const ASTNode* node)
{
    if (node == NULL || node->type != AST_RELATION_DECL)
        return RELATION_ENDPOINT_NAMED;
    return node->data.relation_decl.between_right_kind;
}

ASTNode*
ast_relation_between_left_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_RELATION_DECL)
        return NULL;
    return node->data.relation_decl.between_left_type;
}

ASTNode*
ast_relation_between_right_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_RELATION_DECL)
        return NULL;
    return node->data.relation_decl.between_right_type;
}

ASTNode**
ast_relation_slots(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_RELATION_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.relation_decl.slot_count;
    return node->data.relation_decl.slots;
}

ASTNode**
ast_relation_refreshes(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_RELATION_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.relation_decl.refresh_count;
    return node->data.relation_decl.refreshes;
}

ASTNode**
ast_relation_shared_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_RELATION_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.relation_decl.shared_count;
    return node->data.relation_decl.shared_fields;
}

ASTNode**
ast_relation_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_RELATION_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.relation_decl.method_count;
    return node->data.relation_decl.methods;
}

const char*
ast_effect_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EFFECT_DECL)
        return NULL;
    return node->data.effect_decl.name;
}

ASTNode**
ast_effect_slots(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_EFFECT_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.effect_decl.slot_count;
    return node->data.effect_decl.slots;
}

ASTNode**
ast_effect_refreshes(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_EFFECT_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.effect_decl.refresh_count;
    return node->data.effect_decl.refreshes;
}

ASTNode**
ast_effect_shared_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_EFFECT_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.effect_decl.shared_count;
    return node->data.effect_decl.shared_fields;
}

ASTNode**
ast_effect_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_EFFECT_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.effect_decl.method_count;
    return node->data.effect_decl.methods;
}

const char*
ast_domain_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_DOMAIN_SLOT)
        return NULL;
    return node->data.domain_slot.slot_name;
}

ASTNode*
ast_domain_slot_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_DOMAIN_SLOT)
        return NULL;
    return node->data.domain_slot.type;
}

bool
ast_domain_slot_is_subject(const ASTNode* node)
{
    return node != NULL && node->type == AST_DOMAIN_SLOT
        && node->data.domain_slot.is_subject;
}

bool
ast_domain_slot_is_vessel(const ASTNode* node)
{
    return node != NULL && node->type == AST_DOMAIN_SLOT
        && node->data.domain_slot.is_vessel;
}

bool
ast_domain_slot_is_tobject(const ASTNode* node)
{
    return node != NULL && node->type == AST_DOMAIN_SLOT
        && node->data.domain_slot.is_tobject;
}

bool
ast_domain_slot_is_binding(const ASTNode* node)
{
    return node != NULL && node->type == AST_DOMAIN_SLOT
        && node->data.domain_slot.is_binding;
}

ASTNode*
ast_domain_slot_initializer(const ASTNode* node)
{
    if (node == NULL || node->type != AST_DOMAIN_SLOT)
        return NULL;
    return node->data.domain_slot.initializer;
}
