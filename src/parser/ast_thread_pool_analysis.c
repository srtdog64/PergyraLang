/*
 * Copyright (c) 2026 Pergyra Language Project
 * Thread-pool surface analysis helpers.
 */

#include "ast_analysis.h"

static bool
ast_array_uses_thread_pool_surface(ASTNode *const *nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (ast_uses_thread_pool_surface(nodes[i]))
            return true;
    }
    return false;
}

bool
ast_uses_thread_pool_surface(const ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_TASK_GROUP:
        return true;
    case AST_BLOCK:
        return ast_array_uses_thread_pool_surface(
            node->data.block.statements, node->data.block.count);
    case AST_LET_DECL:
        return ast_uses_thread_pool_surface(node->data.let_decl.type)
            || ast_uses_thread_pool_surface(node->data.let_decl.initializer);
    case AST_RETURN:
        return ast_uses_thread_pool_surface(node->data.return_stmt.value);
    case AST_CALL:
        return ast_uses_thread_pool_surface(node->data.call.callee)
            || ast_array_uses_thread_pool_surface(
                node->data.call.arguments, node->data.call.arg_count);
    case AST_BINARY:
        return ast_uses_thread_pool_surface(node->data.binary.left)
            || ast_uses_thread_pool_surface(node->data.binary.right);
    case AST_UNARY:
        return ast_uses_thread_pool_surface(node->data.unary.operand);
    case AST_ASSIGNMENT:
        return ast_uses_thread_pool_surface(node->data.assignment.target)
            || ast_uses_thread_pool_surface(node->data.assignment.value);
    case AST_MEMBER_ACCESS:
        return ast_uses_thread_pool_surface(node->data.member.object);
    case AST_ARRAY_ACCESS:
        return ast_uses_thread_pool_surface(node->data.array_access.array)
            || ast_uses_thread_pool_surface(node->data.array_access.index);
    case AST_ARRAY_LITERAL:
        return ast_array_uses_thread_pool_surface(
            node->data.array_literal.elements, node->data.array_literal.count);
    case AST_TUPLE_LITERAL:
        return ast_array_uses_thread_pool_surface(
            node->data.tuple_literal.elements, node->data.tuple_literal.count);
    case AST_MAP_LITERAL:
        return ast_array_uses_thread_pool_surface(
                node->data.map_literal.keys, node->data.map_literal.count)
            || ast_array_uses_thread_pool_surface(
                node->data.map_literal.values, node->data.map_literal.count);
    case AST_CAST:
        return ast_uses_thread_pool_surface(node->data.cast.operand);
    case AST_IF_STMT:
        return ast_uses_thread_pool_surface(node->data.if_stmt.condition)
            || ast_uses_thread_pool_surface(node->data.if_stmt.then_branch)
            || ast_uses_thread_pool_surface(node->data.if_stmt.else_branch);
    case AST_FOR_LOOP:
        return ast_uses_thread_pool_surface(node->data.for_loop.range_start)
            || ast_uses_thread_pool_surface(node->data.for_loop.range_end)
            || ast_uses_thread_pool_surface(node->data.for_loop.iterable)
            || ast_uses_thread_pool_surface(node->data.for_loop.body);
    case AST_WHILE_LOOP:
        return ast_uses_thread_pool_surface(node->data.while_loop.condition)
            || ast_uses_thread_pool_surface(node->data.while_loop.body);
    case AST_MATCH_STMT:
        return ast_uses_thread_pool_surface(node->data.match_stmt.subject)
            || ast_uses_thread_pool_surface(node->data.match_stmt.default_body)
            || ast_array_uses_thread_pool_surface(
                node->data.match_stmt.cases, node->data.match_stmt.case_count);
    case AST_MATCH_CASE:
        return ast_uses_thread_pool_surface(node->data.match_case.pattern)
            || ast_uses_thread_pool_surface(node->data.match_case.guard)
            || ast_uses_thread_pool_surface(node->data.match_case.body);
    case AST_SELECT_STMT:
        return ast_array_uses_thread_pool_surface(
                node->data.select_stmt.cases, node->data.select_stmt.case_count)
            || ast_uses_thread_pool_surface(node->data.select_stmt.default_case);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return ast_uses_thread_pool_surface(node->data.event_op.event)
            || ast_uses_thread_pool_surface(node->data.event_op.handler);
    case AST_EVENT_INVOKE:
        return ast_uses_thread_pool_surface(node->data.event_invoke.event)
            || ast_array_uses_thread_pool_surface(
                node->data.event_invoke.arguments, node->data.event_invoke.arg_count);
    default:
        return false;
    }
}
