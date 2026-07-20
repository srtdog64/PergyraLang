/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend runtime declaration registry.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

#include "../compiler/mir_abi_layout.h"
#include "../compiler/mir_machine_layer.h"

#include <string.h>

static const MIRResourceRuntimeRow *
llvm_runtime_resource_row_or_error(LLVMGenCtx *ctx,
    const char *abi_type_name,
    const char *operation,
    const char *expected_call_shape,
    const char *missing_message)
{
    MIRResourceAbiKind kind;
    const char *inner_start;
    const char *inner_end;
    char inner_type_name[128];
    size_t inner_len;
    const MIRResourceRuntimeRow *row =
        NULL;

    if (abi_type_name == NULL)
        goto missing;
    if (strncmp(abi_type_name, "SecureSlot<", 11) == 0) {
        kind = MIR_RESOURCE_ABI_SECURE_SLOT;
    } else if (strncmp(abi_type_name, "DeviceSlot<", 11) == 0) {
        kind = MIR_RESOURCE_ABI_DEVICE_SLOT;
    } else if (strncmp(abi_type_name, "Slot<", 5) == 0) {
        kind = MIR_RESOURCE_ABI_SLOT;
    } else {
        goto missing;
    }
    inner_start = strchr(abi_type_name, '<');
    inner_end = strrchr(abi_type_name, '>');
    if (inner_start == NULL || inner_end == NULL || inner_end <= inner_start + 1)
        goto missing;
    inner_start++;
    inner_len = (size_t)(inner_end - inner_start);
    if (inner_len >= sizeof(inner_type_name))
        goto missing;
    memcpy(inner_type_name, inner_start, inner_len);
    inner_type_name[inner_len] = '\0';
    row = llvm_slot_runtime_row_for_operation(
        NULL, ctx, kind, inner_type_name, operation);

    if (row == NULL || row->runtime_fn == NULL || row->call_shape == NULL) {
        if (ctx != NULL && ctx->has_error)
            return NULL;
        goto missing;
    }
    (void)expected_call_shape;
    return row;

missing:
    llvm_set_error(ctx, "%s", missing_message != NULL
        ? missing_message
        : "runtime ABI row is missing");
    return NULL;
}

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
    const MIRResourceRuntimeRow *row = NULL;
    const char *expected_shape =
        llvm_slot_runtime_expected_call_shape(kind, operation);

    if (inst != NULL && inst->resource_runtime_fact_present) {
        row = &inst->resource_runtime_fact;
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
    } else if (inst != NULL && inst->kind == MIR_INST_RESOURCE_OP
               && llvm_slot_runtime_operation_requires_lowered_row(operation)) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR resource operation is missing its lowered runtime-call ABI row");
        return NULL;
    } else {
        row = mir_abi_resource_runtime_row_by_kind(
            kind, inner_type_name, operation);
    }

    if (row == NULL || row->runtime_fn == NULL || row->call_shape == NULL)
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
    if (ctx != NULL && ctx->current_mir_instruction != NULL
        && rir_machine_contact_kind_is_present(
            ctx->current_mir_instruction->machine_contact_kind)
        && !mir_machine_layer_fact_matches_runtime_operation(
            ctx->current_mir_instruction, row->resource_op_name)) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM slot runtime row disagrees with machine-layer runtime operation");
        return NULL;
    }
    return row;
}

static bool
llvm_runtime_export_name(char *out,
    size_t out_size,
    const char *op,
    const char *suffix)
{
    int written;

    if (out == NULL || out_size == 0 || op == NULL || suffix == NULL)
        return false;

    written = snprintf(out, out_size, "pgy_%s_%s", op, suffix);
    return written >= 0 && (size_t)written < out_size;
}

void
llvm_declare_runtime(LLVMGenCtx *ctx)
{
    if (ctx->type_task_handle == NULL) {
        LLVMTypeRef task_handle_fields[] = { ctx->type_i8ptr };
        ctx->type_task_handle = LLVMStructCreateNamed(ctx->context,
            "PgyTaskHandle");
        LLVMStructSetBody(ctx->type_task_handle, task_handle_fields, 1, 0);
    }

    llvm_declare_runtime_core_builtins(ctx);

    {
        LLVMTypeRef allocator_ptr_ty = LLVMPointerType(ctx->type_allocator, 0);
        struct {
            const char *name;
            bool has_capacity;
        } allocator_exports[] = {
            { "pgy_allocator_debug_init", false },
            { "pgy_allocator_destroy_export", false },
            { "pgy_allocator_persistent_init", false },
            { "pgy_allocator_pool_init", true },
            { "pgy_allocator_result_init", false },
            { "pgy_allocator_scratch_init", false },
            { "pgy_allocator_system_init", false },
            { "pgy_allocator_tracing_init", false },
        };
        for (size_t i = 0;
             i < sizeof(allocator_exports) / sizeof(allocator_exports[0]);
             i++) {
            LLVMTypeRef params[2] = { allocator_ptr_ty, ctx->type_i64 };
            unsigned param_count =
                allocator_exports[i].has_capacity ? 2u : 1u;
            LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params,
                param_count, 0);
            LLVMValueRef fn = LLVMAddFunction(ctx->module,
                allocator_exports[i].name, ft);
            llvm_register_function(ctx, allocator_exports[i].name, fn, ft,
                ctx->type_void);
        }
    }

    struct {
        const char *abi_type_name;
        const char *device_abi_type_name;
        const char *suffix;
        LLVMTypeRef slot_ty;
        LLVMTypeRef device_slot_ty;
        LLVMTypeRef val_ty;
    } slot_types[] = {
        { "Slot<Int>",    "DeviceSlot<Int>",    "Int",    ctx->slot_type_Int,    ctx->device_slot_type_Int,    ctx->type_i32   },
        { "Slot<Long>",   "DeviceSlot<Long>",   "Long",   ctx->slot_type_Long,   ctx->device_slot_type_Long,   ctx->type_i64   },
        { "Slot<Float>",  "DeviceSlot<Float>",  "Float",  ctx->slot_type_Float,  ctx->device_slot_type_Float,  ctx->type_f32   },
        { "Slot<Double>", "DeviceSlot<Double>", "Double", ctx->slot_type_Double, ctx->device_slot_type_Double, ctx->type_f64   },
        { "Slot<Bool>",   "DeviceSlot<Bool>",   "Bool",   ctx->slot_type_Bool,   ctx->device_slot_type_Bool,   ctx->type_i1    },
        { "Slot<String>", "DeviceSlot<String>", "String", ctx->slot_type_String, ctx->device_slot_type_String, ctx->type_i8ptr },
    };

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *abi_type_name = slot_types[i].abi_type_name;
        const char *device_abi_type_name = slot_types[i].device_abi_type_name;
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef slot_ty = slot_types[i].slot_ty;
        LLVMTypeRef device_slot_ty = slot_types[i].device_slot_ty;
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        LLVMTypeRef ptr_ty = LLVMPointerType(slot_ty, 0);
        LLVMTypeRef device_ptr_ty = LLVMPointerType(device_slot_ty, 0);

        { LLVMTypeRef ft = LLVMFunctionType(slot_ty, NULL, 0, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, abi_type_name, "Claim",
                  "returns_container",
                  "slot claim runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, slot_ty); }
        { LLVMTypeRef params[] = { ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, abi_type_name, "Write",
                  "container_ptr_value_to_void",
                  "slot write runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, abi_type_name, "Read",
                  "container_ptr_to_value",
                  "slot read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, abi_type_name, "Release",
                  "container_ptr_to_void",
                  "slot release runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_slot_struct_type(ctx, suffix);
          LLVMTypeRef params[] = { ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(pinned_ty, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, abi_type_name, "PinRead",
                  "container_ptr_to_pinned_view",
                  "slot pin-read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty);
          row = llvm_runtime_resource_row_or_error(ctx, abi_type_name,
              "PinWrite", "container_ptr_to_pinned_view",
              "slot pin-write runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty); }
        { LLVMTypeRef pinned_ty = llvm_pinned_slot_struct_type(ctx, suffix);
          LLVMTypeRef params[] = { LLVMPointerType(pinned_ty, 0), ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, abi_type_name,
                  "PinReadInit", "pinned_view_ptr_container_ptr_to_void",
                  "slot pin-read init runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void);
          row = llvm_runtime_resource_row_or_error(ctx, abi_type_name,
              "PinWriteInit", "pinned_view_ptr_container_ptr_to_void",
              "slot pin-write init runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_slot_struct_type(ctx, suffix);
          LLVMTypeRef params[] = { LLVMPointerType(pinned_ty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, abi_type_name, "Unpin",
                  "pinned_view_ptr_to_void",
                  "slot unpin runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef ft = LLVMFunctionType(device_slot_ty, NULL, 0, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, device_abi_type_name,
                  "Claim", "returns_container",
                  "device slot claim runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, device_slot_ty); }
        { LLVMTypeRef params[] = { device_ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, device_abi_type_name,
                  "Write", "container_ptr_value_to_void",
                  "device slot write runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { device_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, device_abi_type_name,
                  "Read", "container_ptr_to_value",
                  "device slot read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { device_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, device_abi_type_name,
                  "Release", "container_ptr_to_void",
                  "device slot release runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { device_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_task_handle, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_runtime_resource_row_or_error(ctx, device_abi_type_name,
                  "SubmitRead", "container_ptr_to_task_handle",
                  "device slot submit-read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_task_handle); }
    }

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        LLVMTypeRef val_ptr_ty = LLVMPointerType(val_ty, 0);
        LLVMTypeRef handle_ptr_ty = LLVMPointerType(ctx->type_i8ptr, 0);
        char fn_name[64];

        { LLVMTypeRef params[] = { val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_new", suffix)) {
              llvm_set_error(ctx, "Rc new runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_clone", suffix)) {
              llvm_set_error(ctx, "Rc clone runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(val_ptr_ty, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_get", suffix)) {
              llvm_set_error(ctx, "Rc get runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ptr_ty); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_downgrade", suffix)) {
              llvm_set_error(ctx, "Rc downgrade runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { handle_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "rc_drop", suffix)) {
              llvm_set_error(ctx, "Rc drop runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { ctx->type_i8ptr };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "weak_upgrade", suffix)) {
              llvm_set_error(ctx, "Weak upgrade runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { handle_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "weak_drop", suffix)) {
              llvm_set_error(ctx, "Weak drop runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }

    for (size_t i = 0; i < sizeof(slot_types) / sizeof(slot_types[0]); i++) {
        const char *suffix = slot_types[i].suffix;
        LLVMTypeRef arr_ty = llvm_array_struct_type(ctx, suffix);
        LLVMTypeRef arr_ptr_ty = LLVMPointerType(arr_ty, 0);
        LLVMTypeRef val_ty = slot_types[i].val_ty;
        char fn_name[64];

        { LLVMTypeRef params[] = { ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(arr_ty, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_new", suffix)) {
              llvm_set_error(ctx, "Array new runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, arr_ty); }
        { LLVMTypeRef params[] = {
              ctx->type_i64, LLVMPointerType(ctx->type_allocator, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_i8ptr, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name),
                  "box_array_new_ptr", suffix)) {
              llvm_set_error(ctx, "BoxArray new runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
              ctx->type_i8ptr); }
        { LLVMTypeRef params[] = { arr_ptr_ty, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_push", suffix)) {
              llvm_set_error(ctx, "Array push runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_get", suffix)) {
              llvm_set_error(ctx, "Array get runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_set", suffix)) {
              llvm_set_error(ctx, "Array set runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { arr_ptr_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_pop", suffix)) {
              llvm_set_error(ctx, "Array pop runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        /* Closure #81: declare pgy_array_sort_<T>(val_ty *data, i64 len)
         * so ArraySort can lower to a runtime call. The runtime function
         * sorts in place; the LLVM emit threads the original array value
         * back as the call result to match the C-backend statement-expr
         * `({ pgy_array_sort_X(arr.data, arr.length); arr; })`. */
        { LLVMTypeRef val_ptr_ty = LLVMPointerType(val_ty, 0);
          LLVMTypeRef params[] = { val_ptr_ty, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_sort", suffix)) {
              llvm_set_error(ctx, "Array sort runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(llvm_slice_struct_type(ctx, suffix), 0),
                                   ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(val_ty, params, 2, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "slice_get", suffix)) {
              llvm_set_error(ctx, "Slice get runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, val_ty); }
        { LLVMTypeRef params[] = { LLVMPointerType(llvm_slice_struct_type(ctx, suffix), 0),
                                   ctx->type_i64, val_ty };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "slice_set", suffix)) {
              llvm_set_error(ctx, "Slice set runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(llvm_slice_struct_type(ctx, suffix), 0) };
          LLVMTypeRef ft = LLVMFunctionType(arr_ty, params, 1, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "slice_copy", suffix)) {
              llvm_set_error(ctx, "Slice copy runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, arr_ty); }
        { LLVMTypeRef params[] = { arr_ptr_ty, ctx->type_i64, ctx->type_i64 };
          LLVMTypeRef ft = LLVMFunctionType(llvm_slice_struct_type(ctx, suffix), params, 3, 0);
          if (!llvm_runtime_export_name(fn_name, sizeof(fn_name), "array_slice", suffix)) {
              llvm_set_error(ctx, "Array slice runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fn_name, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft,
              llvm_slice_struct_type(ctx, suffix)); }
    }

    llvm_declare_runtime_task_memory(ctx);
    llvm_declare_runtime_raw_collections(ctx);
    llvm_declare_runtime_channels(ctx);
    llvm_declare_runtime_secure_slots(ctx);
}

#endif
