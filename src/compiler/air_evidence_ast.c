/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR AST containment helper owner for evidence matching.
 */

#include "air_internal.h"
#include "../parser/ast_api.h"

bool
air_ast_contains_node(const ASTNode *container, const ASTNode *needle)
{
    if (container == NULL || needle == NULL)
        return false;
    if (container == needle)
        return true;
    switch (container->type) {
    case AST_INTENT_STEP:
        if (air_ast_contains_node(ast_intent_step_using_expr(container), needle)
            || air_ast_contains_node(ast_intent_step_intent_expr(container), needle)
            || air_ast_contains_node(ast_intent_step_pre_expr(container), needle)
            || air_ast_contains_node(ast_intent_step_guard_expr(container), needle)
            || air_ast_contains_node(ast_intent_step_post_expr(container), needle)
            || air_ast_contains_node(ast_intent_step_invariant_expr(container), needle)
            || air_ast_contains_node(ast_intent_step_expect_expr(container), needle)) {
            return true;
        }
        for (size_t i = 0; i < ast_intent_step_on_expr_count(container); i++) {
            if (air_ast_contains_node(ast_intent_step_on_exprs(container, NULL)[i], needle))
                return true;
        }
        for (size_t i = 0; i < ast_intent_step_compensate_expr_count(container); i++) {
            if (air_ast_contains_node(ast_intent_step_compensate_exprs(container, NULL)[i], needle))
                return true;
        }
        return false;
    case AST_CALL:
        if (air_ast_contains_node(ast_call_callee(container), needle))
            return true;
        for (size_t i = 0; i < ast_call_arg_count(container); i++) {
            if (air_ast_contains_node(ast_call_argument(container, i), needle))
                return true;
        }
        return false;
    case AST_MEMBER_ACCESS:
        return air_ast_contains_node(ast_member_object(container), needle);
    case AST_ARRAY_ACCESS:
        return air_ast_contains_node(ast_array_access_array(container), needle)
            || air_ast_contains_node(ast_array_access_index(container), needle);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(container); i++) {
            if (air_ast_contains_node(ast_array_literal_element(container, i), needle))
                return true;
        }
        return false;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(container); i++) {
            if (air_ast_contains_node(ast_tuple_literal_element(container, i), needle))
                return true;
        }
        return false;
    case AST_ASSIGNMENT:
        return air_ast_contains_node(ast_assignment_target(container), needle)
            || air_ast_contains_node(ast_assignment_value(container), needle);
    case AST_BINARY:
        return air_ast_contains_node(ast_binary_left(container), needle)
            || air_ast_contains_node(ast_binary_right(container), needle);
    case AST_UNARY:
        return air_ast_contains_node(ast_unary_operand(container), needle);
    case AST_CHANNEL_SEND:
        return air_ast_contains_node(ast_channel_send_channel(container), needle)
            || air_ast_contains_node(ast_channel_send_value(container), needle);
    case AST_CHANNEL_RECV:
        return air_ast_contains_node(ast_channel_recv_channel(container), needle);
    case AST_AWAIT_EXPR:
        return air_ast_contains_node(ast_await_expression(container), needle);
    case AST_SPAWN_EXPR:
        if (air_ast_contains_node(ast_spawn_function(container), needle))
            return true;
        for (size_t i = 0; i < ast_spawn_arg_count(container); i++) {
            if (air_ast_contains_node(ast_spawn_argument(container, i), needle))
                return true;
        }
        return false;
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(container); i++) {
            if (air_ast_contains_node(ast_block_statement(container, i), needle))
                return true;
        }
        return false;
    case AST_LET_DECL:
        return air_ast_contains_node(ast_let_initializer(container), needle);
    case AST_LET_DESTRUCTURE:
        return air_ast_contains_node(
            ast_let_destructure_initializer(container), needle);
    case AST_WITH_STMT:
        return air_ast_contains_node(ast_with_body(container), needle);
    case AST_FOR_LOOP:
        return air_ast_contains_node(ast_for_range_start(container), needle)
            || air_ast_contains_node(ast_for_range_end(container), needle)
            || air_ast_contains_node(ast_for_iterable(container), needle)
            || air_ast_contains_node(ast_for_body(container), needle);
    case AST_WHILE_LOOP:
        return air_ast_contains_node(ast_while_condition(container), needle)
            || air_ast_contains_node(ast_while_body(container), needle);
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(container); i++) {
            if (air_ast_contains_node(ast_parallel_task(container, i), needle))
                return true;
        }
        return false;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(container); i++) {
            if (air_ast_contains_node(ast_async_block_statement(container, i), needle))
                return true;
        }
        return false;
    case AST_TASK_GROUP:
        for (size_t i = 0; i < ast_task_group_task_count(container); i++) {
            if (air_ast_contains_node(ast_task_group_task(container, i), needle))
                return true;
        }
        return false;
    case AST_UNSAFE_BLOCK:
        return air_ast_contains_node(ast_unsafe_block_body(container), needle);
    case AST_TRANSACTION_BLOCK:
        return air_ast_contains_node(ast_transaction_block_body(container), needle);
    case AST_DEFER_STMT:
        return air_ast_contains_node(ast_defer_body(container), needle);
    case AST_RETURN:
        return air_ast_contains_node(ast_return_value(container), needle);
    case AST_IF_STMT:
        return air_ast_contains_node(ast_if_condition(container), needle)
            || air_ast_contains_node(ast_if_then_branch(container), needle)
            || air_ast_contains_node(ast_if_else_branch(container), needle);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(container); i++) {
            if (air_ast_contains_node(ast_select_case(container, i), needle))
                return true;
        }
        return air_ast_contains_node(ast_select_default_case(container), needle);
    case AST_MATCH_STMT:
        if (air_ast_contains_node(ast_match_subject(container), needle))
            return true;
        for (size_t i = 0; i < ast_match_case_count(container); i++) {
            if (air_ast_contains_node(ast_match_case_at(container, i), needle))
                return true;
        }
        return air_ast_contains_node(ast_match_default_body(container), needle);
    case AST_MATCH_CASE:
        if (air_ast_contains_node(ast_match_case_pattern(container), needle)
            || air_ast_contains_node(ast_match_case_guard(container), needle)
            || air_ast_contains_node(ast_match_case_body(container), needle)) {
            return true;
        }
        for (size_t i = 0; i < ast_match_case_pattern_count(container); i++) {
            if (air_ast_contains_node(ast_match_case_pattern_at(container, i), needle))
                return true;
        }
        return false;
    case AST_EVENT_INVOKE:
        if (air_ast_contains_node(ast_event_invoke_event(container), needle))
            return true;
        for (size_t i = 0; i < ast_event_invoke_arg_count(container); i++) {
            if (air_ast_contains_node(ast_event_invoke_argument(container, i), needle))
                return true;
        }
        return false;
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return air_ast_contains_node(ast_event_op_event(container), needle)
            || air_ast_contains_node(ast_event_op_handler(container), needle);
    case AST_PARTY_SHARED:
        return air_ast_contains_node(
            ast_party_shared_initializer(container), needle);
    case AST_PARTY_INSTANCE:
        for (size_t i = 0;
             i < ast_party_instance_assignment_count(container); i++) {
            if (air_ast_contains_node(
                    ast_party_instance_assignment_value(container, i), needle))
                return true;
        }
        return false;
    case AST_WORLD_SYSTEMIC:
        return air_ast_contains_node(ast_world_roster_initializer(container), needle);
    case AST_WORLD_ZONE:
        return air_ast_contains_node(ast_world_zone_initializer(container), needle);
    case AST_DOMAIN_SLOT:
        return air_ast_contains_node(ast_domain_slot_initializer(container), needle);
    case AST_LAMBDA_EXPR:
        return air_ast_contains_node(ast_lambda_body(container), needle);
    default:
        return false;
    }
}
