/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer escape collection helpers.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "slot_analyzer_internal.h"
#include "typed_map_query_fact_owner.h"
#include "function_param_publication_owner.h"
#include "function_param_flow_summary_internal.h"
#include "../common/match_variant_policy.h"

bool
slot_escape_record(SlotEscapeEntry **entries, size_t *count, size_t *capacity,
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
        if (new_cap > SIZE_MAX / sizeof(SlotEscapeEntry))
            return false;
        SlotEscapeEntry *new_entries = realloc(*entries,
            new_cap * sizeof(SlotEscapeEntry));
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

unsigned
slot_escape_mask_in_program(ASTNode *node, const char *slot_name,
                            const SlotFunctionLookup *program_root,
                            const SlotSummaryOrigin *origin,
                            bool *failed_out)
{
    SlotEscapeEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    unsigned mask = SLOT_ESCAPE_NONE;

    if (node == NULL || slot_name == NULL)
        return SLOT_ESCAPE_NONE;

    if (!collect_slot_escapes(node, &entries, &count, &capacity, program_root,
            origin, slot_name, failed_out)) {
        free(entries);
        if (failed_out != NULL)
            *failed_out = true;
        return SLOT_ESCAPE_RETURN | SLOT_ESCAPE_CALL | SLOT_ESCAPE_CHANNEL;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].name, slot_name) == 0) {
            mask = entries[i].mask;
            break;
        }
    }
    free(entries);
    return mask;
}

bool
collect_slot_escapes(ASTNode *node, SlotEscapeEntry **entries,
                     size_t *count, size_t *capacity,
                     const SlotFunctionLookup *program_root,
                     const SlotSummaryOrigin *origin,
                     const char *selected_name,
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
            if (!collect_slot_escapes(ast_block_statement(node, i), entries,
                    count, capacity, program_root, origin, selected_name, failed_out))
                return false;
        break;
    case AST_IF_STMT:
        if (!collect_slot_escapes(ast_if_condition(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)
            || !collect_slot_escapes(ast_if_then_branch(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)
            || !collect_slot_escapes(ast_if_else_branch(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    case AST_WITH_STMT:
        if (!collect_slot_escapes(ast_with_body(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    case AST_FOR_LOOP:
        if (!collect_slot_escapes(ast_for_range_start(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)
            || !collect_slot_escapes(ast_for_range_end(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)
            || !collect_slot_escapes(ast_for_iterable(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)
            || !collect_slot_escapes(ast_for_body(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    case AST_WHILE_LOOP:
        if (!collect_slot_escapes(ast_while_condition(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)
            || !collect_slot_escapes(ast_while_body(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            if (!collect_slot_escapes(ast_parallel_task(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out))
                return false;
        if (!collect_slot_escapes(ast_parallel_join_collection(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out) ||
            !collect_slot_escapes(ast_parallel_join_range_end(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_LET_DECL:
        if (!collect_slot_escapes(ast_let_initializer(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    case AST_LET_DESTRUCTURE:
        return collect_slot_escapes(ast_let_destructure_initializer(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_BINARY:
        if (!collect_slot_escapes(ast_binary_left(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out) ||
            !collect_slot_escapes(ast_binary_right(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_UNARY:
        return collect_slot_escapes(ast_unary_operand(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_MEMBER_ACCESS:
        return collect_slot_escapes(ast_member_object(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_ARRAY_ACCESS:
        if (!collect_slot_escapes(ast_array_access_array(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out) ||
            !collect_slot_escapes(ast_array_access_index(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++)
            if (!collect_slot_escapes(ast_array_literal_element(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++)
            if (!collect_slot_escapes(ast_tuple_literal_element(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++)
            if (!collect_slot_escapes(ast_map_literal_key(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out) ||
                !collect_slot_escapes(ast_map_literal_value(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_SET_LITERAL:
        for (size_t i = 0; i < ast_set_literal_count(node); i++)
            if (!collect_slot_escapes(ast_set_literal_element(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_CAST:
        return collect_slot_escapes(ast_cast_operand(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_TYPE_TEST:
        return collect_slot_escapes(ast_type_test_operand(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_AWAIT_EXPR:
        return collect_slot_escapes(ast_await_expression(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_CHANNEL_RECV:
        return collect_slot_escapes(ast_channel_recv_channel(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_PARTY_INSTANCE:
        for (size_t i = 0; i < ast_party_instance_assignment_count(node); i++)
            if (!collect_slot_escapes(ast_party_instance_assignment_value(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            if (!collect_slot_escapes(ast_select_case(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        return collect_slot_escapes(ast_select_default_case(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_MATCH_STMT:
        if (!collect_slot_escapes(ast_match_subject(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)) return false;
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            if (!collect_slot_escapes(ast_match_case_at(node, i), entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        return collect_slot_escapes(ast_match_default_body(node), entries, count,
            capacity, program_root, origin, selected_name, failed_out);
    case AST_MATCH_CASE: {
        int family = ast_match_case_semantic_subject_family(node);
        if (family == PGY_MATCH_SUBJECT_UNKNOWN) {
            ASTNode *single = ast_match_case_pattern(node);
            bool single_seen = false;
            for (size_t i = 0; i < ast_match_case_pattern_count(node); i++) {
                ASTNode *pattern = ast_match_case_pattern_at(node, i);
                single_seen |= pattern == single;
                if (!collect_slot_escapes(pattern, entries, count, capacity,
                        program_root, origin, selected_name, failed_out)) return false;
            }
            if (!single_seen && !collect_slot_escapes(single, entries, count, capacity,
                    program_root, origin, selected_name, failed_out)) return false;
        } else if (family != PGY_MATCH_SUBJECT_OPTION && family != PGY_MATCH_SUBJECT_RESULT &&
                   family != PGY_MATCH_SUBJECT_ENUM) {
            if (failed_out != NULL) *failed_out = true;
            return false;
        }
        if (!collect_slot_escapes(ast_match_case_guard(node), entries, count, capacity,
                program_root, origin, selected_name, failed_out)) return false;
        return collect_slot_escapes(ast_match_case_body(node), entries, count, capacity,
            program_root, origin, selected_name, failed_out);
    }
    case AST_GIVE_STMT:
        return collect_slot_escapes(ast_give_value(node), entries, count, capacity,
            program_root, origin, selected_name, failed_out);
    case AST_UNSAFE_BLOCK:
        return collect_slot_escapes(ast_unsafe_block_body(node), entries, count, capacity,
            program_root, origin, selected_name, failed_out);
    case AST_TRANSACTION_BLOCK:
        if (!collect_slot_escapes(ast_transaction_block_body(node), entries, count, capacity,
                program_root, origin, selected_name, failed_out)) return false;
        for (size_t i = 0; i < node->data.transaction_block.compensation_count; i++)
            if (!collect_slot_escapes(node->data.transaction_block.compensations[i], entries, count,
                    capacity, program_root, origin, selected_name, failed_out)) return false;
        break;
    case AST_FAIL_STMT:
        return collect_slot_escapes(ast_fail_stmt_reason(node), entries, count, capacity,
            program_root, origin, selected_name, failed_out);
    case AST_DEFER_STMT:
        return collect_slot_escapes(ast_defer_body(node), entries, count, capacity,
            program_root, origin, selected_name, failed_out);
    case AST_BIND_STMT:
        return collect_slot_escapes(ast_bind_statement_subject(node), entries, count, capacity,
            program_root, origin, selected_name, failed_out);
    case AST_ASSIGNMENT:
        if (!collect_slot_escapes(ast_assignment_target(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out)
            || !collect_slot_escapes(ast_assignment_value(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    case AST_CALL: {
        ASTNode *callee = ast_call_callee(node);

        if (!collect_slot_escapes(callee, entries, count, capacity,
                program_root, origin, selected_name, failed_out))
            return false;
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            ASTNode *arg = ast_call_argument(node, i);
            const ASTNode *root = NULL;
            const SemanticContext *ctx = program_root == NULL ? NULL : program_root->ctx;
            ASTNode *callee_decl = semantic_param_publication_callee(ctx, node, i);
            SemanticPublishedValue value = semantic_param_publication_value(ctx, arg);
            bool local = semantic_param_publication_local_call(ctx, node, i);
            /* This is only a query-local CALL classification. The same
             * summary must compute inserted backing through the receiver. */
            bool private_store = origin != NULL && origin->private_stores_owner != NULL &&
                origin->private_stores_owner == origin->function_decl &&
                origin->private_stores_owner_id == ast_node_stable_id(origin->function_decl) &&
                semantic_param_publication_private_array_push(ctx, node, origin->function_decl);
            local |= private_store;
            bool read = semantic_typed_map_query_receiver_read(ctx, node, i);
            bool result_depends = false, external_retains = true;
            bool relation = semantic_param_publication_checked_result_relation(ctx, node, i, &result_depends, &external_retains);
            local |= relation && !external_retains;
            if (!semantic_param_publication_checked_argument(ctx, node, i) ||
                (!local && !value.ok) || (((!local && !read) || (relation && result_depends)) && !value.meta && !value.copy_escape &&
                (!slot_call_argument_storage_root(arg, &root) ||
                 (root != NULL && !semantic_param_publication_value(ctx, root).ok)))) {
                if (failed_out != NULL) *failed_out = true;
                return false;
            }
            if (root != NULL && ((!local && !read) || (relation && result_depends))
                && ast_identifier_name(root) != NULL) {
                bool handled = relation && !external_retains;
                if (relation && result_depends && !slot_escape_record(entries, count, capacity,
                        ast_identifier_name(root), SLOT_ESCAPE_RETURN)) return false;

                if (!handled && callee_decl != NULL) {
                    FuncParam *param = ast_func_param(callee_decl, i);
                    if (param != NULL && param->name != NULL) {
                        if (param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_DEFAULT) {
                            bool direct_self_reborrow = origin != NULL
                                && param->mode == PARAM_MODE_REF && arg == root
                                && callee_decl == origin->function_decl
                                && !callee_decl->is_async_decl
                                && i == origin->param_index
                                && origin->param_name != NULL
                                && strcmp(ast_identifier_name(root),
                                          origin->param_name) == 0
                                && ast_identifier_binding_syntax_id(root) ==
                                   ast_func_param_stable_id(ast_func_param(
                                       origin->function_decl, origin->param_index));
                            /* This demand produces only this root's name row.
                             * Typed validation above and argument traversal
                             * below remain unconditional in a named query. */
                            if (selected_name == NULL ||
                                strcmp(ast_identifier_name(root), selected_name) == 0) {
                                unsigned callee_mask = direct_self_reborrow
                                    ? SLOT_PARAM_SUMMARY_NONE
                                    : function_param_flow_summary_for_call(
                                        program_root, node, i);
                                if ((callee_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                                    if (!slot_escape_record(entries, count, capacity,
                                            ast_identifier_name(root), SLOT_ESCAPE_RETURN))
                                        return false;
                                }
                                if ((callee_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0) {
                                    if (!slot_escape_record(entries, count, capacity,
                                            ast_identifier_name(root), SLOT_ESCAPE_CHANNEL))
                                        return false;
                                }
                                if ((callee_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) {
                                    if (!slot_escape_record(entries, count, capacity,
                                            ast_identifier_name(root), SLOT_ESCAPE_CALL))
                                        return false;
                                }
                            }
                            handled = true;
                        } else if (param->mode == PARAM_MODE_OWN) {
                            if (!slot_escape_record(entries, count, capacity,
                                    ast_identifier_name(root), SLOT_ESCAPE_CALL))
                                return false;
                            handled = true;
                        }
                    }
                }

                if (!handled) {
                    if (!slot_escape_record(entries, count, capacity,
                            ast_identifier_name(root), SLOT_ESCAPE_CALL))
                        return false;
                }
            }
            if (!collect_slot_escapes(arg, entries, count, capacity, program_root, origin, selected_name, failed_out))
                return false;
        }
        break;
    }
    case AST_CHANNEL_SEND:
        if (!collect_slot_escapes(ast_channel_send_channel(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        if (ast_channel_send_value(node) != NULL
            && ast_channel_send_value(node)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_channel_send_value(node)) != NULL) {
            if (!slot_escape_record(entries, count, capacity,
                    ast_identifier_name(ast_channel_send_value(node)),
                    SLOT_ESCAPE_CHANNEL))
                return false;
        }
        if (!collect_slot_escapes(ast_channel_send_value(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    case AST_RETURN: {
        ASTNode *value = ast_return_value(node);
        if (value != NULL && (value->type == AST_IDENTIFIER ||
            value->type == AST_MEMBER_ACCESS || value->type == AST_ARRAY_ACCESS)) {
            const SemanticContext *ctx = program_root == NULL ? NULL : program_root->ctx;
            SemanticPublishedValue published = semantic_param_publication_value(ctx, value);
            const ASTNode *root = NULL;
            if (!published.ok || (!published.meta && !published.copy_escape &&
                (!slot_call_argument_storage_root(value, &root) ||
                 (root != NULL && !semantic_param_publication_value(ctx, root).ok)))) {
                if (failed_out != NULL) *failed_out = true;
                return false;
            }
            if (root != NULL && !slot_escape_record(entries, count, capacity,
                    ast_identifier_name(root), SLOT_ESCAPE_RETURN)) return false;
        }
        if (!collect_slot_escapes(ast_return_value(node), entries, count,
                capacity, program_root, origin, selected_name, failed_out))
            return false;
        break;
    }
    case AST_NUMBER: case AST_STRING: case AST_BOOLEAN: case AST_IDENTIFIER:
    case AST_BREAK: case AST_CONTINUE: case AST_CONTEXT_ACCESS:
    case AST_PROGRAM: case AST_FUNC_DECL: case AST_CLASS_DECL: case AST_EXTERN_BLOCK:
    case AST_TYPE_ALIAS: case AST_ENUM_DECL: case AST_TYPE: case AST_CHANNEL_TYPE:
    case AST_FUTURE_TYPE: case AST_ABILITY_DECL: case AST_ROLE_DECL: case AST_PARTY_DECL:
    case AST_ROSTER_DECL: case AST_WORLD_DECL: case AST_INTENT_DECL: case AST_RELATION_DECL:
    case AST_EFFECT_DECL: case AST_ZONE_DECL: case AST_LIFECYCLE_DECL: case AST_EVENT_DECL:
    case AST_EVENT_HANDLER_TYPE: case AST_IMPORT_DECL: case AST_USE_DECL: case AST_NAMESPACE_DECL:
        break;
    case AST_ASYNC_BLOCK: case AST_LAMBDA_EXPR: case AST_SPAWN_EXPR:
    case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE: case AST_EVENT_INVOKE: {
        const SemanticContext *ctx = program_root == NULL ? NULL : program_root->ctx;
        SemanticPublishedDeferred deferred;
        if (!semantic_param_publication_query_deferred(ctx, node, &deferred)) {
            if (failed_out != NULL) *failed_out = true;
            return false;
        }
        for (size_t c = 0; c < deferred.capture_count; c++) {
            const PublishedDeferredCapture *capture = &deferred.captures[c];
            if (!capture->copy_escape && !slot_escape_record(entries, count, capacity,
                    capture->name, SLOT_ESCAPE_CALL)) return false;
        }
        if (node->type == AST_LAMBDA_EXPR || node->type == AST_ASYNC_BLOCK) break;
        if (node->type == AST_SPAWN_EXPR) {
            ASTNode *call = ast_spawn_function(node);
            for (size_t i = 0; i < ast_call_arg_count(call); i++) {
                ASTNode *arg = ast_call_argument(call, i);
                SemanticPublishedValue value = semantic_param_publication_value(ctx, arg);
                const ASTNode *root = NULL;
                if (!semantic_param_publication_checked_argument(ctx, call, i) || !value.ok ||
                    (!value.meta && !value.copy_escape && !slot_call_argument_storage_root(arg, &root)) ||
                    (root != NULL && !semantic_param_publication_value(ctx, root).ok)) {
                    if (failed_out != NULL) *failed_out = true;
                    return false;
                }
                if (root != NULL && !slot_escape_record(entries, count, capacity,
                        ast_identifier_name(root), SLOT_ESCAPE_CALL)) return false;
            }
            return collect_slot_escapes(call, entries, count, capacity,
                program_root, origin, selected_name, failed_out);
        }
        ASTNode *event = node->type == AST_EVENT_INVOKE ? ast_event_invoke_event(node) : ast_event_op_event(node);
        if (!collect_slot_escapes(event, entries, count, capacity, program_root, origin, selected_name, failed_out)) return false;
        size_t count_args = node->type == AST_EVENT_INVOKE ? ast_event_invoke_arg_count(node) : 1;
        for (size_t i = 0; i < count_args; i++) {
            ASTNode *arg = node->type == AST_EVENT_INVOKE ? ast_event_invoke_argument(node, i) : ast_event_op_handler(node);
            SemanticPublishedValue value = semantic_param_publication_value(ctx, arg);
            const ASTNode *root = NULL;
            if (!value.ok || (!value.meta && !value.copy_escape && !slot_call_argument_storage_root(arg, &root)) ||
                (root != NULL && !semantic_param_publication_value(ctx, root).ok)) {
                if (failed_out != NULL) *failed_out = true;
                return false;
            }
            if (root != NULL && !slot_escape_record(entries, count, capacity,
                    ast_identifier_name(root), SLOT_ESCAPE_CALL)) return false;
            if (!collect_slot_escapes(arg, entries, count, capacity, program_root, origin, selected_name, failed_out)) return false;
        }
        break;
    }
    default:
        /* Capture and unknown execution facts cannot be replaced by an empty
         * escape map. Bounded parallel remains owned by its capture checker. */
        if (failed_out != NULL) *failed_out = true;
        return false;
    }
    return true;
}
