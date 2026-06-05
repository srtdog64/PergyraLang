/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM slot read/write/release lowering utilities.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "codegen_slot_type_policy.h"

bool
llvm_slot_inner_has_external_runtime_helpers(const char *inner)
{
    return pgy_classify_type(inner) != PGY_TK_UNKNOWN;
}

LLVMValueRef
llvm_direct_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                      const char *inner)
{
    LLVMTypeRef inner_ty;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;
    LLVMValueRef value_ptr;

    if (slot_var == NULL || inner == NULL)
        return NULL;

    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || inner_ty == NULL)
        return NULL;
    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        slot_ty = llvm_slot_struct_type(ctx, inner);
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 0, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, inner_ty, value_ptr,
                          llvm_tmp_name(ctx));
}

void
llvm_direct_secure_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                              LLVMValueRef value)
{
    llvm_direct_slot_write(ctx, slot_var, value);
}

void
llvm_emit_structural_secure_slot_write(LLVMGenCtx *ctx,
                                       LLVMVarEntry *slot_var,
                                       LLVMValueRef value)
{
    llvm_direct_secure_slot_write(ctx, slot_var, value);
}

LLVMValueRef
llvm_direct_secure_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                             const char *inner)
{
    LLVMTypeRef inner_ty;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;
    LLVMValueRef value_ptr;

    if (slot_var == NULL || inner == NULL)
        return NULL;
    inner_ty = pergyra_type_to_llvm(ctx, inner);
    if (ctx->has_error || inner_ty == NULL)
        return NULL;
    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        slot_ty = llvm_secure_slot_struct_type(ctx, inner);
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 0, llvm_tmp_name(ctx));
    return LLVMBuildLoad2(ctx->builder, inner_ty, value_ptr,
                          llvm_tmp_name(ctx));
}

LLVMValueRef
llvm_emit_structural_secure_slot_read(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                                      const char *inner)
{
    return llvm_direct_secure_slot_read(ctx, slot_var, inner);
}

void
llvm_direct_secure_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    LLVMValueRef token_ptr;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;

    llvm_direct_slot_release(ctx, slot_var);
    if (slot_var == NULL)
        return;

    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        const char *inner = llvm_lookup_slot_inner(ctx, slot_var->name);
        if (inner == NULL || inner[0] == '\0') {
            if (!ctx->has_error) {
                llvm_set_error_at_with_hints(ctx, NULL,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                    PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                    "LLVM secure slot release for '%s' requires concrete SecureSlot<T> metadata",
                    slot_var->name != NULL ? slot_var->name : "<slot>");
            }
            return;
        }
        slot_ty = llvm_secure_slot_struct_type(ctx, inner);
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    token_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 2, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(ctx->type_i64, 0, 0), token_ptr);
}

void
llvm_emit_structural_secure_slot_release(LLVMGenCtx *ctx,
                                         LLVMVarEntry *slot_var)
{
    llvm_direct_secure_slot_release(ctx, slot_var);
}

LLVMTypeRef
llvm_required_slot_struct_type_for_var(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                                       bool secure)
{
    const char *inner;
    if (ctx == NULL || slot_var == NULL)
        return NULL;
    inner = llvm_lookup_slot_inner(ctx, slot_var->name);
    if (inner == NULL || inner[0] == '\0') {
        if (!ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, NULL,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                "LLVM slot helper for '%s' requires concrete Slot<T> metadata",
                slot_var->name != NULL ? slot_var->name : "<slot>");
        }
        return NULL;
    }
    return secure ? llvm_secure_slot_struct_type(ctx, inner)
                  : llvm_slot_struct_type(ctx, inner);
}

void
llvm_direct_slot_write(LLVMGenCtx *ctx, LLVMVarEntry *slot_var,
                       LLVMValueRef value)
{
    LLVMValueRef value_ptr;
    LLVMValueRef occupied_ptr;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;

    if (slot_var == NULL || value == NULL)
        return;

    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        bool secure = llvm_lookup_slot_is_secure(ctx, slot_var->name);
        slot_ty = llvm_required_slot_struct_type_for_var(ctx, slot_var, secure);
        if (slot_ty == NULL)
            return;
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 0, llvm_tmp_name(ctx));
    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, value, value_ptr);
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        occupied_ptr);
}

void
llvm_direct_slot_release(LLVMGenCtx *ctx, LLVMVarEntry *slot_var)
{
    LLVMValueRef occupied_ptr;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_ptr;

    if (slot_var == NULL)
        return;

    if (slot_var->type != NULL
        && LLVMGetTypeKind(slot_var->type) == LLVMPointerTypeKind) {
        bool secure = llvm_lookup_slot_is_secure(ctx, slot_var->name);
        slot_ty = llvm_required_slot_struct_type_for_var(ctx, slot_var, secure);
        if (slot_ty == NULL)
            return;
        slot_ptr = LLVMBuildLoad2(ctx->builder, slot_var->type,
                                  slot_var->alloca, llvm_tmp_name(ctx));
    } else {
        slot_ty = slot_var->type;
        slot_ptr = slot_var->alloca;
    }
    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty,
        slot_ptr, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0),
        occupied_ptr);
}

LLVMValueRef
llvm_slot_runtime_arg(LLVMGenCtx *ctx, LLVMVarEntry *var)
{
    if (ctx == NULL || var == NULL)
        return NULL;
    if (var->type != NULL && LLVMGetTypeKind(var->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, var->type, var->alloca,
                              llvm_tmp_name(ctx));
    }
    return var->alloca;
}

bool
llvm_require_secure_token_var(LLVMGenCtx *ctx, ASTNode *node,
                              const char *slot_name,
                              const char *operation_name,
                              LLVMVarEntry *out)
{
    bool has_token_var =
        llvm_lookup_secure_token_var(ctx, slot_name, out);
    if (!has_token_var && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM secure slot %s requires paired token binding '%s_token'",
            operation_name != NULL ? operation_name : "operation",
            slot_name != NULL ? slot_name : "<slot>");
    }
    return has_token_var;
}

#endif /* PGY_LLVM_ENABLED */
