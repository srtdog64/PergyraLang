/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * MIR resource runtime-call ABI row materialization and consumer linking.
 */

#include "mir_resource_runtime_population.h"

#include <string.h>

#include "mir_abi_layout.h"
#include "mir_machine_layer.h"

static bool
mir_resource_runtime_operation_has_row(const char *operation)
{
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

bool
mir_materialize_resource_runtime_row(MIRRoutine *routine,
                                     const char *abi_type_name,
                                     const char *operation,
                                     MIRResourceRuntimeRow *out)
{
    const MIRResourceRuntimeRow *row;

    if (routine == NULL || abi_type_name == NULL || operation == NULL
        || out == NULL)
        return true;
    if (!mir_resource_runtime_operation_has_row(operation)) {
        return true;
    }

    row = mir_abi_resource_runtime_row_for_type_name(
        abi_type_name, operation);
    if (row == NULL)
        return true;

    *out = *row;
    out->domain = pgy_arena_strdup(
        &routine->scratch, row->domain);
    out->abi_type_name = pgy_arena_strdup(
        &routine->scratch, row->abi_type_name);
    out->resource_op_name = pgy_arena_strdup(
        &routine->scratch, row->resource_op_name);
    out->runtime_fn = pgy_arena_strdup(
        &routine->scratch, row->runtime_fn);
    out->target_kind = pgy_arena_strdup(
        &routine->scratch, row->target_kind);
    out->materialization = pgy_arena_strdup(
        &routine->scratch, row->materialization);
    out->call_shape = pgy_arena_strdup(
        &routine->scratch, row->call_shape);
    out->runtime_call_abi_id = mir_abi_resource_runtime_row_id(out);
    if (out->domain == NULL
        || out->abi_type_name == NULL
        || out->resource_op_name == NULL
        || out->runtime_fn == NULL
        || out->target_kind == NULL
        || out->materialization == NULL
        || out->call_shape == NULL
        || out->runtime_call_abi_id == 0) {
        return false;
    }
    return true;
}

bool
mir_materialize_resource_runtime_fact(MIRRoutine *routine,
                                      MIRInstruction *inst)
{
    const char *operation;

    if (routine == NULL || inst == NULL || inst->abi_type_name == NULL)
        return true;
    operation = (inst->kind == MIR_INST_DEF
                 || inst->kind == MIR_INST_DESTRUCTURE)
        ? "Claim"
        : mir_machine_layer_runtime_operation(inst);
    if (operation == NULL)
        operation = inst->name;
    if (!mir_materialize_resource_runtime_row(
            routine, inst->abi_type_name, operation,
            &inst->resource_runtime_fact))
        return false;
    if (inst->resource_runtime_fact.runtime_call_abi_id != 0)
        inst->resource_runtime_fact_present = true;
    if (inst->resource_runtime_fact_present
        && inst->resource_runtime_fact.resource_op_name != NULL
        && strcmp(inst->resource_runtime_fact.resource_op_name, "Claim") == 0) {
        const char *aux_operations[] = {"Read", "Write"};
        for (size_t i = 0; i < sizeof(aux_operations) / sizeof(aux_operations[0]); i++) {
            if (inst->resource_runtime_aux_fact_count
                >= sizeof(inst->resource_runtime_aux_facts)
                    / sizeof(inst->resource_runtime_aux_facts[0]))
                break;
            MIRResourceRuntimeRow *aux =
                &inst->resource_runtime_aux_facts[inst->resource_runtime_aux_fact_count];
            if (!mir_materialize_resource_runtime_row(
                    routine, inst->abi_type_name, aux_operations[i], aux))
                return false;
            if (aux->runtime_call_abi_id != 0)
                inst->resource_runtime_aux_fact_count++;
        }
    } else if (inst->resource_runtime_fact_present
               && inst->resource_runtime_fact.resource_op_name != NULL
               && (strcmp(inst->resource_runtime_fact.resource_op_name,
                          "PinRead") == 0
                   || strcmp(inst->resource_runtime_fact.resource_op_name,
                             "PinWrite") == 0)) {
        const char *aux_operations[] = {
            strcmp(inst->resource_runtime_fact.resource_op_name, "PinRead") == 0
                ? "PinReadInit" : "PinWriteInit",
            "Unpin",
            "UnpinCleanup"
        };
        for (size_t i = 0; i < sizeof(aux_operations) / sizeof(aux_operations[0]); i++) {
            if (inst->resource_runtime_aux_fact_count
                >= sizeof(inst->resource_runtime_aux_facts)
                    / sizeof(inst->resource_runtime_aux_facts[0]))
                break;
            MIRResourceRuntimeRow *aux =
                &inst->resource_runtime_aux_facts[
                    inst->resource_runtime_aux_fact_count];
            if (!mir_materialize_resource_runtime_row(
                    routine, inst->abi_type_name, aux_operations[i], aux))
                return false;
            if (aux->runtime_call_abi_id != 0)
                inst->resource_runtime_aux_fact_count++;
        }
    } else if (inst->resource_runtime_fact_present
               && inst->resource_runtime_fact.resource_op_name != NULL
               && (strcmp(inst->resource_runtime_fact.resource_op_name,
                          "Read") == 0
                   || strcmp(inst->resource_runtime_fact.resource_op_name,
                             "Write") == 0)) {
        const char *aux_operations[] = {
            "PinReadInit", "PinWriteInit", "Unpin", "UnpinCleanup"
        };
        for (size_t i = 0; i < sizeof(aux_operations) / sizeof(aux_operations[0]); i++) {
            if (inst->resource_runtime_aux_fact_count
                >= sizeof(inst->resource_runtime_aux_facts)
                    / sizeof(inst->resource_runtime_aux_facts[0]))
                break;
            MIRResourceRuntimeRow *aux =
                &inst->resource_runtime_aux_facts[
                    inst->resource_runtime_aux_fact_count];
            if (!mir_materialize_resource_runtime_row(
                    routine, inst->abi_type_name, aux_operations[i], aux))
                return false;
            if (aux->runtime_call_abi_id != 0)
                inst->resource_runtime_aux_fact_count++;
        }
    }
    return true;
}

bool
mir_link_resource_runtime_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        MIRBasicBlock *block = &routine->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            MIRInstruction *consumer = &block->instructions[ii];
            MIRInstruction *source = NULL;
            bool ambiguous = false;

            if (consumer->kind == MIR_INST_RESOURCE_OP)
                continue;
            for (size_t ri = 0; ri < block->instruction_count; ri++) {
                MIRInstruction *resource = &block->instructions[ri];
                if (resource->kind != MIR_INST_RESOURCE_OP
                    || !resource->resource_runtime_fact_present
                    || !mir_instruction_consumes_resource_source(
                        resource, consumer)) {
                    continue;
                }
                if (source != NULL) {
                    ambiguous = true;
                    break;
                }
                source = resource;
            }
            if (ambiguous || source == NULL)
                continue;
            if (consumer->resource_runtime_fact_present
                && (consumer->resource_runtime_fact.runtime_fn == NULL
                    || strcmp(consumer->resource_runtime_fact.runtime_fn,
                              source->resource_runtime_fact.runtime_fn) != 0
                    || consumer->resource_runtime_fact.runtime_call_abi_id == 0
                    || consumer->resource_runtime_fact.runtime_call_abi_id
                        != source->resource_runtime_fact.runtime_call_abi_id)) {
                return false;
            }
            consumer->resource_runtime_fact = source->resource_runtime_fact;
            consumer->resource_runtime_fact_present = true;
        }
    }
    return true;
}
