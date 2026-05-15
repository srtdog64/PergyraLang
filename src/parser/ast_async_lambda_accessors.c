/*
 * Copyright (c) 2026 Pergyra Language Project
 * Split AST accessor owner. Keep responsibility slices below the 600 LOC signal.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

ASTNode**
ast_lambda_params(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_LAMBDA_EXPR) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.lambda_expr.param_count;
    return node->data.lambda_expr.params;
}

size_t
ast_lambda_param_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LAMBDA_EXPR)
        return 0;
    return node->data.lambda_expr.param_count;
}

ASTNode*
ast_lambda_param(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_LAMBDA_EXPR)
        return NULL;
    if (index >= node->data.lambda_expr.param_count)
        return NULL;
    return node->data.lambda_expr.params != NULL
        ? node->data.lambda_expr.params[index]
        : NULL;
}

ASTNode*
ast_lambda_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LAMBDA_EXPR)
        return NULL;
    return node->data.lambda_expr.body;
}

ASTNode*
ast_lambda_return_type(const ASTNode* node)
{
    if (node == NULL || node->type != AST_LAMBDA_EXPR)
        return NULL;
    return node->data.lambda_expr.return_type;
}

bool
ast_lambda_is_async(const ASTNode* node)
{
    return node != NULL && node->type == AST_LAMBDA_EXPR
        && node->data.lambda_expr.is_async;
}
