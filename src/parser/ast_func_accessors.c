/*
 * Copyright (c) 2026 Pergyra Language Project
 * Function declaration AST accessors.
 */

#include "ast.h"
#include "ast_api.h"
#include "../common/string_compat.h"

#include <stdlib.h>

size_t
ast_func_param_count(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return 0;
    if (node->is_async_decl)
        return node->data.async_func_decl.param_count;
    return node->data.func_decl.param_count;
}

FuncParam **
ast_func_params(const ASTNode *node, size_t *count_out)
{
    if (count_out != NULL)
        *count_out = ast_func_param_count(node);
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return node->data.async_func_decl.params;
    return node->data.func_decl.params;
}

FuncParam *
ast_func_param(const ASTNode *node, size_t index)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl) {
        if (index >= node->data.async_func_decl.param_count)
            return NULL;
        return node->data.async_func_decl.params != NULL
            ? node->data.async_func_decl.params[index]
            : NULL;
    }
    if (index >= node->data.func_decl.param_count)
        return NULL;
    return node->data.func_decl.params != NULL
        ? node->data.func_decl.params[index]
        : NULL;
}

GenericParams *
ast_func_generic_params(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return node->data.async_func_decl.generic_params;
    return node->data.func_decl.generic_params;
}

WhereClause *
ast_func_where_clause(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return node->data.async_func_decl.where_clause;
    return node->data.func_decl.where_clause;
}

ASTNode *
ast_func_return_type(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return node->data.async_func_decl.return_type;
    return node->data.func_decl.return_type;
}

const char *
ast_func_semantic_return_type_name(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return node->data.async_func_decl.semantic_return_type_name;
    return node->data.func_decl.semantic_return_type_name;
}

bool
ast_func_set_semantic_return_type_name_copy(ASTNode *node,
                                            const char *type_name)
{
    char *copy = NULL;

    if (node == NULL || node->type != AST_FUNC_DECL || type_name == NULL)
        return false;
    copy = pergyra_strdup(type_name);
    if (copy == NULL)
        return false;
    if (node->is_async_decl) {
        free(node->data.async_func_decl.semantic_return_type_name);
        node->data.async_func_decl.semantic_return_type_name = copy;
        return true;
    }
    free(node->data.func_decl.semantic_return_type_name);
    node->data.func_decl.semantic_return_type_name = copy;
    return true;
}

ASTNode *
ast_func_body(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return node->data.async_func_decl.body;
    return node->data.func_decl.body;
}

bool
ast_func_set_return_type(ASTNode *node, ASTNode *return_type)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return false;
    if (node->is_async_decl) {
        if (node->data.async_func_decl.return_type != NULL)
            return false;
        node->data.async_func_decl.return_type = return_type;
        return true;
    }
    if (node->data.func_decl.return_type != NULL)
        return false;
    node->data.func_decl.return_type = return_type;
    return true;
}

bool
ast_func_attach_body(ASTNode *node, ASTNode *body)
{
    if (node == NULL || node->type != AST_FUNC_DECL) {
        return false;
    }
    if (node->is_async_decl) {
        if (node->data.async_func_decl.body != NULL)
            return false;
        node->data.async_func_decl.body = body;
        return true;
    }
    if (node->data.func_decl.body != NULL)
        return false;
    node->data.func_decl.body = body;
    return true;
}

ASTNode *
ast_func_detach_body(ASTNode *node)
{
    ASTNode *body;

    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl) {
        body = node->data.async_func_decl.body;
        node->data.async_func_decl.body = NULL;
        return body;
    }
    body = node->data.func_decl.body;
    node->data.func_decl.body = NULL;
    return body;
}

bool
ast_func_is_action(const ASTNode *node)
{
    return node != NULL && node->type == AST_FUNC_DECL
        && !node->is_async_decl
        && node->data.func_decl.is_action;
}

AccessModifier
ast_func_access(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return ACCESS_PUBLIC;
    if (node->is_async_decl)
        return node->data.async_func_decl.access;
    return node->data.func_decl.access;
}

bool
ast_func_has_explicit_access(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return false;
    if (node->is_async_decl)
        return node->has_explicit_access;
    return node->data.func_decl.has_explicit_access;
}

bool
ast_func_has_effects_clause(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return false;
    if (node->is_async_decl)
        return node->data.async_func_decl.has_effects_clause;
    return node->data.func_decl.has_effects_clause;
}

uint32_t
ast_func_declared_effects(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return 0;
    if (node->is_async_decl)
        return node->data.async_func_decl.declared_effects;
    return node->data.func_decl.declared_effects;
}

StructuredComment *
ast_func_doc_comment(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return node->data.async_func_decl.doc_comment;
    return node->data.func_decl.doc_comment;
}

size_t
ast_func_required_ability_count(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return 0;
    if (node->is_async_decl)
        return 0;
    return node->data.func_decl.required_ability_count;
}

ASTNode **
ast_func_required_abilities(const ASTNode *node, size_t *count_out)
{
    if (count_out != NULL)
        *count_out = ast_func_required_ability_count(node);
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return NULL;
    return node->data.func_decl.required_abilities;
}

ASTNode *
ast_func_required_ability(const ASTNode *node, size_t index)
{
    if (node == NULL || node->type != AST_FUNC_DECL
        || node->is_async_decl
        || index >= node->data.func_decl.required_ability_count) {
        return NULL;
    }
    return node->data.func_decl.required_abilities != NULL
        ? node->data.func_decl.required_abilities[index]
        : NULL;
}

const char *
ast_func_within_zone(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return NULL;
    return node->data.func_decl.within_zone;
}

bool
ast_func_set_within_zone_copy(ASTNode *node, const char *within_zone)
{
    char *copy = NULL;

    if (node == NULL || node->type != AST_FUNC_DECL)
        return false;
    if (node->is_async_decl)
        return false;
    if (within_zone != NULL) {
        copy = pergyra_strdup(within_zone);
        if (copy == NULL)
            return false;
    }

    free(node->data.func_decl.within_zone);
    node->data.func_decl.within_zone = copy;
    return true;
}

const char *
ast_func_causes_effect(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    if (node->is_async_decl)
        return NULL;
    return node->data.func_decl.causes_effect;
}

size_t
ast_func_authorized_by_count(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return 0;
    if (node->is_async_decl)
        return 0;
    return node->data.func_decl.authorized_by_count;
}

const char *
ast_func_authorized_by(const ASTNode *node, size_t index)
{
    if (node == NULL || node->type != AST_FUNC_DECL
        || node->is_async_decl
        || node->data.func_decl.authorized_by == NULL
        || index >= node->data.func_decl.authorized_by_count) {
        return NULL;
    }
    return node->data.func_decl.authorized_by[index];
}

const char *
ast_async_func_name(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL || !node->is_async_decl)
        return NULL;
    return node->data.async_func_decl.name;
}

size_t
ast_async_func_param_count(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL || !node->is_async_decl)
        return 0;
    return node->data.async_func_decl.param_count;
}

FuncParam **
ast_async_func_params(const ASTNode *node, size_t *count_out)
{
    if (count_out != NULL)
        *count_out = ast_async_func_param_count(node);
    if (node == NULL || node->type != AST_FUNC_DECL || !node->is_async_decl)
        return NULL;
    return node->data.async_func_decl.params;
}

FuncParam *
ast_async_func_param(const ASTNode *node, size_t index)
{
    if (node == NULL || node->type != AST_FUNC_DECL || !node->is_async_decl)
        return NULL;
    if (index >= node->data.async_func_decl.param_count)
        return NULL;
    return node->data.async_func_decl.params != NULL
        ? node->data.async_func_decl.params[index]
        : NULL;
}

ASTNode *
ast_async_func_body(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL || !node->is_async_decl)
        return NULL;
    return node->data.async_func_decl.body;
}
