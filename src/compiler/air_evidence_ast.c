/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR AST containment helper owner for evidence matching.
 */

#include "air_internal.h"

bool
air_ast_contains_node(const ASTNode *container, const ASTNode *needle)
{
    if (container == NULL || needle == NULL)
        return false;
    if (container == needle)
        return true;
    switch (container->type) {
    case AST_INTENT_STEP:
        if (air_ast_contains_node(container->data.intent_step.using_expr, needle)
            || air_ast_contains_node(container->data.intent_step.intent_expr, needle)
            || air_ast_contains_node(container->data.intent_step.pre_expr, needle)
            || air_ast_contains_node(container->data.intent_step.guard_expr, needle)
            || air_ast_contains_node(container->data.intent_step.post_expr, needle)
            || air_ast_contains_node(container->data.intent_step.invariant_expr, needle)
            || air_ast_contains_node(container->data.intent_step.expect_expr, needle)) {
            return true;
        }
        for (size_t i = 0; i < container->data.intent_step.on_expr_count; i++) {
            if (air_ast_contains_node(container->data.intent_step.on_exprs[i], needle))
                return true;
        }
        for (size_t i = 0; i < container->data.intent_step.compensate_expr_count; i++) {
            if (air_ast_contains_node(container->data.intent_step.compensate_exprs[i], needle))
                return true;
        }
        return false;
    case AST_CALL:
        if (air_ast_contains_node(container->data.call.callee, needle))
            return true;
        for (size_t i = 0; i < container->data.call.arg_count; i++) {
            if (air_ast_contains_node(container->data.call.arguments[i], needle))
                return true;
        }
        return false;
    case AST_MEMBER_ACCESS:
        return air_ast_contains_node(container->data.member.object, needle);
    case AST_ARRAY_ACCESS:
        return air_ast_contains_node(container->data.array_access.array, needle)
            || air_ast_contains_node(container->data.array_access.index, needle);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < container->data.array_literal.count; i++) {
            if (air_ast_contains_node(container->data.array_literal.elements[i], needle))
                return true;
        }
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < container->data.tuple_literal.count; i++) {
            if (air_ast_contains_node(container->data.tuple_literal.elements[i], needle))
                return true;
        }
        return false;
    case AST_ASSIGNMENT:
        return air_ast_contains_node(container->data.assignment.target, needle)
            || air_ast_contains_node(container->data.assignment.value, needle);
    case AST_BINARY:
        return air_ast_contains_node(container->data.binary.left, needle)
            || air_ast_contains_node(container->data.binary.right, needle);
    case AST_UNARY:
        return air_ast_contains_node(container->data.unary.operand, needle);
    case AST_CHANNEL_SEND:
        return air_ast_contains_node(container->data.channel_send.channel, needle)
            || air_ast_contains_node(container->data.channel_send.value, needle);
    case AST_CHANNEL_RECV:
        return air_ast_contains_node(container->data.channel_recv.channel, needle);
    case AST_AWAIT_EXPR:
        return air_ast_contains_node(container->data.await_expr.expression, needle);
    case AST_SPAWN_EXPR:
        if (air_ast_contains_node(container->data.spawn_expr.function, needle))
            return true;
        for (size_t i = 0; i < container->data.spawn_expr.arg_count; i++) {
            if (air_ast_contains_node(container->data.spawn_expr.arguments[i], needle))
                return true;
        }
        return false;
    case AST_BLOCK:
        for (size_t i = 0; i < container->data.block.count; i++) {
            if (air_ast_contains_node(container->data.block.statements[i], needle))
                return true;
        }
        return false;
    case AST_LET_DECL:
        return air_ast_contains_node(container->data.let_decl.initializer, needle);
    case AST_LET_DESTRUCTURE:
        return air_ast_contains_node(container->data.let_destructure.initializer, needle);
    case AST_WITH_STMT:
        return air_ast_contains_node(container->data.with_stmt.body, needle);
    case AST_FOR_LOOP:
        return air_ast_contains_node(container->data.for_loop.range_start, needle)
            || air_ast_contains_node(container->data.for_loop.range_end, needle)
            || air_ast_contains_node(container->data.for_loop.iterable, needle)
            || air_ast_contains_node(container->data.for_loop.body, needle);
    case AST_WHILE_LOOP:
        return air_ast_contains_node(container->data.while_loop.condition, needle)
            || air_ast_contains_node(container->data.while_loop.body, needle);
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < container->data.parallel.task_count; i++) {
            if (air_ast_contains_node(container->data.parallel.tasks[i], needle))
                return true;
        }
        return false;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < container->data.async_block.statement_count; i++) {
            if (air_ast_contains_node(container->data.async_block.statements[i], needle))
                return true;
        }
        return false;
    case AST_TASK_GROUP:
        for (size_t i = 0; i < container->data.task_group.task_count; i++) {
            if (air_ast_contains_node(container->data.task_group.tasks[i], needle))
                return true;
        }
        return false;
    case AST_UNSAFE_BLOCK:
        return air_ast_contains_node(container->data.unsafe_block.body, needle);
    case AST_DEFER_STMT:
        return air_ast_contains_node(container->data.defer_stmt.body, needle);
    case AST_RETURN:
        return air_ast_contains_node(container->data.return_stmt.value, needle);
    case AST_IF_STMT:
        return air_ast_contains_node(container->data.if_stmt.condition, needle)
            || air_ast_contains_node(container->data.if_stmt.then_branch, needle)
            || air_ast_contains_node(container->data.if_stmt.else_branch, needle);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < container->data.select_stmt.case_count; i++) {
            if (air_ast_contains_node(container->data.select_stmt.cases[i], needle))
                return true;
        }
        return air_ast_contains_node(container->data.select_stmt.default_case, needle);
    case AST_MATCH_STMT:
        if (air_ast_contains_node(container->data.match_stmt.subject, needle))
            return true;
        for (size_t i = 0; i < container->data.match_stmt.case_count; i++) {
            if (air_ast_contains_node(container->data.match_stmt.cases[i], needle))
                return true;
        }
        return air_ast_contains_node(container->data.match_stmt.default_body, needle);
    case AST_MATCH_CASE:
        if (air_ast_contains_node(container->data.match_case.pattern, needle)
            || air_ast_contains_node(container->data.match_case.guard, needle)
            || air_ast_contains_node(container->data.match_case.body, needle)) {
            return true;
        }
        for (size_t i = 0; i < container->data.match_case.pattern_count; i++) {
            if (air_ast_contains_node(container->data.match_case.patterns[i], needle))
                return true;
        }
        return false;
    case AST_EVENT_INVOKE:
        if (air_ast_contains_node(container->data.event_invoke.event, needle))
            return true;
        for (size_t i = 0; i < container->data.event_invoke.arg_count; i++) {
            if (air_ast_contains_node(container->data.event_invoke.arguments[i], needle))
                return true;
        }
        return false;
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return air_ast_contains_node(container->data.event_op.event, needle)
            || air_ast_contains_node(container->data.event_op.handler, needle);
    case AST_PARTY_SHARED:
        return air_ast_contains_node(container->data.party_shared.initializer, needle);
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < container->data.party_instance.assignment_count; i++) {
            if (air_ast_contains_node(container->data.party_instance.assignments[i].value, needle))
                return true;
        }
        return false;
    case AST_WORLD_SYSTEMIC:
        return air_ast_contains_node(container->data.world_roster.initializer, needle);
    case AST_WORLD_ZONE:
        return air_ast_contains_node(container->data.world_zone.initializer, needle);
    case AST_DOMAIN_SLOT:
        return air_ast_contains_node(container->data.domain_slot.initializer, needle);
    case AST_LAMBDA_EXPR:
        return air_ast_contains_node(container->data.lambda_expr.body, needle);
    default:
        return false;
    }
}
