/*
 * Copyright (c) 2025 Pergyra Language Project
 * Async/channel AST constructors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdlib.h>

ASTNode* ast_create_async_function(const char* name, bool is_async) {
    ASTNode* node = ast_create_node(AST_FUNC_DECL);
    if (!node) return NULL;
    node->is_async_decl = is_async;

    /* AST_FUNC_DECL uses a shared union for sync/async declarations.
     * Initialize action-only fields explicitly so async funcs never
     * inherit stale subject-action metadata through overlapping storage. */
    node->data.func_decl.is_action = false;
    node->data.func_decl.required_abilities = NULL;
    node->data.func_decl.required_ability_count = 0;
    node->data.func_decl.required_ability_capacity = 0;
    node->data.func_decl.within_zone = NULL;
    node->data.func_decl.causes_effect = NULL;
    node->data.func_decl.authorized_by = NULL;
    node->data.func_decl.authorized_by_count = 0;
    node->data.func_decl.authorized_by_capacity = 0;

    node->data.async_func_decl.name = pergyra_strdup(name);
    node->data.async_func_decl.params = NULL;
    node->data.async_func_decl.param_count = 0;
    node->data.async_func_decl.param_capacity = 0;
    node->data.async_func_decl.return_type = NULL;
    node->data.async_func_decl.body = NULL;
    node->data.async_func_decl.generic_params = NULL;
    node->data.async_func_decl.where_clause = NULL;
    node->data.async_func_decl.has_effects_clause = false;
    node->data.async_func_decl.declared_effects = 0;
    node->data.async_func_decl.access = ACCESS_PUBLIC;
    node->data.async_func_decl.is_async = is_async;
    node->data.async_func_decl.doc_comment = NULL;
    return node;
}

ASTNode* ast_create_await_expression(ASTNode* expression) {
    ASTNode* node = ast_create_node(AST_AWAIT_EXPR);
    if (!node) return NULL;
    node->data.await_expr.expression = expression;
    return node;
}

ASTNode* ast_create_channel_send(ASTNode* channel, ASTNode* value) {
    ASTNode* node = ast_create_node(AST_CHANNEL_SEND);
    if (!node) return NULL;
    node->data.channel_send.channel = channel;
    node->data.channel_send.value = value;
    return node;
}

ASTNode* ast_create_channel_recv(ASTNode* channel) {
    ASTNode* node = ast_create_node(AST_CHANNEL_RECV);
    if (!node) return NULL;
    node->data.channel_recv.channel = channel;
    return node;
}

ASTNode* ast_create_select_statement(void) {
    ASTNode* node = ast_create_node(AST_SELECT_STMT);
    if (!node) return NULL;
    node->data.select_stmt.cases = NULL;
    node->data.select_stmt.case_count = 0;
    node->data.select_stmt.case_capacity = 0;
    node->data.select_stmt.default_case = NULL;
    return node;
}

ASTNode* ast_create_async_block(void) {
    ASTNode* node = ast_create_node(AST_ASYNC_BLOCK);
    if (!node) return NULL;
    node->data.async_block.statements = NULL;
    node->data.async_block.statement_count = 0;
    node->data.async_block.statement_capacity = 0;
    return node;
}

ASTNode* ast_create_spawn_expression(ASTNode* function) {
    ASTNode* node = ast_create_node(AST_SPAWN_EXPR);
    if (!node) return NULL;
    node->data.spawn_expr.function = function;
    node->data.spawn_expr.arguments = NULL;
    node->data.spawn_expr.arg_count = 0;
    return node;
}

ASTNode* ast_create_channel_type(ASTNode* element_type) {
    ASTNode* node = ast_create_node(AST_CHANNEL_TYPE);
    if (!node) return NULL;
    node->data.channel_type.element_type = element_type;
    node->data.channel_type.capacity = NULL;
    return node;
}

ASTNode* ast_create_future_type(ASTNode* value_type) {
    ASTNode* node = ast_create_node(AST_FUTURE_TYPE);
    if (!node) return NULL;
    node->data.future_type.value_type = value_type;
    return node;
}

ASTNode* ast_create_task_group(bool wait_all) {
    ASTNode* node = ast_create_node(AST_TASK_GROUP);
    if (!node) return NULL;
    node->data.task_group.tasks = NULL;
    node->data.task_group.task_count = 0;
    node->data.task_group.wait_all = wait_all;
    return node;
}
