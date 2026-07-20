/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend secure-slot runtime declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

static const MIRResourceRuntimeRow *
llvm_secure_runtime_resource_row_or_error(LLVMGenCtx *ctx,
    const char *inner_type_name,
    const char *operation,
    const char *missing_message)
{
    const MIRResourceRuntimeRow *row =
        llvm_slot_runtime_row_for_operation(
            NULL, ctx, MIR_RESOURCE_ABI_SECURE_SLOT, inner_type_name, operation);

    if (row == NULL || row->runtime_fn == NULL || row->call_shape == NULL) {
        if (ctx != NULL && ctx->has_error)
            return NULL;
        llvm_set_error(ctx, "%s", missing_message != NULL
            ? missing_message
            : "secure slot runtime ABI row is missing");
        return NULL;
    }
    return row;
}

void
llvm_declare_runtime_secure_slots(LLVMGenCtx *ctx)
{
    struct {
        const char *suffix;
        LLVMTypeRef val_ty;
    } slot_types[] = {
        { "Int",    ctx->type_i32   },
        { "Long",   ctx->type_i64   },
        { "Float",  ctx->type_f32   },
        { "Double", ctx->type_f64   },
        { "Bool",   ctx->type_i1    },
        { "String", ctx->type_i8ptr },
    };

    for (size_t si = 0; si < sizeof(slot_types) / sizeof(slot_types[0]); si++) {
        const char *suf = slot_types[si].suffix;
        LLVMTypeRef sty = llvm_secure_slot_struct_type(ctx, suf);
        LLVMTypeRef tty = llvm_secure_token_type(ctx, suf);
        LLVMTypeRef vt = slot_types[si].val_ty;

        { LLVMTypeRef params[] = { LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(sty, params, 1, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, suf, "Claim",
                  "secure slot claim runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, sty); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), vt, LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, suf, "Write",
                  "secure slot write runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, suf, "Read",
                  "secure slot read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          const MIRResourceRuntimeRow *row =
              llvm_secure_runtime_resource_row_or_error(ctx, suf, "Release",
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
              llvm_secure_runtime_resource_row_or_error(ctx, suf, "PinRead",
                  "secure slot pin-read runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty);
          row = llvm_secure_runtime_resource_row_or_error(ctx, suf, "PinWrite",
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
              llvm_secure_runtime_resource_row_or_error(ctx, suf, "PinReadInit",
                  "secure slot pin-read init runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void);
          row = llvm_secure_runtime_resource_row_or_error(ctx, suf,
              "PinWriteInit",
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
              llvm_secure_runtime_resource_row_or_error(ctx, suf, "Unpin",
                  "secure slot unpin runtime ABI row is missing");
          if (row == NULL) {
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, row->runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif
