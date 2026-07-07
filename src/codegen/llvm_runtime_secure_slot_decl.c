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

#include <stdio.h>

static bool
llvm_runtime_secure_slot_name(char *out,
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
        char fname[128];

        { LLVMTypeRef params[] = { LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(sty, params, 1, 0);
          const char *runtime_fn =
              mir_abi_resource_runtime_fn_by_type_name(abi_type_name, "Claim");
          if (runtime_fn == NULL) {
              llvm_set_error(ctx, "secure slot claim runtime ABI row is missing");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, sty); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), vt, LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          const char *runtime_fn =
              mir_abi_resource_runtime_fn_by_type_name(abi_type_name, "Write");
          if (runtime_fn == NULL) {
              llvm_set_error(ctx, "secure slot write runtime ABI row is missing");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
          const char *runtime_fn =
              mir_abi_resource_runtime_fn_by_type_name(abi_type_name, "Read");
          if (runtime_fn == NULL) {
              llvm_set_error(ctx, "secure slot read runtime ABI row is missing");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          const char *runtime_fn =
              mir_abi_resource_runtime_fn_by_type_name(abi_type_name, "Release");
          if (runtime_fn == NULL) {
              llvm_set_error(ctx, "secure slot release runtime ABI row is missing");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, runtime_fn, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(pinned_ty, params, 2, 0);
          if (!llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_pin_read", suf)) {
              llvm_set_error(ctx, "secure slot pin-read runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty);
          if (!llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_pin_write", suf)) {
              llvm_set_error(ctx, "secure slot pin-write runtime name is too long");
              return;
          }
          fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = {
              LLVMPointerType(pinned_ty, 0),
              LLVMPointerType(sty, 0),
              LLVMPointerType(tty, 0)
          };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          if (!llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_pin_read_init", suf)) {
              llvm_set_error(ctx, "secure slot pin-read init runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void);
          if (!llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_pin_write_init", suf)) {
              llvm_set_error(ctx, "secure slot pin-write init runtime name is too long");
              return;
          }
          fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = { LLVMPointerType(pinned_ty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          if (!llvm_runtime_secure_slot_name(fname, sizeof(fname), "secure_unpin", suf)) {
              llvm_set_error(ctx, "secure slot unpin runtime name is too long");
              return;
          }
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif
