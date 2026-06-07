#include "mir_fact_validate.h"
#include "mir_fact_validate_internal.h"

bool
mir_validate_terminator_provenance(const MIRRoutine *routine,
                                   const MIRBasicBlock *block,
                                   size_t block_index,
                                   char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    if (!mir_validate_instruction_inventory_shape(routine,
                                                  block,
                                                  block_index,
                                                  "terminator provenance validation",
                                                  error_message))
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_BRANCH && inst->kind != MIR_INST_RETURN)
            continue;
        if (!mir_instruction_has_source_terminator_kind(inst)) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has CFG terminator without HIR source terminator kind",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && !mir_instruction_source_terminator_matches(
                inst, HIR_BLOCK_BRANCH)) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] branch source terminator is not HIR_BLOCK_BRANCH",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_RETURN
            && !mir_instruction_source_terminator_matches(
                inst, HIR_BLOCK_RETURN)) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] return source terminator is not HIR_BLOCK_RETURN",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && inst->branch_shape == MIR_BRANCH_EXPR
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] branch is missing MIR terminator expression fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && inst->branch_shape == MIR_BRANCH_MATCH_CASE
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] match-case branch is missing MIR match subject fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && mir_instruction_branch_requires_source_emit(inst)
            && !inst->requires_source_branch_emit) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] branch is missing source-branch emit fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->requires_source_branch_emit
            && (inst->kind != MIR_INST_BRANCH
                || mir_instruction_source_payload(inst) == NULL
                || !mir_instruction_branch_requires_source_emit(inst)
                || !mir_instruction_source_branch_payload_matches_shape(inst))) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-branch emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_BRANCH
            && mir_instruction_branch_requires_source_emit(inst)
            && (mir_instruction_source_payload(inst) == NULL
                || !mir_instruction_source_branch_payload_matches_shape(inst))) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source-branch emit fact is invalid",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
        if (inst->kind == MIR_INST_RETURN
            && mir_instruction_source_terminator_has_value(inst)
            && inst->expr0 == NULL) {
            if (error_message != NULL) {
                *error_message = mir_fact_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] return is missing MIR terminator expression fact",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
    }

    return true;
}
