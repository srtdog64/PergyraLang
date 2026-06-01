/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR match-region reachability.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_match_region.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const MIRInstruction *
llvm_mir_find_incoming_match_branch(const MIRRoutine *routine,
                                    const MIRBasicBlock *block)
{
    size_t target_id = SIZE_MAX;

    if (routine == NULL || block == NULL)
        return NULL;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (&routine->blocks[i] == block) {
            target_id = i;
            break;
        }
    }
    if (target_id == SIZE_MAX)
        return NULL;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *pred = &routine->blocks[i];
        if (!pred->has_succ_true || pred->succ_true != target_id)
            continue;
        for (size_t j = 0; j < pred->instruction_count; j++) {
            const MIRInstruction *inst = &pred->instructions[j];
            if (inst->kind == MIR_INST_BRANCH
                && inst->branch_shape == MIR_BRANCH_MATCH_CASE) {
                return inst;
            }
        }
    }
    return NULL;
}

static bool
llvm_mir_block_reaches(const MIRRoutine *routine,
                       size_t from_id,
                       size_t target_id,
                       bool *visited)
{
    const MIRBasicBlock *block;

    if (routine == NULL || visited == NULL)
        return false;
    if (from_id >= routine->block_count || target_id >= routine->block_count)
        return false;
    if (from_id == target_id)
        return true;
    if (visited[from_id])
        return false;
    visited[from_id] = true;

    block = &routine->blocks[from_id];
    if (block->has_succ_true
        && llvm_mir_block_reaches(routine, block->succ_true,
                                  target_id, visited)) {
        return true;
    }
    if (block->has_succ_false
        && llvm_mir_block_reaches(routine, block->succ_false,
                                  target_id, visited)) {
        return true;
    }
    return false;
}

bool
llvm_mir_case_true_region_contains(const MIRRoutine *routine,
                                   const MIRBasicBlock *case_block,
                                   size_t target_id)
{
    bool *visited;
    bool reaches_true;
    bool reaches_false;

    if (routine == NULL || case_block == NULL)
        return false;
    if (!case_block->has_succ_true || !case_block->has_succ_false)
        return false;
    if (target_id >= routine->block_count)
        return false;

    visited = calloc(routine->block_count, sizeof(bool));
    if (visited == NULL)
        return false;
    reaches_true = llvm_mir_block_reaches(routine, case_block->succ_true,
                                          target_id, visited);
    memset(visited, 0, routine->block_count * sizeof(bool));
    reaches_false = llvm_mir_block_reaches(routine, case_block->succ_false,
                                           target_id, visited);
    free(visited);
    return reaches_true && !reaches_false;
}

#endif /* PGY_LLVM_ENABLED */
