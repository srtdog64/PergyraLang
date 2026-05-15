/*
 * Copyright (c) 2026 Pergyra Language Project
 * Split AST accessor owner. Keep responsibility slices below the 600 LOC signal.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

ASTNode**
ast_block_statements(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_BLOCK) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.block.count;
    return node->data.block.statements;
}

size_t
ast_block_statement_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BLOCK)
        return 0;
    return node->data.block.count;
}

ASTNode*
ast_block_statement(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_BLOCK)
        return NULL;
    if (index >= node->data.block.count)
        return NULL;
    return node->data.block.statements != NULL
        ? node->data.block.statements[index]
        : NULL;
}

ASTNode**
ast_block_detach_statements(ASTNode* node, size_t* count_out)
{
    ASTNode **statements;

    if (node == NULL || node->type != AST_BLOCK) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }

    statements = node->data.block.statements;
    if (count_out != NULL)
        *count_out = node->data.block.count;
    node->data.block.statements = NULL;
    node->data.block.count = 0;
    node->data.block.capacity = 0;
    return statements;
}

bool
ast_block_is_pin_block(const ASTNode* node)
{
    return node != NULL && node->type == AST_BLOCK
        && node->data.block.is_pin_block;
}

bool
ast_block_pin_view_is_write(const ASTNode* node)
{
    return node != NULL && node->type == AST_BLOCK
        && node->data.block.pin_view_is_write;
}

const char*
ast_block_pin_source_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BLOCK)
        return NULL;
    return node->data.block.pin_source_name;
}

const char*
ast_block_pin_view_name(const ASTNode* node)
{
    if (node == NULL || node->type != AST_BLOCK)
        return NULL;
    return node->data.block.pin_view_name;
}

ASTNode*
ast_match_subject(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MATCH_STMT)
        return NULL;
    return node->data.match_stmt.subject;
}

ASTNode**
ast_match_cases(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_MATCH_STMT) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.match_stmt.case_count;
    return node->data.match_stmt.cases;
}

size_t
ast_match_case_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MATCH_STMT)
        return 0;
    return node->data.match_stmt.case_count;
}

ASTNode*
ast_match_case_at(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_MATCH_STMT)
        return NULL;
    if (index >= node->data.match_stmt.case_count)
        return NULL;
    return node->data.match_stmt.cases != NULL
        ? node->data.match_stmt.cases[index]
        : NULL;
}

ASTNode*
ast_match_default_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MATCH_STMT)
        return NULL;
    return node->data.match_stmt.default_body;
}

ASTNode*
ast_match_case_pattern(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MATCH_CASE)
        return NULL;
    return node->data.match_case.pattern;
}

ASTNode**
ast_match_case_patterns(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_MATCH_CASE) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.match_case.pattern_count;
    return node->data.match_case.patterns;
}

size_t
ast_match_case_pattern_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MATCH_CASE)
        return 0;
    return node->data.match_case.pattern_count;
}

ASTNode*
ast_match_case_pattern_at(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_MATCH_CASE)
        return NULL;
    if (index >= node->data.match_case.pattern_count)
        return NULL;
    return node->data.match_case.patterns != NULL
        ? node->data.match_case.patterns[index]
        : NULL;
}

ASTNode*
ast_match_case_guard(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MATCH_CASE)
        return NULL;
    return node->data.match_case.guard;
}

ASTNode*
ast_match_case_body(const ASTNode* node)
{
    if (node == NULL || node->type != AST_MATCH_CASE)
        return NULL;
    return node->data.match_case.body;
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

ASTNode*
ast_event_op_event(const ASTNode* node)
{
    if (node == NULL
        || (node->type != AST_EVENT_SUBSCRIBE
            && node->type != AST_EVENT_UNSUBSCRIBE)) {
        return NULL;
    }
    return node->data.event_op.event;
}

ASTNode*
ast_event_op_handler(const ASTNode* node)
{
    if (node == NULL
        || (node->type != AST_EVENT_SUBSCRIBE
            && node->type != AST_EVENT_UNSUBSCRIBE)) {
        return NULL;
    }
    return node->data.event_op.handler;
}

ASTNode*
ast_event_invoke_event(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_INVOKE)
        return NULL;
    return node->data.event_invoke.event;
}

ASTNode**
ast_event_invoke_arguments(const ASTNode* node, size_t* count_out)
{
    if (node == NULL || node->type != AST_EVENT_INVOKE) {
        if (count_out != NULL)
            *count_out = 0;
        return NULL;
    }
    if (count_out != NULL)
        *count_out = node->data.event_invoke.arg_count;
    return node->data.event_invoke.arguments;
}

size_t
ast_event_invoke_arg_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_EVENT_INVOKE)
        return 0;
    return node->data.event_invoke.arg_count;
}

ASTNode*
ast_event_invoke_argument(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_EVENT_INVOKE)
        return NULL;
    if (index >= node->data.event_invoke.arg_count)
        return NULL;
    return node->data.event_invoke.arguments != NULL
        ? node->data.event_invoke.arguments[index]
        : NULL;
}

