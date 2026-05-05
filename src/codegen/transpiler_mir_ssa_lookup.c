/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA lookup helpers.
 */

#include "transpiler_mir_ssa_lookup.h"

#include <string.h>

#include "transpiler_mir_ssa_map.h"

const char *
transpiler_find_prior_block_ssa_name(const MIRBasicBlock *block,
                                     size_t limit_inst_index,
                                     const char *base_name)
{
    const char *resolved = NULL;

    if (block == NULL || base_name == NULL)
        return NULL;

    for (size_t i = 0; i < block->instruction_count && i < limit_inst_index; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        char parsed_base[128];
        size_t version = 0;

        if ((inst->kind != MIR_INST_DEF && inst->kind != MIR_INST_PHI)
            || inst->result_name == NULL) {
            continue;
        }
        if (!transpiler_parse_versioned_name(inst->result_name, parsed_base,
                                             sizeof(parsed_base), &version)) {
            continue;
        }
        if (strcmp(parsed_base, base_name) == 0)
            resolved = inst->result_name;
    }

    return resolved;
}

const char *
transpiler_find_block_exit_ssa_name(const MIRBasicBlock *block,
                                    const char *base_name)
{
    if (block == NULL || base_name == NULL || block->ssa_exit_values == NULL)
        return NULL;

    for (size_t i = 0; i < block->ssa_exit_value_count; i++) {
        const char *versioned = block->ssa_exit_values[i];
        char parsed_base[128];
        size_t version = 0;

        if (versioned == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned,
                                             parsed_base,
                                             sizeof(parsed_base),
                                             &version)) {
            continue;
        }
        if (strcmp(parsed_base, base_name) == 0)
            return versioned;
    }

    return NULL;
}

const char *
transpiler_find_block_renamed_ssa_name(const MIRBasicBlock *block,
                                       const char *base_name)
{
    if (block == NULL || base_name == NULL || block->renamed_locals == NULL)
        return NULL;

    for (size_t i = 0; i < block->renamed_local_count; i++) {
        const char *versioned = block->renamed_locals[i];
        char parsed_base[128];
        size_t version = 0;

        if (versioned == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned,
                                             parsed_base,
                                             sizeof(parsed_base),
                                             &version)) {
            continue;
        }
        if (strcmp(parsed_base, base_name) == 0)
            return versioned;
    }

    return NULL;
}

const char *
transpiler_find_routine_exit_ssa_name(const MIRRoutine *mir_routine,
                                      const char *base_name)
{
    if (mir_routine == NULL || base_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < mir_routine->block_count; bi++) {
        const char *versioned =
            transpiler_find_block_exit_ssa_name(&mir_routine->blocks[bi], base_name);
        if (versioned != NULL)
            return versioned;
    }

    return NULL;
}
