#ifndef PERGYRA_MIR_CFG_CONTRACT_PIN_H
#define PERGYRA_MIR_CFG_CONTRACT_PIN_H

#include <string.h>

#include "mir.h"

static bool
mir_block_has_pin_cleanup_edge(const MIRBasicBlock *block)
{
    if (block == NULL || !block->is_pin_region)
        return false;

    if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0')
        return false;
    if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0')
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        const char *expected_access = block->pin_view_is_write ? "write" : "read";
        if (inst->kind != MIR_INST_CLEANUP_EDGE
            || inst->name == NULL
            || strcmp(inst->name, "pin-unpin-cleanup-edge") != 0) {
            continue;
        }
        if (inst->slot_anchor == NULL
            || strcmp(inst->slot_anchor, block->pin_source_name) != 0)
            continue;
        if (inst->arg0 == NULL || strcmp(inst->arg0, block->pin_view_name) != 0)
            continue;
        if (inst->arg1 == NULL || strcmp(inst->arg1, expected_access) != 0)
            continue;
        return true;
    }
    return false;
}

#endif
