#include "mir_abi_layout.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

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
                || !inst->resource_runtime_fact_present
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
                &inst->resource_runtime_fact;
            if (inst->kind != MIR_INST_RESOURCE_OP
                || !inst->resource_runtime_fact_present
                || row->abi_type_name == NULL
                || strcmp(row->abi_type_name, abi_type_name) != 0
                || row->resource_op_name == NULL
                || strcmp(row->resource_op_name, resource_op_name) != 0)
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
                &inst->resource_runtime_fact;
            const bool is_resource_owner =
                inst->kind == MIR_INST_RESOURCE_OP
                || (inst->kind == MIR_INST_DEF
                    && row->resource_op_name != NULL
                    && strcmp(row->resource_op_name, "Claim") == 0);
            if (is_resource_owner
                && inst->resource_runtime_fact_present
                && row->abi_type_name != NULL
                && strcmp(row->abi_type_name, abi_type_name) == 0)
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
    const MIRInstruction *owner;
    const char *container_name;
    char abi_type_name[96];
    const MIRResourceRuntimeRow *row;
    static _Thread_local MIRResourceRuntimeRow derived_row;
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
    owner = mir_abi_resource_runtime_owner_for_mir_abi(
        routine, kind, inner_type_name);
    if (owner == NULL)
        return NULL;
    row = mir_abi_resource_runtime_row_by_type_name(abi_type_name,
                                                    resource_op_name);
    if (row == NULL)
        row = mir_abi_resource_runtime_row_for_type_name(abi_type_name,
                                                         resource_op_name);
    if (row == NULL)
        return NULL;
    derived_row = *row;
    derived_row.runtime_call_abi_id =
        mir_abi_resource_runtime_row_id(&derived_row);
    return &derived_row;
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
    const MIRResourceRuntimeRow *derived_row;
    static _Thread_local MIRResourceRuntimeRow pin_row;
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

    derived_row = mir_abi_resource_runtime_row_by_type_name(
        abi_type_name, resource_op_name);
    if (derived_row == NULL)
        derived_row = mir_abi_resource_runtime_row_for_type_name(
            abi_type_name, resource_op_name);
    if (derived_row == NULL)
        return NULL;
    pin_row = *derived_row;
    pin_row.runtime_call_abi_id = mir_abi_resource_runtime_row_id(&pin_row);
    return &pin_row;
}
