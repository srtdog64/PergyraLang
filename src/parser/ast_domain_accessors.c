/*
 * Copyright (c) 2026 Pergyra Language Project
 * Read-only AST accessors for domain declarations.
 */

#include "ast_constructors_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
ast_intent_decl_return_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.return_type;
}

bool
ast_intent_decl_has_typed_result(const ASTNode* node)
{
    return ast_intent_decl_return_type(node) != NULL;
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

const char*
ast_intent_decl_success_terminal_step(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.success_terminal.step_name;
}

uint32_t
ast_intent_decl_success_terminal_step_syntax_id(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.success_terminal.step_syntax_id;
}

ASTNode*
ast_intent_decl_success_terminal_expr(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return NULL;
    return node->data.intent_decl.success_terminal.expr;
}

size_t
ast_intent_decl_failure_terminal_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.failure_terminal_count;
}

const char*
ast_intent_decl_failure_terminal_step(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_INTENT_DECL
        || index >= node->data.intent_decl.failure_terminal_count) {
        return NULL;
    }
    return node->data.intent_decl.failure_terminals[index].step_name;
}

uint32_t
ast_intent_decl_failure_terminal_step_syntax_id(const ASTNode* node,
                                                size_t index)
{
    if (node == NULL || node->type != AST_INTENT_DECL
        || index >= node->data.intent_decl.failure_terminal_count) {
        return 0;
    }
    return node->data.intent_decl.failure_terminals[index].step_syntax_id;
}

ASTNode*
ast_intent_decl_failure_terminal_expr(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_INTENT_DECL
        || index >= node->data.intent_decl.failure_terminal_count) {
        return NULL;
    }
    return node->data.intent_decl.failure_terminals[index].expr;
}

bool
ast_intent_decl_set_success_terminal_step_syntax_id(ASTNode* node,
                                                    uint32_t syntax_id)
{
    if (node == NULL || node->type != AST_INTENT_DECL || syntax_id == 0)
        return false;
    node->data.intent_decl.success_terminal.step_syntax_id = syntax_id;
    return true;
}

bool
ast_intent_decl_set_failure_terminal_step_syntax_id(ASTNode* node,
                                                    size_t index,
                                                    uint32_t syntax_id)
{
    if (node == NULL || node->type != AST_INTENT_DECL || syntax_id == 0
        || index >= node->data.intent_decl.failure_terminal_count) {
        return false;
    }
    node->data.intent_decl.failure_terminals[index].step_syntax_id = syntax_id;
    return true;
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

int
ast_intent_decl_retry_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_INTENT_DECL)
        return 0;
    return node->data.intent_decl.retry_count;
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

size_t
ast_class_field_destructure_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CLASS_DECL)
        return 0;
    return node->data.class_decl.field_destructure_count;
}

ASTNode*
ast_class_field_destructure_at(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_CLASS_DECL
        || index >= node->data.class_decl.field_destructure_count)
        return NULL;
    return node->data.class_decl.field_destructures[index];
}

bool
ast_class_append_field_destructure(ASTNode* node, ASTNode* destructure)
{
    size_t cap;
    ASTNode **grown;

    if (node == NULL || node->type != AST_CLASS_DECL || destructure == NULL)
        return false;
    if (node->data.class_decl.field_destructure_count
            == node->data.class_decl.field_destructure_capacity) {
        cap = node->data.class_decl.field_destructure_capacity == 0
            ? 2 : node->data.class_decl.field_destructure_capacity * 2;
        grown = realloc(node->data.class_decl.field_destructures,
                        cap * sizeof(ASTNode *));
        if (grown == NULL)
            return false;
        node->data.class_decl.field_destructures = grown;
        node->data.class_decl.field_destructure_capacity = cap;
    }
    node->data.class_decl.field_destructures
        [node->data.class_decl.field_destructure_count++] = destructure;
    return true;
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
