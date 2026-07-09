/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend secure-slot runtime declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

#include "../compiler/mir_abi_layout.h"

#include <string.h>

static const MIRResourceRuntimeRow *
llvm_secure_runtime_resource_row_or_error(LLVMGenCtx *ctx,
    const char *abi_type_name,
    const char *operation,
    const char *expected_call_shape,
    const char *missing_message)
{
    const MIRResourceRuntimeRow *row =
        mir_abi_resource_runtime_row_by_type_name(abi_type_name, operation);

    if (row == NULL || row->runtime_fn == NULL || row->call_shape == NULL) {
        llvm_set_error(ctx, "%s", missing_message != NULL
            ? missing_message
            : "secure slot runtime ABI row is missing");
        return NULL;
    }
    if (expected_call_shape == NULL ||
        strcmp(row->call_shape, expected_call_shape) != 0) {
        llvm_set_error(ctx,
            "LLVM secure slot declaration for %s %s requires MIR ABI call shape %s",
            abi_type_name != NULL ? abi_type_name : "<unknown>",
            operation != NULL ? operation : "<unknown>",
            expected_call_shape != NULL ? expected_call_shape : "<missing>");
        return NULL;
    }
    return row;
}

void
llvm_declare_runtime_secure_slots(LLVMGenCtx *ctx)
{
    struct {
        const char *abi_type_name;
        const char *suffix;
        LLVMTypeRef val_ty;
    } slot_types[] = {
        { "SecureSlot<Int>",    "Int",    ctx->type_i32   },
        { "SecureSlot<Long>",   "Long",   ctx->type_i64   },
        { "SecureSlot<Float>",  "Float",  ctx->type_f32   },
        { "SecureSlot<Double>", "Double", ctx->type_f64   },
        { "SecureSlot<Bool>",   "Bool",   ctx->type_i1    },
        { "SecureSlot<String>", "String", ctx->type_i8ptr },
    };

    for (size_t si = 0; si < sizeof(slot_types) / sizeof(slot_types[0]); si++) {
        const char *abi_type_name = slot_types[si].abi_type_name;
        const char *suf = slot_types[si].suffix;
        LLVMTypeRef sty = llvm_secure_slot_struct_type(ctx, suf);
        LLVMTypeRef tty = llvm_secure_token_type(ctx, suf);
        LLVMTypeRef vt = slot_types[si].val_ty;

        { LLVMTypeRef params[] = { LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(sty, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
                  "Claim", "token_ptr_to_container",
                  "secure slot claim runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, sty); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), vt, LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
                  "Write", "container_ptr_value_token_ptr_to_void",
                  "secure slot write runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
                  "Read", "container_ptr_token_ptr_to_value",
                  "secure slot read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
                  "Release", "container_ptr_token_ptr_to_void",
                  "secure slot release runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(pinned_ty, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
                  "PinRead", "container_ptr_token_ptr_to_pinned_view",
                  "secure slot pin-read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty);
          row = llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
              "PinWrite", "container_ptr_token_ptr_to_pinned_view",
              "secure slot pin-write runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = {
              LLVMPointerType(pinned_ty, 0),
              LLVMPointerType(sty, 0),
              LLVMPointerType(tty, 0)
          };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
                  "PinReadInit",
                  "pinned_view_ptr_container_ptr_token_ptr_to_void",
                  "secure slot pin-read init runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void);
          row = llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
              "PinWriteInit",
              "pinned_view_ptr_container_ptr_token_ptr_to_void",
              "secure slot pin-write init runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = { LLVMPointerType(pinned_ty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, abi_type_name,
                  "Unpin", "pinned_view_ptr_to_void",
                  "secure slot unpin runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif
