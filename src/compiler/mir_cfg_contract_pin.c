#include "mir_cfg_contract_pin.h"

#include <string.h>

#include "mir_cleanup_fact_names.h"

const MIRInstruction *
mir_block_find_pin_cleanup_edge_fact(const MIRBasicBlock *block)
{
    const char *expected_access;

    if (block == NULL || !block->is_pin_region)
        return NULL;

    if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0')
        return NULL;
    if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0')
        return NULL;
    if (block->instruction_count > 0 && block->instructions == NULL)
        return NULL;

    expected_access = block->pin_view_is_write
        ? MIR_PIN_CLEANUP_ACCESS_WRITE
        : MIR_PIN_CLEANUP_ACCESS_READ;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_CLEANUP_EDGE
            || inst->name == NULL
            || strcmp(inst->name, MIR_CLEANUP_FACT_PIN_UNPIN_EDGE) != 0) {
            continue;
        }
        if (inst->slot_anchor == NULL
            || strcmp(inst->slot_anchor, block->pin_source_name) != 0)
            continue;
        if (inst->arg0 == NULL || strcmp(inst->arg0, block->pin_view_name) != 0)
            continue;
        if (inst->arg1 == NULL || strcmp(inst->arg1, expected_access) != 0)
            continue;
        return inst;
    }
    return NULL;
}

bool
mir_block_has_pin_cleanup_edge(const MIRBasicBlock *block)
{
    return mir_block_find_pin_cleanup_edge_fact(block) != NULL;
}

const char *
mir_block_pin_cleanup_missing_reason(const MIRBasicBlock *block)
{
    if (block == NULL)
        return "pin block is missing";
    if (!block->is_pin_region)
        return "block is not marked as a pin region";
    if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0')
        return "pin source slot is missing";
    if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0')
        return "pin view name is missing";
    if (mir_block_find_pin_cleanup_edge_fact(block) == NULL)
        return "pin cleanup fact does not match source slot, view, and access mode";
    return NULL;
}

bool
mir_block_has_pin_guard_amortization_region(const MIRBasicBlock *block)
{
    return block != NULL
        && block->is_pin_region
        && block->pin_source_name != NULL
        && block->pin_source_name[0] != '\0'
        && block->pin_view_name != NULL
        && block->pin_view_name[0] != '\0'
        && mir_block_has_pin_cleanup_edge(block);
}

const char *
mir_block_pin_guard_amortization_missing_reason(const MIRBasicBlock *block)
{
    const char *cleanup_reason;

    if (block == NULL)
        return "pin guard-amortization region is missing";
    if (!block->is_pin_region)
        return "block is not a pin region";
    if (block->pin_source_name == NULL || block->pin_source_name[0] == '\0')
        return "pin source slot is missing";
    if (block->pin_view_name == NULL || block->pin_view_name[0] == '\0')
        return "pin view name is missing";
    cleanup_reason = mir_block_pin_cleanup_missing_reason(block);
    if (cleanup_reason != NULL)
        return cleanup_reason;
    return NULL;
}
