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
            origin, failed_out)) {
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
                    count, capacity, program_root, origin, failed_out))
                return false;
        break;
    case AST_IF_STMT:
        if (!collect_slot_escapes(ast_if_condition(node), entries, count,
                capacity, program_root, origin, failed_out)
            || !collect_slot_escapes(ast_if_then_branch(node), entries, count,
                capacity, program_root, origin, failed_out)
            || !collect_slot_escapes(ast_if_else_branch(node), entries, count,
                capacity, program_root, origin, failed_out))
            return false;
        break;
    case AST_WITH_STMT:
        if (!collect_slot_escapes(ast_with_body(node), entries, count,
                capacity, program_root, origin, failed_out))
            return false;
        break;
    case AST_FOR_LOOP:
        if (!collect_slot_escapes(ast_for_range_start(node), entries, count,
                capacity, program_root, origin, failed_out)
            || !collect_slot_escapes(ast_for_range_end(node), entries, count,
                capacity, program_root, origin, failed_out)
            || !collect_slot_escapes(ast_for_iterable(node), entries, count,
                capacity, program_root, origin, failed_out)
            || !collect_slot_escapes(ast_for_body(node), entries, count,
                capacity, program_root, origin, failed_out))
            return false;
        break;
    case AST_WHILE_LOOP:
        if (!collect_slot_escapes(ast_while_condition(node), entries, count,
                capacity, program_root, origin, failed_out)
            || !collect_slot_escapes(ast_while_body(node), entries, count,
                capacity, program_root, origin, failed_out))
            return false;
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            if (!collect_slot_escapes(ast_parallel_task(node, i), entries, count,
                    capacity, program_root, origin, failed_out))
                return false;
        break;
    case AST_LET_DECL:
        if (!collect_slot_escapes(ast_let_initializer(node), entries, count,
                capacity, program_root, origin, failed_out))
            return false;
        break;
    case AST_ASSIGNMENT:
        if (!collect_slot_escapes(ast_assignment_target(node), entries, count,
                capacity, program_root, origin, failed_out)
            || !collect_slot_escapes(ast_assignment_value(node), entries, count,
                capacity, program_root, origin, failed_out))
            return false;
        break;
    case AST_CALL: {
        ASTNode *callee = ast_call_callee(node);

        if (!collect_slot_escapes(callee, entries, count, capacity,
                program_root, origin, failed_out))
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
            if (!collect_slot_escapes(arg, entries, count, capacity, program_root, origin, failed_out))
                return false;
        }
        break;
    }
    case AST_CHANNEL_SEND:
        if (!collect_slot_escapes(ast_channel_send_channel(node), entries, count,
                capacity, program_root, origin, failed_out))
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
                capacity, program_root, origin, failed_out))
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
                capacity, program_root, origin, failed_out))
            return false;
        break;
    }
    default:
        break;
    }
    return true;
}
