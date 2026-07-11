/*
 * Copyright (c) 2026 Pergyra Language Project
 * AST-owned executable identifier-reference analysis.
 */

#include "ast_analysis.h"

#include <string.h>

static bool ast_array_contains_identifier_ref(ASTNode *const *nodes,
                                              size_t count,
                                              ASTIdentifierPredicate predicate,
                                              void *userdata);
static bool ast_array_contains_free_identifier_ref(ASTNode *const *nodes,
                                                   size_t count,
                                                   const char *name);
static bool ast_array_patterns_bind_name(ASTNode *const *patterns,
                                         size_t count,
                                         const char *name);

static bool
ast_name_matches(const char *candidate, const char *name)
{
    return candidate != NULL && name != NULL && strcmp(candidate, name) == 0;
}

static bool
ast_lambda_param_binds_name(const ASTNode *param, const char *name)
{
    if (param == NULL || name == NULL)
        return false;
    if (param->type == AST_LET_DECL)
        return ast_name_matches(param->data.let_decl.name, name);
    if (param->type == AST_IDENTIFIER)
        return ast_name_matches(param->data.identifier.name, name);
    return false;
}

static bool
ast_lambda_params_bind_name(ASTNode *const *params, size_t count,
                            const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (ast_lambda_param_binds_name(params[i], name))
            return true;
    }
    return false;
}

static bool
ast_let_destructure_binds_name(const ASTNode *node, const char *name)
{
    if (node == NULL || node->type != AST_LET_DESTRUCTURE)
        return false;
    for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
        if (ast_name_matches(node->data.let_destructure.names[i], name))
            return true;
    }
    return false;
}

static bool
ast_pattern_binds_name(const ASTNode *pattern, const char *name)
{
    if (pattern == NULL || name == NULL)
        return false;

    switch (pattern->type) {
    case AST_IDENTIFIER:
        return false;
    case AST_CALL:
        return ast_array_patterns_bind_name(
            pattern->data.call.arguments, pattern->data.call.arg_count, name);
    case AST_ARRAY_LITERAL:
        return ast_array_patterns_bind_name(
            pattern->data.array_literal.elements, pattern->data.array_literal.count, name);
    case AST_TUPLE_LITERAL:
        return ast_array_patterns_bind_name(
            pattern->data.tuple_literal.elements, pattern->data.tuple_literal.count, name);
    default:
        return false;
    }
}

static bool
ast_match_case_binds_name(const ASTNode *node, const char *name)
{
    if (node == NULL || node->type != AST_MATCH_CASE)
        return false;
    if (ast_pattern_binds_name(node->data.match_case.pattern, name))
        return true;
    for (size_t i = 0; i < node->data.match_case.pattern_count; i++) {
        if (ast_pattern_binds_name(node->data.match_case.patterns[i], name))
            return true;
    }
    return false;
}

static bool
ast_array_patterns_bind_name(ASTNode *const *patterns,
                             size_t count,
                             const char *name)
{
    for (size_t i = 0; i < count; i++) {
        ASTNode *pattern = patterns[i];
        if (pattern != NULL
            && pattern->type == AST_IDENTIFIER
            && ast_name_matches(pattern->data.identifier.name, name)) {
            return true;
        }
        if (ast_pattern_binds_name(pattern, name))
            return true;
    }
    return false;
}

static bool
ast_statement_binds_name_after_initializer(const ASTNode *node,
                                           const char *name)
{
    if (node == NULL || name == NULL)
        return false;
    if (node->type == AST_LET_DECL)
        return ast_name_matches(node->data.let_decl.name, name);
    if (node->type == AST_LET_DESTRUCTURE)
        return ast_let_destructure_binds_name(node, name);
    return false;
}

static bool
ast_block_contains_free_identifier_ref(const ASTNode *node, const char *name)
{
    if (node == NULL || node->type != AST_BLOCK)
        return false;

    for (size_t i = 0; i < node->data.block.count; i++) {
        ASTNode *stmt = node->data.block.statements[i];
        if (ast_contains_free_identifier_ref(stmt, name))
            return true;
        if (ast_statement_binds_name_after_initializer(stmt, name))
            return false;
    }
    return false;
}

static bool
ast_array_contains_identifier_ref(ASTNode *const *nodes,
                                  size_t count,
                                  ASTIdentifierPredicate predicate,
                                  void *userdata)
{
    for (size_t i = 0; i < count; i++) {
        if (ast_contains_identifier_ref(nodes[i], predicate, userdata))
            return true;
    }
    return false;
}

static bool
ast_array_contains_free_identifier_ref(ASTNode *const *nodes,
                                       size_t count,
                                       const char *name)
{
    for (size_t i = 0; i < count; i++) {
        if (ast_contains_free_identifier_ref(nodes[i], name))
            return true;
    }
    return false;
}

bool
ast_contains_identifier_ref(const ASTNode *node,
                            ASTIdentifierPredicate predicate,
                            void *userdata)
{
    if (node == NULL || predicate == NULL)
        return false;

    switch (node->type) {
    case AST_IDENTIFIER:
        return predicate(node->data.identifier.name, userdata);
    case AST_BLOCK:
        return ast_array_contains_identifier_ref(
            node->data.block.statements, node->data.block.count, predicate, userdata);
    case AST_LET_DECL:
        return ast_contains_identifier_ref(node->data.let_decl.initializer, predicate, userdata);
    case AST_LET_DESTRUCTURE:
        return ast_contains_identifier_ref(node->data.let_destructure.initializer, predicate, userdata);
    case AST_WITH_STMT:
        return ast_contains_identifier_ref(node->data.with_stmt.body, predicate, userdata);
    case AST_PARALLEL_BLOCK:
        return ast_array_contains_identifier_ref(
            node->data.parallel.tasks, node->data.parallel.task_count, predicate, userdata);
    case AST_FOR_LOOP:
        return ast_contains_identifier_ref(node->data.for_loop.range_start, predicate, userdata)
            || ast_contains_identifier_ref(node->data.for_loop.range_end, predicate, userdata)
            || ast_contains_identifier_ref(node->data.for_loop.iterable, predicate, userdata)
            || ast_contains_identifier_ref(node->data.for_loop.body, predicate, userdata);
    case AST_WHILE_LOOP:
        return ast_contains_identifier_ref(node->data.while_loop.condition, predicate, userdata)
            || ast_contains_identifier_ref(node->data.while_loop.body, predicate, userdata);
    case AST_IF_STMT:
        return ast_contains_identifier_ref(node->data.if_stmt.condition, predicate, userdata)
            || ast_contains_identifier_ref(node->data.if_stmt.then_branch, predicate, userdata)
            || ast_contains_identifier_ref(node->data.if_stmt.else_branch, predicate, userdata);
    case AST_RETURN:
        return ast_contains_identifier_ref(node->data.return_stmt.value, predicate, userdata);
    case AST_GIVE_STMT:
        return ast_contains_identifier_ref(node->data.give_stmt.value, predicate, userdata);
    case AST_SELECT_STMT:
        return ast_array_contains_identifier_ref(
                node->data.select_stmt.cases, node->data.select_stmt.case_count, predicate, userdata)
            || ast_contains_identifier_ref(node->data.select_stmt.default_case, predicate, userdata);
    case AST_MATCH_STMT:
        return ast_contains_identifier_ref(node->data.match_stmt.subject, predicate, userdata)
            || ast_array_contains_identifier_ref(
                node->data.match_stmt.cases, node->data.match_stmt.case_count, predicate, userdata)
            || ast_contains_identifier_ref(node->data.match_stmt.default_body, predicate, userdata);
    case AST_MATCH_CASE:
        return ast_array_contains_identifier_ref(
                node->data.match_case.patterns, node->data.match_case.pattern_count,
                predicate, userdata)
            || ast_contains_identifier_ref(node->data.match_case.guard, predicate, userdata)
            || ast_contains_identifier_ref(node->data.match_case.body, predicate, userdata);
    case AST_BINARY:
        return ast_contains_identifier_ref(node->data.binary.left, predicate, userdata)
            || ast_contains_identifier_ref(node->data.binary.right, predicate, userdata);
    case AST_UNARY:
        return ast_contains_identifier_ref(node->data.unary.operand, predicate, userdata);
    case AST_CALL:
        return ast_contains_identifier_ref(node->data.call.callee, predicate, userdata)
            || ast_array_contains_identifier_ref(
                node->data.call.arguments, node->data.call.arg_count, predicate, userdata);
    case AST_MEMBER_ACCESS:
        return ast_contains_identifier_ref(node->data.member.object, predicate, userdata);
    case AST_ARRAY_ACCESS:
        return ast_contains_identifier_ref(node->data.array_access.array, predicate, userdata)
            || ast_contains_identifier_ref(node->data.array_access.index, predicate, userdata);
    case AST_ARRAY_LITERAL:
        return ast_array_contains_identifier_ref(
            node->data.array_literal.elements, node->data.array_literal.count, predicate, userdata);
    case AST_TUPLE_LITERAL:
        return ast_array_contains_identifier_ref(
            node->data.tuple_literal.elements, node->data.tuple_literal.count, predicate, userdata);
    case AST_MAP_LITERAL:
        return ast_array_contains_identifier_ref(
            node->data.map_literal.keys, node->data.map_literal.count, predicate, userdata)
            || ast_array_contains_identifier_ref(
            node->data.map_literal.values, node->data.map_literal.count, predicate, userdata);
    case AST_CAST:
        return ast_contains_identifier_ref(node->data.cast.operand, predicate, userdata);
    case AST_TYPE_TEST:
        return ast_contains_identifier_ref(node->data.type_test.operand, predicate, userdata);
    case AST_ASSIGNMENT:
        return ast_contains_identifier_ref(node->data.assignment.target, predicate, userdata)
            || ast_contains_identifier_ref(node->data.assignment.value, predicate, userdata);
    case AST_AWAIT_EXPR:
        return ast_contains_identifier_ref(node->data.await_expr.expression, predicate, userdata);
    case AST_CHANNEL_SEND:
        return ast_contains_identifier_ref(node->data.channel_send.channel, predicate, userdata)
            || ast_contains_identifier_ref(node->data.channel_send.value, predicate, userdata);
    case AST_CHANNEL_RECV:
        return ast_contains_identifier_ref(node->data.channel_recv.channel, predicate, userdata);
    case AST_ASYNC_BLOCK:
        return ast_array_contains_identifier_ref(
            node->data.async_block.statements, node->data.async_block.statement_count,
            predicate, userdata);
    case AST_SPAWN_EXPR:
        return ast_contains_identifier_ref(node->data.spawn_expr.function, predicate, userdata)
            || ast_array_contains_identifier_ref(
                node->data.spawn_expr.arguments, node->data.spawn_expr.arg_count, predicate, userdata);
    case AST_TASK_GROUP:
        return ast_array_contains_identifier_ref(
            node->data.task_group.tasks, node->data.task_group.task_count, predicate, userdata);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return ast_contains_identifier_ref(node->data.event_op.event, predicate, userdata)
            || ast_contains_identifier_ref(node->data.event_op.handler, predicate, userdata);
    case AST_EVENT_INVOKE:
        return ast_contains_identifier_ref(node->data.event_invoke.event, predicate, userdata)
            || ast_array_contains_identifier_ref(
                node->data.event_invoke.arguments, node->data.event_invoke.arg_count,
                predicate, userdata);
    case AST_LAMBDA_EXPR:
        return ast_contains_identifier_ref(node->data.lambda_expr.body, predicate, userdata);
    case AST_UNSAFE_BLOCK:
        return ast_contains_identifier_ref(node->data.unsafe_block.body, predicate, userdata);
    case AST_TRANSACTION_BLOCK:
        return ast_contains_identifier_ref(node->data.transaction_block.body, predicate, userdata);
    case AST_FAIL_STMT:
        return ast_contains_identifier_ref(node->data.fail_stmt.reason, predicate, userdata);
    case AST_DEFER_STMT:
        return ast_contains_identifier_ref(node->data.defer_stmt.body, predicate, userdata);
    default:
        return false;
    }
}

bool
ast_contains_free_identifier_ref(const ASTNode *node, const char *name)
{
    if (node == NULL || name == NULL || name[0] == '\0')
        return false;

    switch (node->type) {
    case AST_IDENTIFIER:
        return ast_name_matches(node->data.identifier.name, name);
    case AST_BLOCK:
        return ast_block_contains_free_identifier_ref(node, name);
    case AST_LET_DECL:
        return ast_contains_free_identifier_ref(node->data.let_decl.initializer, name);
    case AST_LET_DESTRUCTURE:
        return ast_contains_free_identifier_ref(node->data.let_destructure.initializer, name);
    case AST_WITH_STMT:
        return ast_contains_free_identifier_ref(node->data.with_stmt.body, name);
    case AST_PARALLEL_BLOCK:
        return ast_array_contains_free_identifier_ref(
            node->data.parallel.tasks, node->data.parallel.task_count, name);
    case AST_FOR_LOOP:
        return ast_contains_free_identifier_ref(node->data.for_loop.range_start, name)
            || ast_contains_free_identifier_ref(node->data.for_loop.range_end, name)
            || ast_contains_free_identifier_ref(node->data.for_loop.iterable, name)
            || (!ast_name_matches(node->data.for_loop.variable, name)
                && ast_contains_free_identifier_ref(node->data.for_loop.body, name));
    case AST_WHILE_LOOP:
        return ast_contains_free_identifier_ref(node->data.while_loop.condition, name)
            || ast_contains_free_identifier_ref(node->data.while_loop.body, name);
    case AST_IF_STMT:
        return ast_contains_free_identifier_ref(node->data.if_stmt.condition, name)
            || ast_contains_free_identifier_ref(node->data.if_stmt.then_branch, name)
            || ast_contains_free_identifier_ref(node->data.if_stmt.else_branch, name);
    case AST_RETURN:
        return ast_contains_free_identifier_ref(node->data.return_stmt.value, name);
    case AST_GIVE_STMT:
        return ast_contains_free_identifier_ref(node->data.give_stmt.value, name);
    case AST_SELECT_STMT:
        return ast_array_contains_free_identifier_ref(
                node->data.select_stmt.cases, node->data.select_stmt.case_count, name)
            || ast_contains_free_identifier_ref(node->data.select_stmt.default_case, name);
    case AST_MATCH_STMT:
        return ast_contains_free_identifier_ref(node->data.match_stmt.subject, name)
            || ast_array_contains_free_identifier_ref(
                node->data.match_stmt.cases, node->data.match_stmt.case_count, name)
            || ast_contains_free_identifier_ref(node->data.match_stmt.default_body, name);
    case AST_MATCH_CASE:
        if (ast_match_case_binds_name(node, name))
            return false;
        return ast_contains_free_identifier_ref(node->data.match_case.guard, name)
            || ast_contains_free_identifier_ref(node->data.match_case.body, name);
    case AST_BINARY:
        return ast_contains_free_identifier_ref(node->data.binary.left, name)
            || ast_contains_free_identifier_ref(node->data.binary.right, name);
    case AST_UNARY:
        return ast_contains_free_identifier_ref(node->data.unary.operand, name);
    case AST_CALL:
        return ast_contains_free_identifier_ref(node->data.call.callee, name)
            || ast_array_contains_free_identifier_ref(
                node->data.call.arguments, node->data.call.arg_count, name);
    case AST_MEMBER_ACCESS:
        return ast_contains_free_identifier_ref(node->data.member.object, name);
    case AST_ARRAY_ACCESS:
        return ast_contains_free_identifier_ref(node->data.array_access.array, name)
            || ast_contains_free_identifier_ref(node->data.array_access.index, name);
    case AST_ARRAY_LITERAL:
        return ast_array_contains_free_identifier_ref(
            node->data.array_literal.elements, node->data.array_literal.count, name);
    case AST_TUPLE_LITERAL:
        return ast_array_contains_free_identifier_ref(
            node->data.tuple_literal.elements, node->data.tuple_literal.count, name);
    case AST_MAP_LITERAL:
        return ast_array_contains_free_identifier_ref(
            node->data.map_literal.keys, node->data.map_literal.count, name)
            || ast_array_contains_free_identifier_ref(
            node->data.map_literal.values, node->data.map_literal.count, name);
    case AST_CAST:
        return ast_contains_free_identifier_ref(node->data.cast.operand, name);
    case AST_TYPE_TEST:
        return ast_contains_free_identifier_ref(node->data.type_test.operand, name);
    case AST_ASSIGNMENT:
        return ast_contains_free_identifier_ref(node->data.assignment.target, name)
            || ast_contains_free_identifier_ref(node->data.assignment.value, name);
    case AST_AWAIT_EXPR:
        return ast_contains_free_identifier_ref(node->data.await_expr.expression, name);
    case AST_CHANNEL_SEND:
        return ast_contains_free_identifier_ref(node->data.channel_send.channel, name)
            || ast_contains_free_identifier_ref(node->data.channel_send.value, name);
    case AST_CHANNEL_RECV:
        return ast_contains_free_identifier_ref(node->data.channel_recv.channel, name);
    case AST_ASYNC_BLOCK:
        return ast_array_contains_free_identifier_ref(
            node->data.async_block.statements, node->data.async_block.statement_count, name);
    case AST_SPAWN_EXPR:
        return ast_contains_free_identifier_ref(node->data.spawn_expr.function, name)
            || ast_array_contains_free_identifier_ref(
                node->data.spawn_expr.arguments, node->data.spawn_expr.arg_count, name);
    case AST_TASK_GROUP:
        return ast_array_contains_free_identifier_ref(
            node->data.task_group.tasks, node->data.task_group.task_count, name);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return ast_contains_free_identifier_ref(node->data.event_op.event, name)
            || ast_contains_free_identifier_ref(node->data.event_op.handler, name);
    case AST_EVENT_INVOKE:
        return ast_contains_free_identifier_ref(node->data.event_invoke.event, name)
            || ast_array_contains_free_identifier_ref(
                node->data.event_invoke.arguments, node->data.event_invoke.arg_count, name);
    case AST_LAMBDA_EXPR:
        if (ast_lambda_params_bind_name(node->data.lambda_expr.params,
                node->data.lambda_expr.param_count, name)) {
            return false;
        }
        return ast_contains_free_identifier_ref(node->data.lambda_expr.body, name);
    case AST_UNSAFE_BLOCK:
        return ast_contains_free_identifier_ref(node->data.unsafe_block.body, name);
    case AST_TRANSACTION_BLOCK:
        return ast_contains_free_identifier_ref(node->data.transaction_block.body, name);
    case AST_FAIL_STMT:
        return ast_contains_free_identifier_ref(node->data.fail_stmt.reason, name);
    case AST_DEFER_STMT:
        return ast_contains_free_identifier_ref(node->data.defer_stmt.body, name);
    default:
        return false;
    }
}

/*
 * Does this statement (transitively) assign to `name`? An assignment counts
 * when the target's root identifier -- through member and index projections
 * (`x.f = v`, `x[i] = v`) -- is `name`. Shared owner for the parallel
 * boundary's writer analysis: the semantic race/snapshot admission and both
 * backend capture emitters must agree on who writes, so they all consume
 * this walk (docs/177 F2, docs/178).
 */
bool
ast_statement_assigns_identifier(const ASTNode *node, const char *name)
{
    if (node == NULL || name == NULL)
        return false;
    switch (node->type) {
    case AST_ASSIGNMENT: {
        const ASTNode *root = node->data.assignment.target;
        while (root != NULL) {
            if (root->type == AST_MEMBER_ACCESS)
                root = root->data.member.object;
            else if (root->type == AST_ARRAY_ACCESS)
                root = root->data.array_access.array;
            else
                break;
        }
        if (root != NULL && root->type == AST_IDENTIFIER
            && ast_name_matches(root->data.identifier.name, name))
            return true;
        return ast_statement_assigns_identifier(
            node->data.assignment.value, name);
    }
    case AST_BLOCK: {
        for (size_t i = 0; i < node->data.block.count; i++) {
            if (ast_statement_assigns_identifier(
                    node->data.block.statements[i], name))
                return true;
        }
        return false;
    }
    case AST_IF_STMT:
        return ast_statement_assigns_identifier(
                   node->data.if_stmt.then_branch, name)
            || ast_statement_assigns_identifier(
                   node->data.if_stmt.else_branch, name);
    case AST_WHILE_LOOP:
        return ast_statement_assigns_identifier(
                   node->data.while_loop.body, name);
    case AST_FOR_LOOP:
        return ast_statement_assigns_identifier(
                   node->data.for_loop.body, name);
    case AST_GIVE_STMT:
        /* The give value is an expression position; any assignment shape
         * inside it must stay visible to the replicated-arm analysis. */
        return ast_statement_assigns_identifier(
                   node->data.give_stmt.value, name);
    default:
        return false;
    }
}
