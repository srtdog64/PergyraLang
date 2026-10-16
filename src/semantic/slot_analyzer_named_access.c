/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer access collection and call-alias propagation.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "slot_analyzer_internal.h"
#include "typed_map_query_fact_owner.h"
#include "function_param_publication_owner.h"
#include "../common/match_variant_policy.h"

unsigned
slot_access_mask_for_named_symbol(ASTNode *node, const char *symbol_name,
                                  const SlotFunctionLookup *program_root)
{
    unsigned mask = 0;

    if (node == NULL || symbol_name == NULL)
        return 0;
    bool reachable;
    if (program_root != NULL && semantic_param_publication_execution(program_root->ctx, node, &reachable)
        && !reachable) return 0;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_block_statement(node, i), symbol_name, program_root);
        break;

    case AST_LET_DECL:
        mask |= slot_access_mask_for_named_symbol(
            ast_let_initializer(node), symbol_name, program_root);
        break;

    case AST_IF_STMT:
        mask |= slot_access_mask_for_named_symbol(
            ast_if_condition(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_if_then_branch(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_if_else_branch(node), symbol_name, program_root);
        break;

    case AST_WITH_STMT:
        mask |= slot_access_mask_for_named_symbol(
            ast_with_slot_type(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_with_body(node), symbol_name, program_root);
        break;

    case AST_FOR_LOOP:
        mask |= slot_access_mask_for_named_symbol(ast_for_iterable(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_for_range_start(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_for_range_end(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_for_body(node), symbol_name, program_root);
        break;

    case AST_WHILE_LOOP:
        mask |= slot_access_mask_for_named_symbol(
            ast_while_condition(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_while_body(node), symbol_name, program_root);
        break;

    case AST_LAMBDA_EXPR: {
        SemanticPublishedDeferred deferred;
        const SemanticContext *ctx = program_root == NULL ? NULL : program_root->ctx;
        if (!semantic_param_publication_query_deferred(ctx, node, &deferred))
            return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
        for (size_t i = 0; i < deferred.capture_count; i++) {
            const ASTNode *root = NULL;
            ASTNode *source = (ASTNode *)deferred.captures[i].source;
            if (!slot_call_argument_storage_root(source, &root))
                return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
            if (root != NULL && strcmp(ast_identifier_name(root), symbol_name) == 0)
                mask |= SLOT_ACCESS_READ;
            mask |= slot_access_mask_for_named_symbol(source, symbol_name, program_root);
        }
        break;
    }
    case AST_ASYNC_BLOCK: case AST_SPAWN_EXPR:
    case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE: case AST_EVENT_INVOKE: {
        SemanticPublishedDeferred deferred;
        const SemanticContext *ctx = program_root == NULL ? NULL : program_root->ctx;
        if (!semantic_param_publication_query_deferred(ctx, node, &deferred))
            return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
        if (node->type == AST_SPAWN_EXPR) {
            mask |= slot_access_mask_for_named_symbol(ast_spawn_function(node), symbol_name, program_root);
            for (size_t i = 0; i < ast_spawn_arg_count(node); i++)
                mask |= slot_access_mask_for_named_symbol(ast_spawn_argument(node, i), symbol_name, program_root);
        } else if (node->type == AST_EVENT_INVOKE) {
            mask |= slot_access_mask_for_named_symbol(ast_event_invoke_event(node), symbol_name, program_root);
            for (size_t i = 0; i < ast_event_invoke_arg_count(node); i++)
                mask |= slot_access_mask_for_named_symbol(ast_event_invoke_argument(node, i), symbol_name, program_root);
        } else if (node->type != AST_ASYNC_BLOCK) {
            mask |= slot_access_mask_for_named_symbol(ast_event_op_event(node), symbol_name, program_root);
            mask |= slot_access_mask_for_named_symbol(ast_event_op_handler(node), symbol_name, program_root);
        }
        break;
    }

    case AST_PARALLEL_BLOCK:
        mask |= slot_access_mask_for_named_symbol(ast_parallel_join_collection(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(ast_parallel_join_range_end(node), symbol_name, program_root);
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_parallel_task(node, i), symbol_name, program_root);
        break;

    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_select_case(node, i), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_select_default_case(node), symbol_name, program_root);
        break;

    case AST_MATCH_STMT:
        mask |= slot_access_mask_for_named_symbol(
            ast_match_subject(node), symbol_name, program_root);
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_match_case_at(node, i), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_match_default_body(node), symbol_name, program_root);
        break;

    case AST_MATCH_CASE:
        if (ast_match_case_semantic_subject_family(node) == PGY_MATCH_SUBJECT_UNKNOWN) {
            mask |= slot_access_mask_for_named_symbol(ast_match_case_pattern(node), symbol_name, program_root);
            for (size_t i = 0; i < ast_match_case_pattern_count(node); i++)
                mask |= slot_access_mask_for_named_symbol(ast_match_case_pattern_at(node, i), symbol_name, program_root);
        } else if (ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_OPTION &&
                   ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_RESULT &&
                   ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_ENUM) {
            return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
        }
        mask |= slot_access_mask_for_named_symbol(
            ast_match_case_guard(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_match_case_body(node), symbol_name, program_root);
        break;

    case AST_ASSIGNMENT:
        mask |= slot_access_mask_for_named_symbol(
            ast_assignment_target(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_assignment_value(node), symbol_name, program_root);
        break;

    case AST_CAST:
        mask |= slot_access_mask_for_named_symbol(ast_cast_operand(node), symbol_name, program_root);
        break;
    case AST_TYPE_TEST:
        mask |= slot_access_mask_for_named_symbol(ast_type_test_operand(node), symbol_name, program_root);
        break;
    case AST_LET_DESTRUCTURE:
        mask |= slot_access_mask_for_named_symbol(ast_let_destructure_initializer(node), symbol_name, program_root);
        break;
    case AST_GIVE_STMT:
        mask |= slot_access_mask_for_named_symbol(ast_give_value(node), symbol_name, program_root);
        break;
    case AST_AWAIT_EXPR:
        mask |= slot_access_mask_for_named_symbol(ast_await_expression(node), symbol_name, program_root);
        break;
    case AST_UNSAFE_BLOCK:
        mask |= slot_access_mask_for_named_symbol(ast_unsafe_block_body(node), symbol_name, program_root);
        break;
    case AST_TRANSACTION_BLOCK:
        mask |= slot_access_mask_for_named_symbol(ast_transaction_block_body(node), symbol_name, program_root);
        if (node->data.transaction_block.compensation_count != 0 &&
            node->data.transaction_block.compensations == NULL)
            return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
        for (size_t i = 0; i < node->data.transaction_block.compensation_count; i++)
            mask |= slot_access_mask_for_named_symbol(
                node->data.transaction_block.compensations[i], symbol_name, program_root);
        break;
    case AST_FAIL_STMT:
        mask |= slot_access_mask_for_named_symbol(ast_fail_stmt_reason(node), symbol_name, program_root);
        break;
    case AST_DEFER_STMT:
        mask |= slot_access_mask_for_named_symbol(ast_defer_body(node), symbol_name, program_root);
        break;
    case AST_BIND_STMT:
        mask |= slot_access_mask_for_named_symbol(ast_bind_statement_subject(node), symbol_name, program_root);
        break;
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++) {
        mask |= slot_access_mask_for_named_symbol(ast_array_literal_element(node, i), symbol_name, program_root);
        }
        break;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
        mask |= slot_access_mask_for_named_symbol(ast_tuple_literal_element(node, i), symbol_name, program_root);
        }
        break;
    case AST_SET_LITERAL:
        for (size_t i = 0; i < ast_set_literal_count(node); i++) {
        mask |= slot_access_mask_for_named_symbol(ast_set_literal_element(node, i), symbol_name, program_root);
        }
        break;
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
        mask |= slot_access_mask_for_named_symbol(node->data.party_instance.assignments[i].value, symbol_name, program_root);
        }
        break;
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++) {
        mask |= slot_access_mask_for_named_symbol(ast_map_literal_key(node, i), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(ast_map_literal_value(node, i), symbol_name, program_root);
        }
        break;
    case AST_BINARY:
        mask |= slot_access_mask_for_named_symbol(
            ast_binary_left(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_binary_right(node), symbol_name, program_root);
        break;

    case AST_UNARY:
        mask |= slot_access_mask_for_named_symbol(
            ast_unary_operand(node), symbol_name, program_root);
        break;

    case AST_CALL:
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_IDENTIFIER) {
            const char *name = ast_identifier_name(ast_call_callee(node));
            unsigned access_mask = slot_builtin_access_mask(name);
            const ASTNode *root = NULL;
            if (ast_call_arg_count(node) > 0 &&
                !slot_checked_call_argument_root(program_root, node, 0, &root))
                return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
            if (program_root != NULL && semantic_typed_map_query_receiver_read(program_root->ctx, node, 0))
                access_mask |= SLOT_ACCESS_READ;
            if (access_mask != 0
                && ast_call_arg_count(node) >= 1
                && root != NULL
                && strcmp(ast_identifier_name(root),
                          symbol_name) == 0) {
                mask |= access_mask;
            } else if (program_root != NULL) {
                ASTNode *callee_decl = slot_analyzer_find_function_decl(program_root, name);
                size_t param_count = 0;
                if (callee_decl != NULL) {
                    param_count = ast_func_param_count(callee_decl);
                    if (ast_func_body(callee_decl) != NULL) {
                        for (size_t i = 0; i < param_count && i < ast_call_arg_count(node); i++) {
                            FuncParam *param = ast_func_param(callee_decl, i);
                            const ASTNode *argument_root = NULL;
                            unsigned callee_mask;
                            if (!slot_checked_call_argument_root(program_root, node, i, &argument_root))
                                return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
                            if (param == NULL || param->name == NULL || argument_root == NULL
                                || strcmp(ast_identifier_name(argument_root), symbol_name) != 0)
                                continue;
                            if (param->mode != PARAM_MODE_REF && param->mode != PARAM_MODE_OWN)
                                continue;
                            callee_mask = function_param_flow_summary_demand(
                                program_root, callee_decl, i);
                            if ((callee_mask & SLOT_PARAM_SUMMARY_READ) != 0)
                                mask |= SLOT_ACCESS_READ;
                            if ((callee_mask & SLOT_PARAM_SUMMARY_WRITE) != 0)
                                mask |= SLOT_ACCESS_WRITE;
                            if ((callee_mask & SLOT_PARAM_SUMMARY_RELEASE) != 0)
                                mask |= SLOT_ACCESS_RELEASE;
                        }
                    }
                }
            }
        }

        mask |= slot_access_mask_for_named_symbol(
            ast_call_callee(node), symbol_name, program_root);
        for (size_t i = 0; i < ast_call_arg_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_call_argument(node, i), symbol_name, program_root);
        break;

    case AST_MEMBER_ACCESS:
        mask |= slot_access_mask_for_named_symbol(
            ast_member_object(node), symbol_name, program_root);
        break;

    case AST_ARRAY_ACCESS:
        mask |= slot_access_mask_for_named_symbol(
            ast_array_access_array(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_array_access_index(node), symbol_name, program_root);
        break;

    case AST_CHANNEL_SEND:
        mask |= slot_access_mask_for_named_symbol(
            ast_channel_send_channel(node), symbol_name, program_root);
        mask |= slot_access_mask_for_named_symbol(
            ast_channel_send_value(node), symbol_name, program_root);
        break;

    case AST_CHANNEL_RECV:
        mask |= slot_access_mask_for_named_symbol(
            ast_channel_recv_channel(node), symbol_name, program_root);
        break;

    case AST_RETURN:
        mask |= slot_access_mask_for_named_symbol(
            ast_return_value(node), symbol_name, program_root);
        break;

    case AST_NUMBER: case AST_STRING: case AST_BOOLEAN: case AST_IDENTIFIER:
    case AST_BREAK: case AST_CONTINUE: case AST_CONTEXT_ACCESS:
    case AST_FUNC_DECL: case AST_CLASS_DECL: case AST_EXTERN_BLOCK: case AST_ENUM_DECL:
    case AST_TYPE_ALIAS: case AST_TYPE: case AST_CHANNEL_TYPE: case AST_FUTURE_TYPE:
    case AST_PROGRAM: case AST_NAMESPACE_DECL: case AST_ABILITY_DECL: case AST_ROLE_DECL:
    case AST_PARTY_DECL: case AST_ROSTER_DECL: case AST_WORLD_DECL: case AST_INTENT_DECL:
    case AST_RELATION_DECL: case AST_EFFECT_DECL: case AST_ZONE_DECL: case AST_LIFECYCLE_DECL:
    case AST_EVENT_DECL: case AST_EVENT_HANDLER_TYPE: case AST_IMPORT_DECL: case AST_USE_DECL:
        break;
    default:
        return SLOT_ACCESS_READ | SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE;
    }

    return mask;
}
