/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST domain, intent, and event constructors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

// Ability declaration
ASTNode* ast_create_ability_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ABILITY_DECL);
    node->data.ability_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.ability_decl.require_fields = NULL;
    node->data.ability_decl.require_count = 0;
    node->data.ability_decl.methods = NULL;
    node->data.ability_decl.method_count = 0;
    node->data.ability_decl.generic_params = NULL;
    node->data.ability_decl.where_clause = NULL;
    node->data.ability_decl.access = ACCESS_PUBLIC;
    node->data.ability_decl.has_explicit_access = false;
    node->data.ability_decl.is_innate = false;
    node->data.ability_decl.doc_comment = NULL;
    node->is_exported = true;
    return node;
}

// Role declaration
ASTNode* ast_create_role_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ROLE_DECL);
    node->data.role_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.role_decl.for_type = NULL;
    node->data.role_decl.includes = NULL;
    node->data.role_decl.include_count = 0;
    node->data.role_decl.impl_abilities = NULL;
    node->data.role_decl.impl_count = 0;
    node->data.role_decl.parallel_block = NULL;
    node->data.role_decl.generic_params = NULL;
    node->data.role_decl.where_clause = NULL;
    node->data.role_decl.doc_comment = NULL;
    return node;
}

// Include statement
ASTNode* ast_create_include_statement(const char* role_name) {
    ASTNode* node = ast_create_node(AST_INCLUDE_STMT);
    node->data.include_stmt.role_name = role_name ? pergyra_strdup(role_name) : NULL;
    node->data.include_stmt.type_args = NULL;
    return node;
}

// Require field
ASTNode* ast_create_require_field(const char* name) {
    ASTNode* node = ast_create_node(AST_REQUIRE_FIELD);
    node->data.require_field.name = name ? pergyra_strdup(name) : NULL;
    node->data.require_field.type = NULL;
    return node;
}

// Impl ability block
ASTNode* ast_create_impl_ability(ASTNode* ability_ref) {
    ASTNode* node = ast_create_node(AST_IMPL_ABILITY);
    node->data.impl_ability.ability_ref = ability_ref;
    node->data.impl_ability.methods = NULL;
    node->data.impl_ability.method_count = 0;
    return node;
}

// Override function
ASTNode* ast_create_override_func(ASTNode* func_decl) {
    ASTNode* node = ast_create_node(AST_OVERRIDE_FUNC);
    node->data.override_func.func_decl = func_decl;
    node->data.override_func.calls_super = false;
    return node;
}

ASTNode*
ast_override_func_decl(const ASTNode* node)
{
    if (node == NULL || node->type != AST_OVERRIDE_FUNC)
        return NULL;
    return node->data.override_func.func_decl;
}

bool
ast_override_calls_super(const ASTNode* node)
{
    if (node == NULL || node->type != AST_OVERRIDE_FUNC)
        return false;
    return node->data.override_func.calls_super;
}

// Roster declaration
ASTNode* ast_create_roster_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_ROSTER_DECL);
    node->data.roster_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.roster_decl.party_slots = NULL;
    node->data.roster_decl.party_count = 0;
    node->data.roster_decl.shared_fields = NULL;
    node->data.roster_decl.shared_count = 0;
    node->data.roster_decl.methods = NULL;
    node->data.roster_decl.method_count = 0;
    node->data.roster_decl.generic_params = NULL;
    node->data.roster_decl.doc_comment = NULL;
    return node;
}

// Roster slot
ASTNode* ast_create_roster_slot(const char* slot_name, const char* party_type) {
    ASTNode* node = ast_create_node(AST_SYSTEMIC_SLOT);
    node->data.roster_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.roster_slot.party_type = party_type ? pergyra_strdup(party_type) : NULL;
    node->data.roster_slot.is_array = false;
    return node;
}

ASTNode* ast_create_relation_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_RELATION_DECL);
    node->data.relation_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.relation_decl.slots = NULL;
    node->data.relation_decl.slot_count = 0;
    node->data.relation_decl.refreshes = NULL;
    node->data.relation_decl.refresh_count = 0;
    node->data.relation_decl.shared_fields = NULL;
    node->data.relation_decl.shared_count = 0;
    node->data.relation_decl.methods = NULL;
    node->data.relation_decl.method_count = 0;
    node->data.relation_decl.doc_comment = NULL;
    node->data.relation_decl.between_left_kind = RELATION_ENDPOINT_NAMED;
    node->data.relation_decl.between_right_kind = RELATION_ENDPOINT_NAMED;
    node->data.relation_decl.between_left_type = NULL;
    node->data.relation_decl.between_right_type = NULL;
    node->data.relation_decl.between_left_many = false;
    node->data.relation_decl.between_right_many = false;
    return node;
}

ASTNode* ast_create_effect_declaration(const char* name) {
    ASTNode* node = ast_create_node(AST_EFFECT_DECL);
    node->data.effect_decl.name = name ? pergyra_strdup(name) : NULL;
    node->data.effect_decl.slots = NULL;
    node->data.effect_decl.slot_count = 0;
    node->data.effect_decl.refreshes = NULL;
    node->data.effect_decl.refresh_count = 0;
    node->data.effect_decl.shared_fields = NULL;
    node->data.effect_decl.shared_count = 0;
    node->data.effect_decl.methods = NULL;
    node->data.effect_decl.method_count = 0;
    node->data.effect_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_domain_slot(const char* slot_name, bool is_subject) {
    ASTNode* node = ast_create_node(AST_DOMAIN_SLOT);
    node->data.domain_slot.slot_name = slot_name ? pergyra_strdup(slot_name) : NULL;
    node->data.domain_slot.type = NULL;
    node->data.domain_slot.is_subject = is_subject;
    node->data.domain_slot.is_vessel = false;
    node->data.domain_slot.is_tobject = false;
    node->data.domain_slot.is_binding = is_subject;
    node->data.domain_slot.initializer = NULL;
    return node;
}
