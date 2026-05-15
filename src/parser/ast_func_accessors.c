/*
 * Copyright (c) 2026 Pergyra Language Project
 * Function declaration AST accessors.
 */

#include "ast.h"
#include "ast_api.h"

size_t
ast_func_param_count(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return 0;
    return node->data.func_decl.param_count;
}

FuncParam **
ast_func_params(const ASTNode *node, size_t *count_out)
{
    if (count_out != NULL)
        *count_out = ast_func_param_count(node);
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    return node->data.func_decl.params;
}

FuncParam *
ast_func_param(const ASTNode *node, size_t index)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
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
    return node->data.func_decl.generic_params;
}

WhereClause *
ast_func_where_clause(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    return node->data.func_decl.where_clause;
}

ASTNode *
ast_func_return_type(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    return node->data.func_decl.return_type;
}

ASTNode *
ast_func_body(const ASTNode *node)
{
    if (node == NULL || node->type != AST_FUNC_DECL)
        return NULL;
    return node->data.func_decl.body;
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
