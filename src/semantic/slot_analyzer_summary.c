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
            const char *async_name = ast_async_func_name(stmt);
            if (async_name != NULL && strcmp(async_name, name) == 0)
                return stmt;
        } else {
            const char *stmt_name = ast_declaration_name(stmt);
            if (stmt_name != NULL && strcmp(stmt_name, name) == 0)
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
        params = ast_async_func_params(func_decl, &param_count);
        body = ast_async_func_body(func_decl);
    } else {
        param_count = ast_func_param_count(func_decl);
        body = ast_func_body(func_decl);
    }

    if (body == NULL)
        return;

    for (size_t i = 0; i < param_count && i < ast_call_arg_count(call); i++) {
        FuncParam *param = params != NULL ? params[i] : ast_func_param(func_decl, i);
        ASTNode *arg = ast_call_argument(call, i);
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
            ast_if_condition(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_if_then_branch(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_if_else_branch(node), symbol_name, program_root, depth);
        break;

    case AST_WITH_STMT:
        mask |= slot_access_mask_for_named_symbol(
            ast_with_slot_type(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_with_body(node), symbol_name, program_root, depth);
        break;

    case AST_FOR_LOOP:
        mask |= slot_access_mask_for_named_symbol(
            ast_for_range_start(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_for_range_end(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_for_body(node), symbol_name, program_root, depth);
        break;

    case AST_WHILE_LOOP:
        mask |= slot_access_mask_for_named_symbol(
            ast_while_condition(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_while_body(node), symbol_name, program_root, depth);
        break;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_async_block_statement(node, i), symbol_name, program_root, depth);
        break;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_parallel_task(node, i), symbol_name, program_root, depth);
        break;

    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_select_case(node, i), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_select_default_case(node), symbol_name, program_root, depth);
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
            ast_assignment_target(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_assignment_value(node), symbol_name, program_root, depth);
        break;

    case AST_BINARY:
        mask |= slot_access_mask_for_named_symbol(
            ast_binary_left(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_binary_right(node), symbol_name, program_root, depth);
        break;

    case AST_UNARY:
        mask |= slot_access_mask_for_named_symbol(
            ast_unary_operand(node), symbol_name, program_root, depth);
        break;

    case AST_CALL:
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_IDENTIFIER) {
            const char *name = ast_call_callee(node)->data.identifier.name;
            unsigned access_mask = slot_builtin_access_mask(name);
            if (access_mask != 0
                && ast_call_arg_count(node) >= 1
                && ast_call_argument(node, 0) != NULL
                && ast_call_argument(node, 0)->type == AST_IDENTIFIER
                && strcmp(ast_call_argument(node, 0)->data.identifier.name,
                          symbol_name) == 0) {
                mask |= access_mask;
            } else if (program_root != NULL) {
                ASTNode *callee_decl = slot_analyzer_find_function_decl(program_root, name);
                size_t param_count = 0;
                FuncParam **params = NULL;
                ASTNode *body = NULL;
                if (callee_decl != NULL) {
                    if (callee_decl->is_async_decl) {
                        params = ast_async_func_params(callee_decl,
                            &param_count);
                        body = ast_async_func_body(callee_decl);
                    } else {
                        param_count = ast_func_param_count(callee_decl);
                        body = ast_func_body(callee_decl);
                    }
                    if (body != NULL) {
                        for (size_t i = 0; i < param_count && i < ast_call_arg_count(node); i++) {
                            FuncParam *param = params != NULL
                                ? params[i]
                                : ast_func_param(callee_decl, i);
                            ASTNode *arg = ast_call_argument(node, i);
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
            ast_call_callee(node), symbol_name, program_root, depth);
        for (size_t i = 0; i < ast_call_arg_count(node); i++)
            mask |= slot_access_mask_for_named_symbol(
                ast_call_argument(node, i), symbol_name, program_root, depth);
        break;

    case AST_MEMBER_ACCESS:
        mask |= slot_access_mask_for_named_symbol(
            ast_member_object(node), symbol_name, program_root, depth);
        break;

    case AST_ARRAY_ACCESS:
        mask |= slot_access_mask_for_named_symbol(
            ast_array_access_array(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_array_access_index(node), symbol_name, program_root, depth);
        break;

    case AST_CHANNEL_SEND:
        mask |= slot_access_mask_for_named_symbol(
            ast_channel_send_channel(node), symbol_name, program_root, depth);
        mask |= slot_access_mask_for_named_symbol(
            ast_channel_send_value(node), symbol_name, program_root, depth);
        break;

    case AST_CHANNEL_RECV:
        mask |= slot_access_mask_for_named_symbol(
            ast_channel_recv_channel(node), symbol_name, program_root, depth);
        break;

    case AST_RETURN:
        mask |= slot_access_mask_for_named_symbol(
            ast_return_value(node), symbol_name, program_root, depth);
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
        collect_slot_accesses(ast_if_condition(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_if_then_branch(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_if_else_branch(node), entries, count, capacity, program_root);
        break;

    case AST_WITH_STMT:
        collect_slot_accesses(ast_with_slot_type(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_with_body(node), entries, count, capacity, program_root);
        break;

    case AST_FOR_LOOP:
        collect_slot_accesses(ast_for_range_start(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_for_range_end(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_for_body(node), entries, count, capacity, program_root);
        break;

    case AST_WHILE_LOOP:
        collect_slot_accesses(ast_while_condition(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_while_body(node), entries, count, capacity, program_root);
        break;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++)
            collect_slot_accesses(ast_async_block_statement(node, i), entries, count, capacity, program_root);
        break;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(node); i++)
            collect_slot_accesses(ast_parallel_task(node, i), entries, count, capacity, program_root);
        break;

    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            collect_slot_accesses(ast_select_case(node, i), entries, count, capacity, program_root);
        collect_slot_accesses(ast_select_default_case(node), entries, count, capacity, program_root);
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
        collect_slot_accesses(ast_assignment_target(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_assignment_value(node), entries, count, capacity, program_root);
        break;

    case AST_BINARY:
        collect_slot_accesses(ast_binary_left(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_binary_right(node), entries, count, capacity, program_root);
        break;

    case AST_UNARY:
        collect_slot_accesses(ast_unary_operand(node), entries, count, capacity, program_root);
        break;

    case AST_CALL:
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_IDENTIFIER) {
            const char *name = ast_call_callee(node)->data.identifier.name;
            unsigned access_mask = slot_builtin_access_mask(name);
            if (access_mask != 0
                && ast_call_arg_count(node) >= 1
                && ast_call_argument(node, 0) != NULL
                && ast_call_argument(node, 0)->type == AST_IDENTIFIER) {
                slot_access_record(entries, count, capacity,
                    ast_call_argument(node, 0)->data.identifier.name,
                    access_mask);
            }
        }
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_IDENTIFIER
            && program_root != NULL) {
            ASTNode *callee_decl = slot_analyzer_find_function_decl(
                program_root, ast_call_callee(node)->data.identifier.name);
            if (callee_decl != NULL) {
                slot_access_record_function_aliases(node, callee_decl,
                    entries, count, capacity, program_root, 0);
            }
        }
        collect_slot_accesses(ast_call_callee(node), entries, count, capacity, program_root);
        for (size_t i = 0; i < ast_call_arg_count(node); i++)
            collect_slot_accesses(ast_call_argument(node, i), entries, count, capacity, program_root);
        break;

    case AST_MEMBER_ACCESS:
        collect_slot_accesses(ast_member_object(node), entries, count, capacity, program_root);
        break;

    case AST_ARRAY_ACCESS:
        collect_slot_accesses(ast_array_access_array(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_array_access_index(node), entries, count, capacity, program_root);
        break;

    case AST_CHANNEL_SEND:
        collect_slot_accesses(ast_channel_send_channel(node), entries, count, capacity, program_root);
        collect_slot_accesses(ast_channel_send_value(node), entries, count, capacity, program_root);
        break;

    case AST_CHANNEL_RECV:
        collect_slot_accesses(ast_channel_recv_channel(node), entries, count, capacity, program_root);
        break;

    case AST_RETURN:
        collect_slot_accesses(ast_return_value(node), entries, count, capacity, program_root);
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

