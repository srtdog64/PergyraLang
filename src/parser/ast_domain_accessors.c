/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for domain declarations.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char**
ast_declaration_name_slot(ASTNode* node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_FUNC_DECL:
        return &node->data.func_decl.name;
    case AST_CLASS_DECL:
        return &node->data.class_decl.name;
    case AST_LET_DECL:
        return &node->data.let_decl.name;
    case AST_TYPE_ALIAS:
        return &node->data.type_alias.name;
    case AST_ABILITY_DECL:
        return &node->data.ability_decl.name;
    case AST_ROLE_DECL:
        return &node->data.role_decl.name;
    case AST_PARTY_DECL:
        return &node->data.party_decl.name;
    case AST_ROSTER_DECL:
        return &node->data.roster_decl.name;
    case AST_WORLD_DECL:
        return &node->data.world_decl.name;
    case AST_RELATION_DECL:
        return &node->data.relation_decl.name;
    case AST_EFFECT_DECL:
        return &node->data.effect_decl.name;
    case AST_ZONE_DECL:
        return &node->data.zone_decl.name;
    case AST_EVENT_DECL:
        return &node->data.event_decl.name;
    case AST_ENUM_DECL:
        return &node->data.enum_decl.name;
    default:
        return NULL;
    }
}

const char*
ast_declaration_name(const ASTNode* node)
{
    char **slot = ast_declaration_name_slot((ASTNode*)node);
    return slot != NULL ? *slot : NULL;
}

bool
ast_replace_declaration_name_copy(ASTNode* node, const char* name)
{
    char **slot;
    char *copy;

    if (name == NULL)
        return false;
    slot = ast_declaration_name_slot(node);
    if (slot == NULL)
        return false;
    copy = pergyra_strdup(name);
    if (copy == NULL)
        return false;
    free(*slot);
    *slot = copy;
    return true;
}

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

GenericParams*
ast_class_generic_params(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CLASS_DECL)
        return NULL;
    return node->data.class_decl.generic_params;
}

WhereClause*
ast_class_where_clause(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CLASS_DECL)
        return NULL;
    return node->data.class_decl.where_clause;
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
ast_extern_block_abi(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EXTERN_BLOCK)
        return NULL;
    return node->data.extern_block.abi;
}

ASTNode**
ast_extern_block_declarations(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = 0;
    if (node == NULL || node->type != AST_EXTERN_BLOCK)
        return NULL;
    if (count_out != NULL)
        *count_out = node->data.extern_block.count;
    return node->data.extern_block.declarations;
}

ASTNode*
ast_extern_block_declaration(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_EXTERN_BLOCK
        || index >= node->data.extern_block.count) {
        return NULL;
    }
    return node->data.extern_block.declarations[index];
}

const char*
ast_type_alias_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TYPE_ALIAS)
        return NULL;
    return node->data.type_alias.name;
}

ASTNode*
ast_type_alias_target_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TYPE_ALIAS)
        return NULL;
    return node->data.type_alias.target_type;
}

const char*
ast_event_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_DECL)
        return NULL;
    return node->data.event_decl.name;
}

size_t
ast_event_param_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_DECL)
        return 0;
    return node->data.event_decl.param_count;
}

ASTNode*
ast_event_param(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_EVENT_DECL
        || index >= node->data.event_decl.param_count) {
        return NULL;
    }
    return node->data.event_decl.params[index];
}

ASTNode*
ast_event_return_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_DECL)
        return NULL;
    return node->data.event_decl.return_type;
}

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

GenericParams*
ast_call_generic_args(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.generic_args;
}

size_t
ast_call_generic_arg_count(const ASTNode* node)
{
    GenericParams *generic_args = ast_call_generic_args(node);
    return generic_args != NULL ? generic_args->count : 0;
}

GenericParam*
ast_call_generic_arg(const ASTNode* node, size_t index)
{
    GenericParams *generic_args = ast_call_generic_args(node);
    if (generic_args == NULL
        || generic_args->params == NULL
        || index >= generic_args->count) {
        return NULL;
    }
    return generic_args->params[index];
}

ASTNode*
ast_call_callee(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.callee;
}

size_t
ast_call_arg_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return 0;
    return node->data.call.arg_count;
}

ASTNode**
ast_call_arguments(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_call_arg_count(node);
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.arguments;
}

ASTNode*
ast_call_argument(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_CALL
        || index >= node->data.call.arg_count) {
        return NULL;
    }
    return node->data.call.arguments[index];
}

void
ast_init_call_borrowed_view(ASTNode* node, ASTNode* callee,
                            ASTNode** arguments, size_t arg_count)
{
    if (node == NULL)
        return;
    memset(node, 0, sizeof(*node));
    node->type = AST_CALL;
    node->data.call.callee = callee;
    node->data.call.arguments = arguments;
    node->data.call.arg_count = arg_count;
}

ASTNode*
ast_member_object(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MEMBER_ACCESS)
        return NULL;
    return node->data.member.object;
}

const char*
ast_member_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MEMBER_ACCESS)
        return NULL;
    return node->data.member.name;
}

ASTNode*
ast_array_access_array(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ARRAY_ACCESS)
        return NULL;
    return node->data.array_access.array;
}

ASTNode*
ast_array_access_index(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ARRAY_ACCESS)
        return NULL;
    return node->data.array_access.index;
}

ASTNode*
ast_assignment_target(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    return node->data.assignment.target;
}

ASTNode*
ast_assignment_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    return node->data.assignment.value;
}

ASTNode*
ast_await_expression(const ASTNode* node)
{
    if (node == NULL || node->type != AST_AWAIT_EXPR)
        return NULL;
    return node->data.await_expr.expression;
}

ASTNode*
ast_channel_send_channel(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_SEND)
        return NULL;
    return node->data.channel_send.channel;
}

ASTNode*
ast_channel_send_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_SEND)
        return NULL;
    return node->data.channel_send.value;
}

ASTNode*
ast_channel_recv_channel(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CHANNEL_RECV)
        return NULL;
    return node->data.channel_recv.channel;
}

ASTNode*
ast_unary_operand(const ASTNode* node)
{
    if (node == NULL || node->type != AST_UNARY)
        return NULL;
    return node->data.unary.operand;
}

Token
ast_unary_operator(const ASTNode* node)
{
    if (node == NULL || node->type != AST_UNARY)
        return (Token){0};
    return node->data.unary.op;
}

ASTNode*
ast_binary_left(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BINARY)
        return NULL;
    return node->data.binary.left;
}

ASTNode*
ast_binary_right(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BINARY)
        return NULL;
    return node->data.binary.right;
}

Token
ast_binary_operator(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BINARY)
        return (Token){0};
    return node->data.binary.op;
}

size_t
ast_array_literal_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ARRAY_LITERAL)
        return 0;
    return node->data.array_literal.count;
}

ASTNode*
ast_array_literal_element(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ARRAY_LITERAL
        || index >= node->data.array_literal.count)
        return NULL;
    return node->data.array_literal.elements[index];
}

size_t
ast_tuple_literal_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TUPLE_LITERAL)
        return 0;
    return node->data.tuple_literal.count;
}

ASTNode*
ast_tuple_literal_element(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_TUPLE_LITERAL
        || index >= node->data.tuple_literal.count)
        return NULL;
    return node->data.tuple_literal.elements[index];
}

const char*
ast_break_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BREAK)
        return NULL;
    return node->data.break_stmt.label;
}

const char*
ast_continue_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CONTINUE)
        return NULL;
    return node->data.continue_stmt.label;
}

const char*
ast_for_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.label;
}

const char*
ast_for_variable(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.variable;
}

ASTNode*
ast_for_range_start(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.range_start;
}

ASTNode*
ast_for_range_end(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.range_end;
}

ASTNode*
ast_for_iterable(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.iterable;
}

ASTNode*
ast_for_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_FOR_LOOP)
        return NULL;
    return node->data.for_loop.body;
}

ASTNode*
ast_if_condition(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IF_STMT)
        return NULL;
    return node->data.if_stmt.condition;
}

ASTNode*
ast_if_then_branch(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IF_STMT)
        return NULL;
    return node->data.if_stmt.then_branch;
}

ASTNode*
ast_if_else_branch(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IF_STMT)
        return NULL;
    return node->data.if_stmt.else_branch;
}

const char*
ast_while_label(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WHILE_LOOP)
        return NULL;
    return node->data.while_loop.label;
}

ASTNode*
ast_while_condition(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WHILE_LOOP)
        return NULL;
    return node->data.while_loop.condition;
}

ASTNode*
ast_while_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WHILE_LOOP)
        return NULL;
    return node->data.while_loop.body;
}

ASTNode*
ast_unsafe_block_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_UNSAFE_BLOCK)
        return NULL;
    return node->data.unsafe_block.body;
}

ASTNode*
ast_defer_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_DEFER_STMT)
        return NULL;
    return node->data.defer_stmt.body;
}

ASTNode*
ast_return_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_RETURN)
        return NULL;
    return node->data.return_stmt.value;
}

size_t
ast_event_handler_param_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_HANDLER_TYPE)
        return 0;
    return node->data.event_handler_type.param_count;
}

ASTNode**
ast_event_handler_param_types(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_event_handler_param_count(node);
    if (node == NULL || node->type != AST_EVENT_HANDLER_TYPE)
        return NULL;
    return node->data.event_handler_type.param_types;
}

ASTNode*
ast_event_handler_param_type(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_EVENT_HANDLER_TYPE)
        return NULL;
    if (index >= node->data.event_handler_type.param_count)
        return NULL;
    return node->data.event_handler_type.param_types != NULL
        ? node->data.event_handler_type.param_types[index]
        : NULL;
}

ASTNode**
ast_task_group_tasks(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_TASK_GROUP) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.task_group.task_count;
    return node->data.task_group.tasks;
}

size_t
ast_task_group_task_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_TASK_GROUP)
        return 0;
    return node->data.task_group.task_count;
}

ASTNode*
ast_task_group_task(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_TASK_GROUP)
        return NULL;
    if (index >= node->data.task_group.task_count)
        return NULL;
    return node->data.task_group.tasks != NULL
        ? node->data.task_group.tasks[index]
        : NULL;
}

bool
ast_task_group_wait_all(const ASTNode* node)
{
    return node != NULL && node->type == AST_TASK_GROUP
        && node->data.task_group.wait_all;
}

ASTNode*
ast_spawn_function(const ASTNode* node)
{
    if (node == NULL || node->type != AST_SPAWN_EXPR)
        return NULL;
    return node->data.spawn_expr.function;
}

ASTNode**
ast_spawn_arguments(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_SPAWN_EXPR) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.spawn_expr.arg_count;
    return node->data.spawn_expr.arguments;
}

size_t
ast_spawn_arg_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_SPAWN_EXPR)
        return 0;
    return node->data.spawn_expr.arg_count;
}

ASTNode*
ast_spawn_argument(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_SPAWN_EXPR)
        return NULL;
    if (index >= node->data.spawn_expr.arg_count)
        return NULL;
    return node->data.spawn_expr.arguments != NULL
        ? node->data.spawn_expr.arguments[index]
        : NULL;
}

bool
ast_spawn_is_blocking(const ASTNode* node)
{
    return node != NULL && node->type == AST_SPAWN_EXPR
        && node->data.spawn_expr.is_blocking;
}

ASTNode**
ast_async_block_statements(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_ASYNC_BLOCK) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.async_block.statement_count;
    return node->data.async_block.statements;
}

size_t
ast_async_block_statement_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_ASYNC_BLOCK)
        return 0;
    return node->data.async_block.statement_count;
}

ASTNode*
ast_async_block_statement(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_ASYNC_BLOCK)
        return NULL;
    if (index >= node->data.async_block.statement_count)
        return NULL;
    return node->data.async_block.statements != NULL
        ? node->data.async_block.statements[index]
        : NULL;
}

ASTNode**
ast_parallel_tasks(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.parallel.task_count;
    return node->data.parallel.tasks;
}

size_t
ast_parallel_task_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return 0;
    return node->data.parallel.task_count;
}

ASTNode*
ast_parallel_task(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_PARALLEL_BLOCK)
        return NULL;
    if (index >= node->data.parallel.task_count)
        return NULL;
    return node->data.parallel.tasks != NULL
        ? node->data.parallel.tasks[index]
        : NULL;
}

ASTNode*
ast_with_slot_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WITH_STMT)
        return NULL;
    return node->data.with_stmt.slot_type;
}

const char*
ast_with_alias(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WITH_STMT)
        return NULL;
    return node->data.with_stmt.alias;
}

ASTNode*
ast_with_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WITH_STMT)
        return NULL;
    return node->data.with_stmt.body;
}

bool
ast_with_is_secure(const ASTNode* node)
{
    return node != NULL && node->type == AST_WITH_STMT
        && node->data.with_stmt.is_secure;
}

const char*
ast_with_security_level(const ASTNode* node)
{
    if (node == NULL || node->type != AST_WITH_STMT)
        return NULL;
    return node->data.with_stmt.security_level;
}

ASTNode**
ast_select_cases(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_SELECT_STMT) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.select_stmt.case_count;
    return node->data.select_stmt.cases;
}

size_t
ast_select_case_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_SELECT_STMT)
        return 0;
    return node->data.select_stmt.case_count;
}

ASTNode*
ast_select_case(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_SELECT_STMT)
        return NULL;
    if (index >= node->data.select_stmt.case_count)
        return NULL;
    return node->data.select_stmt.cases != NULL
        ? node->data.select_stmt.cases[index]
        : NULL;
}

ASTNode*
ast_select_default_case(const ASTNode* node)
{
    if (node == NULL || node->type != AST_SELECT_STMT)
        return NULL;
    return node->data.select_stmt.default_case;
}

ASTNode*
ast_event_handler_return_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_HANDLER_TYPE)
        return NULL;
    return node->data.event_handler_type.return_type;
}
