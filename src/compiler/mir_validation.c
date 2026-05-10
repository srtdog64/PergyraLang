#include "mir_validation.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_validation_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    int written;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    written = vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    if (written < 0 || written != length) {
        free(result);
        return NULL;
    }
    return result;
}

static bool
mir_validation_name_set_contains(const char **names, size_t count, const char *name)
{
    if (name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static bool
mir_validation_block_has_value(const MIRBasicBlock *block, const char *name)
{
    if (block == NULL || name == NULL)
        return false;
    return mir_validation_name_set_contains(block->ssa_entry_values, block->ssa_entry_value_count, name)
           || mir_validation_name_set_contains(block->def_names, block->def_name_count, name)
           || mir_validation_name_set_contains(block->ssa_exit_values, block->ssa_exit_value_count, name);
}

static bool
mir_validation_block_has_predecessor(const MIRBasicBlock *block, size_t predecessor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] == predecessor)
            return true;
    }
    return false;
}

static bool
mir_validation_block_can_use_value_before_inst(const MIRBasicBlock *block,
                                               const char *name,
                                               size_t inst_index)
{
    if (block == NULL || name == NULL)
        return false;
    if (block->instruction_count > 0 && block->instructions == NULL)
        return false;
    if (mir_validation_name_set_contains(block->live_in_names, block->live_in_name_count, name)
        || mir_validation_name_set_contains(block->ssa_entry_values, block->ssa_entry_value_count, name)) {
        return true;
    }

    for (size_t i = 0; i < inst_index && i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->result_name != NULL && strcmp(inst->result_name, name) == 0)
            return true;
    }
    return false;
}

bool
mir_validate_block_liveness_sets(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 size_t block_index,
                                 char **error_message)
{
    if (block == NULL)
        return false;

    for (size_t i = 0; i < block->live_out_name_count; i++) {
        const char *name = block->live_out_names[i];
        if (name == NULL)
            continue;
        if (!mir_validation_block_has_value(block, name)
            && !mir_validation_name_set_contains(block->live_in_names,
                                                 block->live_in_name_count,
                                                 name)) {
            if (error_message != NULL) {
                *error_message = mir_validation_strdup_fmt(
                    "MIR routine '%s' block[%zu] live-out '%s' is not produced by block state",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    name);
            }
            return false;
        }
    }

    return true;
}

bool
mir_validate_instruction_uses(const MIRRoutine *routine,
                              const MIRBasicBlock *block,
                              size_t block_index,
                              char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    if (block->instruction_count > 0 && block->instructions == NULL) {
        if (error_message != NULL) {
            *error_message = mir_validation_strdup_fmt(
                "MIR routine '%s' block[%zu] has instruction count without instruction inventory during use validation",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index);
        }
        return false;
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_PHI) {
            for (size_t j = 0; j < inst->phi_incoming_count; j++) {
                size_t pred = inst->phi_incomings[j].predecessor_block;
                const char *value = inst->phi_incomings[j].value_name;
                if (pred >= routine->block_count) {
                    if (error_message != NULL) {
                        *error_message = mir_validation_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi references invalid predecessor %zu",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            pred);
                    }
                    return false;
                }
                if (!mir_validation_block_has_predecessor(block, pred)) {
                    if (error_message != NULL) {
                        *error_message = mir_validation_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi predecessor %zu is not in predecessor list",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            pred);
                    }
                    return false;
                }
                if (!mir_validation_name_set_contains(routine->blocks[pred].ssa_exit_values,
                                                      routine->blocks[pred].ssa_exit_value_count,
                                                      value)
                    && !mir_validation_name_set_contains(routine->blocks[pred].live_out_names,
                                                         routine->blocks[pred].live_out_name_count,
                                                         value)) {
                    if (error_message != NULL) {
                        *error_message = mir_validation_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi incoming '%s' is not available from predecessor %zu",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            value != NULL ? value : "(null)",
                            pred);
                    }
                    return false;
                }
            }
            continue;
        }

        for (size_t j = 0; j < inst->use_count; j++) {
            const char *use = inst->uses[j];
            if (!mir_validation_block_can_use_value_before_inst(block, use, i)) {
                if (error_message != NULL) {
                    *error_message = mir_validation_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] uses '%s' before definition",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        i,
                        use != NULL ? use : "(null)");
                }
                return false;
            }
        }
    }

    return true;
}
