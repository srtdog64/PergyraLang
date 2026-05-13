/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for domain declarations.
 */

#include "ast_constructors_internal.h"

const char*
ast_ability_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return NULL;
    return node->data.ability_decl.name;
}

size_t
ast_ability_method_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return 0;
    return node->data.ability_decl.method_count;
}

ASTNode*
ast_ability_method(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ABILITY_DECL
        || index >= node->data.ability_decl.method_count) {
        return NULL;
    }
    return node->data.ability_decl.methods[index];
}

const char*
ast_role_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_DECL)
        return NULL;
    return node->data.role_decl.name;
}

ASTNode*
ast_role_for_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_DECL)
        return NULL;
    return node->data.role_decl.for_type;
}

size_t
ast_role_include_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_DECL)
        return 0;
    return node->data.role_decl.include_count;
}

ASTNode*
ast_role_include(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ROLE_DECL
        || index >= node->data.role_decl.include_count) {
        return NULL;
    }
    return node->data.role_decl.includes[index];
}

size_t
ast_role_impl_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_DECL)
        return 0;
    return node->data.role_decl.impl_count;
}

ASTNode*
ast_role_impl(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ROLE_DECL
        || index >= node->data.role_decl.impl_count) {
        return NULL;
    }
    return node->data.role_decl.impl_abilities[index];
}

const char*
ast_include_role_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INCLUDE_STMT)
        return NULL;
    return node->data.include_stmt.role_name;
}

GenericParams*
ast_include_type_args(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INCLUDE_STMT)
        return NULL;
    return node->data.include_stmt.type_args;
}

ASTNode*
ast_impl_ability_ref(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IMPL_ABILITY)
        return NULL;
    return node->data.impl_ability.ability_ref;
}

const char*
ast_impl_ability_name(const ASTNode* node)
{
    ASTNode* ability_ref = ast_impl_ability_ref(node);

    if (ability_ref == NULL || ability_ref->type != AST_TYPE)
        return NULL;
    return ability_ref->data.type.name;
}

size_t
ast_impl_ability_method_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IMPL_ABILITY)
        return 0;
    return node->data.impl_ability.method_count;
}

ASTNode*
ast_impl_ability_method(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_IMPL_ABILITY
        || index >= node->data.impl_ability.method_count) {
        return NULL;
    }
    return node->data.impl_ability.methods[index];
}

const char*
ast_roster_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROSTER_DECL)
        return NULL;
    return node->data.roster_decl.name;
}

size_t
ast_roster_party_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROSTER_DECL)
        return 0;
    return node->data.roster_decl.party_count;
}

ASTNode*
ast_roster_party(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ROSTER_DECL
        || index >= node->data.roster_decl.party_count) {
        return NULL;
    }
    return node->data.roster_decl.party_slots[index];
}

size_t
ast_roster_shared_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROSTER_DECL)
        return 0;
    return node->data.roster_decl.shared_count;
}

ASTNode*
ast_roster_shared(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ROSTER_DECL
        || index >= node->data.roster_decl.shared_count) {
        return NULL;
    }
    return node->data.roster_decl.shared_fields[index];
}

ASTNode**
ast_roster_shared_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_roster_shared_count(node);
    if (node == NULL || node->type != AST_ROSTER_DECL)
        return NULL;
    return node->data.roster_decl.shared_fields;
}

size_t
ast_roster_method_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROSTER_DECL)
        return 0;
    return node->data.roster_decl.method_count;
}

ASTNode*
ast_roster_method(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ROSTER_DECL
        || index >= node->data.roster_decl.method_count) {
        return NULL;
    }
    return node->data.roster_decl.methods[index];
}

ASTNode**
ast_roster_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_roster_method_count(node);
    if (node == NULL || node->type != AST_ROSTER_DECL)
        return NULL;
    return node->data.roster_decl.methods;
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
