#include "mir_dce.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    inst->return_expression_use_count = 0;
    inst->use_capacity = 0;
    if (inst->phi_incomings != NULL) {
        for (size_t i = 0; i < inst->phi_incoming_count; i++)
            free((void *)inst->phi_incomings[i].value_name);
    }
    free(inst->phi_incomings);
    inst->phi_incomings = NULL;
    inst->phi_incoming_count = 0;
    free((void *)inst->destructure_binding_names);
    inst->destructure_binding_names = NULL;
    inst->destructure_binding_count = 0;
    free(inst->source_inline_text);
    inst->source_inline_text = NULL;
    free(inst->lifecycle_receiver_name);
    inst->lifecycle_receiver_name = NULL;
    free(inst->lifecycle_op);
    inst->lifecycle_op = NULL;
    free(inst->lifecycle_subject);
    inst->lifecycle_subject = NULL;
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
mir_observe_value_dependency(const MIRRoutine *routine, const char *name,
                             bool *observed, size_t *queue, size_t *queued)
{
    int index = mir_find_value_summary(routine, name);
    /* Parameters and non-SSA source operands need no local definition. */
    if (index < 0 || observed[index])
        return true;
    if (*queued >= routine->value_summary_count)
        return false;
    observed[index] = true;
    queue[(*queued)++] = (size_t)index;
    return true;
}

static bool
mir_observed_phi_closure(const MIRRoutine *routine, bool **observed_out)
{
    size_t count = routine->value_summary_count;
    bool *observed = NULL;
    size_t *queue = NULL;
    size_t queued = 0;
    size_t cursor = 0;

    *observed_out = NULL;
    if (count == 0)
        return true;
    if (count > SIZE_MAX / sizeof(*queue))
        return false;
    observed = calloc(count, sizeof(*observed));
    queue = malloc(count * sizeof(*queue));
    if (observed == NULL || queue == NULL)
        goto fail;
    /* A phi cycle is not an observation. Seed retained instruction operands
     * (including explicit copy-out uses) and source boundaries, then follow owned uses.
     * DEF removal stays disabled while source-backed consumers remain. */
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->instruction_count > 0 && block->instructions == NULL)
            goto fail;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_PHI) {
                if (mir_instruction_has_source_location(inst)
                    && !mir_observe_value_dependency(routine, inst->result_name,
                        observed, queue, &queued))
                    goto fail;
                continue;
            }
            for (size_t ui = 0; ui < inst->use_count; ui++) {
                if (!mir_observe_value_dependency(routine, inst->uses[ui],
                        observed, queue, &queued))
                    goto fail;
            }
        }
    }
    while (cursor < queued) {
        const MIRValueSummary *value = &routine->value_summaries[queue[cursor++]];
        if (value->def_block >= routine->block_count)
            goto fail;
        const MIRBasicBlock *block = &routine->blocks[value->def_block];
        if (value->def_inst >= block->instruction_count)
            goto fail;
        const MIRInstruction *inst = &block->instructions[value->def_inst];
        if (inst->kind != MIR_INST_PHI)
            continue;
        for (size_t ui = 0; ui < inst->use_count; ui++) {
            if (!mir_observe_value_dependency(routine, inst->uses[ui],
                    observed, queue, &queued))
                goto fail;
        }
    }
    free(queue);
    *observed_out = observed;
    return true;
fail:
    free(queue);
    free(observed);
    return false;
}

static bool
mir_instruction_is_dead_value(const MIRRoutine *routine, const MIRInstruction *inst,
                              const bool *observed)
{
    int idx;

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
    if (idx < 0 || observed == NULL)
        return false;
    /* Source-backed PHIs remain conservatively preserved. Value-summary
     * provenance is now richer, but loop-carried seed values still are not
     * distinguished well enough to reopen dead local removal without changing
     * runtime behavior. */
    if (mir_instruction_has_source_location(inst))
        return false;
    return !observed[idx];
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
    return !mir_instruction_source_stmt_has_side_effect_hint(inst);
}

bool
mir_run_dce_on_routine(MIRRoutine *routine, bool *changed_out)
{
    bool changed = false;
    bool *observed = NULL;

    if (changed_out != NULL)
        *changed_out = false;
    if (routine == NULL)
        return false;
    if (!mir_observed_phi_closure(routine, &observed))
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        if (block->instruction_count > 0 && block->instructions == NULL) {
            free(observed);
            return false;
        }
        for (size_t inst_id = block->instruction_count; inst_id-- > 0;) {
            MIRInstruction *inst = &block->instructions[inst_id];
            if (mir_instruction_is_dead_value(routine, inst, observed)
                || mir_instruction_is_dead_stmt(inst)) {
                if (!mir_remove_instruction(block, inst_id)) {
                    free(observed);
                    return false;
                }
                routine->dce_removed_count++;
                changed = true;
            }
        }
    }

    free(observed);
    if (changed_out != NULL)
        *changed_out = changed;
    return true;
}
