/* Resource-surface fact classification owner for MIR fact validation.
 * Split from mir_fact_surface_validate.c by responsibility: slot-family
 * classification, borrow-view semantics, and runtime-row requirements.
 * The instruction surface validator stays in mir_fact_surface_validate.c
 * and consumes these facts through mir_fact_validate_internal.h. */

#include "mir_fact_validate_internal.h"
#include "mir_abi_layout.h"
#include "mir_machine_layer.h"

#include <string.h>

bool
mir_claim_abi_type_is_slot_family(const MIRInstruction *inst)
{
    const char *abi_name = inst != NULL && inst->type_layout != NULL
        ? inst->type_layout->abi_type_name
        : NULL;
    return abi_name != NULL
        && (strncmp(abi_name, "Slot<", 5) == 0
            || strncmp(abi_name, "SecureSlot<", 11) == 0
            || strncmp(abi_name, "DeviceSlot<", 11) == 0);
}

static bool
mir_resource_kind_is_borrow_view(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->rir_op != NULL
        && (inst->rir_op->kind == RIR_OP_BORROW_READ
            || inst->rir_op->kind == RIR_OP_BORROW_WRITE);
}

bool
mir_resource_kind_consumes_view(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_RESOURCE_OP
        && inst->rir_op != NULL
        && (inst->rir_op->kind == RIR_OP_READ
            || inst->rir_op->kind == RIR_OP_WRITE
            || inst->rir_op->kind == RIR_OP_MOVE);
}

static bool
mir_resource_borrow_targets_view(const MIRInstruction *inst,
                                 const char *view_name)
{
    if (inst == NULL || view_name == NULL || view_name[0] == '\0')
        return false;
    return inst->arg1 != NULL && strcmp(inst->arg1, view_name) == 0;
}

const char *
mir_prior_borrow_source_for_view(const MIRRoutine *routine,
                                 size_t before_block,
                                 size_t before_inst,
                                 const char *view_name)
{
    const char *source_slot = NULL;

    if (routine == NULL || view_name == NULL || view_name[0] == '\0')
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *candidate = &block->instructions[ii];
            if (bi == before_block && ii == before_inst)
                return source_slot;
            if (mir_resource_kind_is_borrow_view(candidate)
                && mir_resource_borrow_targets_view(candidate, view_name)
                && (candidate->resource_owner_slot_anchor != NULL
                    || candidate->arg0 != NULL)) {
                source_slot = candidate->resource_owner_slot_anchor != NULL
                    ? candidate->resource_owner_slot_anchor
                    : candidate->arg0;
            }
        }
    }
    return NULL;
}

bool
mir_resource_owner_layout_is_slot_family(const MIRInstruction *inst)
{
    const char *abi_name = inst != NULL && inst->type_layout != NULL
        ? inst->type_layout->abi_type_name
        : NULL;
    return abi_name != NULL
        && (strncmp(abi_name, "Slot<", 5) == 0
            || strncmp(abi_name, "SecureSlot<", 11) == 0
            || strncmp(abi_name, "DeviceSlot<", 11) == 0);
}

static bool
mir_resource_runtime_fact_type_is_slot_family(const MIRInstruction *inst)
{
    const char *abi_name = inst != NULL ? inst->abi_type_name : NULL;
    return abi_name != NULL
        && (strncmp(abi_name, "Slot<", 5) == 0
            || strncmp(abi_name, "SecureSlot<", 11) == 0
            || strncmp(abi_name, "DeviceSlot<", 11) == 0);
}

bool
mir_resource_runtime_fact_requires_row(const MIRInstruction *inst)
{
    const char *operation;
    if (inst == NULL || inst->kind != MIR_INST_RESOURCE_OP
        || !mir_resource_runtime_fact_type_is_slot_family(inst)) {
        return false;
    }
    operation = mir_machine_layer_runtime_operation(inst);
    if (operation == NULL)
        operation = inst->name;
    if (operation == NULL)
        return false;
    return strcmp(operation, "Claim") == 0
        || strcmp(operation, "Read") == 0
        || strcmp(operation, "Write") == 0
        || strcmp(operation, "Release") == 0
        || strcmp(operation, "PinRead") == 0
        || strcmp(operation, "PinWrite") == 0
        || strcmp(operation, "PinReadInit") == 0
        || strcmp(operation, "PinWriteInit") == 0
        || strcmp(operation, "Unpin") == 0
        || strcmp(operation, "UnpinCleanup") == 0
        || strcmp(operation, "SubmitRead") == 0;
}

const MIRInstruction *
mir_resource_runtime_fact_source_for_consumer(
    const MIRBasicBlock *block,
    const MIRInstruction *consumer)
{
    const MIRInstruction *match = NULL;

    if (block == NULL || consumer == NULL
        || consumer->kind == MIR_INST_RESOURCE_OP) {
        return NULL;
    }
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *resource = &block->instructions[i];
        if (resource->kind == MIR_INST_RESOURCE_OP
            && resource->resource_runtime_fact_present
            && mir_instruction_consumes_resource_source(
                resource, consumer)) {
            if (match != NULL)
                return NULL;
            match = resource;
        }
    }
    return match;
}
