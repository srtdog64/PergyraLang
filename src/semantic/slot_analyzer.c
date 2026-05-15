/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot Resource-Boundary Analyzer implementation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "slot_analyzer_internal.h"
#include "type_checker.h"
#include "diag_codes.h"

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
    size_t count = 0;
    size_t capacity = 0;
    Symbol **result = NULL;

    if (out_count == NULL)
        return NULL;
    *out_count = 0;

    for (Scope *s = scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->symbol_count; i++) {
            Symbol *sym = s->symbols[i];
            Symbol **grown;
            size_t new_capacity;

            if (sym == NULL || sym->kind != SYMBOL_SLOT
                || sym->slot_info.state != SLOT_STATE_CLAIMED) {
                continue;
            }

            if (count >= capacity) {
                if (capacity == 0) {
                    new_capacity = 8;
                } else {
                    if (capacity > SIZE_MAX / 2) {
                        free(result);
                        return NULL;
                    }
                    new_capacity = capacity * 2;
                }
                if (new_capacity > SIZE_MAX / sizeof(Symbol *)) {
                    free(result);
                    return NULL;
                }
                grown = realloc(result, new_capacity * sizeof(Symbol *));
                if (grown == NULL) {
                    free(result);
                    return NULL;
                }
                result = grown;
                capacity = new_capacity;
            }
            result[count++] = sym;
        }
    }

    *out_count = count;
    return result;
}

static int
symbol_ptr_compare(const void *lhs, const void *rhs)
{
    uintptr_t a = (uintptr_t)*(Symbol *const *)lhs;
    uintptr_t b = (uintptr_t)*(Symbol *const *)rhs;

    return (a > b) - (a < b);
}

static void
symbol_ptr_array_sort(Symbol **items, size_t count)
{
    if (items == NULL || count < 2)
        return;
    qsort(items, count, sizeof(Symbol *), symbol_ptr_compare);
}

static bool
symbol_ptr_array_contains(Symbol *const *items, size_t count, Symbol *needle)
{
    Symbol *const *found;

    if (items == NULL || count == 0 || needle == NULL)
        return false;
    found = bsearch(&needle, items, count, sizeof(Symbol *),
                    symbol_ptr_compare);
    return found != NULL;
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
        for (size_t i = 0; i < ast_block_statement_count(block); i++) {
            ASTNode *stmt = ast_block_statement(block, i);
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

    ASTNode *body = ast_func_body(func);
    if (body == NULL)
        return true;

    /*
     * Snapshot live slots before entering body.
     * After the body we check that any slot claimed inside
     * has been released (warning L4).
     */
    size_t   live_count  = 0;
    Symbol **live_before = collect_live_slots(sa->ctx->scope, &live_count);
    symbol_ptr_array_sort(live_before, live_count);

    slot_analyze_block(body, sa);

    /* L4: warn about slots that were claimed but not released */
    size_t   after_count  = 0;
    Symbol **live_after   = collect_live_slots(sa->ctx->scope, &after_count);
    SlotEscapeEntry *escapes = NULL;
    size_t escape_count = 0;
    size_t escape_capacity = 0;

    collect_slot_escapes(body, &escapes, &escape_count, &escape_capacity,
        sa->program_root, 0);

    for (size_t i = 0; i < after_count; i++) {
        Symbol *sym = live_after[i];
        if (!symbol_ptr_array_contains(live_before, live_count, sym)) {
            /* This slot was claimed inside the function and not released */
            ASTNode dummy = {0};
            dummy.line   = sym->decl_line;
            dummy.column = sym->decl_col;
            semantic_warning(sa->ctx, &dummy,
                "Slot '%s' claimed at line %u may not be released before function returns.\n"
                "Reason:\n"
                "- slot '%s' was introduced inside this function body\n"
                "- the analyzer still sees it as live at function exit\n"
                "Fix:\n"
                "- release the slot on every exit path\n"
                "- or transfer ownership explicitly before returning",
                sym->name, sym->decl_line, sym->name);
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
                "Slot '%s' escapes via return; non-escaping stack/local optimization is disabled.\n"
                "Reason:\n"
                "- slot '%s' leaves the current function through a return path\n"
                "- escape analysis can no longer prove the handle stays local\n"
                "Fix:\n"
                "- keep the slot local to this function\n"
                "- or accept heap/runtime-backed handling for this path",
                sym->name, sym->name);
        }
        if ((escapes[i].mask & SLOT_ESCAPE_CALL) != 0) {
            semantic_warning(sa->ctx, &dummy,
                "Slot '%s' may escape through helper/function call; escape analysis stays conservative.\n"
                "Reason:\n"
                "- slot '%s' is forwarded into another call boundary\n"
                "- the current helper-call escape analysis cannot prove that the callee keeps it local\n"
                "Fix:\n"
                "- keep the slot in the current routine\n"
                "- or accept conservative non-local handling for this call path",
                sym->name, sym->name);
        }
        if ((escapes[i].mask & SLOT_ESCAPE_CHANNEL) != 0) {
            semantic_warning(sa->ctx, &dummy,
                "Slot '%s' escapes through channel send; stack/local sinking is disabled.\n"
                "Reason:\n"
                "- slot '%s' crosses an asynchronous/message boundary through channel send\n"
                "- after that handoff the analyzer cannot treat it as function-local state\n"
                "Fix:\n"
                "- keep the slot local instead of sending it\n"
                "- or accept non-local ownership/runtime handling for the channel path",
                sym->name, sym->name);
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
    if (with == NULL || ast_with_body(with) == NULL)
        return true;

    return slot_analyze_block(ast_with_body(with), sa);
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
    symbol_ptr_array_sort(snap, snap_count);

    /* Then-branch */
    slot_analyze_block(ast_if_then_branch(ifstmt), sa);

    size_t   after_then_count = 0;
    Symbol **after_then = collect_live_slots(sa->ctx->scope, &after_then_count);
    symbol_ptr_array_sort(after_then, after_then_count);

    /* Else-branch (if present) */
    if (ast_if_else_branch(ifstmt) != NULL) {
        /*
         * Restore snapshot states so else-branch sees the original
         * state ??but we can't easily restore without a deep copy.
         * For now we detect divergence by comparing released sets.
         */
        slot_analyze_block(ast_if_else_branch(ifstmt), sa);

        size_t   after_else_count = 0;
        Symbol **after_else = collect_live_slots(sa->ctx->scope,
                                                 &after_else_count);
        symbol_ptr_array_sort(after_else, after_else_count);

        /*
         * L3 check: a slot that was live before but is now released
         * in one branch and still live in the other.
         */
        for (size_t i = 0; i < snap_count; i++) {
            Symbol *sym = snap[i];

            bool released_in_then =
                !symbol_ptr_array_contains(after_then, after_then_count, sym);
            bool released_in_else =
                !symbol_ptr_array_contains(after_else, after_else_count, sym);

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

    size_t n = ast_parallel_task_count(parallel);
    /* Outer arrays are pass-local scratch: populated, read, and discarded
     * before the function returns.  The per-task inner arrays are still
     * heap-owned by collect_slot_accesses and freed explicitly below. */
    PgyArena *scratch = &sa->ctx->scratch_arena;
    if (n > SIZE_MAX / sizeof(SlotAccessEntry *)
        || n > SIZE_MAX / sizeof(size_t))
        return false;
    SlotAccessEntry **task_accesses =
        pgy_arena_calloc(scratch, n * sizeof(SlotAccessEntry *));
    size_t *task_counts = pgy_arena_calloc(scratch, n * sizeof(size_t));
    size_t *task_caps   = pgy_arena_calloc(scratch, n * sizeof(size_t));

    if (task_accesses == NULL || task_counts == NULL || task_caps == NULL)
        return false;

    for (size_t i = 0; i < n; i++)
        collect_slot_accesses(ast_parallel_task(parallel, i),
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
                        semantic_error_code(sa->ctx, PGY_CODE_SEM_PARALLEL_SLOT_CONFLICT, parallel,
                            "Parallel context slot conflict on '%s': multiple tasks mutate or release the same slot",
                            task_accesses[i][ai].name);
                    } else if ((left_mut && right_read) || (right_mut && left_read)) {
                        semantic_warning_code(sa->ctx, PGY_CODE_SEM_PARALLEL_SLOT_RACE_RISK, parallel,
                            "Parallel context race risk on '%s': one task reads while another mutates or releases the same slot",
                            task_accesses[i][ai].name);
                    }
                }
            }
        }
    }

    /* Recurse into each task */
    for (size_t i = 0; i < n; i++)
        slot_analyze_block(ast_parallel_task(parallel, i), sa);

    for (size_t i = 0; i < n; i++)
        free(task_accesses[i]);
    /* outer arrays (task_accesses, task_counts, task_caps) are arena-owned */

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

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *stmt = ast_program_statement(program, i);
        if (stmt->type == AST_FUNC_DECL)
            slot_analyze_func_body(stmt, sa);
    }

    return !sa->ctx->has_error;
}

unsigned
slot_analyze_escape_flags(ASTNode *node, const char *slot_name)
{
    return slot_analyze_escape_flags_in_program(node, slot_name, NULL);
}

unsigned
slot_analyze_escape_flags_in_program(ASTNode *node, const char *slot_name,
                                     ASTNode *program_root)
{
    return slot_escape_mask_in_program(node, slot_name, program_root, 0);
}

unsigned
slot_analyze_param_summary_in_program(ASTNode *node, const char *slot_name,
                                      ASTNode *program_root)
{
    return slot_param_summary_in_program(node, slot_name, program_root, 0);
}
