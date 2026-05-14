/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for domain declarations.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>

const char*
ast_class_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CLASS_DECL)
        return NULL;
    return node->data.class_decl.name;
}

const char*
ast_intent_decl_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.name;
}

ASTNode**
ast_intent_decl_involves(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_DECL) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_decl.involve_count;
    return node->data.intent_decl.involves;
}

size_t
ast_intent_decl_involve_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.involve_count;
}

ASTNode**
ast_intent_decl_values(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_DECL) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_decl.value_count;
    return node->data.intent_decl.values;
}

size_t
ast_intent_decl_value_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.value_count;
}

ASTNode**
ast_intent_decl_bindings(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_DECL) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_decl.binding_count;
    return node->data.intent_decl.bindings;
}

size_t
ast_intent_decl_binding_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.binding_count;
}

ASTNode**
ast_intent_decl_steps(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_DECL) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_decl.step_count;
    return node->data.intent_decl.steps;
}

size_t
ast_intent_decl_step_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.step_count;
}

bool
ast_intent_decl_is_concurrent(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_DECL
        && node->data.intent_decl.is_concurrent;
}

IntentRollbackPolicy
ast_intent_decl_rollback_policy(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return INTENT_ROLLBACK_NONE;
    return node->data.intent_decl.rollback_policy;
}

ASTNode*
ast_intent_decl_priority_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.priority_expr;
}

ASTNode*
ast_intent_decl_success_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.success_expr;
}

ASTNode*
ast_intent_decl_failure_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.failure_expr;
}

char**
ast_intent_decl_default_who_names(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_DECL) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_decl.default_who_count;
    return node->data.intent_decl.default_who_names;
}

size_t
ast_intent_decl_default_who_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.default_who_count;
}

ASTNode*
ast_intent_decl_default_where_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.default_where_type;
}

const char*
ast_intent_involves_alias(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_INVOLVES)
        return NULL;
    return node->data.intent_involves.alias;
}

ASTNode*
ast_intent_involves_subject_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_INVOLVES)
        return NULL;
    return node->data.intent_involves.subject_type;
}

const char*
ast_intent_value_alias(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_VALUE)
        return NULL;
    return node->data.intent_value.alias;
}

ASTNode*
ast_intent_value_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_VALUE)
        return NULL;
    return node->data.intent_value.value_type;
}

NominalDeclKind
ast_class_nominal_kind(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CLASS_DECL)
        return NOMINAL_DECL_CLASS;
    return node->data.class_decl.nominal_kind;
}

bool
ast_class_is_struct(const ASTNode* node)
{
    return node != NULL && node->type == AST_CLASS_DECL
        && node->data.class_decl.is_struct;
}

ClassField**
ast_class_fields(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_CLASS_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.class_decl.field_count;
    return node->data.class_decl.fields;
}

ASTNode**
ast_class_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_CLASS_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.class_decl.method_count;
    return node->data.class_decl.methods;
}

const char*
ast_enum_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ENUM_DECL)
        return NULL;
    return node->data.enum_decl.name;
}

char**
ast_enum_variants(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ENUM_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.enum_decl.variant_count;
    return node->data.enum_decl.variants;
}

size_t
ast_enum_variant_param_count(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ENUM_DECL
        || index >= node->data.enum_decl.variant_count
        || node->data.enum_decl.variant_param_counts == NULL) {
        return 0;
    }
    return node->data.enum_decl.variant_param_counts[index];
}

ASTNode*
ast_enum_variant_param(const ASTNode* node, size_t variant_index,
                       size_t param_index)
{
    if (node == NULL || node->type != AST_ENUM_DECL
        || variant_index >= node->data.enum_decl.variant_count
        || param_index >= ast_enum_variant_param_count(node, variant_index)
        || node->data.enum_decl.variant_params == NULL
        || node->data.enum_decl.variant_params[variant_index] == NULL) {
        return NULL;
    }
    return node->data.enum_decl.variant_params[variant_index][param_index];
}

ASTNode**
ast_enum_methods(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_ENUM_DECL)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.enum_decl.method_count;
    return node->data.enum_decl.methods;
}

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

const char*
ast_intent_step_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.name;
}

ASTNode*
ast_intent_step_where_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.where_type;
}

ASTNode*
ast_intent_step_using_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.using_expr;
}

ASTNode*
ast_intent_step_intent_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.intent_expr;
}

const char*
ast_intent_step_transfer_from_alias(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.transfer_from_alias;
}

const char*
ast_intent_step_transfer_to_alias(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.transfer_to_alias;
}

bool
ast_intent_step_replace_transfer_to_alias_copy(ASTNode* node, const char* alias)
{
    char *owned_alias;

    if (node == NULL || node->type != AST_INTENT_STEP || alias == NULL)
        return false;
    owned_alias = pergyra_strdup(alias);
    if (owned_alias == NULL)
        return false;
    free(node->data.intent_step.transfer_to_alias);
    node->data.intent_step.transfer_to_alias = owned_alias;
    return true;
}

char**
ast_intent_step_who_names(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.who_count;
    return node->data.intent_step.who_names;
}

size_t
ast_intent_step_who_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.who_count;
}

ASTNode**
ast_intent_step_on_exprs(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.on_expr_count;
    return node->data.intent_step.on_exprs;
}

size_t
ast_intent_step_on_expr_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.on_expr_count;
}

ASTNode**
ast_intent_step_compensate_exprs(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.compensate_expr_count;
    return node->data.intent_step.compensate_exprs;
}

size_t
ast_intent_step_compensate_expr_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.compensate_expr_count;
}

ASTNode*
ast_intent_step_pre_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.pre_expr;
}

ASTNode*
ast_intent_step_guard_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.guard_expr;
}

ASTNode*
ast_intent_step_post_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.post_expr;
}

ASTNode*
ast_intent_step_invariant_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.invariant_expr;
}

ASTNode**
ast_intent_step_required_abilities(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.required_ability_count;
    return node->data.intent_step.required_abilities;
}

size_t
ast_intent_step_required_ability_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.required_ability_count;
}

const char*
ast_intent_step_causes_effect(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.causes_effect;
}

char**
ast_intent_step_authorized_by(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_INTENT_STEP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.intent_step.authorized_by_count;
    return node->data.intent_step.authorized_by;
}

size_t
ast_intent_step_authorized_by_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return 0;
    return node->data.intent_step.authorized_by_count;
}

ASTNode*
ast_intent_step_expect_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return NULL;
    return node->data.intent_step.expect_expr;
}

bool
ast_intent_step_inherited_who_from_intent(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_who_from_intent;
}

bool
ast_intent_step_derived_who_from_on_receiver(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_who_from_on_receiver;
}

bool
ast_intent_step_derived_who_from_single_participant(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_who_from_single_participant;
}

bool
ast_intent_step_inherited_where_from_intent(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_where_from_intent;
}

bool
ast_intent_step_inherited_who_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_who_from_action;
}

bool
ast_intent_step_inherited_where_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_where_from_action;
}

bool
ast_intent_step_inherited_requires_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_requires_from_action;
}

bool
ast_intent_step_inherited_causes_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_causes_from_action;
}

bool
ast_intent_step_inherited_authorized_by_from_action(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.inherited_authorized_by_from_action;
}

bool
ast_intent_step_derived_authorized_by_from_zone(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_authorized_by_from_zone;
}

bool
ast_intent_step_derived_where_from_using(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_where_from_using;
}

bool
ast_intent_step_derived_where_from_transfer(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_where_from_transfer;
}

bool
ast_intent_step_derived_using_from_transfer(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_using_from_transfer;
}

bool
ast_intent_step_derived_using_from_where(const ASTNode* node)
{
    return node != NULL && node->type == AST_INTENT_STEP
        && node->data.intent_step.derived_using_from_where;
}

bool
ast_intent_step_set_where_type(ASTNode* node, ASTNode* where_type)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return false;
    node->data.intent_step.where_type = where_type;
    return true;
}

bool
ast_intent_step_set_using_expr(ASTNode* node, ASTNode* using_expr)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return false;
    node->data.intent_step.using_expr = using_expr;
    return true;
}

bool
ast_intent_step_set_causes_effect_copy(ASTNode* node, const char* causes_effect)
{
    if (node == NULL || node->type != AST_INTENT_STEP || causes_effect == NULL)
        return false;
    node->data.intent_step.causes_effect = pergyra_strdup(causes_effect);
    return node->data.intent_step.causes_effect != NULL;
}

bool
ast_intent_step_append_authorized_by_copy(ASTNode* node, const char* alias)
{
    char **grown;
    char *owned_alias;
    size_t next_capacity;

    if (node == NULL || node->type != AST_INTENT_STEP || alias == NULL)
        return false;
    owned_alias = pergyra_strdup(alias);
    if (owned_alias == NULL)
        return false;
    if (node->data.intent_step.authorized_by_count
        == node->data.intent_step.authorized_by_capacity) {
        next_capacity = node->data.intent_step.authorized_by_capacity == 0
            ? 4
            : node->data.intent_step.authorized_by_capacity * 2;
        if (next_capacity < node->data.intent_step.authorized_by_capacity
            || next_capacity > SIZE_MAX / sizeof(char *)) {
            free(owned_alias);
            return false;
        }
        grown = realloc(node->data.intent_step.authorized_by,
            next_capacity * sizeof(char *));
        if (grown == NULL) {
            free(owned_alias);
            return false;
        }
        node->data.intent_step.authorized_by = grown;
        node->data.intent_step.authorized_by_capacity = next_capacity;
    }
    node->data.intent_step.authorized_by[
        node->data.intent_step.authorized_by_count++] = owned_alias;
    return true;
}

bool
ast_intent_step_append_who_name_copy(ASTNode* node, const char* alias)
{
    char **grown;
    char *owned_alias;
    size_t next_capacity;

    if (node == NULL || node->type != AST_INTENT_STEP || alias == NULL)
        return false;
    owned_alias = pergyra_strdup(alias);
    if (owned_alias == NULL)
        return false;
    if (node->data.intent_step.who_count
        == node->data.intent_step.who_capacity) {
        next_capacity = node->data.intent_step.who_capacity == 0
            ? 4
            : node->data.intent_step.who_capacity * 2;
        if (next_capacity < node->data.intent_step.who_capacity
            || next_capacity > SIZE_MAX / sizeof(char *)) {
            free(owned_alias);
            return false;
        }
        grown = realloc(node->data.intent_step.who_names,
            next_capacity * sizeof(char *));
        if (grown == NULL) {
            free(owned_alias);
            return false;
        }
        node->data.intent_step.who_names = grown;
        node->data.intent_step.who_capacity = next_capacity;
    }
    node->data.intent_step.who_names[
        node->data.intent_step.who_count++] = owned_alias;
    return true;
}

bool
ast_intent_step_append_required_ability_clone(ASTNode* node, ASTNode* ability)
{
    ASTNode **grown;
    ASTNode *ability_copy;
    size_t next_capacity;

    if (node == NULL || node->type != AST_INTENT_STEP || ability == NULL)
        return false;
    ability_copy = ast_clone(ability);
    if (ability_copy == NULL)
        return false;
    if (node->data.intent_step.required_ability_count
        == node->data.intent_step.required_ability_capacity) {
        next_capacity = node->data.intent_step.required_ability_capacity == 0
            ? 4
            : node->data.intent_step.required_ability_capacity * 2;
        if (next_capacity < node->data.intent_step.required_ability_capacity
            || next_capacity > SIZE_MAX / sizeof(ASTNode *)) {
            ast_destroy(ability_copy);
            return false;
        }
        grown = realloc(node->data.intent_step.required_abilities,
            next_capacity * sizeof(ASTNode *));
        if (grown == NULL) {
            ast_destroy(ability_copy);
            return false;
        }
        node->data.intent_step.required_abilities = grown;
        node->data.intent_step.required_ability_capacity = next_capacity;
    }
    node->data.intent_step.required_abilities[
        node->data.intent_step.required_ability_count++] = ability_copy;
    return true;
}

void
ast_intent_step_clear_authorized_by(ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_STEP)
        return;
    for (size_t i = 0; i < node->data.intent_step.authorized_by_count; i++)
        free(node->data.intent_step.authorized_by[i]);
    free(node->data.intent_step.authorized_by);
    node->data.intent_step.authorized_by = NULL;
    node->data.intent_step.authorized_by_count = 0;
    node->data.intent_step.authorized_by_capacity = 0;
}

void
ast_intent_step_mark_inherited_who_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_who_from_action = true;
}

void
ast_intent_step_mark_derived_who_from_on_receiver(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_who_from_on_receiver = true;
}

void
ast_intent_step_mark_derived_who_from_single_participant(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_who_from_single_participant = true;
}

void
ast_intent_step_mark_inherited_where_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_where_from_action = true;
}

void
ast_intent_step_mark_inherited_requires_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_requires_from_action = true;
}

void
ast_intent_step_mark_inherited_causes_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_causes_from_action = true;
}

void
ast_intent_step_mark_inherited_authorized_by_from_action(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.inherited_authorized_by_from_action = true;
}

void
ast_intent_step_mark_derived_authorized_by_from_zone(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_authorized_by_from_zone = true;
}

void
ast_intent_step_mark_derived_where_from_using(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_where_from_using = true;
}

void
ast_intent_step_mark_derived_where_from_transfer(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_where_from_transfer = true;
}

void
ast_intent_step_mark_derived_using_from_transfer(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_using_from_transfer = true;
}

void
ast_intent_step_mark_derived_using_from_where(ASTNode* node)
{
    if (node != NULL && node->type == AST_INTENT_STEP)
        node->data.intent_step.derived_using_from_where = true;
}

const char*
ast_zone_layer_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_LAYER_SLOT)
        return NULL;
    return node->data.zone_layer_slot.slot_name;
}

const char*
ast_zone_layer_slot_layer_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_LAYER_SLOT)
        return NULL;
    return node->data.zone_layer_slot.layer_type;
}

bool
ast_zone_layer_slot_is_relation(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_LAYER_SLOT
        && node->data.zone_layer_slot.is_relation;
}

bool
ast_zone_layer_slot_is_pool(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_LAYER_SLOT
        && node->data.zone_layer_slot.is_pool;
}

int
ast_zone_layer_slot_pool_capacity(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_LAYER_SLOT)
        return 0;
    return node->data.zone_layer_slot.pool_capacity;
}

const char*
ast_zone_state_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.state_name;
}

bool
ast_zone_state_is_relation(const ASTNode* node)
{
    return node != NULL && node->type == AST_ZONE_STATE
        && node->data.zone_state.is_relation;
}

const char*
ast_zone_state_layer_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.layer_slot_name;
}

const char*
ast_zone_state_left_or_target_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.left_or_target_slot_name;
}

const char*
ast_zone_state_right_slot_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ZONE_STATE)
        return NULL;
    return node->data.zone_state.right_slot_name;
}
