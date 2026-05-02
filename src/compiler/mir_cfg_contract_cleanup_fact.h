#ifndef PERGYRA_MIR_CFG_CONTRACT_CLEANUP_FACT_H
#define PERGYRA_MIR_CFG_CONTRACT_CLEANUP_FACT_H

#include <string.h>

#include "mir.h"

static bool
mir_block_has_cleanup_edge_fact(const MIRBasicBlock *block,
                                const char *edge_name)
{
    if (block == NULL || edge_name == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_CLEANUP_EDGE
            && inst->name != NULL
            && strcmp(inst->name, edge_name) == 0) {
            if (inst->slot_anchor == NULL
                || strcmp(inst->slot_anchor, "cleanup") != 0) {
                continue;
            }
            if (inst->arg0 == NULL || strcmp(inst->arg0, "cleanup") != 0)
                continue;
            return true;
        }
    }
    return false;
}

#endif
