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

/*
 * Detect Write(slot, ...) calls in an expression subtree.
 * Returns the slot name being written, or NULL.
 */
static const char *
find_write_target(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    if (node->type == AST_CALL
        && node->data.call.callee->type == AST_IDENTIFIER) {
        const char *name = node->data.call.callee->data.identifier.name;
        if (strcmp(name, "Write") == 0
            && node->data.call.arg_count >= 1
            && node->data.call.arguments[0]->type == AST_IDENTIFIER) {
            return node->data.call.arguments[0]->data.identifier.name;
        }
    }
    return NULL;
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

    /*
     * Write-write conflict detection across parallel tasks.
     * Simple O(n^2) scan over task pairs.
     */
    size_t n = parallel->data.parallel.task_count;

    for (size_t i = 0; i < n; i++) {
        const char *write_i =
            find_write_target(parallel->data.parallel.tasks[i]);
        if (write_i == NULL)
            continue;

        for (size_t j = i + 1; j < n; j++) {
            const char *write_j =
                find_write_target(parallel->data.parallel.tasks[j]);
            if (write_j == NULL)
                continue;

            if (strcmp(write_i, write_j) == 0) {
                semantic_error(sa->ctx, parallel,
                    "Write-write conflict in parallel block: "
                    "both tasks write to slot '%s'",
                    write_i);
            }
        }
    }

    /* Recurse into each task */
    for (size_t i = 0; i < n; i++)
        slot_analyze_block(parallel->data.parallel.tasks[i], sa);

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
