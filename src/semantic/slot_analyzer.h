/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot Lifetime Analyzer
 * Pergyra-specific pass: verifies Slot state machine transitions
 * and reports release errors / leaks.
 */

#ifndef PERGYRA_SLOT_ANALYZER_H
#define PERGYRA_SLOT_ANALYZER_H

#include <stdbool.h>
#include "../parser/ast.h"
#include "../semantic/type_checker.h"
#include "../semantic/symbol_table.h"

/*
 * SlotLifetimeEntry — tracks one slot across all paths
 */
typedef struct
{
    char*     slot_name;
    SlotState state_on_true_path;   /* State after if-branch  */
    SlotState state_on_false_path;  /* State after else-branch */
    uint32_t  decl_line;
} SlotLifetimeEntry;

/*
 * SlotAnalyzer — runs after type checking
 */
typedef struct
{
    SemanticContext*   ctx;           /* Shared context for error emit */
    SlotLifetimeEntry* entries;
    size_t             entry_count;
    size_t             entry_capacity;
    ASTNode*           program_root;
} SlotAnalyzer;

/* -----------------------------------------------------------------
 * Lifecycle
 * ----------------------------------------------------------------- */

SlotAnalyzer* slot_analyzer_create(SemanticContext* ctx);
void          slot_analyzer_destroy(SlotAnalyzer* sa);

/* -----------------------------------------------------------------
 * Analysis entry point
 *
 * Walks the entire annotated AST and verifies:
 *   L1: No access after Release
 *   L2: with-block slots are released at block exit
 *   L3: Slots released on all branches or no branches
 *       (partial release → warning)
 *   L4: Unreleased slots at function exit → warning
 *       (with-block slots are exempt: auto-released)
 * ----------------------------------------------------------------- */

bool slot_analyze_program(ASTNode* program, SlotAnalyzer* sa);

/* -----------------------------------------------------------------
 * Per-node analysis (called recursively)
 * ----------------------------------------------------------------- */

bool slot_analyze_block(ASTNode* block, SlotAnalyzer* sa);
bool slot_analyze_func_body(ASTNode* func, SlotAnalyzer* sa);
bool slot_analyze_with_stmt(ASTNode* with, SlotAnalyzer* sa);

/*
 * If-else analysis:
 * Runs analysis on both branches separately, then merges states.
 * If a slot is Released in one branch but not the other → warning L3.
 */
bool slot_analyze_if_stmt(ASTNode* ifstmt, SlotAnalyzer* sa);

/*
 * Parallel context analysis:
 * Checks that no two tasks in the same parallel context reference
 * the same slot in conflicting positions.
 * Write-write conflict → error.
 * Read-read is fine.
 * Write-read conflict → warning (race risk).
 */
bool slot_analyze_parallel_block(ASTNode* parallel, SlotAnalyzer* sa);

#endif /* PERGYRA_SLOT_ANALYZER_H */
