/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot Lifetime Analyzer implementation
 */

#include <stdlib.h>
#include <string.h>
#include "slot_analyzer.h"

#define INITIAL_ENTRY_CAPACITY 16

/* -----------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------- */

SlotAnalyzer *
slot_analyzer_create(SemanticContext *ctx)
{
    SlotAnalyzer *sa = calloc(1, sizeof(SlotAnalyzer));
    if (sa == NULL)
        return NULL;

    sa->ctx              = ctx;
    sa->program_root     = NULL;
    sa->entry_capacity   = INITIAL_ENTRY_CAPACITY;
    sa->entries          = calloc(INITIAL_ENTRY_CAPACITY,
                                  sizeof(SlotLifetimeEntry));
    if (sa->entries == NULL) {
        free(sa);
        return NULL;
    }
    return sa;
}

void
slot_analyzer_destroy(SlotAnalyzer *sa)
{
    if (sa == NULL)
        return;
    free(sa->entries);
    free(sa);
}

/* -----------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------- */

/*
 * Collect every SYMBOL_SLOT visible in the current scope chain
 * that is still CLAIMED (i.e. not yet released).
 *
 * Returns a malloc'd array of Symbol* that the caller must free.
 * Sets *out_count.
 */
static Symbol **
collect_live_slots(Scope *scope, size_t *out_count)
{
    /* Two-pass: count then fill */
    size_t count = 0;
    for (Scope *s = scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym->kind == SYMBOL_SLOT
                && sym->slot_info.state == SLOT_STATE_CLAIMED) {
                count++;
            }
        }
    }

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    Symbol **result = calloc(count, sizeof(Symbol *));
    if (result == NULL) {
        *out_count = 0;
        return NULL;
    }

    size_t idx = 0;
    for (Scope *s = scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            if (sym->kind == SYMBOL_SLOT
                && sym->slot_info.state == SLOT_STATE_CLAIMED) {
                result[idx++] = sym;
            }
        }
    }

    *out_count = count;
    return result;
}

enum {
    SLOT_ACCESS_READ = 1u << 0,
    SLOT_ACCESS_WRITE = 1u << 1,
    SLOT_ACCESS_RELEASE = 1u << 2
};

typedef struct
{
    const char *name;
    unsigned    mask;
} SlotAccessEntry;

typedef struct
{
    const char *name;
    unsigned    mask;
} SlotEscapeEntry;

static void
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
        size_t new_cap = *capacity == 0 ? 8 : (*capacity * 2);
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

static void
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

static ASTNode *
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

static unsigned
slot_access_mask_for_named_symbol(ASTNode *node, const char *symbol_name,
                                  ASTNode *program_root, int depth);

static void
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

        mask = slot_access_mask_for_named_symbol(
            body, param->name, program_root, depth + 1);
        if ((mask & SLOT_ACCESS_READ) != 0) {
            slot_access_record(entries, count, capacity,
                arg->data.identifier.name, SLOT_ACCESS_READ);
        }
        if ((mask & SLOT_ACCESS_WRITE) != 0) {
            slot_access_record(entries, count, capacity,
                arg->data.identifier.name, SLOT_ACCESS_WRITE);
        }
        if ((mask & SLOT_ACCESS_RELEASE) != 0) {
            slot_access_record(entries, count, capacity,
                arg->data.identifier.name, SLOT_ACCESS_RELEASE);
        }
    }
}

static void
collect_slot_escapes(ASTNode *node, SlotEscapeEntry **entries,
                     size_t *count, size_t *capacity)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            collect_slot_escapes(node->data.block.statements[i], entries, count, capacity);
        break;
    case AST_IF_STMT:
        collect_slot_escapes(node->data.if_stmt.condition, entries, count, capacity);
        collect_slot_escapes(node->data.if_stmt.then_branch, entries, count, capacity);
        collect_slot_escapes(node->data.if_stmt.else_branch, entries, count, capacity);
        break;
    case AST_WITH_STMT:
        collect_slot_escapes(node->data.with_stmt.body, entries, count, capacity);
        break;
    case AST_FOR_LOOP:
        collect_slot_escapes(node->data.for_loop.range_start, entries, count, capacity);
        collect_slot_escapes(node->data.for_loop.range_end, entries, count, capacity);
        collect_slot_escapes(node->data.for_loop.iterable, entries, count, capacity);
        collect_slot_escapes(node->data.for_loop.body, entries, count, capacity);
        break;
    case AST_WHILE_LOOP:
        collect_slot_escapes(node->data.while_loop.condition, entries, count, capacity);
        collect_slot_escapes(node->data.while_loop.body, entries, count, capacity);
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            collect_slot_escapes(node->data.parallel.tasks[i], entries, count, capacity);
        break;
    case AST_LET_DECL:
        collect_slot_escapes(node->data.let_decl.initializer, entries, count, capacity);
        break;
    case AST_ASSIGNMENT:
        collect_slot_escapes(node->data.assignment.target, entries, count, capacity);
        collect_slot_escapes(node->data.assignment.value, entries, count, capacity);
        break;
    case AST_CALL:
        collect_slot_escapes(node->data.call.callee, entries, count, capacity);
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            ASTNode *arg = node->data.call.arguments[i];
            if (arg != NULL && arg->type == AST_IDENTIFIER
                && arg->data.identifier.name != NULL) {
                slot_escape_record(entries, count, capacity,
                    arg->data.identifier.name, SLOT_ESCAPE_CALL);
            }
            collect_slot_escapes(arg, entries, count, capacity);
        }
        break;
    case AST_CHANNEL_SEND:
        collect_slot_escapes(node->data.channel_send.channel, entries, count, capacity);
        if (node->data.channel_send.value != NULL
            && node->data.channel_send.value->type == AST_IDENTIFIER
            && node->data.channel_send.value->data.identifier.name != NULL) {
            slot_escape_record(entries, count, capacity,
                node->data.channel_send.value->data.identifier.name,
                SLOT_ESCAPE_CHANNEL);
        }
        collect_slot_escapes(node->data.channel_send.value, entries, count, capacity);
        break;
    case AST_RETURN:
        if (node->data.return_stmt.value != NULL
            && node->data.return_stmt.value->type == AST_IDENTIFIER
            && node->data.return_stmt.value->data.identifier.name != NULL) {
            slot_escape_record(entries, count, capacity,
                node->data.return_stmt.value->data.identifier.name,
                SLOT_ESCAPE_RETURN);
        }
        collect_slot_escapes(node->data.return_stmt.value, entries, count, capacity);
        break;
    default:
        break;
    }
}

static unsigned
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
            if ((strcmp(name, "Write") == 0
                 || strcmp(name, "ViewWrite") == 0
                 || strcmp(name, "Move") == 0)
                && node->data.call.arg_count >= 1
                && node->data.call.arguments[0] != NULL
                && node->data.call.arguments[0]->type == AST_IDENTIFIER
                && strcmp(node->data.call.arguments[0]->data.identifier.name, symbol_name) == 0) {
                mask |= SLOT_ACCESS_WRITE;
            } else if ((strcmp(name, "Read") == 0
                        || strcmp(name, "ViewRead") == 0)
                       && node->data.call.arg_count >= 1
                       && node->data.call.arguments[0] != NULL
                       && node->data.call.arguments[0]->type == AST_IDENTIFIER
                       && strcmp(node->data.call.arguments[0]->data.identifier.name, symbol_name) == 0) {
                mask |= SLOT_ACCESS_READ;
            } else if (strcmp(name, "Release") == 0
                       && node->data.call.arg_count >= 1
                       && node->data.call.arguments[0] != NULL
                       && node->data.call.arguments[0]->type == AST_IDENTIFIER
                       && strcmp(node->data.call.arguments[0]->data.identifier.name, symbol_name) == 0) {
                mask |= SLOT_ACCESS_RELEASE;
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

static void
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
            if ((strcmp(name, "Write") == 0
                 || strcmp(name, "ViewWrite") == 0
                 || strcmp(name, "Move") == 0)
                && node->data.call.arg_count >= 1
                && node->data.call.arguments[0] != NULL
                && node->data.call.arguments[0]->type == AST_IDENTIFIER) {
                slot_access_record(entries, count, capacity,
                    node->data.call.arguments[0]->data.identifier.name,
                    SLOT_ACCESS_WRITE);
            } else if ((strcmp(name, "Read") == 0
                        || strcmp(name, "ViewRead") == 0)
                       && node->data.call.arg_count >= 1
                       && node->data.call.arguments[0] != NULL
                       && node->data.call.arguments[0]->type == AST_IDENTIFIER) {
                slot_access_record(entries, count, capacity,
                    node->data.call.arguments[0]->data.identifier.name,
                    SLOT_ACCESS_READ);
            } else if (strcmp(name, "Release") == 0
                       && node->data.call.arg_count >= 1
                       && node->data.call.arguments[0] != NULL
                       && node->data.call.arguments[0]->type == AST_IDENTIFIER) {
                slot_access_record(entries, count, capacity,
                    node->data.call.arguments[0]->data.identifier.name,
                    SLOT_ACCESS_RELEASE);
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

/* -----------------------------------------------------------------
 * Per-node analysis
 * ----------------------------------------------------------------- */

bool
slot_analyze_block(ASTNode *block, SlotAnalyzer *sa)
{
    if (block == NULL)
        return true;

    if (block->type == AST_BLOCK) {
        for (size_t i = 0; i < block->data.block.count; i++) {
            ASTNode *stmt = block->data.block.statements[i];
            bool ok = true;

            switch (stmt->type) {
            case AST_IF_STMT:
                ok = slot_analyze_if_stmt(stmt, sa);
                break;
            case AST_WITH_STMT:
                ok = slot_analyze_with_stmt(stmt, sa);
                break;
            case AST_PARALLEL_BLOCK:
                ok = slot_analyze_parallel_block(stmt, sa);
                break;
            case AST_BLOCK:
                ok = slot_analyze_block(stmt, sa);
                break;
            default:
                break;
            }

            if (!ok)
                return false;
        }
    }
    return true;
}

bool
slot_analyze_func_body(ASTNode *func, SlotAnalyzer *sa)
{
    if (func == NULL)
        return true;

    ASTNode *body = func->data.func_decl.body;
    if (body == NULL)
        return true;

    /*
     * Snapshot live slots before entering body.
     * After the body we check that any slot claimed inside
     * has been released (warning L4).
     */
    size_t   live_count  = 0;
    Symbol **live_before = collect_live_slots(sa->ctx->scope, &live_count);

    slot_analyze_block(body, sa);

    /* L4: warn about slots that were claimed but not released */
    size_t   after_count  = 0;
    Symbol **live_after   = collect_live_slots(sa->ctx->scope, &after_count);
    SlotEscapeEntry *escapes = NULL;
    size_t escape_count = 0;
    size_t escape_capacity = 0;

    collect_slot_escapes(body, &escapes, &escape_count, &escape_capacity);

    for (size_t i = 0; i < after_count; i++) {
        Symbol *sym = live_after[i];
        bool was_live_before = false;
        for (size_t j = 0; j < live_count; j++) {
            if (live_before[j] == sym) {
                was_live_before = true;
                break;
            }
        }
        if (!was_live_before) {
            /* This slot was claimed inside the function and not released */
            ASTNode dummy = {0};
            dummy.line   = sym->decl_line;
            dummy.column = sym->decl_col;
            semantic_warning(sa->ctx, &dummy,
                "Slot '%s' claimed at line %u may not be released "
                "before function returns",
                sym->name, sym->decl_line);
        }
    }

    for (size_t i = 0; i < escape_count; i++) {
        Symbol *sym = scope_lookup(sa->ctx->scope, escapes[i].name);
        ASTNode dummy = {0};
        if (sym == NULL || sym->kind != SYMBOL_SLOT)
            continue;
        dummy.line = sym->decl_line;
        dummy.column = sym->decl_col;
        if ((escapes[i].mask & SLOT_ESCAPE_RETURN) != 0) {
            semantic_warning(sa->ctx, &dummy,
                "Slot '%s' escapes via return; non-escaping stack/local optimization is disabled",
                sym->name);
        }
        if ((escapes[i].mask & SLOT_ESCAPE_CALL) != 0) {
            semantic_warning(sa->ctx, &dummy,
                "Slot '%s' may escape through helper/function call; escape analysis stays conservative",
                sym->name);
        }
        if ((escapes[i].mask & SLOT_ESCAPE_CHANNEL) != 0) {
            semantic_warning(sa->ctx, &dummy,
                "Slot '%s' escapes through channel send; stack/local sinking is disabled",
                sym->name);
        }
    }

    free(live_before);
    free(live_after);
    free(escapes);
    return true;
}

bool
slot_analyze_with_stmt(ASTNode *with, SlotAnalyzer *sa)
{
    /*
     * with-block performs automatic release at scope exit.
     * The type checker already registered the slot and marked
     * it RELEASED when the with-scope was destroyed.
     * Here we just recurse into the body.
     */
    if (with == NULL || with->data.with_stmt.body == NULL)
        return true;

    return slot_analyze_block(with->data.with_stmt.body, sa);
}

bool
slot_analyze_if_stmt(ASTNode *ifstmt, SlotAnalyzer *sa)
{
    if (ifstmt == NULL)
        return true;

    /*
     * L3: snapshot slot states before branches,
     * run each branch, compare states afterward.
     *
     * We take a lightweight snapshot by recording the current
     * CLAIMED/RELEASED state of all visible slots.
     */
    size_t   snap_count = 0;
    Symbol **snap       = collect_live_slots(sa->ctx->scope, &snap_count);

    /* Then-branch */
    slot_analyze_block(ifstmt->data.if_stmt.then_branch, sa);

    size_t   after_then_count = 0;
    Symbol **after_then = collect_live_slots(sa->ctx->scope, &after_then_count);

    /* Else-branch (if present) */
    if (ifstmt->data.if_stmt.else_branch != NULL) {
        /*
         * Restore snapshot states so else-branch sees the original
         * state — but we can't easily restore without a deep copy.
         * For now we detect divergence by comparing released sets.
         */
        slot_analyze_block(ifstmt->data.if_stmt.else_branch, sa);

        size_t   after_else_count = 0;
        Symbol **after_else = collect_live_slots(sa->ctx->scope,
                                                 &after_else_count);

        /*
         * L3 check: a slot that was live before but is now released
         * in one branch and still live in the other.
         */
        for (size_t i = 0; i < snap_count; i++) {
            Symbol *sym = snap[i];

            bool released_in_then = true;
            for (size_t j = 0; j < after_then_count; j++) {
                if (after_then[j] == sym) {
                    released_in_then = false;
                    break;
                }
            }

            bool released_in_else = true;
            for (size_t j = 0; j < after_else_count; j++) {
                if (after_else[j] == sym) {
                    released_in_else = false;
                    break;
                }
            }

            if (released_in_then != released_in_else) {
                ASTNode dummy = {0};
                dummy.line   = ifstmt->line;
                dummy.column = ifstmt->column;
                semantic_warning(sa->ctx, &dummy,
                    "Slot '%s' is released on only one branch of if/else",
                    sym->name);
            }
        }

        free(after_else);
    }

    free(snap);
    free(after_then);
    return true;
}

bool
slot_analyze_parallel_block(ASTNode *parallel, SlotAnalyzer *sa)
{
    if (parallel == NULL)
        return true;

    size_t n = parallel->data.parallel.task_count;
    SlotAccessEntry **task_accesses = calloc(n, sizeof(SlotAccessEntry *));
    size_t *task_counts = calloc(n, sizeof(size_t));
    size_t *task_caps = calloc(n, sizeof(size_t));

    if (task_accesses == NULL || task_counts == NULL || task_caps == NULL) {
        free(task_accesses);
        free(task_counts);
        free(task_caps);
        return false;
    }

    for (size_t i = 0; i < n; i++)
        collect_slot_accesses(parallel->data.parallel.tasks[i],
            &task_accesses[i], &task_counts[i], &task_caps[i], sa->program_root);

    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            for (size_t ai = 0; ai < task_counts[i]; ai++) {
                for (size_t aj = 0; aj < task_counts[j]; aj++) {
                    if (strcmp(task_accesses[i][ai].name, task_accesses[j][aj].name) != 0)
                        continue;

                    unsigned left = task_accesses[i][ai].mask;
                    unsigned right = task_accesses[j][aj].mask;
                    bool left_mut = (left & (SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE)) != 0;
                    bool right_mut = (right & (SLOT_ACCESS_WRITE | SLOT_ACCESS_RELEASE)) != 0;
                    bool left_read = (left & SLOT_ACCESS_READ) != 0;
                    bool right_read = (right & SLOT_ACCESS_READ) != 0;

                    if (left_mut && right_mut) {
                        semantic_error(sa->ctx, parallel,
                            "Parallel context slot conflict on '%s': multiple tasks mutate or release the same slot",
                            task_accesses[i][ai].name);
                    } else if ((left_mut && right_read) || (right_mut && left_read)) {
                        semantic_warning(sa->ctx, parallel,
                            "Parallel context race risk on '%s': one task reads while another mutates or releases the same slot",
                            task_accesses[i][ai].name);
                    }
                }
            }
        }
    }

    /* Recurse into each task */
    for (size_t i = 0; i < n; i++)
        slot_analyze_block(parallel->data.parallel.tasks[i], sa);

    for (size_t i = 0; i < n; i++)
        free(task_accesses[i]);
    free(task_accesses);
    free(task_counts);
    free(task_caps);

    return !sa->ctx->has_error;
}

/* -----------------------------------------------------------------
 * Program entry point
 * ----------------------------------------------------------------- */

bool
slot_analyze_program(ASTNode *program, SlotAnalyzer *sa)
{
    if (program == NULL || program->type != AST_PROGRAM)
        return false;

    sa->program_root = program;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt->type == AST_FUNC_DECL)
            slot_analyze_func_body(stmt, sa);
    }

    return !sa->ctx->has_error;
}

unsigned
slot_analyze_escape_flags(ASTNode *node, const char *slot_name)
{
    SlotEscapeEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    unsigned mask = SLOT_ESCAPE_NONE;

    if (node == NULL || slot_name == NULL)
        return SLOT_ESCAPE_NONE;

    collect_slot_escapes(node, &entries, &count, &capacity);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(entries[i].name, slot_name) == 0) {
            mask = entries[i].mask;
            break;
        }
    }
    free(entries);
    return mask;
}
