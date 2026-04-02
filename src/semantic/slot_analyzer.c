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
collect_slot_accesses(ASTNode *node, SlotAccessEntry **entries,
                      size_t *count, size_t *capacity)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            collect_slot_accesses(node->data.block.statements[i], entries, count, capacity);
        break;

    case AST_LET_DECL:
        collect_slot_accesses(node->data.let_decl.initializer, entries, count, capacity);
        break;

    case AST_IF_STMT:
        collect_slot_accesses(node->data.if_stmt.condition, entries, count, capacity);
        collect_slot_accesses(node->data.if_stmt.then_branch, entries, count, capacity);
        collect_slot_accesses(node->data.if_stmt.else_branch, entries, count, capacity);
        break;

    case AST_WITH_STMT:
        collect_slot_accesses(node->data.with_stmt.slot_type, entries, count, capacity);
        collect_slot_accesses(node->data.with_stmt.body, entries, count, capacity);
        break;

    case AST_FOR_LOOP:
        collect_slot_accesses(node->data.for_loop.range_start, entries, count, capacity);
        collect_slot_accesses(node->data.for_loop.range_end, entries, count, capacity);
        collect_slot_accesses(node->data.for_loop.body, entries, count, capacity);
        break;

    case AST_WHILE_LOOP:
        collect_slot_accesses(node->data.while_loop.condition, entries, count, capacity);
        collect_slot_accesses(node->data.while_loop.body, entries, count, capacity);
        break;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            collect_slot_accesses(node->data.async_block.statements[i], entries, count, capacity);
        break;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            collect_slot_accesses(node->data.parallel.tasks[i], entries, count, capacity);
        break;

    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++)
            collect_slot_accesses(node->data.select_stmt.cases[i], entries, count, capacity);
        collect_slot_accesses(node->data.select_stmt.default_case, entries, count, capacity);
        break;

    case AST_MATCH_STMT:
        collect_slot_accesses(node->data.match_stmt.subject, entries, count, capacity);
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            collect_slot_accesses(node->data.match_stmt.cases[i], entries, count, capacity);
        collect_slot_accesses(node->data.match_stmt.default_body, entries, count, capacity);
        break;

    case AST_MATCH_CASE:
        collect_slot_accesses(node->data.match_case.pattern, entries, count, capacity);
        collect_slot_accesses(node->data.match_case.guard, entries, count, capacity);
        collect_slot_accesses(node->data.match_case.body, entries, count, capacity);
        break;

    case AST_ASSIGNMENT:
        collect_slot_accesses(node->data.assignment.target, entries, count, capacity);
        collect_slot_accesses(node->data.assignment.value, entries, count, capacity);
        break;

    case AST_BINARY:
        collect_slot_accesses(node->data.binary.left, entries, count, capacity);
        collect_slot_accesses(node->data.binary.right, entries, count, capacity);
        break;

    case AST_UNARY:
        collect_slot_accesses(node->data.unary.operand, entries, count, capacity);
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
        collect_slot_accesses(node->data.call.callee, entries, count, capacity);
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            collect_slot_accesses(node->data.call.arguments[i], entries, count, capacity);
        break;

    case AST_MEMBER_ACCESS:
        collect_slot_accesses(node->data.member.object, entries, count, capacity);
        break;

    case AST_ARRAY_ACCESS:
        collect_slot_accesses(node->data.array_access.array, entries, count, capacity);
        collect_slot_accesses(node->data.array_access.index, entries, count, capacity);
        break;

    case AST_CHANNEL_SEND:
        collect_slot_accesses(node->data.channel_send.channel, entries, count, capacity);
        collect_slot_accesses(node->data.channel_send.value, entries, count, capacity);
        break;

    case AST_CHANNEL_RECV:
        collect_slot_accesses(node->data.channel_recv.channel, entries, count, capacity);
        break;

    case AST_RETURN:
        collect_slot_accesses(node->data.return_stmt.value, entries, count, capacity);
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

    free(live_before);
    free(live_after);
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
            &task_accesses[i], &task_counts[i], &task_caps[i]);

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
                            "Parallel slot conflict on '%s': multiple tasks mutate or release the same slot",
                            task_accesses[i][ai].name);
                    } else if ((left_mut && right_read) || (right_mut && left_read)) {
                        semantic_warning(sa->ctx, parallel,
                            "Parallel slot race risk on '%s': one task reads while another mutates or releases the same slot",
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

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt->type == AST_FUNC_DECL)
            slot_analyze_func_body(stmt, sa);
    }

    return !sa->ctx->has_error;
}
