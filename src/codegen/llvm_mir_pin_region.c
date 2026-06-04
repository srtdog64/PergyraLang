/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR pin-region enter/exit emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

#include <stdio.h>

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
                       bool is_secure, bool is_write, const char *inner)
{
    int written;

    if (buf == NULL || buf_size == 0 || inner == NULL)
        return false;
    written = snprintf(buf, buf_size,
                       is_secure ? "pgy_secure_pin_%s_init_%s"
                                 : "pgy_pin_%s_init_%s",
                       is_write ? "write" : "read", inner);
    if (written >= 0 && (size_t)written < buf_size)
        return true;
    llvm_set_mir_topology_invalid(ctx,
        "MIR pin init runtime name is too long");
    return false;
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
    LLVMVarEntry *slot_entry;
    LLVMTypeRef pin_ty;
    LLVMValueRef pin_alloca;
    LLVMFuncEntry *pin_fn;
    LLVMValueRef args[3];
    LLVMValueRef slot_ptr_arg;
    LLVMVarEntry *view_entry;
    char pin_name[64];
    char fn_name[128];
    char token_name[256];

    if (block == NULL || ctx == NULL || !block->is_pin_region)
        return true;
    if (block->pin_source_name == NULL)
        return true;

    inner = llvm_lookup_slot_inner(ctx, block->pin_source_name);
    slot_entry = llvm_scope_lookup(ctx, block->pin_source_name);
    if (inner == NULL || slot_entry == NULL || slot_entry->alloca == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR pin block cannot resolve source slot");
        return false;
    }

    is_secure = llvm_lookup_slot_is_secure(ctx, block->pin_source_name);
    if (!llvm_mir_pin_local_name(ctx, block, pin_name, sizeof(pin_name)))
        return false;
    slot_ptr_arg = llvm_mir_slot_pointer_arg(ctx, slot_entry);
    if (is_secure) {
        LLVMVarEntry *token_entry;
        LLVMValueRef token_alloca;
        if (!llvm_mir_pin_token_name(ctx, token_name, sizeof(token_name),
                block->pin_source_name))
            return false;
        token_entry = llvm_scope_lookup(ctx, token_name);
        if (token_entry == NULL || token_entry->alloca == NULL) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR secure pin block cannot resolve paired token");
            return false;
        }
        token_alloca = token_entry->alloca;
        if (!llvm_mir_pin_init_name(ctx, fn_name, sizeof(fn_name), true,
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
        if (!llvm_mir_pin_init_name(ctx, fn_name, sizeof(fn_name), false,
                block->pin_view_is_write, inner))
            return false;
        pin_ty = llvm_pinned_slot_struct_type(ctx, inner);
        pin_fn = llvm_lookup_function(ctx, fn_name);
        if (pin_fn == NULL || pin_fn->fn == NULL) {
            llvm_set_error_at_with_hints(ctx, NULL,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR pin requires registered runtime function '%s'",
                fn_name);
            return false;
        }
        pin_alloca = llvm_create_entry_alloca(ctx, pin_ty, pin_name);
        args[0] = pin_alloca;
        args[1] = slot_ptr_arg;
        LLVMBuildCall2(ctx->builder, pin_fn->fn_type, pin_fn->fn,
                       args, 2, "");
    }

    llvm_scope_declare(ctx, pergyra_strdup(pin_name), pin_alloca, pin_ty);
    if (block->pin_view_name != NULL) {
        view_entry = llvm_scope_lookup(ctx, block->pin_view_name);
        if (view_entry == NULL) {
            LLVMTypeRef view_slot_ty = is_secure
                ? llvm_secure_slot_struct_type(ctx, inner)
                : llvm_slot_struct_type(ctx, inner);
            llvm_scope_declare(ctx, pergyra_strdup(block->pin_view_name),
                               slot_ptr_arg, view_slot_ty);
        }
        llvm_register_slot_var(ctx, pergyra_strdup(block->pin_view_name),
                               inner, is_secure);
        if (is_secure) {
            LLVMVarEntry *token_entry;
            if (!llvm_mir_pin_token_name(ctx, token_name, sizeof(token_name),
                    block->pin_source_name))
                return false;
            token_entry = llvm_scope_lookup(ctx, token_name);
            if (token_entry != NULL) {
                char view_token_name[256];
                LLVMValueRef token_alloca = token_entry->alloca;
                LLVMTypeRef token_type = token_entry->type;
                if (!llvm_mir_pin_token_name(ctx, view_token_name,
                        sizeof(view_token_name), block->pin_view_name))
                    return false;
                if (llvm_scope_lookup(ctx, view_token_name) == NULL) {
                    llvm_scope_declare(ctx, pergyra_strdup(view_token_name),
                                       token_alloca, token_type);
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
    LLVMVarEntry *pin_entry;
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
    pin_entry = llvm_scope_lookup(ctx, pin_name);
    if (pin_entry == NULL || pin_entry->alloca == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR pin block cannot resolve pin local at exit");
        return false;
    }

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

    args[0] = pin_entry->alloca;
    LLVMBuildCall2(ctx->builder, unpin_fn->fn_type, unpin_fn->fn,
                   args, 1, "");
    return true;
}

#endif /* PGY_LLVM_ENABLED */
