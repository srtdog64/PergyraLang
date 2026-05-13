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

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
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
