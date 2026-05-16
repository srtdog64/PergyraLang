/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR CFG control policy helpers.
 */

#include "transpiler_mir_cfg_policy.h"

#include <string.h>

const char *
transpiler_mir_for_in_length_field(const char *collection_type)
{
    if (collection_type != NULL
        && (strncmp(collection_type, "Array<", 6) == 0
            || strncmp(collection_type, "Slice<", 6) == 0)) {
        return "length";
    }
    return "count";
}

const MIRInstruction *
transpiler_mir_find_incoming_for_in_branch(const MIRRoutine *routine,
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
                && inst->branch_shape == MIR_BRANCH_FOR_IN) {
                return inst;
            }
        }
    }
    return NULL;
}

const MIRInstruction *
transpiler_mir_find_loop_branch_inst(const MIRBasicBlock *block)
{
    if (block == NULL)
        return NULL;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_BRANCH
            && (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                || inst->branch_shape == MIR_BRANCH_FOR_IN)) {
            return inst;
        }
    }
    return NULL;
}
