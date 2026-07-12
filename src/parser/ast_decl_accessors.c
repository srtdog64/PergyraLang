/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for declaration shell, literal, and namespace nodes.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>

static char**
ast_declaration_name_slot(ASTNode* node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_FUNC_DECL:
        if (node->is_async_decl)
            return &node->data.async_func_decl.name;
        return &node->data.func_decl.name;
    case AST_CLASS_DECL:
        return &node->data.class_decl.name;
    case AST_LET_DECL:
        return &node->data.let_decl.name;
    case AST_TYPE_ALIAS:
        return &node->data.type_alias.name;
    case AST_LIFECYCLE_DECL:
        return &node->data.lifecycle_decl.subject;
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

GenericParams*
ast_declaration_generic_params(const ASTNode* node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
    case AST_FUNC_DECL:
        if (node->is_async_decl)
            return node->data.async_func_decl.generic_params;
        return node->data.func_decl.generic_params;
    case AST_CLASS_DECL:
        return node->data.class_decl.generic_params;
    case AST_ABILITY_DECL:
        return node->data.ability_decl.generic_params;
    case AST_ROLE_DECL:
        return node->data.role_decl.generic_params;
    case AST_PARTY_DECL:
        return node->data.party_decl.generic_params;
    case AST_ROSTER_DECL:
        return node->data.roster_decl.generic_params;
    default:
        return NULL;
    }
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
ast_let_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LET_DECL)
        return NULL;
    return node->data.let_decl.name;
}

ASTNode*
ast_let_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LET_DECL)
        return NULL;
    return node->data.let_decl.type;
}

ASTNode*
ast_let_initializer(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LET_DECL)
        return NULL;
    return node->data.let_decl.initializer;
}

bool
ast_let_attach_initializer(ASTNode* node, ASTNode* initializer)
{
    if (node == NULL || node->type != AST_LET_DECL)
        return false;
    node->data.let_decl.initializer = initializer;
    return true;
}

bool
ast_let_is_mutable(const ASTNode* node)
{
    return node != NULL && node->type == AST_LET_DECL
        && node->data.let_decl.is_mutable;
}

bool
ast_let_is_alias(const ASTNode* node)
{
    return node != NULL && node->type == AST_LET_DECL
        && node->data.let_decl.is_alias;
}

double
ast_number_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_NUMBER)
        return 0.0;
    return node->data.number.value;
}

bool
ast_number_is_long(const ASTNode* node)
{
    return node != NULL && node->type == AST_NUMBER
        && node->data.number.is_long;
}

bool
ast_number_is_float(const ASTNode* node)
{
    return node != NULL && node->type == AST_NUMBER
        && node->data.number.is_float;
}

bool
ast_number_is_duration(const ASTNode* node)
{
    return node != NULL && node->type == AST_NUMBER
        && node->data.number.is_duration;
}

/* Duration literal seal (docs/181 SS2.3): the parser normalizes
 * `<digits><unit>` to nanoseconds and re-stamps the node in one step so
 * a half-marked literal (duration without the i64 lane) cannot exist. */
bool
ast_number_make_duration(ASTNode* node, double ns_value)
{
    if (node == NULL || node->type != AST_NUMBER)
        return false;
    node->data.number.value = ns_value;
    node->data.number.is_long = true;
    node->data.number.is_float = false;
    node->data.number.is_duration = true;
    return true;
}

const char*
ast_string_value(const ASTNode* node)
{
    if (node == NULL || node->type != AST_STRING)
        return NULL;
    return node->data.string.value;
}

bool
ast_boolean_value(const ASTNode* node)
{
    return node != NULL && node->type == AST_BOOLEAN
        && node->data.boolean.value;
}

const char*
ast_identifier_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IDENTIFIER)
        return NULL;
    return node->data.identifier.name;
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
ast_use_module_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_USE_DECL)
        return NULL;
    return node->data.use_decl.module_name;
}

const char*
ast_import_path(const ASTNode* node)
{
    if (node == NULL || node->type != AST_IMPORT_DECL)
        return NULL;
    return node->data.import_decl.path;
}

const char*
ast_namespace_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return NULL;
    return node->data.namespace_decl.name;
}

size_t
ast_namespace_statement_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return 0;
    return node->data.namespace_decl.count;
}

ASTNode**
ast_namespace_statements(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_namespace_statement_count(node);
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return NULL;
    return node->data.namespace_decl.statements;
}

ASTNode*
ast_namespace_statement(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL
        || index >= node->data.namespace_decl.count)
        return NULL;
    return node->data.namespace_decl.statements[index];
}

void
ast_destroy_namespace_shell_only(ASTNode* node)
{
    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return;
    free(node->data.namespace_decl.name);
    free(node->data.namespace_decl.statements);
    node->data.namespace_decl.name = NULL;
    node->data.namespace_decl.statements = NULL;
    node->data.namespace_decl.count = 0;
    free(node);
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
ast_lifecycle_subject(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LIFECYCLE_DECL)
        return NULL;
    return node->data.lifecycle_decl.subject;
}

size_t
ast_lifecycle_transition_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LIFECYCLE_DECL)
        return 0;
    return node->data.lifecycle_decl.transition_count;
}

const LifecycleTransitionDecl*
ast_lifecycle_transition(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_LIFECYCLE_DECL
        || index >= node->data.lifecycle_decl.transition_count) {
        return NULL;
    }
    return &node->data.lifecycle_decl.transitions[index];
}

const char*
ast_event_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_DECL)
        return NULL;
    return node->data.event_decl.name;
}
