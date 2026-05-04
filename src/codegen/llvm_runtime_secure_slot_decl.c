/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend secure-slot runtime declarations.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

#include <stdio.h>

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
        char fname[128];

        { LLVMTypeRef params[] = { LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(sty, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_claim_secure_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, sty); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), vt, LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_write_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(vt, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_read_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, vt); }
        { LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_release_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = { LLVMPointerType(sty, 0), LLVMPointerType(tty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(pinned_ty, params, 2, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_pin_read_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty);
          snprintf(fname, sizeof(fname), "pgy_secure_pin_write_%s", suf);
          fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, pinned_ty); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = {
              LLVMPointerType(pinned_ty, 0),
              LLVMPointerType(sty, 0),
              LLVMPointerType(tty, 0)
          };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 3, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_pin_read_init_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void);
          snprintf(fname, sizeof(fname), "pgy_secure_pin_write_init_%s", suf);
          fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
        { LLVMTypeRef pinned_ty = llvm_pinned_secure_slot_struct_type(ctx, suf);
          LLVMTypeRef params[] = { LLVMPointerType(pinned_ty, 0) };
          LLVMTypeRef ft = LLVMFunctionType(ctx->type_void, params, 1, 0);
          snprintf(fname, sizeof(fname), "pgy_secure_unpin_%s", suf);
          LLVMValueRef fn = LLVMAddFunction(ctx->module, fname, ft);
          llvm_register_function(ctx, LLVMGetValueName(fn), fn, ft, ctx->type_void); }
    }
}

#endif
