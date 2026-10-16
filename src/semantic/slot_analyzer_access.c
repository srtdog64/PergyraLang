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

bool
slot_access_record(SlotAccessEntry **entries, size_t *count, size_t *capacity,
                   const char *name, unsigned mask)
{
    if (name == NULL || entries == NULL || count == NULL || capacity == NULL)
        return false;

    for (size_t i = 0; i < *count; i++) {
        if (strcmp((*entries)[i].name, name) == 0) {
            (*entries)[i].mask |= mask;
            return true;
        }
    }

    if (*count >= *capacity) {
        size_t new_cap = 8;
        if (*capacity != 0) {
            if (*capacity > SIZE_MAX / 2)
                return false;
            new_cap = *capacity * 2;
        }
        if (new_cap > SIZE_MAX / sizeof(SlotAccessEntry))
            return false;
        SlotAccessEntry *new_entries = realloc(*entries,
            new_cap * sizeof(SlotAccessEntry));
        if (new_entries == NULL)
            return false;
        *entries = new_entries;
        *capacity = new_cap;
    }

    (*entries)[*count].name = name;
    (*entries)[*count].mask = mask;
    (*count)++;
    return true;
}

static bool
slot_access_record_function_aliases(ASTNode *call, ASTNode *func_decl,
                                    SlotAccessEntry **entries,
                                    size_t *count, size_t *capacity,
                                    const SlotFunctionLookup *program_root)
{
    size_t param_count = 0;
    ASTNode *body = NULL;

    if (call == NULL || func_decl == NULL)
        return true;

    param_count = ast_func_param_count(func_decl);
    body = ast_func_body(func_decl);

    if (body == NULL)
        return true;

    for (size_t i = 0; i < param_count && i < ast_call_arg_count(call); i++) {
        FuncParam *param = ast_func_param(func_decl, i);
        const ASTNode *root = NULL;
        unsigned mask = 0;
        if (!slot_checked_call_argument_root(program_root, call, i, &root)) return false;
        if (param == NULL || root == NULL)
            continue;
        if (param->mode != PARAM_MODE_REF && param->mode != PARAM_MODE_OWN)
            continue;

        mask = function_param_flow_summary_demand(
            program_root, func_decl, i);
        if ((mask & SLOT_PARAM_SUMMARY_READ) != 0) {
            if (!slot_access_record(entries, count, capacity,
                    ast_identifier_name(root), SLOT_ACCESS_READ))
                return false;
        }
        if ((mask & SLOT_PARAM_SUMMARY_WRITE) != 0) {
            if (!slot_access_record(entries, count, capacity,
                    ast_identifier_name(root), SLOT_ACCESS_WRITE))
                return false;
        }
        if ((mask & SLOT_PARAM_SUMMARY_RELEASE) != 0) {
            if (!slot_access_record(entries, count, capacity,
                    ast_identifier_name(root), SLOT_ACCESS_RELEASE))
                return false;
        }
    }
    return true;
}

bool
collect_slot_accesses(ASTNode *node, SlotAccessEntry **entries,
                      size_t *count, size_t *capacity,
                      const SlotFunctionLookup *program_root,
                      bool *failed_out)
{
    if (node == NULL)
        return true;
    bool reachable;
    if (program_root != NULL && semantic_param_publication_execution(program_root->ctx, node, &reachable)
        && !reachable) return true;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            if (!collect_slot_accesses(ast_block_statement(node, i), entries,
                    count, capacity, program_root, failed_out))
                return false;
        break;

    case AST_LET_DECL:
        if (!collect_slot_accesses(ast_let_initializer(node), entries, count,
                capacity, program_root, failed_out))
            return false;
        break;

    case AST_IF_STMT:
        if (!collect_slot_accesses(ast_if_condition(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_if_then_branch(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_if_else_branch(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_WITH_STMT:
        if (!collect_slot_accesses(ast_with_slot_type(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_with_body(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_FOR_LOOP:
        if (!collect_slot_accesses(ast_for_iterable(node), entries, count, capacity, program_root, failed_out)) return false;
        if (!collect_slot_accesses(ast_for_range_start(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_for_range_end(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_for_body(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_WHILE_LOOP:
        if (!collect_slot_accesses(ast_while_condition(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_while_body(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_LAMBDA_EXPR: {
        SemanticPublishedDeferred deferred;
        const SemanticContext *ctx = program_root == NULL ? NULL : program_root->ctx;
        if (!semantic_param_publication_query_deferred(ctx, node, &deferred)) {
            if (failed_out != NULL) *failed_out = true;
            return false;
        }
        for (size_t i = 0; i < deferred.capture_count; i++) {
            const ASTNode *root = NULL;
            ASTNode *source = (ASTNode *)deferred.captures[i].source;
            if (!slot_call_argument_storage_root(source, &root)) {
                if (failed_out != NULL) *failed_out = true;
                return false;
            }
            if (root != NULL && !slot_access_record(entries, count, capacity, ast_identifier_name(root), SLOT_ACCESS_READ)) return false;
            if (!collect_slot_accesses(source, entries, count, capacity, program_root, failed_out)) return false;
        }
        break;
    }
    case AST_ASYNC_BLOCK: case AST_SPAWN_EXPR:
    case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE: case AST_EVENT_INVOKE: {
        SemanticPublishedDeferred deferred;
        const SemanticContext *ctx = program_root == NULL ? NULL : program_root->ctx;
        if (!semantic_param_publication_query_deferred(ctx, node, &deferred)) {
            if (failed_out != NULL) *failed_out = true;
            return false;
        }
        if (node->type == AST_SPAWN_EXPR) {
            if (!collect_slot_accesses(ast_spawn_function(node), entries, count, capacity, program_root, failed_out)) return false;
            for (size_t i = 0; i < ast_spawn_arg_count(node); i++)
                if (!collect_slot_accesses(ast_spawn_argument(node, i), entries, count, capacity, program_root, failed_out)) return false;
        } else if (node->type == AST_EVENT_INVOKE) {
            if (!collect_slot_accesses(ast_event_invoke_event(node), entries, count, capacity, program_root, failed_out)) return false;
            for (size_t i = 0; i < ast_event_invoke_arg_count(node); i++)
                if (!collect_slot_accesses(ast_event_invoke_argument(node, i), entries, count, capacity, program_root, failed_out)) return false;
        } else if (node->type != AST_ASYNC_BLOCK &&
            (!collect_slot_accesses(ast_event_op_event(node), entries, count, capacity, program_root, failed_out) ||
             !collect_slot_accesses(ast_event_op_handler(node), entries, count, capacity, program_root, failed_out))) return false;
        break;
    }

    case AST_PARALLEL_BLOCK:
        if (!collect_slot_accesses(ast_parallel_join_collection(node), entries, count, capacity, program_root, failed_out) ||
            !collect_slot_accesses(ast_parallel_join_range_end(node), entries, count, capacity, program_root, failed_out)) return false;
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            if (!collect_slot_accesses(ast_parallel_task(node, i), entries, count, capacity, program_root, failed_out))
                return false;
        break;

    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            if (!collect_slot_accesses(ast_select_case(node, i), entries, count, capacity, program_root, failed_out))
                return false;
        if (!collect_slot_accesses(ast_select_default_case(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_MATCH_STMT:
        if (!collect_slot_accesses(ast_match_subject(node), entries, count, capacity, program_root, failed_out))
            return false;
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            if (!collect_slot_accesses(ast_match_case_at(node, i), entries, count, capacity, program_root, failed_out))
                return false;
        if (!collect_slot_accesses(ast_match_default_body(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_MATCH_CASE:
        if (ast_match_case_semantic_subject_family(node) == PGY_MATCH_SUBJECT_UNKNOWN) {
            if (!collect_slot_accesses(ast_match_case_pattern(node), entries, count, capacity, program_root, failed_out)) return false;
            for (size_t i = 0; i < ast_match_case_pattern_count(node); i++)
                if (!collect_slot_accesses(ast_match_case_pattern_at(node, i), entries, count, capacity, program_root, failed_out)) return false;
        } else if (ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_OPTION &&
                   ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_RESULT &&
                   ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_ENUM) {
            if (failed_out != NULL) *failed_out = true;
            return false;
        }
        if (!collect_slot_accesses(ast_match_case_guard(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_match_case_body(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_ASSIGNMENT:
        if (!collect_slot_accesses(ast_assignment_target(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_assignment_value(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_CAST:
        if (!collect_slot_accesses(ast_cast_operand(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_TYPE_TEST:
        if (!collect_slot_accesses(ast_type_test_operand(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_LET_DESTRUCTURE:
        if (!collect_slot_accesses(ast_let_destructure_initializer(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_GIVE_STMT:
        if (!collect_slot_accesses(ast_give_value(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_AWAIT_EXPR:
        if (!collect_slot_accesses(ast_await_expression(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_UNSAFE_BLOCK:
        if (!collect_slot_accesses(ast_unsafe_block_body(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_TRANSACTION_BLOCK:
        if (!collect_slot_accesses(ast_transaction_block_body(node), entries, count, capacity, program_root, failed_out)) return false;
        if (node->data.transaction_block.compensation_count != 0 &&
            node->data.transaction_block.compensations == NULL) {
            if (failed_out != NULL) *failed_out = true;
            return false;
        }
        for (size_t i = 0; i < node->data.transaction_block.compensation_count; i++)
            if (!collect_slot_accesses(node->data.transaction_block.compensations[i], entries,
                    count, capacity, program_root, failed_out)) return false;
        break;
    case AST_FAIL_STMT:
        if (!collect_slot_accesses(ast_fail_stmt_reason(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_DEFER_STMT:
        if (!collect_slot_accesses(ast_defer_body(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_BIND_STMT:
        if (!collect_slot_accesses(ast_bind_statement_subject(node), entries, count, capacity, program_root, failed_out)) return false;
        break;
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++) {
        if (!collect_slot_accesses(ast_array_literal_element(node, i), entries, count, capacity, program_root, failed_out)) return false;
        }
        break;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
        if (!collect_slot_accesses(ast_tuple_literal_element(node, i), entries, count, capacity, program_root, failed_out)) return false;
        }
        break;
    case AST_SET_LITERAL:
        for (size_t i = 0; i < ast_set_literal_count(node); i++) {
        if (!collect_slot_accesses(ast_set_literal_element(node, i), entries, count, capacity, program_root, failed_out)) return false;
        }
        break;
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
        if (!collect_slot_accesses(node->data.party_instance.assignments[i].value, entries, count, capacity, program_root, failed_out)) return false;
        }
        break;
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++) {
        if (!collect_slot_accesses(ast_map_literal_key(node, i), entries, count, capacity, program_root, failed_out)) return false;
        if (!collect_slot_accesses(ast_map_literal_value(node, i), entries, count, capacity, program_root, failed_out)) return false;
        }
        break;
    case AST_BINARY:
        if (!collect_slot_accesses(ast_binary_left(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_binary_right(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_UNARY:
        if (!collect_slot_accesses(ast_unary_operand(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_CALL:
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_IDENTIFIER) {
            const char *name = ast_identifier_name(ast_call_callee(node));
            unsigned access_mask = slot_builtin_access_mask(name);
            const ASTNode *root = NULL;
            if (ast_call_arg_count(node) > 0 &&
                !slot_checked_call_argument_root(program_root, node, 0, &root)) {
                if (failed_out != NULL) *failed_out = true;
                return false;
            }
            if (program_root != NULL && semantic_typed_map_query_receiver_read(program_root->ctx, node, 0))
                access_mask |= SLOT_ACCESS_READ;
            if (access_mask != 0
                && ast_call_arg_count(node) >= 1
                && root != NULL) {
                if (!slot_access_record(entries, count, capacity,
                        ast_identifier_name(root),
                        access_mask))
                    return false;
            }
        }
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_IDENTIFIER
            && program_root != NULL) {
            ASTNode *callee_decl = slot_analyzer_find_function_decl(
                program_root, ast_identifier_name(ast_call_callee(node)));
            if (callee_decl != NULL) {
                if (!slot_access_record_function_aliases(node, callee_decl,
                        entries, count, capacity, program_root))
                    return false;
            }
        }
        if (!collect_slot_accesses(ast_call_callee(node), entries, count, capacity, program_root, failed_out))
            return false;
        for (size_t i = 0; i < ast_call_arg_count(node); i++)
            if (!collect_slot_accesses(ast_call_argument(node, i), entries, count, capacity, program_root, failed_out))
                return false;
        break;

    case AST_MEMBER_ACCESS:
        if (!collect_slot_accesses(ast_member_object(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_ARRAY_ACCESS:
        if (!collect_slot_accesses(ast_array_access_array(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_array_access_index(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_CHANNEL_SEND:
        if (!collect_slot_accesses(ast_channel_send_channel(node), entries, count, capacity, program_root, failed_out)
            || !collect_slot_accesses(ast_channel_send_value(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_CHANNEL_RECV:
        if (!collect_slot_accesses(ast_channel_recv_channel(node), entries, count, capacity, program_root, failed_out))
            return false;
        break;

    case AST_RETURN:
        if (!collect_slot_accesses(ast_return_value(node), entries, count, capacity, program_root, failed_out))
            return false;
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
        if (failed_out != NULL) *failed_out = true;
        return false;
    }
    (void)failed_out;
    return true;
}
