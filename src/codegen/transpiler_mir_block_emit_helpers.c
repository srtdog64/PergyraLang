/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR block statement emission helpers.
 */

#include "transpiler_mir_block_emit_helpers.h"

bool
transpiler_mir_seed_block_phi_names(const MIRBasicBlock *block,
                                    TranspilerSSANameMap *ssa_map_out)
{
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        char base[128];
        size_t version = 0;
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        if (!transpiler_parse_versioned_name(inst->result_name,
                                             base,
                                             sizeof(base),
                                             &version))
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map_out,
                                         base,
                                         inst->result_name))
            return false;
    }
    return true;
}

bool
transpiler_mir_inst_is_cfg_container(const MIRInstruction *inst)
{
    return inst != NULL
        && mir_instruction_source_is_cfg_container(inst);
}

bool
transpiler_mir_seed_pin_view_alias(const MIRBasicBlock *block,
                                   TranspilerSSANameMap *ssa_map_out)
{
    if (!block->is_pin_region
        || block->pin_view_name == NULL
        || block->pin_source_name == NULL) {
        return true;
    }
    return transpiler_ssa_name_map_set(ssa_map_out,
                                       block->pin_view_name,
                                       block->pin_source_name);
}
