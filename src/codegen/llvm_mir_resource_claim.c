#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_runtime_internal.h"

#include <stdio.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir_abi_layout.h"

static const char *
llvm_mir_claim_inner_type_name(const MIRInstruction *inst,
                               char *buf,
                               size_t buf_size)
{
    const char *abi_name;
    const char *open;
    const char *close;
    size_t len;

    if (inst == NULL || buf == NULL || buf_size == 0) {
        return NULL;
    }
    abi_name = inst->abi_type_name != NULL
        ? inst->abi_type_name
        : (inst->type_layout != NULL ? inst->type_layout->abi_type_name : NULL);
    if (abi_name == NULL)
        return NULL;
    open = strchr(abi_name, '<');
    close = strrchr(abi_name, '>');
    if (open == NULL || close == NULL || close <= open + 1)
        return NULL;
    len = (size_t)(close - open - 1);
    if (len >= buf_size)
        return NULL;
    memcpy(buf, open + 1, len);
    buf[len] = '\0';
    return buf[0] != '\0' ? buf : NULL;
}

static bool
llvm_mir_claim_is_secure(const MIRInstruction *inst)
{
    const char *abi_name = inst != NULL && inst->abi_type_name != NULL
        ? inst->abi_type_name
        : (inst != NULL && inst->type_layout != NULL
            ? inst->type_layout->abi_type_name
            : NULL);
    if (abi_name != NULL && strncmp(abi_name, "SecureSlot<", 11) == 0)
        return true;
    return inst != NULL && inst->arg1 != NULL
        && strcmp(inst->arg1, "SecureSlot") == 0;
}

void
llvm_mir_emit_with_claim_only(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    const char *alias;
    bool is_secure;
    const char *inner;
    LLVMTypeRef slot_ty;
    LLVMValueRef alloca_val;
    LLVMValueRef claimed_ptr;
    char inner_buf[128];

    if (inst == NULL || ctx == NULL || !mir_instruction_is_with_slot_claim(inst))
        return;

    alias = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
    if (alias == NULL || llvm_lookup_slot_inner(ctx, alias) != NULL)
        return;

    is_secure = llvm_mir_claim_is_secure(inst);
    inner = llvm_mir_claim_inner_type_name(inst, inner_buf, sizeof(inner_buf));
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, inst->expr0,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM MIR with-slot claim for '%s' requires concrete Slot<T> metadata",
            alias != NULL ? alias : "<slot>");
        return;
    }

    slot_ty = is_secure
        ? llvm_secure_slot_struct_type(ctx, inner)
        : llvm_slot_struct_type(ctx, inner);
    alloca_val = llvm_create_entry_alloca(ctx, slot_ty, alias);
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca_val);
    claimed_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca_val, 1,
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);
    if (is_secure) {
        LLVMTypeRef token_ty = llvm_secure_token_type(ctx, inner);
        char token_name[256];
        LLVMValueRef token_alloca;
        LLVMValueRef slot_ptr_i64;
        LLVMValueRef token_id;
        LLVMValueRef slot_token_ptr;
        LLVMValueRef token_id_ptr;
        LLVMValueRef token_write_ptr;
        LLVMValueRef token_read_ptr;

        snprintf(token_name, sizeof(token_name), "%s_token", alias);
        token_alloca = llvm_create_entry_alloca(ctx, token_ty, token_name);
        LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);
        slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder, alloca_val, ctx->type_i64,
            llvm_tmp_name(ctx));
        token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
            LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
            llvm_tmp_name(ctx));
        slot_token_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca_val,
            2, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);
        token_id_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty, token_alloca,
            0, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, token_id, token_id_ptr);
        token_write_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty,
            token_alloca, 1, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_write_ptr);
        token_read_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty,
            token_alloca, 2, llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
            token_read_ptr);
        llvm_scope_declare(ctx, pergyra_strdup(token_name), token_alloca,
                           token_ty);
    }
    llvm_scope_declare(ctx, alias, alloca_val, slot_ty);
    llvm_register_slot_var(ctx, alias, inner, is_secure);
}

void
llvm_mir_emit_with_release_only(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    const char *alias;
    const char *inner;
    bool is_secure;
    const MIRResourceRuntimeRow *row;
    LLVMFuncEntry *release_fn;
    LLVMVarEntry slot_var;
    char inner_buf[128];

    if (inst == NULL || ctx == NULL
        || !mir_instruction_source_is_with_slot_release(inst))
        return;

    alias = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
    inner = alias != NULL ? llvm_lookup_slot_inner(ctx, alias) : NULL;
    is_secure = alias != NULL && llvm_lookup_slot_is_secure(ctx, alias);
    if (inner == NULL)
        inner = llvm_mir_claim_inner_type_name(inst, inner_buf,
                                               sizeof(inner_buf));
    if (alias == NULL || inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, inst->expr0,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR with-slot release requires owner slot metadata");
        return;
    }

    row = llvm_slot_runtime_row_for_operation(
        inst->ast, ctx,
        is_secure ? MIR_RESOURCE_ABI_SECURE_SLOT : MIR_RESOURCE_ABI_SLOT,
        inner, "Release");
    if (row == NULL || row->runtime_fn == NULL) {
        llvm_set_error_at_with_hints(ctx, inst->expr0,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR with-slot release requires MIR ABI runtime function row");
        return;
    }
    release_fn = llvm_lookup_function(ctx, row->runtime_fn);
    if (release_fn == NULL) {
        llvm_required_runtime_function(ctx, inst->expr0,
            is_secure ? "secure slot" : "slot", "with-release",
            row->runtime_fn);
        return;
    }
    if (!llvm_scope_lookup_snapshot(ctx, alias, &slot_var)) {
        llvm_set_error_at_with_hints(ctx, inst->expr0,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR with-slot release is missing slot binding '%s'",
            alias);
        return;
    }
    if (is_secure) {
        LLVMVarEntry token_var;
        if (!llvm_lookup_secure_token_var(ctx, alias, &token_var)) {
            llvm_set_error_at_with_hints(ctx, inst->expr0,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM MIR secure release requires paired token binding '%s_token'",
                alias);
            return;
        }
        LLVMValueRef args[] = { slot_var.alloca, token_var.alloca };
        LLVMBuildCall2(ctx->builder, release_fn->fn_type, release_fn->fn,
                       args, 2, "");
    } else {
        LLVMValueRef args[] = { slot_var.alloca };
        LLVMBuildCall2(ctx->builder, release_fn->fn_type, release_fn->fn,
                       args, 1, "");
    }
    llvm_mark_slot_released(ctx, alias);
}

#endif /* PGY_LLVM_ENABLED */
