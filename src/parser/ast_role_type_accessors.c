/*
 * Copyright (c) 2026 Pergyra Language Project
 * Split AST accessor owner. Keep responsibility slices below the 600 LOC signal.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char*
ast_ability_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return NULL;
    return node->data.ability_decl.name;
}

AccessModifier
ast_ability_access(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return ACCESS_PRIVATE;
    return node->data.ability_decl.access;
}

bool
ast_ability_has_explicit_access(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return false;
    return node->data.ability_decl.has_explicit_access;
}

bool
ast_ability_is_innate(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return false;
    return node->data.ability_decl.is_innate;
}

GenericParams*
ast_ability_generic_params(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return NULL;
    return node->data.ability_decl.generic_params;
}

WhereClause*
ast_ability_where_clause(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return NULL;
    return node->data.ability_decl.where_clause;
}

size_t
ast_ability_require_field_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ABILITY_DECL)
        return 0;
    return node->data.ability_decl.require_count;
}

ASTNode*
ast_ability_require_field(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ABILITY_DECL
        || index >= node->data.ability_decl.require_count) {
        return NULL;
    }
    return node->data.ability_decl.require_fields[index];
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

GenericParams*
ast_role_generic_params(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_DECL)
        return NULL;
    return node->data.role_decl.generic_params;
}

WhereClause*
ast_role_where_clause(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_DECL)
        return NULL;
    return node->data.role_decl.where_clause;
}

ASTNode*
ast_role_parallel_block(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_DECL)
        return NULL;
    return node->data.role_decl.parallel_block;
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

GenericParams*
ast_roster_generic_params(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROSTER_DECL)
        return NULL;
    return node->data.roster_decl.generic_params;
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
ast_role_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_SLOT)
        return NULL;
    return node->data.role_slot.slot_name;
}

bool
ast_role_slot_is_dynamic(const ASTNode* node)
{
    return node != NULL && node->type == AST_ROLE_SLOT
        && node->data.role_slot.is_dynamic;
}

size_t
ast_role_slot_required_ability_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ROLE_SLOT)
        return 0;
    return node->data.role_slot.ability_count;
}

ASTNode*
ast_role_slot_required_ability(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ROLE_SLOT
        || index >= node->data.role_slot.ability_count) {
        return NULL;
    }
    return node->data.role_slot.required_abilities[index];
}

ASTNode**
ast_role_slot_required_abilities(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_role_slot_required_ability_count(node);
    if (node == NULL || node->type != AST_ROLE_SLOT)
        return NULL;
    return node->data.role_slot.required_abilities;
}

const char*
ast_party_shared_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_SHARED)
        return NULL;
    return node->data.party_shared.name;
}

ASTNode*
ast_party_shared_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_SHARED)
        return NULL;
    return node->data.party_shared.type;
}

ASTNode*
ast_party_shared_initializer(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARTY_SHARED)
        return NULL;
    return node->data.party_shared.initializer;
}

const char*
ast_require_field_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_REQUIRE_FIELD)
        return NULL;
    return node->data.require_field.name;
}

ASTNode*
ast_require_field_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_REQUIRE_FIELD)
        return NULL;
    return node->data.require_field.type;
}

const char*
ast_type_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TYPE)
        return NULL;
    return node->data.type.name;
}

GenericParams*
ast_type_generic_args(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TYPE)
        return NULL;
    return node->data.type.generic_args;
}

size_t
ast_generic_param_count(const GenericParams* params)
{
    return params != NULL ? params->count : 0;
}

GenericParam*
ast_generic_param_at(const GenericParams* params, size_t index)
{
    if (params == NULL || params->params == NULL || index >= params->count)
        return NULL;
    return params->params[index];
}

const char*
ast_generic_param_name(const GenericParam* param)
{
    return param != NULL ? param->name : NULL;
}

ASTNode*
ast_generic_param_constraint(const GenericParam* param)
{
    return param != NULL ? param->constraint : NULL;
}

ASTNode*
ast_generic_param_default_type(const GenericParam* param)
{
    return param != NULL ? param->default_type : NULL;
}

size_t
ast_type_tuple_element_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TYPE
        || node->data.type.tuple_elements == NULL)
        return 0;
    return node->data.type.tuple_element_count;
}

ASTNode*
ast_type_tuple_element(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_TYPE
        || node->data.type.tuple_elements == NULL
        || index >= node->data.type.tuple_element_count)
        return NULL;
    return node->data.type.tuple_elements[index];
}

bool
ast_replace_type_name_copy(ASTNode* node, const char* type_name)
{
    char *owned_type_name;

    if (node == NULL || node->type != AST_TYPE || type_name == NULL)
        return false;
    owned_type_name = pergyra_strdup(type_name);
    if (owned_type_name == NULL)
        return false;
    free(node->data.type.name);
    node->data.type.name = owned_type_name;
    return true;
}

