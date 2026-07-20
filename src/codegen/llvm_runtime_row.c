/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR runtime-row selection and verification.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

#include "../compiler/mir_abi_layout.h"
#include "../compiler/mir_machine_layer.h"

#include <string.h>

static const char *
llvm_slot_runtime_expected_call_shape(MIRResourceAbiKind kind,
                                      const char *operation)
{
    bool secure = kind == MIR_RESOURCE_ABI_SECURE_SLOT;

    if (operation == NULL)
        return NULL;
    if (strcmp(operation, "Claim") == 0)
        return secure ? "token_ptr_to_container" : "returns_container";
    if (strcmp(operation, "Read") == 0)
        return secure ? "container_ptr_token_ptr_to_value"
                      : "container_ptr_to_value";
    if (strcmp(operation, "Write") == 0)
        return secure ? "container_ptr_value_token_ptr_to_void"
                      : "container_ptr_value_to_void";
    if (strcmp(operation, "Release") == 0)
        return secure ? "container_ptr_token_ptr_to_void"
                      : "container_ptr_to_void";
    if (strcmp(operation, "SubmitRead") == 0)
        return "container_ptr_to_task_handle";
    if (strcmp(operation, "PinRead") == 0 ||
        strcmp(operation, "PinWrite") == 0) {
        return secure ? "container_ptr_token_ptr_to_pinned_view"
                      : "container_ptr_to_pinned_view";
    }
    if (strcmp(operation, "PinReadInit") == 0 ||
        strcmp(operation, "PinWriteInit") == 0) {
        return secure ? "pinned_view_ptr_container_ptr_token_ptr_to_void"
                      : "pinned_view_ptr_container_ptr_to_void";
    }
    if (strcmp(operation, "Unpin") == 0 ||
        strcmp(operation, "UnpinCleanup") == 0)
        return "pinned_view_ptr_to_void";
    return NULL;
}

static bool
llvm_slot_runtime_operation_requires_lowered_row(const char *operation)
{
    return operation != NULL
        && (strcmp(operation, "Claim") == 0
            || strcmp(operation, "Read") == 0
            || strcmp(operation, "Write") == 0
            || strcmp(operation, "Release") == 0
            || strcmp(operation, "PinRead") == 0
            || strcmp(operation, "PinWrite") == 0
            || strcmp(operation, "PinReadInit") == 0
            || strcmp(operation, "PinWriteInit") == 0
            || strcmp(operation, "Unpin") == 0
            || strcmp(operation, "UnpinCleanup") == 0
            || strcmp(operation, "SubmitRead") == 0);
}

static bool
llvm_slot_runtime_operation_is_synthetic_pin(const char *operation)
{
    return operation != NULL
        && (strcmp(operation, "PinReadInit") == 0
            || strcmp(operation, "PinWriteInit") == 0
            || strcmp(operation, "Unpin") == 0
            || strcmp(operation, "UnpinCleanup") == 0);
}

const MIRResourceRuntimeRow *
llvm_slot_runtime_row_for_operation(ASTNode *node,
                                    LLVMGenCtx *ctx,
                                    MIRResourceAbiKind kind,
                                    const char *inner_type_name,
                                    const char *operation)
{
    const MIRInstruction *inst = ctx != NULL
        ? ctx->current_mir_instruction
        : NULL;
    const MIRInstruction *source_inst = NULL;
    const MIRResourceRuntimeRow *row = NULL;
    bool row_is_mir_fact = false;
    const char *expected_shape =
        llvm_slot_runtime_expected_call_shape(kind, operation);

    if (ctx != NULL && ctx->current_mir_routine != NULL) {
        if (node != NULL) {
            source_inst = mir_abi_resource_runtime_instruction_for_source(
                ctx->current_mir_routine,
                ast_node_stable_id(node));
            if (source_inst != NULL) {
                const MIRResourceRuntimeRow *source_row =
                    &source_inst->resource_runtime_fact;
                if (source_row->resource_op_name != NULL
                    && operation != NULL
                    && strcmp(source_row->resource_op_name, operation) == 0) {
                    row = source_row;
                    row_is_mir_fact = true;
                } else {
                    source_inst = NULL;
                }
            }
        }
        if (row == NULL && llvm_slot_runtime_operation_requires_lowered_row(
                operation)
            && !llvm_slot_runtime_operation_is_synthetic_pin(operation)) {
            source_inst = mir_abi_resource_runtime_instruction_for_abi(
                ctx->current_mir_routine, kind, inner_type_name, operation);
            if (source_inst != NULL) {
                row = &source_inst->resource_runtime_fact;
                row_is_mir_fact = true;
            }
            if (row == NULL) {
                row = mir_abi_resource_runtime_row_for_mir_abi(
                    ctx->current_mir_routine, kind, inner_type_name,
                    operation);
                if (row != NULL)
                    row_is_mir_fact = true;
                if (row != NULL)
                    source_inst = mir_abi_resource_runtime_owner_for_mir_abi(
                        ctx->current_mir_routine, kind, inner_type_name);
            }
        }
    }
    if (row != NULL) {
        /* Nested resource expressions consume their own MIR row. */
    } else if (inst != NULL && inst->resource_runtime_fact_present) {
        row = &inst->resource_runtime_fact;
        row_is_mir_fact = true;
        if (row->resource_op_name == NULL
            || operation == NULL
            || strcmp(row->resource_op_name, operation) != 0) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR resource operation has a mismatched runtime-call ABI row");
            return NULL;
        }
        if (row->runtime_call_abi_id == 0
            || row->runtime_call_abi_id
                != mir_abi_resource_runtime_row_id(row)
            || !mir_abi_resource_runtime_row_matches_owner(row)) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR resource operation has an invalid runtime-call ABI identity");
            return NULL;
        }
    } else if (inst != NULL && inst->kind == MIR_INST_RESOURCE_OP
               && llvm_slot_runtime_operation_requires_lowered_row(operation)) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR resource operation is missing its lowered runtime-call ABI row");
        return NULL;
    } else if (ctx != NULL && ctx->current_mir_routine != NULL
               && llvm_slot_runtime_operation_is_synthetic_pin(operation)) {
        row = mir_abi_resource_runtime_pin_row_for_mir(
            ctx->current_mir_routine, kind, inner_type_name, operation);
        if (row != NULL) {
            row_is_mir_fact = true;
        } else {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR source operation has no active instruction-owned runtime-call ABI row");
            return NULL;
        }
    } else if (ctx != NULL && ctx->current_mir_routine != NULL
               && llvm_slot_runtime_operation_requires_lowered_row(operation)) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR source operation is missing its lowered runtime-call ABI row");
        return NULL;
    } else {
        row = mir_abi_resource_runtime_row_by_kind(
            kind, inner_type_name, operation);
    }

    if (row_is_mir_fact && row != NULL
        && (row->runtime_call_abi_id == 0
            || row->runtime_call_abi_id
                != mir_abi_resource_runtime_row_id(row)
            || !mir_abi_resource_runtime_row_matches_owner(row))) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR resource operation has an invalid runtime-call ABI identity");
        return NULL;
    }
    if (row == NULL || row->resource_op_name == NULL || operation == NULL
        || strcmp(row->resource_op_name, operation) != 0
        || row->runtime_fn == NULL || row->call_shape == NULL)
        return NULL;
    if (expected_shape != NULL &&
        strcmp(row->call_shape, expected_shape) != 0) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM slot operation %s requires MIR ABI call shape %s",
            operation != NULL ? operation : "<unknown>", expected_shape);
        return NULL;
    }
    if (ctx != NULL && ctx->current_mir_routine != NULL) {
        const MIRInstruction *layout_inst = source_inst != NULL
            ? source_inst
            : inst;
        const bool synthetic_pin = node == NULL
            && llvm_slot_runtime_operation_is_synthetic_pin(operation);
        if (synthetic_pin) {
            layout_inst = mir_abi_resource_runtime_pin_owner_for_mir(
                ctx->current_mir_routine,
                kind,
                inner_type_name);
        }
        if ((layout_inst == NULL || layout_inst->type_layout == NULL
             || layout_inst->abi_layout_id == 0
             || layout_inst->abi_layout_id
                 != mir_abi_layout_id(layout_inst->type_layout))
            && !mir_abi_resource_runtime_row_is_constructed_nominal(row)) {
            llvm_set_error_at_with_hints(ctx, node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR source operation has a missing or mismatched ABI layout identity");
            return NULL;
        }
    }
    const MIRInstruction *machine_inst = source_inst != NULL
        ? source_inst
        : (ctx != NULL ? ctx->current_mir_instruction : NULL);
    if (machine_inst != NULL
        && rir_machine_contact_kind_is_present(
            machine_inst->machine_contact_kind)
        && !mir_machine_layer_fact_matches_runtime_operation(
            machine_inst, row->resource_op_name)) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM slot runtime row disagrees with machine-layer runtime operation");
        return NULL;
    }
    return row;
}

#endif /* PGY_LLVM_ENABLED */
