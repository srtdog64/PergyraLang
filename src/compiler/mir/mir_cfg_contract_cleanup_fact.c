#include "mir_cfg_contract_cleanup_fact.h"

#include <string.h>

#include "mir_cleanup_fact_names.h"

bool
mir_block_has_cleanup_edge_fact(const MIRBasicBlock *block,
                                const char *edge_name)
{
    if (block == NULL || edge_name == NULL)
        return false;
    if (block->instruction_count > 0 && block->instructions == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_CLEANUP_EDGE
            && inst->name != NULL
            && strcmp(inst->name, edge_name) == 0) {
            if (inst->slot_anchor == NULL
                || strcmp(inst->slot_anchor, MIR_CLEANUP_FACT_ANCHOR) != 0) {
                continue;
            }
            if (inst->arg0 == NULL
                || strcmp(inst->arg0, MIR_CLEANUP_FACT_ANCHOR) != 0)
                continue;
            return true;
        }
    }
    return false;
}

bool
mir_block_has_expected_cleanup_edge_fact(const MIRRoutine *routine,
                                         size_t block_index)
{
    const char *edge_name;

    if (routine == NULL || block_index >= routine->block_count)
        return false;

    edge_name = mir_cleanup_edge_fact_name_for_block(routine, block_index);
    return mir_block_has_cleanup_edge_fact(&routine->blocks[block_index],
                                           edge_name);
}
