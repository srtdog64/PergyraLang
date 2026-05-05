#include "mir_liveness_dce.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static const char *
mir_liveness_summary_slot_anchor(const MIRInstruction *inst)
{
    if (inst == NULL)
        return NULL;
    if (inst->slot_anchor != NULL)
        return inst->slot_anchor;
    if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI)
        && inst->name != NULL) {
        return inst->name;
    }
    return inst->arg0;
}

static bool
mir_append_value_summary(MIRRoutine *routine,
                         const char *name,
                         size_t def_block,
                         size_t def_inst)
{
    MIRValueSummary summary;

    if (routine == NULL || name == NULL)
        return false;
    if (mir_find_value_summary(routine, name) >= 0)
        return true;

    memset(&summary, 0, sizeof(summary));
    summary.name = pergyra_strdup(name);
    if (summary.name == NULL)
        return false;
    summary.slot_anchor = NULL;
    summary.def_block = def_block;
    summary.def_inst = def_inst;
    summary.first_use_block = SIZE_MAX;
    summary.last_use_block = SIZE_MAX;
    summary.ast_write_count = 0;
    summary.used_outside_def_block = false;
    summary.used_by_phi = false;
    summary.crosses_block_boundary = false;
    summary.has_ast_reassignment = false;

    if (routine->value_summary_count == routine->value_summary_capacity) {
        size_t next_capacity =
            routine->value_summary_capacity == 0 ? 8 : routine->value_summary_capacity * 2;
        MIRValueSummary *grown =
            realloc(routine->value_summaries, next_capacity * sizeof(MIRValueSummary));
        if (grown == NULL) {
            free((void *)summary.name);
            return false;
        }
        routine->value_summaries = grown;
        routine->value_summary_capacity = next_capacity;
    }
    routine->value_summaries[routine->value_summary_count++] = summary;
    return true;
}

bool
mir_build_value_summaries(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->value_summary_count; i++)
        free((void *)routine->value_summaries[i].name);
    free(routine->value_summaries);
    routine->value_summaries = NULL;
    routine->value_summary_count = 0;
    routine->value_summary_capacity = 0;
    routine->has_use_def_summary = false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            if (inst->result_name != NULL) {
                if (!mir_append_value_summary(routine, inst->result_name, block_id, inst_id))
                    return false;
                {
                    int idx = mir_find_value_summary(routine, inst->result_name);
                    if (idx >= 0 && routine->value_summaries[idx].slot_anchor == NULL)
                        routine->value_summaries[idx].slot_anchor =
                            mir_liveness_summary_slot_anchor(inst);
                }
            }
        }
    }

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];

        for (size_t i = 0; i < block->live_in_name_count; i++) {
            int idx = mir_find_value_summary(routine, block->live_in_names[i]);
            if (idx >= 0)
                routine->value_summaries[idx].live_in_block_count++;
        }
        for (size_t i = 0; i < block->live_out_name_count; i++) {
            int idx = mir_find_value_summary(routine, block->live_out_names[i]);
            if (idx >= 0)
                routine->value_summaries[idx].live_out_block_count++;
        }
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            for (size_t use_i = 0; use_i < inst->use_count; use_i++) {
                int idx = mir_find_value_summary(routine, inst->uses[use_i]);
                MIRValueSummary *summary;
                if (idx < 0)
                    continue;
                summary = &routine->value_summaries[idx];
                summary->use_count++;
                if (summary->first_use_block == SIZE_MAX)
                    summary->first_use_block = block_id;
                summary->last_use_block = block_id;
                if (block_id != summary->def_block) {
                    summary->used_outside_def_block = true;
                    summary->crosses_block_boundary = true;
                }
                if (inst->kind == MIR_INST_PHI) {
                    summary->used_by_phi = true;
                    summary->crosses_block_boundary = true;
                }
                if (block->is_cleanup)
                    summary->reaches_cleanup = true;
            }
        }
    }

    for (size_t i = 0; i < routine->value_summary_count; i++) {
        MIRValueSummary *summary = &routine->value_summaries[i];
        if (summary->live_in_block_count > 0
            || summary->live_out_block_count > 0
            || summary->used_outside_def_block
            || summary->used_by_phi) {
            summary->crosses_block_boundary = true;
        }
    }

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            const char *write_name;
            if (inst->kind != MIR_INST_DEF)
                continue;
            write_name = mir_liveness_summary_slot_anchor(inst);
            if (write_name == NULL)
                continue;
            for (size_t summary_id = 0; summary_id < routine->value_summary_count; summary_id++) {
                MIRValueSummary *summary = &routine->value_summaries[summary_id];
                if (summary->slot_anchor == NULL)
                    continue;
                if (strcmp(summary->slot_anchor, write_name) != 0)
                    continue;
                summary->ast_write_count++;
                if (summary->ast_write_count > 1)
                    summary->has_ast_reassignment = true;
            }
        }
    }

    routine->has_use_def_summary = true;
    return true;
}
