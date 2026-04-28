/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer escape collection helpers.
 */

#include <stdlib.h>
#include <string.h>

#include "slot_analyzer_internal.h"

void
slot_escape_record(SlotEscapeEntry **entries, size_t *count, size_t *capacity,
                   const char *name, unsigned mask)
{
    if (name == NULL || entries == NULL || count == NULL || capacity == NULL)
        return;

    for (size_t i = 0; i < *count; i++) {
        if (strcmp((*entries)[i].name, name) == 0) {
            (*entries)[i].mask |= mask;
            return;
        }
    }

    if (*count >= *capacity) {
        size_t new_cap = *capacity == 0 ? 8 : (*capacity * 2);
        SlotEscapeEntry *new_entries = realloc(*entries,
            new_cap * sizeof(SlotEscapeEntry));
        if (new_entries == NULL)
            return;
        *entries = new_entries;
        *capacity = new_cap;
    }

    (*entries)[*count].name = name;
    (*entries)[*count].mask = mask;
    (*count)++;
}

static bool
slot_call_is_non_escape_builtin(ASTNode *callee)
{
    const char *name;

    if (callee == NULL || callee->type != AST_IDENTIFIER
        || callee->data.identifier.name == NULL) {
        return false;
    }

    name = callee->data.identifier.name;
    return strcmp(name, "Read") == 0
        || strcmp(name, "Write") == 0
        || strcmp(name, "Release") == 0
        || strcmp(name, "ReadView") == 0
        || strcmp(name, "WriteView") == 0;
}

unsigned
slot_escape_mask_in_program(ASTNode *node, const char *slot_name,
                            ASTNode *program_root, int depth)
{
    SlotEscapeEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    unsigned mask = SLOT_ESCAPE_NONE;

    if (node == NULL || slot_name == NULL)
        return SLOT_ESCAPE_NONE;
    if (depth > 6)
        return SLOT_ESCAPE_CALL;

    collect_slot_escapes(node, &entries, &count, &capacity, program_root, depth);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].name, slot_name) == 0) {
            mask = entries[i].mask;
            break;
        }
    }
    free(entries);
    return mask;
}

void
collect_slot_escapes(ASTNode *node, SlotEscapeEntry **entries,
                     size_t *count, size_t *capacity,
                     ASTNode *program_root, int depth)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            collect_slot_escapes(node->data.block.statements[i], entries,
                count, capacity, program_root, depth);
        break;
    case AST_IF_STMT:
        collect_slot_escapes(node->data.if_stmt.condition, entries, count,
            capacity, program_root, depth);
        collect_slot_escapes(node->data.if_stmt.then_branch, entries, count,
            capacity, program_root, depth);
        collect_slot_escapes(node->data.if_stmt.else_branch, entries, count,
            capacity, program_root, depth);
        break;
    case AST_WITH_STMT:
        collect_slot_escapes(node->data.with_stmt.body, entries, count,
            capacity, program_root, depth);
        break;
    case AST_FOR_LOOP:
        collect_slot_escapes(node->data.for_loop.range_start, entries, count,
            capacity, program_root, depth);
        collect_slot_escapes(node->data.for_loop.range_end, entries, count,
            capacity, program_root, depth);
        collect_slot_escapes(node->data.for_loop.iterable, entries, count,
            capacity, program_root, depth);
        collect_slot_escapes(node->data.for_loop.body, entries, count,
            capacity, program_root, depth);
        break;
    case AST_WHILE_LOOP:
        collect_slot_escapes(node->data.while_loop.condition, entries, count,
            capacity, program_root, depth);
        collect_slot_escapes(node->data.while_loop.body, entries, count,
            capacity, program_root, depth);
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            collect_slot_escapes(node->data.parallel.tasks[i], entries, count,
                capacity, program_root, depth);
        break;
    case AST_LET_DECL:
        collect_slot_escapes(node->data.let_decl.initializer, entries, count,
            capacity, program_root, depth);
        break;
    case AST_ASSIGNMENT:
        collect_slot_escapes(node->data.assignment.target, entries, count,
            capacity, program_root, depth);
        collect_slot_escapes(node->data.assignment.value, entries, count,
            capacity, program_root, depth);
        break;
    case AST_CALL: {
        ASTNode *callee_decl = NULL;
        size_t param_count = 0;
        FuncParam **params = NULL;
        ASTNode *body = NULL;

        collect_slot_escapes(node->data.call.callee, entries, count, capacity,
            program_root, depth);
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER
            && node->data.call.callee->data.identifier.name != NULL
            && program_root != NULL) {
            callee_decl = slot_analyzer_find_function_decl(
                program_root, node->data.call.callee->data.identifier.name);
            if (callee_decl != NULL) {
                if (callee_decl->is_async_decl) {
                    param_count = callee_decl->data.async_func_decl.param_count;
                    params = callee_decl->data.async_func_decl.params;
                    body = callee_decl->data.async_func_decl.body;
                } else {
                    param_count = callee_decl->data.func_decl.param_count;
                    params = callee_decl->data.func_decl.params;
                    body = callee_decl->data.func_decl.body;
                }
            }
        }

        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            ASTNode *arg = node->data.call.arguments[i];
            if (arg != NULL && arg->type == AST_IDENTIFIER
                && !slot_call_is_non_escape_builtin(node->data.call.callee)
                && arg->data.identifier.name != NULL) {
                bool handled = false;

                if (body != NULL && i < param_count) {
                    FuncParam *param = params != NULL ? params[i] : NULL;
                    if (param != NULL && param->name != NULL) {
                        if (param->mode == PARAM_MODE_REF) {
                            unsigned callee_mask = slot_param_summary_in_program(
                                body, param->name, program_root, depth + 1);
                            if ((callee_mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0) {
                                slot_escape_record(entries, count, capacity,
                                    arg->data.identifier.name, SLOT_ESCAPE_RETURN);
                            }
                            if ((callee_mask & SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE) != 0) {
                                slot_escape_record(entries, count, capacity,
                                    arg->data.identifier.name, SLOT_ESCAPE_CHANNEL);
                            }
                            if ((callee_mask & SLOT_PARAM_SUMMARY_CALL_ESCAPE) != 0) {
                                slot_escape_record(entries, count, capacity,
                                    arg->data.identifier.name, SLOT_ESCAPE_CALL);
                            }
                            handled = true;
                        } else if (param->mode == PARAM_MODE_OWN) {
                            slot_escape_record(entries, count, capacity,
                                arg->data.identifier.name, SLOT_ESCAPE_CALL);
                            handled = true;
                        }
                    }
                }

                if (!handled) {
                    slot_escape_record(entries, count, capacity,
                        arg->data.identifier.name, SLOT_ESCAPE_CALL);
                }
            }
            collect_slot_escapes(arg, entries, count, capacity, program_root, depth);
        }
        break;
    }
    case AST_CHANNEL_SEND:
        collect_slot_escapes(node->data.channel_send.channel, entries, count,
            capacity, program_root, depth);
        if (node->data.channel_send.value != NULL
            && node->data.channel_send.value->type == AST_IDENTIFIER
            && node->data.channel_send.value->data.identifier.name != NULL) {
            slot_escape_record(entries, count, capacity,
                node->data.channel_send.value->data.identifier.name,
                SLOT_ESCAPE_CHANNEL);
        }
        collect_slot_escapes(node->data.channel_send.value, entries, count,
            capacity, program_root, depth);
        break;
    case AST_RETURN:
        if (node->data.return_stmt.value != NULL
            && node->data.return_stmt.value->type == AST_IDENTIFIER
            && node->data.return_stmt.value->data.identifier.name != NULL) {
            slot_escape_record(entries, count, capacity,
                node->data.return_stmt.value->data.identifier.name,
                SLOT_ESCAPE_RETURN);
        }
        collect_slot_escapes(node->data.return_stmt.value, entries, count,
            capacity, program_root, depth);
        break;
    default:
        break;
    }
}
