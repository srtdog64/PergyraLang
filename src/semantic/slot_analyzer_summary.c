/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot analyzer summary/escape/access helpers.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "slot_analyzer_internal.h"

void
slot_access_record(SlotAccessEntry **entries, size_t *count, size_t *capacity,
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
        size_t new_cap = 8;
        if (*capacity != 0) {
            if (*capacity > SIZE_MAX / 2)
                return;
            new_cap = *capacity * 2;
        }
        if (new_cap > SIZE_MAX / sizeof(SlotAccessEntry))
            return;
        SlotAccessEntry *new_entries = realloc(*entries,
            new_cap * sizeof(SlotAccessEntry));
        if (new_entries == NULL)
            return;
        *entries = new_entries;
        *capacity = new_cap;
    }

    (*entries)[*count].name = name;
    (*entries)[*count].mask = mask;
    (*count)++;
}

ASTNode *
slot_analyzer_find_function_decl(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_FUNC_DECL)
            continue;

        if (stmt->is_async_decl) {
            if (stmt->data.async_func_decl.name != NULL
                && strcmp(stmt->data.async_func_decl.name, name) == 0)
                return stmt;
        } else {
            if (stmt->data.func_decl.name != NULL
                && strcmp(stmt->data.func_decl.name, name) == 0)
                return stmt;
        }
    }

    return NULL;
}

unsigned
slot_access_mask_for_named_symbol(ASTNode *node, const char *symbol_name,
                                  ASTNode *program_root, int depth);

unsigned
slot_param_summary_in_program(ASTNode *node, const char *slot_name,
                              ASTNode *program_root, int depth);

void
slot_access_record_function_aliases(ASTNode *call, ASTNode *func_decl,
                                    SlotAccessEntry **entries,
                                    size_t *count, size_t *capacity,
                                    ASTNode *program_root, int depth)
{
    size_t param_count = 0;
    FuncParam **params = NULL;
    ASTNode *body = NULL;

    if (call == NULL || func_decl == NULL)
        return;

    if (func_decl->is_async_decl) {
        param_count = func_decl->data.async_func_decl.param_count;
        params = func_decl->data.async_func_decl.params;
        body = func_decl->data.async_func_decl.body;
    } else {
        param_count = func_decl->data.func_decl.param_count;
        params = func_decl->data.func_decl.params;
        body = func_decl->data.func_decl.body;
    }

    if (body == NULL)
        return;

    for (size_t i = 0; i < param_count && i < call->data.call.arg_count; i++) {
        FuncParam *param = params != NULL ? params[i] : NULL;
        ASTNode *arg = call->data.call.arguments[i];
        unsigned mask = 0;

        if (param == NULL || arg == NULL || arg->type != AST_IDENTIFIER
            || arg->data.identifier.name == NULL)
            continue;
        if (param->mode != PARAM_MODE_REF && param->mode != PARAM_MODE_OWN)
            continue;

        mask = slot_param_summary_in_program(
            body, param->name, program_root, depth + 1);
        if ((mask & SLOT_PARAM_SUMMARY_READ) != 0) {
            slot_access_record(entries, count, capacity,
                arg->data.identifier.name, SLOT_ACCESS_READ);
        }
        if ((mask & SLOT_PARAM_SUMMARY_WRITE) != 0) {
            slot_access_record(entries, count, capacity,
                arg->data.identifier.name, SLOT_ACCESS_WRITE);
        }
        if ((mask & SLOT_PARAM_SUMMARY_RELEASE) != 0) {
            slot_access_record(entries, count, capacity,
                arg->data.identifier.name, SLOT_ACCESS_RELEASE);
        }
    }
}

unsigned
slot_access_mask_for_named_symbol(ASTNode *node, const char *symbol_name,
                                  ASTNode *program_root, int depth)
{
    unsigned mask = 0;

    if (node == NULL || symbol_name == NULL || depth > 6)
        return 0;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            mask |= slot_access_mask_for_named_symbol(
                node->data.block.statements[i], symbol_name, program_root, depth);
        break;

    case AST_LET_DECL:
        mask |= slot_access_mask_for_named_symbol(
            node->data.let_decl.initializer, symbol_name, program_root, depth);
        break;

    case AST_IF_STMT:
        mask |= slot_access_mask_for_named_symbol(
            node->data.if_stmt.condition, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.if_stmt.then_branch, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.if_stmt.else_branch, symbol_name, program_root, depth);
        break;

    case AST_WITH_STMT:
        mask |= slot_access_mask_for_named_symbol(
            node->data.with_stmt.slot_type, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.with_stmt.body, symbol_name, program_root, depth);
        break;

    case AST_FOR_LOOP:
        mask |= slot_access_mask_for_named_symbol(
            node->data.for_loop.range_start, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.for_loop.range_end, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.for_loop.body, symbol_name, program_root, depth);
        break;

    case AST_WHILE_LOOP:
        mask |= slot_access_mask_for_named_symbol(
            node->data.while_loop.condition, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.while_loop.body, symbol_name, program_root, depth);
        break;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            mask |= slot_access_mask_for_named_symbol(
                node->data.async_block.statements[i], symbol_name, program_root, depth);
        break;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            mask |= slot_access_mask_for_named_symbol(
                node->data.parallel.tasks[i], symbol_name, program_root, depth);
        break;

    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++)
            mask |= slot_access_mask_for_named_symbol(
                node->data.select_stmt.cases[i], symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.select_stmt.default_case, symbol_name, program_root, depth);
        break;

    case AST_MATCH_STMT:
        mask |= slot_access_mask_for_named_symbol(
            node->data.match_stmt.subject, symbol_name, program_root, depth);
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            mask |= slot_access_mask_for_named_symbol(
                node->data.match_stmt.cases[i], symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.match_stmt.default_body, symbol_name, program_root, depth);
        break;

    case AST_MATCH_CASE:
        mask |= slot_access_mask_for_named_symbol(
            node->data.match_case.pattern, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.match_case.guard, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.match_case.body, symbol_name, program_root, depth);
        break;

    case AST_ASSIGNMENT:
        mask |= slot_access_mask_for_named_symbol(
            node->data.assignment.target, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.assignment.value, symbol_name, program_root, depth);
        break;

    case AST_BINARY:
        mask |= slot_access_mask_for_named_symbol(
            node->data.binary.left, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.binary.right, symbol_name, program_root, depth);
        break;

    case AST_UNARY:
        mask |= slot_access_mask_for_named_symbol(
            node->data.unary.operand, symbol_name, program_root, depth);
        break;

    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER) {
            const char *name = node->data.call.callee->data.identifier.name;
            unsigned access_mask = slot_builtin_access_mask(name);
            if (access_mask != 0
                && node->data.call.arg_count >= 1
                && node->data.call.arguments[0] != NULL
                && node->data.call.arguments[0]->type == AST_IDENTIFIER
                && strcmp(node->data.call.arguments[0]->data.identifier.name,
                          symbol_name) == 0) {
                mask |= access_mask;
            } else if (program_root != NULL) {
                ASTNode *callee_decl = slot_analyzer_find_function_decl(program_root, name);
                size_t param_count = 0;
                FuncParam **params = NULL;
                ASTNode *body = NULL;
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
                    if (body != NULL) {
                        for (size_t i = 0; i < param_count && i < node->data.call.arg_count; i++) {
                            FuncParam *param = params != NULL ? params[i] : NULL;
                            ASTNode *arg = node->data.call.arguments[i];
                            if (param == NULL || param->name == NULL || arg == NULL
                                || arg->type != AST_IDENTIFIER
                                || strcmp(arg->data.identifier.name, symbol_name) != 0)
                                continue;
                            if (param->mode != PARAM_MODE_REF && param->mode != PARAM_MODE_OWN)
                                continue;
                            mask |= slot_access_mask_for_named_symbol(
                                body, param->name, program_root, depth + 1);
                        }
                    }
                }
            }
        }

        mask |= slot_access_mask_for_named_symbol(
            node->data.call.callee, symbol_name, program_root, depth);
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            mask |= slot_access_mask_for_named_symbol(
                node->data.call.arguments[i], symbol_name, program_root, depth);
        break;

    case AST_MEMBER_ACCESS:
        mask |= slot_access_mask_for_named_symbol(
            node->data.member.object, symbol_name, program_root, depth);
        break;

    case AST_ARRAY_ACCESS:
        mask |= slot_access_mask_for_named_symbol(
            node->data.array_access.array, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.array_access.index, symbol_name, program_root, depth);
        break;

    case AST_CHANNEL_SEND:
        mask |= slot_access_mask_for_named_symbol(
            node->data.channel_send.channel, symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            node->data.channel_send.value, symbol_name, program_root, depth);
        break;

    case AST_CHANNEL_RECV:
        mask |= slot_access_mask_for_named_symbol(
            node->data.channel_recv.channel, symbol_name, program_root, depth);
        break;

    case AST_RETURN:
        mask |= slot_access_mask_for_named_symbol(
            node->data.return_stmt.value, symbol_name, program_root, depth);
        break;

    default:
        break;
    }

    return mask;
}

void
collect_slot_accesses(ASTNode *node, SlotAccessEntry **entries,
                      size_t *count, size_t *capacity,
                      ASTNode *program_root)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            collect_slot_accesses(node->data.block.statements[i], entries, count, capacity, program_root);
        break;

    case AST_LET_DECL:
        collect_slot_accesses(node->data.let_decl.initializer, entries, count, capacity, program_root);
        break;

    case AST_IF_STMT:
        collect_slot_accesses(node->data.if_stmt.condition, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.if_stmt.then_branch, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.if_stmt.else_branch, entries, count, capacity, program_root);
        break;

    case AST_WITH_STMT:
        collect_slot_accesses(node->data.with_stmt.slot_type, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.with_stmt.body, entries, count, capacity, program_root);
        break;

    case AST_FOR_LOOP:
        collect_slot_accesses(node->data.for_loop.range_start, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.for_loop.range_end, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.for_loop.body, entries, count, capacity, program_root);
        break;

    case AST_WHILE_LOOP:
        collect_slot_accesses(node->data.while_loop.condition, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.while_loop.body, entries, count, capacity, program_root);
        break;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            collect_slot_accesses(node->data.async_block.statements[i], entries, count, capacity, program_root);
        break;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            collect_slot_accesses(node->data.parallel.tasks[i], entries, count, capacity, program_root);
        break;

    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++)
            collect_slot_accesses(node->data.select_stmt.cases[i], entries, count, capacity, program_root);
        collect_slot_accesses(node->data.select_stmt.default_case, entries, count, capacity, program_root);
        break;

    case AST_MATCH_STMT:
        collect_slot_accesses(node->data.match_stmt.subject, entries, count, capacity, program_root);
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            collect_slot_accesses(node->data.match_stmt.cases[i], entries, count, capacity, program_root);
        collect_slot_accesses(node->data.match_stmt.default_body, entries, count, capacity, program_root);
        break;

    case AST_MATCH_CASE:
        collect_slot_accesses(node->data.match_case.pattern, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.match_case.guard, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.match_case.body, entries, count, capacity, program_root);
        break;

    case AST_ASSIGNMENT:
        collect_slot_accesses(node->data.assignment.target, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.assignment.value, entries, count, capacity, program_root);
        break;

    case AST_BINARY:
        collect_slot_accesses(node->data.binary.left, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.binary.right, entries, count, capacity, program_root);
        break;

    case AST_UNARY:
        collect_slot_accesses(node->data.unary.operand, entries, count, capacity, program_root);
        break;

    case AST_CALL:
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER) {
            const char *name = node->data.call.callee->data.identifier.name;
            unsigned access_mask = slot_builtin_access_mask(name);
            if (access_mask != 0
                && node->data.call.arg_count >= 1
                && node->data.call.arguments[0] != NULL
                && node->data.call.arguments[0]->type == AST_IDENTIFIER) {
                slot_access_record(entries, count, capacity,
                    node->data.call.arguments[0]->data.identifier.name,
                    access_mask);
            }
        }
        if (node->data.call.callee != NULL
            && node->data.call.callee->type == AST_IDENTIFIER
            && program_root != NULL) {
            ASTNode *callee_decl = slot_analyzer_find_function_decl(
                program_root, node->data.call.callee->data.identifier.name);
            if (callee_decl != NULL) {
                slot_access_record_function_aliases(node, callee_decl,
                    entries, count, capacity, program_root, 0);
            }
        }
        collect_slot_accesses(node->data.call.callee, entries, count, capacity, program_root);
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            collect_slot_accesses(node->data.call.arguments[i], entries, count, capacity, program_root);
        break;

    case AST_MEMBER_ACCESS:
        collect_slot_accesses(node->data.member.object, entries, count, capacity, program_root);
        break;

    case AST_ARRAY_ACCESS:
        collect_slot_accesses(node->data.array_access.array, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.array_access.index, entries, count, capacity, program_root);
        break;

    case AST_CHANNEL_SEND:
        collect_slot_accesses(node->data.channel_send.channel, entries, count, capacity, program_root);
        collect_slot_accesses(node->data.channel_send.value, entries, count, capacity, program_root);
        break;

    case AST_CHANNEL_RECV:
        collect_slot_accesses(node->data.channel_recv.channel, entries, count, capacity, program_root);
        break;

    case AST_RETURN:
        collect_slot_accesses(node->data.return_stmt.value, entries, count, capacity, program_root);
        break;

    default:
        break;
    }
}

unsigned
slot_param_summary_in_program(ASTNode *node, const char *slot_name,
                              ASTNode *program_root, int depth)
{
    unsigned summary = SLOT_PARAM_SUMMARY_NONE;
    unsigned access_mask = 0;
    unsigned escape_mask = 0;

    if (node == NULL || slot_name == NULL)
        return SLOT_PARAM_SUMMARY_NONE;

    access_mask = slot_access_mask_for_named_symbol(
        node, slot_name, program_root, depth);
    escape_mask = slot_escape_mask_in_program(
        node, slot_name, program_root, depth);

    if ((access_mask & SLOT_ACCESS_READ) != 0)
        summary |= SLOT_PARAM_SUMMARY_READ;
    if ((access_mask & SLOT_ACCESS_WRITE) != 0)
        summary |= SLOT_PARAM_SUMMARY_WRITE;
    if ((access_mask & SLOT_ACCESS_RELEASE) != 0)
        summary |= SLOT_PARAM_SUMMARY_RELEASE;
    if ((escape_mask & SLOT_ESCAPE_RETURN) != 0)
        summary |= SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
    if ((escape_mask & SLOT_ESCAPE_CALL) != 0)
        summary |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
    if ((escape_mask & SLOT_ESCAPE_CHANNEL) != 0)
        summary |= SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    return summary;
}

