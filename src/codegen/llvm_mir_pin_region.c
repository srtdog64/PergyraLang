/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR pin-region enter/exit emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <stdio.h>

#include "../compiler/mir_cfg_contract_pin.h"
#include "../common/string_compat.h"

static bool
llvm_mir_pin_local_name(LLVMGenCtx *ctx, const MIRBasicBlock *block,
                        char *buf, size_t buf_size)
{
    int written;

    if (buf == NULL || buf_size == 0)
        return false;
    written = snprintf(buf, buf_size, "__pgy_mir_pin_%zu",
                       block != NULL ? block->id : 0);
    if (written >= 0 && (size_t)written < buf_size)
        return true;
    llvm_set_mir_topology_invalid(ctx, "MIR pin local name is too long");
    return false;
}

static bool
llvm_mir_pin_token_name(LLVMGenCtx *ctx, char *buf, size_t buf_size,
                        const char *name)
{
    int written;

    if (buf == NULL || buf_size == 0 || name == NULL)
        return false;
    written = snprintf(buf, buf_size, "%s_token", name);
    if (written >= 0 && (size_t)written < buf_size)
        return true;
    llvm_set_mir_topology_invalid(ctx, "MIR pin token name is too long");
    return false;
}

static bool
llvm_mir_pin_init_name(LLVMGenCtx *ctx, char *buf, size_t buf_size,
                       bool is_write, const char *inner)
{
    int written;

    if (buf == NULL || buf_size == 0 || inner == NULL)
        return false;
    written = snprintf(buf, buf_size, "pgy_secure_pin_%s_init_%s",
                       is_write ? "write" : "read", inner);
    if (written >= 0 && (size_t)written < buf_size)
        return true;
    llvm_set_mir_topology_invalid(ctx,
        "MIR pin init runtime name is too long");
    return false;
}

static bool
llvm_mir_emit_pin_panic_if(LLVMGenCtx *ctx, LLVMValueRef cond,
                           const char *reason)
{
    LLVMFuncEntry *panic_fn;
    LLVMBasicBlockRef fail_bb;
    LLVMBasicBlockRef cont_bb;
    LLVMValueRef reason_arg;

    if (ctx == NULL || cond == NULL)
        return false;
    if (ctx->current_function == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR pin guard requires an active function");
        return false;
    }
    panic_fn = llvm_lookup_function(ctx,
        "pgy_runtime_panic_internal_invariant_export");
    if (panic_fn == NULL || panic_fn->fn == NULL) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR pin guard requires registered runtime function '%s'",
            "pgy_runtime_panic_internal_invariant_export");
        return false;
    }

    fail_bb = LLVMAppendBasicBlockInContext(ctx->context,
        ctx->current_function, "pgy.pin.guard.fail");
    cont_bb = LLVMAppendBasicBlockInContext(ctx->context,
        ctx->current_function, "pgy.pin.guard.cont");
    LLVMBuildCondBr(ctx->builder, cond, fail_bb, cont_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    reason_arg = LLVMBuildGlobalStringPtr(ctx->builder,
        reason != NULL ? reason : "slot pin guard failed", llvm_tmp_name(ctx));
    LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
                   &reason_arg, 1, "");
    LLVMBuildUnreachable(ctx->builder);

    LLVMPositionBuilderAtEnd(ctx->builder, cont_bb);
    return true;
}

static bool
llvm_mir_emit_plain_pin_inline_enter(LLVMGenCtx *ctx,
                                     LLVMTypeRef slot_ty,
                                     LLVMTypeRef pin_ty,
                                     LLVMValueRef pin_alloca,
                                     LLVMValueRef slot_ptr_arg,
                                     bool is_write)
{
    LLVMValueRef null_slot;
    LLVMValueRef is_null;
    LLVMValueRef occupied_ptr;
    LLVMValueRef occupied;
    LLVMValueRef is_released;
    LLVMValueRef slot_field;
    LLVMValueRef active_field;
    LLVMValueRef write_field;

    if (ctx == NULL || slot_ty == NULL || pin_ty == NULL
        || pin_alloca == NULL || slot_ptr_arg == NULL) {
        return false;
    }

    null_slot = LLVMConstNull(LLVMTypeOf(slot_ptr_arg));
    is_null = LLVMBuildICmp(ctx->builder, LLVMIntEQ, slot_ptr_arg, null_slot,
                            llvm_tmp_name(ctx));
    if (!llvm_mir_emit_pin_panic_if(ctx, is_null,
            is_write ? "null slot pin write" : "null slot pin read")) {
        return false;
    }

    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, slot_ptr_arg, 1,
                                       llvm_tmp_name(ctx));
    occupied = LLVMBuildLoad2(ctx->builder, ctx->type_i1, occupied_ptr,
                              llvm_tmp_name(ctx));
    is_released = LLVMBuildICmp(ctx->builder, LLVMIntEQ, occupied,
        LLVMConstInt(ctx->type_i1, 0, 0), llvm_tmp_name(ctx));
    if (!llvm_mir_emit_pin_panic_if(ctx, is_released,
            is_write ? "released slot write" : "released slot read")) {
        return false;
    }

    slot_field = LLVMBuildStructGEP2(ctx->builder, pin_ty, pin_alloca, 0,
                                     llvm_tmp_name(ctx));
    active_field = LLVMBuildStructGEP2(ctx->builder, pin_ty, pin_alloca, 1,
                                       llvm_tmp_name(ctx));
    write_field = LLVMBuildStructGEP2(ctx->builder, pin_ty, pin_alloca, 2,
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, slot_ptr_arg, slot_field);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0),
                   active_field);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, is_write ? 1 : 0, 0),
                   write_field);
    return true;
}

static bool
llvm_mir_emit_plain_pin_inline_exit(LLVMGenCtx *ctx,
                                    LLVMTypeRef slot_ty,
                                    LLVMTypeRef pin_ty,
                                    LLVMValueRef pin_alloca)
{
    LLVMValueRef slot_field;
    LLVMValueRef active_field;
    LLVMValueRef null_slot;

    if (ctx == NULL || slot_ty == NULL || pin_ty == NULL || pin_alloca == NULL)
        return false;
    slot_field = LLVMBuildStructGEP2(ctx->builder, pin_ty, pin_alloca, 0,
                                     llvm_tmp_name(ctx));
    active_field = LLVMBuildStructGEP2(ctx->builder, pin_ty, pin_alloca, 1,
                                       llvm_tmp_name(ctx));
    null_slot = LLVMConstNull(LLVMPointerType(slot_ty, 0));
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0),
                   active_field);
    LLVMBuildStore(ctx->builder, null_slot, slot_field);
    return true;
}

static bool
llvm_mir_unpin_name(LLVMGenCtx *ctx, char *buf, size_t buf_size,
                    bool is_secure, const char *inner)
{
    int written;

    if (buf == NULL || buf_size == 0 || inner == NULL)
        return false;
    written = snprintf(buf, buf_size,
                       is_secure ? "pgy_secure_unpin_%s" : "pgy_unpin_%s",
                       inner);
    if (written >= 0 && (size_t)written < buf_size)
        return true;
    llvm_set_mir_topology_invalid(ctx,
        "MIR pin cleanup runtime name is too long");
    return false;
}

static LLVMValueRef
llvm_mir_slot_pointer_arg(LLVMGenCtx *ctx, LLVMVarEntry *entry)
{
    if (ctx == NULL || entry == NULL)
        return NULL;
    if (entry->type != NULL
        && LLVMGetTypeKind(entry->type) == LLVMPointerTypeKind) {
        return LLVMBuildLoad2(ctx->builder, entry->type, entry->alloca,
                              llvm_tmp_name(ctx));
    }
    return entry->alloca;
}

bool
llvm_mir_emit_pin_enter(const MIRBasicBlock *block, LLVMGenCtx *ctx)
{
    const char *inner;
    bool is_secure;
    LLVMVarEntry slot_entry;
    LLVMTypeRef pin_ty;
    LLVMValueRef pin_alloca;
    LLVMFuncEntry *pin_fn;
    LLVMValueRef args[3];
    LLVMValueRef slot_ptr_arg;
    LLVMVarEntry view_entry;
    char pin_name[64];
    char fn_name[128];
    char token_name[256];

    if (block == NULL || ctx == NULL || !block->is_pin_region)
        return true;
    if (block->pin_source_name == NULL)
        return true;

    inner = llvm_lookup_slot_inner(ctx, block->pin_source_name);
    if (inner == NULL
        || !llvm_scope_lookup_snapshot(ctx, block->pin_source_name,
            &slot_entry)
        || slot_entry.alloca == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR pin block cannot resolve source slot");
        return false;
    }

    is_secure = llvm_lookup_slot_is_secure(ctx, block->pin_source_name);
    if (!llvm_mir_pin_local_name(ctx, block, pin_name, sizeof(pin_name)))
        return false;
    slot_ptr_arg = llvm_mir_slot_pointer_arg(ctx, &slot_entry);
    if (is_secure) {
        LLVMVarEntry token_entry;
        LLVMValueRef token_alloca;
        if (!llvm_mir_pin_token_name(ctx, token_name, sizeof(token_name),
                block->pin_source_name))
            return false;
        if (!llvm_scope_lookup_snapshot(ctx, token_name, &token_entry)
            || token_entry.alloca == NULL) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR secure pin block cannot resolve paired token");
            return false;
        }
        token_alloca = token_entry.alloca;
        if (!llvm_mir_pin_init_name(ctx, fn_name, sizeof(fn_name),
                block->pin_view_is_write, inner))
            return false;
        pin_ty = llvm_pinned_secure_slot_struct_type(ctx, inner);
        pin_fn = llvm_lookup_function(ctx, fn_name);
        if (pin_fn == NULL || pin_fn->fn == NULL) {
            llvm_set_error_at_with_hints(ctx, NULL,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR secure pin requires registered runtime function '%s'",
                fn_name);
            return false;
        }
        pin_alloca = llvm_create_entry_alloca(ctx, pin_ty, pin_name);
        args[0] = pin_alloca;
        args[1] = slot_ptr_arg;
        args[2] = token_alloca;
        LLVMBuildCall2(ctx->builder, pin_fn->fn_type, pin_fn->fn,
                       args, 3, "");
    } else {
        if (!mir_block_has_pin_guard_amortization_region(block)) {
            llvm_set_mir_topology_invalid(ctx,
                mir_block_pin_guard_amortization_missing_reason(block));
            return false;
        }
        pin_ty = llvm_pinned_slot_struct_type(ctx, inner);
        if (pin_ty == NULL || ctx->has_error)
            return false;
        pin_alloca = llvm_create_entry_alloca(ctx, pin_ty, pin_name);
        if (!llvm_mir_emit_plain_pin_inline_enter(ctx,
                llvm_slot_struct_type(ctx, inner), pin_ty, pin_alloca,
                slot_ptr_arg, block->pin_view_is_write)) {
            return false;
        }
    }

    llvm_scope_declare(ctx, pergyra_strdup(pin_name), pin_alloca, pin_ty);
    if (block->pin_view_name != NULL) {
        if (!llvm_scope_lookup_snapshot(ctx, block->pin_view_name,
                &view_entry)) {
            LLVMTypeRef view_slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            llvm_scope_declare(ctx, pergyra_strdup(block->pin_view_name),
                               slot_ptr_arg, view_slot_ty);
        }
        llvm_register_slot_var(ctx, pergyra_strdup(block->pin_view_name),
                               inner, is_secure);
        if (is_secure) {
            LLVMVarEntry token_entry;
            if (!llvm_mir_pin_token_name(ctx, token_name, sizeof(token_name),
                    block->pin_source_name))
                return false;
            if (llvm_scope_lookup_snapshot(ctx, token_name, &token_entry)) {
                char view_token_name[256];
                LLVMValueRef token_alloca = token_entry.alloca;
                LLVMTypeRef token_type = token_entry.type;
                if (!llvm_mir_pin_token_name(ctx, view_token_name,
                        sizeof(view_token_name), block->pin_view_name))
                    return false;
                {
                    LLVMVarEntry existing_token_entry;
                    if (!llvm_scope_lookup_snapshot(ctx, view_token_name,
                            &existing_token_entry)) {
                        llvm_scope_declare(ctx, pergyra_strdup(view_token_name),
                                           token_alloca, token_type);
                    }
                }
            }
        }
    }
    return true;
}

bool
llvm_mir_emit_pin_exit(const MIRBasicBlock *block, LLVMGenCtx *ctx)
{
    const char *inner;
    bool is_secure;
    LLVMVarEntry pin_entry;
    LLVMFuncEntry *unpin_fn;
    LLVMValueRef args[1];
    char pin_name[64];
    char fn_name[128];

    if (block == NULL || ctx == NULL || !block->is_pin_region)
        return true;
    if (block->pin_source_name == NULL)
        return true;

    inner = llvm_lookup_slot_inner(ctx, block->pin_source_name);
    if (inner == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR pin block cannot resolve source slot at exit");
        return false;
    }

    is_secure = llvm_lookup_slot_is_secure(ctx, block->pin_source_name);
    if (!llvm_mir_pin_local_name(ctx, block, pin_name, sizeof(pin_name)))
        return false;
    if (!llvm_scope_lookup_snapshot(ctx, pin_name, &pin_entry)
        || pin_entry.alloca == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR pin block cannot resolve pin local at exit");
        return false;
    }

    if (!is_secure)
        return llvm_mir_emit_plain_pin_inline_exit(ctx,
            llvm_slot_struct_type(ctx, inner), pin_entry.type,
            pin_entry.alloca);

    if (!llvm_mir_unpin_name(ctx, fn_name, sizeof(fn_name), is_secure, inner))
        return false;
    unpin_fn = llvm_lookup_function(ctx, fn_name);
    if (unpin_fn == NULL || unpin_fn->fn == NULL) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR pin cleanup requires registered runtime function '%s'",
            fn_name);
        return false;
    }

    args[0] = pin_entry.alloca;
    LLVMBuildCall2(ctx->builder, unpin_fn->fn_type, unpin_fn->fn,
                   args, 1, "");
    return true;
}

#endif /* PGY_LLVM_ENABLED */
