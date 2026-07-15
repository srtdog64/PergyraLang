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

static bool
slot_call_is_non_escape_builtin(ASTNode *callee)
{
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_name(callee) == NULL) {
        return false;
    }

    return slot_builtin_call_is_local_non_escape(
        ast_identifier_name(callee));
}

unsigned
slot_escape_mask_in_program(ASTNode *node, const char *slot_name,
                            const SlotFunctionLookup *program_root, int depth,
                            const SlotSummaryOrigin *origin,
                            bool *failed_out)
{
    SlotEscapeEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    unsigned mask = SLOT_ESCAPE_NONE;

    if (node == NULL || slot_name == NULL)
        return SLOT_ESCAPE_NONE;
    if (depth > 6)
        return SLOT_ESCAPE_CALL;

    if (!collect_slot_escapes(node, &entries, &count, &capacity, program_root,
            depth, origin, failed_out)) {
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
                     const SlotFunctionLookup *program_root, int depth,
                     const SlotSummaryOrigin *origin,
                     bool *failed_out)
{
    if (node == NULL)
        return true;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            if (!collect_slot_escapes(ast_block_statement(node, i), entries,
                    count, capacity, program_root, depth, origin, failed_out))
                return false;
        break;
    case AST_IF_STMT:
        if (!collect_slot_escapes(ast_if_condition(node), entries, count,
                capacity, program_root, depth, origin, failed_out)
            || !collect_slot_escapes(ast_if_then_branch(node), entries, count,
                capacity, program_root, depth, origin, failed_out)
            || !collect_slot_escapes(ast_if_else_branch(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    case AST_WITH_STMT:
        if (!collect_slot_escapes(ast_with_body(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    case AST_FOR_LOOP:
        if (!collect_slot_escapes(ast_for_range_start(node), entries, count,
                capacity, program_root, depth, origin, failed_out)
            || !collect_slot_escapes(ast_for_range_end(node), entries, count,
                capacity, program_root, depth, origin, failed_out)
            || !collect_slot_escapes(ast_for_iterable(node), entries, count,
                capacity, program_root, depth, origin, failed_out)
            || !collect_slot_escapes(ast_for_body(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    case AST_WHILE_LOOP:
        if (!collect_slot_escapes(ast_while_condition(node), entries, count,
                capacity, program_root, depth, origin, failed_out)
            || !collect_slot_escapes(ast_while_body(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            if (!collect_slot_escapes(ast_parallel_task(node, i), entries, count,
                    capacity, program_root, depth, origin, failed_out))
                return false;
        break;
    case AST_LET_DECL:
        if (!collect_slot_escapes(ast_let_initializer(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    case AST_ASSIGNMENT:
        if (!collect_slot_escapes(ast_assignment_target(node), entries, count,
                capacity, program_root, depth, origin, failed_out)
            || !collect_slot_escapes(ast_assignment_value(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    case AST_CALL: {
        ASTNode *callee_decl = NULL;
        size_t param_count = 0;
        ASTNode *body = NULL;
        ASTNode *callee = ast_call_callee(node);

        if (!collect_slot_escapes(callee, entries, count, capacity,
                program_root, depth, origin, failed_out))
            return false;
        if (callee != NULL
            && callee->type == AST_IDENTIFIER
            && ast_identifier_name(callee) != NULL
            && program_root != NULL) {
            callee_decl = slot_analyzer_find_function_decl(
                program_root, ast_identifier_name(callee));
            if (callee_decl != NULL) {
                param_count = ast_func_param_count(callee_decl);
                body = ast_func_body(callee_decl);
            }
        }

        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            ASTNode *arg = ast_call_argument(node, i);
            if (arg != NULL && arg->type == AST_IDENTIFIER
                && !slot_call_is_non_escape_builtin(callee)
                && ast_identifier_name(arg) != NULL) {
                bool handled = false;

                if (body != NULL && i < param_count) {
                    FuncParam *param = ast_func_param(callee_decl, i);
                    if (param != NULL && param->name != NULL) {
                        if (param->mode == PARAM_MODE_REF) {
                            bool direct_self_reborrow = origin != NULL
                                && callee_decl == origin->function_decl
                                && !callee_decl->is_async_decl
                                && i == origin->param_index
                                && origin->param_name != NULL
                                && strcmp(ast_identifier_name(arg),
                                          origin->param_name) == 0;
                            unsigned callee_mask = direct_self_reborrow
                                ? SLOT_PARAM_SUMMARY_NONE
                                : function_param_flow_summary_demand(
                                    program_root, callee_decl, i);
                            if ((callee_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                                if (!slot_escape_record(entries, count, capacity,
                                        ast_identifier_name(arg), SLOT_ESCAPE_RETURN))
                                    return false;
                            }
                            if ((callee_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0) {
                                if (!slot_escape_record(entries, count, capacity,
                                        ast_identifier_name(arg), SLOT_ESCAPE_CHANNEL))
                                    return false;
                            }
                            if ((callee_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) {
                                if (!slot_escape_record(entries, count, capacity,
                                        ast_identifier_name(arg), SLOT_ESCAPE_CALL))
                                    return false;
                            }
                            handled = true;
                        } else if (param->mode == PARAM_MODE_OWN) {
                            if (!slot_escape_record(entries, count, capacity,
                                    ast_identifier_name(arg), SLOT_ESCAPE_CALL))
                                return false;
                            handled = true;
                        }
                    }
                }

                if (!handled) {
                    if (!slot_escape_record(entries, count, capacity,
                            ast_identifier_name(arg), SLOT_ESCAPE_CALL))
                        return false;
                }
            }
            if (!collect_slot_escapes(arg, entries, count, capacity, program_root,
                    depth, origin, failed_out))
                return false;
        }
        break;
    }
    case AST_CHANNEL_SEND:
        if (!collect_slot_escapes(ast_channel_send_channel(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
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
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    case AST_RETURN:
        if (ast_return_value(node) != NULL
            && ast_return_value(node)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_return_value(node)) != NULL) {
            if (!slot_escape_record(entries, count, capacity,
                    ast_identifier_name(ast_return_value(node)),
                    SLOT_ESCAPE_RETURN))
                return false;
        }
        if (!collect_slot_escapes(ast_return_value(node), entries, count,
                capacity, program_root, depth, origin, failed_out))
            return false;
        break;
    default:
        break;
    }
    return true;
}
