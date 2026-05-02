#ifndef PERGYRA_MIR_LIVENESS_DCE_H
#define PERGYRA_MIR_LIVENESS_DCE_H

static bool
mir_append_block_set(const char ***names, size_t *count, size_t *capacity, const char *name)
{
    if (name == NULL)
        return true;
    return append_name_unique(names, count, capacity, name);
}

static bool
mir_collect_block_defs_uses(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;

    for (size_t i = 0; i < routine->block_count; i++) {
        MIRBasicBlock *block = &routine->blocks[i];
        for (size_t j = 0; j < block->instruction_count; j++) {
            MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_PHI) {
                if (inst->result_name != NULL) {
                    if (!mir_append_block_set(&block->def_names,
                                              &block->def_name_count,
                                              &block->def_name_capacity,
                                              inst->result_name))
                        return false;
                }
                continue;
            }
            for (size_t k = 0; k < inst->use_count; k++) {
                const char *use = inst->uses[k];
                if (!mir_name_set_contains(block->def_names, block->def_name_count, use)) {
                    if (!mir_append_block_set(&block->use_names,
                                              &block->use_name_count,
                                              &block->use_name_capacity,
                                              use))
                        return false;
                }
            }
            if (inst->result_name != NULL) {
                if (!mir_append_block_set(&block->def_names,
                                          &block->def_name_count,
                                          &block->def_name_capacity,
                                          inst->result_name))
                    return false;
            }
        }
    }

    return true;
}

static bool
mir_collect_successor_live_in(const MIRRoutine *routine,
                              size_t predecessor_block,
                              const MIRBasicBlock *block,
                              const char ***names,
                              size_t *count,
                              size_t *capacity)
{
    size_t succs[5];
    size_t succ_count = 0;

    if (routine == NULL || block == NULL || names == NULL || count == NULL || capacity == NULL)
        return false;

    if (block->has_succ_true)
        succs[succ_count++] = block->succ_true;
    if (block->has_succ_false)
        succs[succ_count++] = block->succ_false;
    if (block->has_cleanup_succ)
        succs[succ_count++] = block->cleanup_succ;
    if (block->has_rollback_succ)
        succs[succ_count++] = block->rollback_succ;
    if (block->has_invalidation_succ)
        succs[succ_count++] = block->invalidation_succ;

    for (size_t i = 0; i < succ_count; i++) {
        size_t succ = succs[i];
        if (succ >= routine->block_count)
            continue;
        for (size_t j = 0; j < routine->blocks[succ].live_in_name_count; j++) {
            if (!mir_append_block_set(names,
                                      count,
                                      capacity,
                                      routine->blocks[succ].live_in_names[j]))
                return false;
        }
        for (size_t j = 0; j < routine->blocks[succ].instruction_count; j++) {
            const MIRInstruction *inst = &routine->blocks[succ].instructions[j];
            if (inst->kind != MIR_INST_PHI)
                continue;
            for (size_t k = 0; k < inst->phi_incoming_count; k++) {
                if (inst->phi_incomings[k].predecessor_block == predecessor_block) {
                    if (!mir_append_block_set(names,
                                              count,
                                              capacity,
                                              inst->phi_incomings[k].value_name))
                        return false;
                }
            }
        }
    }

    return true;
}

static bool
mir_postorder_visit(const MIRRoutine *routine,
                    size_t block_index,
                    bool *visited,
                    size_t *order,
                    size_t *order_count)
{
    const MIRBasicBlock *block;
    size_t succs[5];
    size_t succ_count = 0;

    if (routine == NULL || visited == NULL || order == NULL || order_count == NULL)
        return false;
    if (block_index >= routine->block_count)
        return true;
    if (visited[block_index])
        return true;

    visited[block_index] = true;
    block = &routine->blocks[block_index];

    if (block->has_succ_true)
        succs[succ_count++] = block->succ_true;
    if (block->has_succ_false)
        succs[succ_count++] = block->succ_false;
    if (block->has_cleanup_succ)
        succs[succ_count++] = block->cleanup_succ;
    if (block->has_rollback_succ)
        succs[succ_count++] = block->rollback_succ;
    if (block->has_invalidation_succ)
        succs[succ_count++] = block->invalidation_succ;

    for (size_t i = 0; i < succ_count; i++) {
        if (!mir_postorder_visit(routine, succs[i], visited, order, order_count))
            return false;
    }

    order[(*order_count)++] = block_index;
    return true;
}

static bool
mir_build_liveness_postorder(const MIRRoutine *routine, size_t **order_out, size_t *count_out)
{
    bool *visited = NULL;
    size_t *order = NULL;
    size_t order_count = 0;

    if (order_out != NULL)
        *order_out = NULL;
    if (count_out != NULL)
        *count_out = 0;
    if (routine == NULL || order_out == NULL || count_out == NULL)
        return false;
    if (routine->block_count == 0)
        return true;

    visited = calloc(routine->block_count, sizeof(bool));
    order = calloc(routine->block_count, sizeof(size_t));
    if (visited == NULL || order == NULL) {
        free(visited);
        free(order);
        return false;
    }

    if (!mir_postorder_visit(routine, routine->entry_block, visited, order, &order_count)) {
        free(visited);
        free(order);
        return false;
    }

    for (size_t i = 0; i < routine->block_count; i++) {
        if (!visited[i] && !mir_postorder_visit(routine, i, visited, order, &order_count)) {
            free(visited);
            free(order);
            return false;
        }
    }

    free(visited);
    *order_out = order;
    *count_out = order_count;
    return true;
}

static void
mir_clear_block_name_set(const char ***names, size_t *count, size_t *capacity)
{
    free((void *)*names);
    *names = NULL;
    *count = 0;
    if (capacity != NULL)
        *capacity = 0;
}

static int
mir_find_value_summary(const MIRRoutine *routine, const char *name)
{
    if (routine == NULL || name == NULL)
        return -1;
    for (size_t i = 0; i < routine->value_summary_count; i++) {
        if (routine->value_summaries[i].name != NULL
            && strcmp(routine->value_summaries[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
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

static bool
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
                        routine->value_summaries[idx].slot_anchor = mir_instruction_slot_anchor(inst);
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
            write_name = mir_instruction_slot_anchor(inst);
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

#include "mir_dce.h"

static bool
mir_compute_liveness(MIRRoutine *routine)
{
    bool changed;
    size_t *order = NULL;
    size_t order_count = 0;

    if (routine == NULL)
        return false;
    if (!mir_collect_block_defs_uses(routine))
        return false;
    if (!mir_build_liveness_postorder(routine, &order, &order_count))
        return false;

    do {
        changed = false;
        for (size_t order_index = 0; order_index < order_count; order_index++) {
            size_t idx = order[order_index];
            MIRBasicBlock *block = &routine->blocks[idx];
            const char **new_live_out = NULL;
            size_t new_live_out_count = 0;
            size_t new_live_out_capacity = 0;
            const char **new_live_in = NULL;
            size_t new_live_in_count = 0;
            size_t new_live_in_capacity = 0;
            bool same_live_out = true;
            bool same_live_in = true;

            if (!mir_collect_successor_live_in(routine,
                                               idx,
                                               block,
                                               &new_live_out,
                                               &new_live_out_count,
                                               &new_live_out_capacity)) {
                free((void *)new_live_out);
                free((void *)new_live_in);
                return false;
            }

            for (size_t i = 0; i < block->use_name_count; i++) {
                if (!mir_append_block_set(&new_live_in,
                                          &new_live_in_count,
                                          &new_live_in_capacity,
                                          block->use_names[i])) {
                    free((void *)new_live_out);
                    free((void *)new_live_in);
                    return false;
                }
            }
            for (size_t i = 0; i < new_live_out_count; i++) {
                const char *name = new_live_out[i];
                if (!mir_name_set_contains(block->def_names, block->def_name_count, name)) {
                    if (!mir_append_block_set(&new_live_in,
                                              &new_live_in_count,
                                              &new_live_in_capacity,
                                              name)) {
                        free((void *)new_live_out);
                        free((void *)new_live_in);
                        return false;
                    }
                }
            }

            if (new_live_out_count != block->live_out_name_count)
                same_live_out = false;
            else {
                for (size_t i = 0; i < new_live_out_count; i++) {
                    if (!mir_name_set_contains(block->live_out_names, block->live_out_name_count, new_live_out[i])) {
                        same_live_out = false;
                        break;
                    }
                }
            }

            if (new_live_in_count != block->live_in_name_count)
                same_live_in = false;
            else {
                for (size_t i = 0; i < new_live_in_count; i++) {
                    if (!mir_name_set_contains(block->live_in_names, block->live_in_name_count, new_live_in[i])) {
                        same_live_in = false;
                        break;
                    }
                }
            }

            if (!same_live_out) {
                mir_clear_block_name_set(&block->live_out_names,
                                         &block->live_out_name_count,
                                         &block->live_out_name_capacity);
                block->live_out_names = new_live_out;
                block->live_out_name_count = new_live_out_count;
                block->live_out_name_capacity = new_live_out_capacity;
                new_live_out = NULL;
                new_live_out_capacity = 0;
                changed = true;
            }
            if (!same_live_in) {
                mir_clear_block_name_set(&block->live_in_names,
                                         &block->live_in_name_count,
                                         &block->live_in_name_capacity);
                block->live_in_names = new_live_in;
                block->live_in_name_count = new_live_in_count;
                block->live_in_name_capacity = new_live_in_capacity;
                new_live_in = NULL;
                new_live_in_capacity = 0;
                changed = true;
            }

            free((void *)new_live_out);
            free((void *)new_live_in);
        }
    } while (changed);

    free(order);
    routine->live_value_count = 0;
    routine->has_liveness = true;
    for (size_t i = 0; i < routine->block_count; i++)
        routine->live_value_count += routine->blocks[i].live_in_name_count + routine->blocks[i].live_out_name_count;
    return mir_build_value_summaries(routine);
}

#endif
