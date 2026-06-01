/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR CFG control policy helpers.
 */

#include "transpiler_mir_cfg_policy.h"

#include "transpiler_type_mapping.h"

const char *
transpiler_mir_for_in_length_field(const char *collection_type)
{
    if (transpiler_type_name_is_array_or_slice(collection_type)) {
        return "length";
    }
    return "count";
}

const MIRInstruction *
transpiler_mir_find_incoming_for_in_branch(const MIRRoutine *routine,
                                           const MIRBasicBlock *block)
{
    const MIRInstruction *inst =
        transpiler_mir_find_incoming_loop_branch(routine, block);
    return inst != NULL && inst->branch_shape == MIR_BRANCH_FOR_IN
        ? inst
        : NULL;
}

const MIRInstruction *
transpiler_mir_find_incoming_loop_branch(const MIRRoutine *routine,
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
                && (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                    || inst->branch_shape == MIR_BRANCH_FOR_IN)) {
                return inst;
            }
        }
    }
    return NULL;
}

const MIRInstruction *
transpiler_mir_find_backedge_loop_branch(const MIRRoutine *routine,
                                         const MIRBasicBlock *block)
{
    const MIRBasicBlock *target;

    if (routine == NULL || block == NULL)
        return NULL;
    if (!block->has_succ_true || block->succ_true >= routine->block_count)
        return NULL;
    if (block->id <= block->succ_true)
        return NULL;
    target = &routine->blocks[block->succ_true];
    if (target == block)
        return NULL;
    return transpiler_mir_find_loop_branch_inst(target);
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
