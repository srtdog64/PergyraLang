#include "mir_dce.h"

#include <stdlib.h>
#include <string.h>

#include "mir_cfg_contract_control.h"
#include "mir_liveness_dce.h"

static bool
mir_free_instruction_payload(MIRInstruction *inst)
{
    if (inst == NULL)
        return true;
    free((void *)inst->result_name);
    inst->result_name = NULL;
    for (size_t i = 0; i < inst->use_count; i++)
        free((void *)inst->uses[i]);
    free((void *)inst->uses);
    inst->uses = NULL;
    inst->use_count = 0;
    inst->use_capacity = 0;
    if (inst->phi_incomings != NULL) {
        for (size_t i = 0; i < inst->phi_incoming_count; i++)
            free((void *)inst->phi_incomings[i].value_name);
    }
    free(inst->phi_incomings);
    inst->phi_incomings = NULL;
    inst->phi_incoming_count = 0;
    return true;
}

void
mir_reset_routine_analysis(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    routine->live_value_count = 0;
    routine->has_liveness = false;
    routine->has_use_def_summary = false;
    for (size_t i = 0; i < routine->value_summary_count; i++)
        free((void *)routine->value_summaries[i].name);
    free(routine->value_summaries);
    routine->value_summaries = NULL;
    routine->value_summary_count = 0;

    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        mir_clear_block_name_set(&block->def_names, &block->def_name_count, &block->def_name_capacity);
        mir_clear_block_name_set(&block->use_names, &block->use_name_count, &block->use_name_capacity);
        mir_clear_block_name_set(&block->live_in_names,
                                 &block->live_in_name_count,
                                 &block->live_in_name_capacity);
        mir_clear_block_name_set(&block->live_out_names,
                                 &block->live_out_name_count,
                                 &block->live_out_name_capacity);
    }
}

bool
mir_recompute_analysis(MIRRoutine *routine)
{
    mir_reset_routine_analysis(routine);
    return mir_compute_liveness(routine);
}

static bool
mir_remove_instruction(MIRBasicBlock *block, size_t index)
{
    if (block == NULL || index >= block->instruction_count)
        return false;
    mir_free_instruction_payload(&block->instructions[index]);
    if (index + 1 < block->instruction_count) {
        memmove(&block->instructions[index],
                &block->instructions[index + 1],
                (block->instruction_count - index - 1) * sizeof(MIRInstruction));
    }
    block->instruction_count--;
    if (block->instruction_count == 0) {
        free(block->instructions);
        block->instructions = NULL;
    } else {
        MIRInstruction *shrunk = realloc(block->instructions,
                                         block->instruction_count * sizeof(MIRInstruction));
        if (shrunk != NULL)
            block->instructions = shrunk;
    }
    return true;
}

static bool
mir_instruction_is_dead_value(const MIRRoutine *routine, const MIRInstruction *inst)
{
    int idx;
    const MIRValueSummary *summary;

    if (routine == NULL || inst == NULL || inst->result_name == NULL)
        return false;
    if (inst->kind == MIR_INST_DEF)
        /* DEF instructions can carry source-local initializers whose later
         * uses still flow through AST-backed STMTs during the beta MIR bridge.
         * Removing them is not semantics-preserving until every body consumer
         * uses complete MIR use edges. */
        return false;
    if (inst->kind != MIR_INST_PHI)
        return false;
    idx = mir_find_value_summary(routine, inst->result_name);
    if (idx < 0)
        return false;
    summary = &routine->value_summaries[idx];
    /* AST-backed PHIs remain conservatively preserved. Value-summary
     * provenance is now richer, but loop-carried seed values still are not
     * distinguished well enough to reopen dead local removal without changing
     * runtime behavior. */
    if (mir_instruction_source_payload(inst) != NULL)
        return false;
    return summary->use_count == 0
           && summary->live_in_block_count == 0
           && summary->live_out_block_count == 0
           && !summary->reaches_cleanup;
}

static bool
mir_stmt_is_semantic_carrier(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT || inst->name == NULL)
        return false;

    return mir_instruction_is_intent_semantic_carrier(inst);
}

static bool
mir_instruction_is_dead_stmt(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT)
        return false;
    if (mir_stmt_is_semantic_carrier(inst))
        return false;
    if (mir_instruction_has_source_location(inst))
        return !mir_instruction_source_stmt_has_side_effect_hint(inst);
    if (mir_instruction_source_payload(inst) == NULL)
        return true;
    return !mir_source_ast_stmt_has_side_effect_hint(
        mir_instruction_source_payload(inst));
}

bool
mir_run_dce_on_routine(MIRRoutine *routine, bool *changed_out)
{
    bool changed = false;

    if (changed_out != NULL)
        *changed_out = false;
    if (routine == NULL)
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        if (block->instruction_count > 0 && block->instructions == NULL)
            return false;
        for (size_t inst_id = block->instruction_count; inst_id-- > 0;) {
            MIRInstruction *inst = &block->instructions[inst_id];
            if (mir_instruction_is_dead_value(routine, inst)
                || mir_instruction_is_dead_stmt(inst)) {
                if (!mir_remove_instruction(block, inst_id))
                    return false;
                routine->dce_removed_count++;
                changed = true;
            }
        }
    }

    if (changed_out != NULL)
        *changed_out = changed;
    return true;
}
