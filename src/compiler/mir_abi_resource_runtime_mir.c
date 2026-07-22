#include "mir_abi_layout.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

const MIRResourceRuntimeRow *
mir_abi_resource_runtime_row_for_instruction(
    const MIRInstruction *instruction,
    const char *resource_op_name)
{
    if (instruction == NULL || resource_op_name == NULL)
        return NULL;
    if (instruction->resource_runtime_fact_present
        && instruction->resource_runtime_fact.resource_op_name != NULL
        && strcmp(instruction->resource_runtime_fact.resource_op_name,
                  resource_op_name) == 0)
        return &instruction->resource_runtime_fact;
    if (instruction->resource_runtime_aux_fact_count
        > sizeof(instruction->resource_runtime_aux_facts)
            / sizeof(instruction->resource_runtime_aux_facts[0]))
        return NULL;
    for (size_t i = 0; i < instruction->resource_runtime_aux_fact_count; i++) {
        const MIRResourceRuntimeRow *aux =
            &instruction->resource_runtime_aux_facts[i];
        if (aux->resource_op_name != NULL
            && strcmp(aux->resource_op_name, resource_op_name) == 0)
            return aux;
    }
    return NULL;
}

const MIRInstruction *
mir_abi_resource_runtime_instruction_for_source(const MIRRoutine *routine,
                                                uint32_t source_stable_id)
{
    const MIRInstruction *match = NULL;

    if (routine == NULL || source_stable_id == 0) {
        return NULL;
    }
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind != MIR_INST_RESOURCE_OP
                || (!inst->resource_runtime_fact_present
                    && inst->resource_runtime_aux_fact_count == 0)
                || inst->source_stable_id != source_stable_id) {
                continue;
            }
            if (match != NULL)
                return NULL;
            match = inst;
        }
    }
    return match;
}

const MIRInstruction *
mir_abi_resource_runtime_instruction_for_abi(
    const MIRRoutine *routine,
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name)
{
    const char *container_name;
    char abi_type_name[96];
    int written;

    if (routine == NULL || inner_type_name == NULL
        || resource_op_name == NULL)
        return NULL;
    switch (kind) {
    case MIR_RESOURCE_ABI_SLOT:
        container_name = "Slot";
        break;
    case MIR_RESOURCE_ABI_SECURE_SLOT:
        container_name = "SecureSlot";
        break;
    case MIR_RESOURCE_ABI_DEVICE_SLOT:
        container_name = "DeviceSlot";
        break;
    default:
        return NULL;
    }
    written = snprintf(abi_type_name, sizeof(abi_type_name), "%s<%s>",
                       container_name, inner_type_name);
    if (written < 0 || (size_t)written >= sizeof(abi_type_name))
        return NULL;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const MIRResourceRuntimeRow *row =
                mir_abi_resource_runtime_row_for_instruction(
                    inst, resource_op_name);
            if (row == NULL || row->abi_type_name == NULL
                || strcmp(row->abi_type_name, abi_type_name) != 0
                || row->resource_op_name == NULL)
                continue;
            return inst;
        }
    }
    return NULL;
}

const MIRInstruction *
mir_abi_resource_runtime_owner_for_mir_abi(const MIRRoutine *routine,
                                           MIRResourceAbiKind kind,
                                           const char *inner_type_name)
{
    const char *container_name;
    char abi_type_name[96];
    int written;

    if (routine == NULL || inner_type_name == NULL)
        return NULL;
    switch (kind) {
    case MIR_RESOURCE_ABI_SLOT:
        container_name = "Slot";
        break;
    case MIR_RESOURCE_ABI_SECURE_SLOT:
        container_name = "SecureSlot";
        break;
    case MIR_RESOURCE_ABI_DEVICE_SLOT:
        container_name = "DeviceSlot";
        break;
    default:
        return NULL;
    }
    written = snprintf(abi_type_name, sizeof(abi_type_name), "%s<%s>",
                       container_name, inner_type_name);
    if (written < 0 || (size_t)written >= sizeof(abi_type_name))
        return NULL;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const MIRResourceRuntimeRow *row =
                mir_abi_resource_runtime_row_for_instruction(inst, "Read");
            const MIRResourceRuntimeRow *write_row =
                mir_abi_resource_runtime_row_for_instruction(inst, "Write");
            if (inst->kind == MIR_INST_RESOURCE_OP
                && ((row != NULL && row->abi_type_name != NULL
                     && strcmp(row->abi_type_name, abi_type_name) == 0)
                    || (write_row != NULL && write_row->abi_type_name != NULL
                        && strcmp(write_row->abi_type_name, abi_type_name) == 0)))
                return inst;
        }
    }
    return NULL;
}

const MIRResourceRuntimeRow *
mir_abi_resource_runtime_row_for_mir_abi(
    const MIRRoutine *routine,
    MIRResourceAbiKind kind,
    const char *inner_type_name,
    const char *resource_op_name)
{
    const MIRInstruction *instruction;

    /* Active MIR may only borrow a runtime-call row that lowering attached to
     * this routine.  In particular, do not reconstruct a row from the global
     * ABI vocabulary after the owner lookup: that would let a missing MIR
     * fact silently re-enter through a backend-side table lookup. */
    instruction = mir_abi_resource_runtime_instruction_for_abi(
        routine, kind, inner_type_name, resource_op_name);
    return mir_abi_resource_runtime_row_for_instruction(
        instruction, resource_op_name);
}

const MIRInstruction *
mir_abi_resource_runtime_pin_owner_for_mir(const MIRRoutine *routine,
                                           MIRResourceAbiKind kind,
                                           const char *inner_type_name)
{
    const char *container_name;
    char abi_type_name[96];
    const MIRInstruction *owner = NULL;
    int written;

    if (routine == NULL || inner_type_name == NULL)
        return NULL;
    switch (kind) {
    case MIR_RESOURCE_ABI_SLOT:
        container_name = "Slot";
        break;
    case MIR_RESOURCE_ABI_SECURE_SLOT:
        container_name = "SecureSlot";
        break;
    case MIR_RESOURCE_ABI_DEVICE_SLOT:
        container_name = "DeviceSlot";
        break;
    default:
        return NULL;
    }
    written = snprintf(abi_type_name, sizeof(abi_type_name), "%s<%s>",
                       container_name, inner_type_name);
    if (written < 0 || (size_t)written >= sizeof(abi_type_name))
        return NULL;

    /* A pin enter/exit row is authorized by a concrete same-type resource
     * operation.  All rows for one canonical ABI type share the same layout;
     * use the first owner row and fail closed when none exists. */
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const MIRResourceRuntimeRow *row =
                &inst->resource_runtime_fact;
            if (inst->kind != MIR_INST_RESOURCE_OP
                || !inst->resource_runtime_fact_present
                || row->abi_type_name == NULL
                || strcmp(row->abi_type_name, abi_type_name) != 0)
                continue;
            if (strcmp(row->resource_op_name, "Read") != 0
                && strcmp(row->resource_op_name, "Write") != 0)
                continue;
            if (owner == NULL)
                owner = inst;
        }
    }
    return owner;
}

const MIRResourceRuntimeRow *
mir_abi_resource_runtime_pin_row_for_mir(const MIRRoutine *routine,
                                         MIRResourceAbiKind kind,
                                         const char *inner_type_name,
                                         const char *resource_op_name)
{
    const char *container_name;
    char abi_type_name[96];
    const MIRInstruction *owner_inst;
    int written;

    if (routine == NULL || inner_type_name == NULL
        || resource_op_name == NULL)
        return NULL;
    switch (kind) {
    case MIR_RESOURCE_ABI_SLOT:
        container_name = "Slot";
        break;
    case MIR_RESOURCE_ABI_SECURE_SLOT:
        container_name = "SecureSlot";
        break;
    case MIR_RESOURCE_ABI_DEVICE_SLOT:
        container_name = "DeviceSlot";
        break;
    default:
        return NULL;
    }
    written = snprintf(abi_type_name, sizeof(abi_type_name), "%s<%s>",
                       container_name, inner_type_name);
    if (written < 0 || (size_t)written >= sizeof(abi_type_name))
        return NULL;

    owner_inst = mir_abi_resource_runtime_pin_owner_for_mir(
        routine, kind, inner_type_name);
    if (owner_inst == NULL)
        return NULL;

    (void)owner_inst;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const MIRResourceRuntimeRow *row =
                mir_abi_resource_runtime_row_for_instruction(
                    inst, resource_op_name);
            if (row != NULL && row->abi_type_name != NULL
                && strcmp(row->abi_type_name, abi_type_name) == 0)
                return row;
        }
    }
    return NULL;
}
