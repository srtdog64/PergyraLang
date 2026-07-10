/*
 * Copyright (c) 2026 Pergyra Language Project
 * HIR validation pass.
 */

#include "hir.h"

#include <stdio.h>

#include "../common/string_compat.h"

static char *
hir_validate_strdup_fmt(const char *fmt, const char *routine_name, size_t block_id)
{
    char buffer[512];
    snprintf(buffer,
             sizeof(buffer),
             fmt,
             routine_name != NULL ? routine_name : "(anonymous)",
             block_id);
    return pergyra_strdup(buffer);
}

static char *
hir_validate_strdup_edge_fmt(const char *fmt,
                             const char *routine_name,
                             size_t block_id,
                             size_t edge_id)
{
    char buffer[512];
    snprintf(buffer,
             sizeof(buffer),
             fmt,
             routine_name != NULL ? routine_name : "(anonymous)",
             block_id,
             edge_id);
    return pergyra_strdup(buffer);
}

static bool
hir_validate_successor(const HIRRoutine *routine,
                       size_t block_index,
                       const char *edge_name,
                       size_t successor,
                       char **error_message)
{
    if (successor < routine->cfg.block_count) {
        const HIRBasicBlock *target = &routine->cfg.blocks[successor];
        for (size_t i = 0; i < target->predecessor_count; i++) {
            if (target->predecessors != NULL && target->predecessors[i] == block_index)
                return true;
        }

        if (error_message != NULL) {
            char buffer[512];
            snprintf(buffer,
                     sizeof(buffer),
                     "HIR routine '%s' block[%zu] %s successor %zu is missing reciprocal predecessor",
                     routine->name != NULL ? routine->name : "(anonymous)",
                     block_index,
                     edge_name != NULL ? edge_name : "CFG",
                     successor);
            *error_message = pergyra_strdup(buffer);
        }
        return false;
    }

    if (error_message != NULL) {
        *error_message = hir_validate_strdup_edge_fmt(
            "HIR routine '%s' block[%zu] has out-of-range successor %zu",
            routine->name,
            block_index,
            successor);
    }
    return false;
}

static bool
hir_validate_predecessors(const HIRRoutine *routine,
                          const HIRBasicBlock *block,
                          size_t block_index,
                          char **error_message)
{
    if (block->predecessor_count == 0)
        return true;
    if (block->predecessors == NULL) {
        if (error_message != NULL) {
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' block[%zu] has predecessor count without predecessor array",
                routine->name,
                block_index);
        }
        return false;
    }
    if (block->predecessor_count > block->predecessor_capacity) {
        if (error_message != NULL) {
            *error_message = hir_validate_strdup_fmt(
                "HIR routine '%s' block[%zu] has predecessor count above predecessor capacity",
                routine->name,
                block_index);
        }
        return false;
    }
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] < routine->cfg.block_count)
            continue;
        if (error_message != NULL) {
            *error_message = hir_validate_strdup_edge_fmt(
                "HIR routine '%s' block[%zu] has out-of-range predecessor %zu",
                routine->name,
                block_index,
                block->predecessors[i]);
        }
        return false;
    }
    return true;
}

bool
hir_validate(const HIRProgram *hir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;

    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("HIR validation requires program");
        return false;
    }

    HIRRoutineInventory inventory;
    hir_routine_inventory_from_program(hir, &inventory);

    for (size_t i = 0; i < inventory.count; i++) {
        const HIRRoutine *routine = hir_routine_inventory_get(&inventory, i);
        if (routine == NULL) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("HIR validation has invalid routine inventory");
            return false;
        }
        if (i >= UINT32_MAX || routine->routine_id != (uint32_t)(i + 1)) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR routine inventory has non-canonical RoutineId");
            }
            return false;
        }
        if (routine->kind != HIR_TOPLEVEL_EXECUTABLE
            && routine->source_syntax_id == 0) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR source-backed routine is missing SyntaxNodeId");
            }
            return false;
        }
        for (size_t j = 0; j < i; j++) {
            const HIRRoutine *prior = hir_routine_inventory_get(
                &inventory, j);
            if (routine->source_syntax_id != 0 && prior != NULL
                && prior->source_syntax_id == routine->source_syntax_id) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup(
                        "HIR routines share one source SyntaxNodeId");
                }
                return false;
            }
        }
        if (routine->direct_call_count > 0
            && (routine->direct_calls == NULL
                || routine->direct_call_decl_ids == NULL)) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR direct-call facts are incomplete");
            }
            return false;
        }
        if (routine->callee_routine_count > 0
            && routine->callee_routine_ids == NULL) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR callgraph edges are missing RoutineId storage");
            }
            return false;
        }
        for (size_t j = 0; j < routine->callee_routine_count; j++) {
            uint32_t callee_id = routine->callee_routine_ids[j];
            const HIRRoutine *callee = callee_id > 0
                && callee_id <= inventory.count
                ? hir_routine_inventory_get(&inventory,
                    (size_t)callee_id - 1)
                : NULL;
            if (callee == NULL || callee->routine_id != callee_id) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup(
                        "HIR callgraph edge references an invalid RoutineId");
                }
                return false;
            }
        }
        if (!routine->has_cfg) {
            if (routine->cfg.blocks != NULL || routine->cfg.block_count != 0) {
                if (error_message != NULL) {
                    *error_message = pergyra_strdup(
                        "HIR routine has CFG blocks but is not marked as CFG-backed");
                }
                return false;
            }
            continue;
        }

        if (routine->cfg.blocks == NULL || routine->cfg.block_count == 0) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR CFG-backed routine has no CFG blocks");
            }
            return false;
        }
        if (routine->cfg.entry_block >= routine->cfg.block_count) {
            if (error_message != NULL) {
                *error_message = pergyra_strdup(
                    "HIR CFG-backed routine has invalid entry block");
            }
            return false;
        }

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            if (block->id != j) {
                if (error_message != NULL) {
                    *error_message = hir_validate_strdup_edge_fmt(
                        "HIR routine '%s' block[%zu] has mismatched block id %zu",
                        routine->name,
                        j,
                        block->id);
                }
                return false;
            }
            if (!hir_validate_predecessors(routine, block, j, error_message))
                return false;
        }

        for (size_t j = 0; j < routine->cfg.block_count; j++) {
            const HIRBasicBlock *block = &routine->cfg.blocks[j];
            if (block->has_succ_true
                && !hir_validate_successor(routine, j, "true",
                                           block->succ_true, error_message)) {
                return false;
            }
            if (block->has_succ_false
                && !hir_validate_successor(routine, j, "false",
                                           block->succ_false, error_message)) {
                return false;
            }

            if (!block->is_pin_region)
                continue;
            if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0') {
                if (error_message != NULL) {
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' pin-region block[%zu] missing pin source name",
                        routine->name,
                        j);
                }
                return false;
            }
            if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0') {
                if (error_message != NULL) {
                    *error_message = hir_validate_strdup_fmt(
                        "HIR routine '%s' pin-region block[%zu] missing pin view name",
                        routine->name,
                        j);
                }
                return false;
            }
        }
    }

    return true;
}
