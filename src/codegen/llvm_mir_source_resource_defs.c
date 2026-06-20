#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_source_resource_defs.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_scope_bind.h"
#include "../parser/ast_api.h"

static bool
llvm_mir_initializer_is_channel_call(ASTNode *init)
{
    ASTNode *callee;

    if (init == NULL || init->type != AST_CALL)
        return false;
    callee = ast_call_callee(init);
    return callee != NULL
        && callee->type == AST_IDENTIFIER
        && ast_identifier_name(callee) != NULL
        && strcmp(ast_identifier_name(callee), "Channel") == 0;
}

static bool
llvm_mir_try_emit_source_channel_let(const MIRInstruction *inst,
                                     LLVMValueRef alloca,
                                     LLVMGenCtx *ctx,
                                     const char *expected_type_name,
                                     bool *handled)
{
    ASTNode *init = inst != NULL ? inst->expr0 : NULL;
    char inner[128];
    char init_fn_name[160];
    char base_name[128];
    LLVMFuncEntry *init_fn;
    LLVMValueRef cap;
    LLVMValueRef args[2];
    LLVMTypeRef channel_storage_type;
    int written;

    if (handled != NULL)
        *handled = false;
    if (inst == NULL || alloca == NULL
        || ctx == NULL || handled == NULL)
        return true;
    if (!llvm_mir_initializer_is_channel_call(init))
        return true;

    if (expected_type_name == NULL
        || pgy_classify_type(expected_type_name) != PGY_TK_CHANNEL
        || !llvm_constructed_arg_name_copy(expected_type_name, 0, inner,
            sizeof(inner))
        || inner[0] == '\0'
        || strcmp(inner, "Unknown") == 0) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source-local Channel let '%s' requires concrete Channel<T> metadata",
            inst->result_name != NULL ? inst->result_name : "(anonymous)");
        return false;
    }
    written = snprintf(init_fn_name, sizeof(init_fn_name),
        "pgy_channel_init_%s", inner);
    if (written < 0 || (size_t)written >= sizeof(init_fn_name)) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source-local Channel let runtime symbol is too long for '%s'",
            inner);
        return false;
    }
    init_fn = llvm_lookup_function(ctx, init_fn_name);
    if (init_fn == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source-local Channel let requires runtime export '%s'",
            init_fn_name);
        return false;
    }
    cap = LLVMConstInt(ctx->type_i64, 16, 0);
    if (ast_call_arg_count(init) > 0) {
        LLVMValueRef raw_cap = llvm_emit_expression(ast_call_argument(init, 0),
            ctx);
        if (raw_cap == NULL)
            return false;
        cap = LLVMBuildZExt(ctx->builder, raw_cap, ctx->type_i64,
            llvm_tmp_name(ctx));
    }
    args[0] = alloca;
    args[1] = cap;
    LLVMBuildCall2(ctx->builder, init_fn->fn_type, init_fn->fn, args, 2, "");
    if (llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        channel_storage_type =
            LLVMArrayType(LLVMInt8TypeInContext(ctx->context), 256);
        llvm_mir_bind_base_local_scope(ctx, base_name, alloca,
            channel_storage_type, expected_type_name);
        llvm_register_typed_var_abi_binding(ctx, base_name, alloca,
            expected_type_name);
        llvm_register_channel_var_binding(ctx, base_name, alloca, inner);
    }
    *handled = true;
    return !ctx->has_error;
}

static bool
llvm_mir_initializer_is_claim_call(ASTNode *init, const char **callee_out)
{
    ASTNode *callee;
    const char *callee_name;

    if (callee_out != NULL)
        *callee_out = NULL;
    if (init == NULL || init->type != AST_CALL)
        return false;
    callee = ast_call_callee(init);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    callee_name = ast_identifier_name(callee);
    if (callee_name == NULL)
        return false;
    if (strcmp(callee_name, "ClaimSlot") != 0
        && strcmp(callee_name, "ClaimSecureSlot") != 0
        && strcmp(callee_name, "ClaimDeviceSlot") != 0) {
        return false;
    }
    if (callee_out != NULL)
        *callee_out = callee_name;
    return true;
}

static bool
llvm_mir_claim_type_matches_callee(PgyTypeKind kind, const char *callee_name)
{
    if (callee_name == NULL)
        return false;
    if (strcmp(callee_name, "ClaimSlot") == 0)
        return kind == PGY_TK_SLOT;
    if (strcmp(callee_name, "ClaimSecureSlot") == 0)
        return kind == PGY_TK_SECURE_SLOT;
    if (strcmp(callee_name, "ClaimDeviceSlot") == 0)
        return kind == PGY_TK_DEVICE_SLOT;
    return false;
}

static void
llvm_mir_emit_secure_claim_token_for_local(LLVMGenCtx *ctx,
                                           const char *base_name,
                                           const char *inner,
                                           LLVMTypeRef slot_ty,
                                           LLVMValueRef alloca)
{
    LLVMTypeRef token_ty;
    LLVMValueRef token_alloca;
    LLVMValueRef slot_ptr_i64;
    LLVMValueRef token_id;
    LLVMValueRef slot_token_ptr;
    LLVMValueRef token_id_ptr;
    LLVMValueRef token_write_ptr;
    LLVMValueRef token_read_ptr;
    char token_name[256];
    char *owned_token_name;
    int written;

    if (ctx == NULL || base_name == NULL || inner == NULL || slot_ty == NULL
        || alloca == NULL) {
        return;
    }
    written = snprintf(token_name, sizeof(token_name), "%s_token", base_name);
    if (written < 0 || (size_t)written >= sizeof(token_name)) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source-local SecureSlot token name is too long for '%s'",
            base_name);
        return;
    }
    owned_token_name = pgy_arena_strdup(&ctx->persistent, token_name);
    if (owned_token_name == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR source-local SecureSlot token allocation failed");
        return;
    }

    token_ty = llvm_secure_token_type(ctx, inner);
    token_alloca = llvm_create_entry_alloca(ctx, token_ty, owned_token_name);
    LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);
    slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder, alloca, ctx->type_i64,
        llvm_tmp_name(ctx));
    token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
        LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
        llvm_tmp_name(ctx));
    slot_token_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca, 2,
        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, token_id, slot_token_ptr);
    token_id_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty, token_alloca,
        0, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, token_id, token_id_ptr);
    token_write_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty, token_alloca,
        1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        token_write_ptr);
    token_read_ptr = LLVMBuildStructGEP2(ctx->builder, token_ty, token_alloca,
        2, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        token_read_ptr);
    llvm_scope_declare(ctx, owned_token_name, token_alloca, token_ty);
}

static bool
llvm_mir_try_emit_source_claim_let(const MIRInstruction *inst,
                                   LLVMValueRef alloca,
                                   LLVMGenCtx *ctx,
                                   const char *expected_type_name,
                                   bool *handled)
{
    ASTNode *init = inst != NULL ? inst->expr0 : NULL;
    const char *callee_name;
    PgyTypeKind kind;
    char inner[128];
    char base_name[128];
    LLVMTypeRef slot_ty;
    LLVMValueRef claimed_ptr;
    bool is_secure;
    bool is_device;

    if (handled != NULL)
        *handled = false;
    if (inst == NULL || alloca == NULL
        || ctx == NULL || handled == NULL) {
        return true;
    }
    if (!llvm_mir_initializer_is_claim_call(init, &callee_name))
        return true;

    kind = pgy_classify_type(expected_type_name);
    if (!llvm_mir_claim_type_matches_callee(kind, callee_name)
        || !llvm_constructed_arg_name_copy(expected_type_name, 0, inner,
            sizeof(inner))
        || inner[0] == '\0'
        || strcmp(inner, "Unknown") == 0) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source-local %s let '%s' requires matching concrete slot metadata",
            callee_name != NULL ? callee_name : "ClaimSlot",
            inst->result_name != NULL ? inst->result_name : "(anonymous)");
        return false;
    }

    slot_ty = pergyra_type_to_llvm(ctx, expected_type_name);
    if (slot_ty == NULL || ctx->has_error) {
        if (!ctx->has_error) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM MIR source-local %s let '%s' has no LLVM ABI type",
                callee_name != NULL ? callee_name : "ClaimSlot",
                inst->result_name != NULL ? inst->result_name : "(anonymous)");
        }
        return false;
    }
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca);
    claimed_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca, 1,
        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);

    is_secure = kind == PGY_TK_SECURE_SLOT;
    is_device = kind == PGY_TK_DEVICE_SLOT;
    if (llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        llvm_mir_bind_base_local_scope(ctx, base_name, alloca, slot_ty,
            expected_type_name);
        llvm_register_typed_var_abi_binding(ctx, base_name, alloca,
            expected_type_name);
        if (is_device) {
            llvm_register_device_slot_var_binding(ctx, base_name, alloca,
                inner);
        } else {
            llvm_register_slot_var_binding(ctx, base_name, alloca, inner,
                is_secure);
        }
        if (is_secure)
            llvm_mir_emit_secure_claim_token_for_local(ctx, base_name, inner,
                slot_ty, alloca);
    }
    *handled = true;
    return !ctx->has_error;
}

static bool
llvm_mir_try_emit_source_secure_slot_sugar_let(const MIRInstruction *inst,
                                               LLVMValueRef alloca,
                                               LLVMGenCtx *ctx,
                                               const char *expected_type_name,
                                               bool *handled)
{
    ASTNode *init = inst != NULL ? inst->expr0 : NULL;
    char inner[128];
    char base_name[128];
    LLVMTypeRef slot_ty;
    LLVMValueRef value;
    LLVMValueRef value_ptr;
    LLVMValueRef occupied_ptr;

    if (handled != NULL)
        *handled = false;
    if (inst == NULL || alloca == NULL
        || ctx == NULL || handled == NULL) {
        return true;
    }
    if (!mir_instruction_uses_source_local_decl_emit(inst))
        return true;
    if (pgy_classify_type(expected_type_name) != PGY_TK_SECURE_SLOT)
        return true;
    if (!llvm_constructed_arg_name_copy(expected_type_name, 0, inner,
            sizeof(inner))
        || inner[0] == '\0'
        || strcmp(inner, "Unknown") == 0) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source-local SecureSlot sugar let '%s' requires concrete slot metadata",
            inst->result_name != NULL ? inst->result_name : "(anonymous)");
        return false;
    }
    if (init != NULL
        && init->type == AST_CALL
        && ast_call_callee(init) != NULL
        && ast_call_callee(init)->type == AST_IDENTIFIER
        && strcmp(ast_identifier_name(ast_call_callee(init)),
                  "ClaimSecureSlot") == 0) {
        return true;
    }

    slot_ty = pergyra_type_to_llvm(ctx, expected_type_name);
    if (ctx->has_error || slot_ty == NULL)
        return false;
    if (!llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source-local SecureSlot sugar let requires versioned local name");
        return false;
    }

    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), alloca);
    llvm_mir_bind_base_local_scope(ctx, base_name, alloca, slot_ty,
        expected_type_name);
    llvm_register_typed_var_abi_binding(ctx, base_name, alloca,
        expected_type_name);
    llvm_register_slot_var_binding(ctx, base_name, alloca, inner, true);
    llvm_mir_emit_secure_claim_token_for_local(ctx, base_name, inner,
        slot_ty, alloca);
    if (ctx->has_error)
        return false;

    if (init != NULL) {
        value = llvm_emit_expression(init, ctx);
        if (value == NULL) {
            if (!ctx->has_error) {
                llvm_set_mir_inventory_missing(ctx,
                    "LLVM MIR source-local SecureSlot sugar initializer could not be lowered");
            }
            return false;
        }
        value_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca, 0,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, value, value_ptr);
    }
    occupied_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, alloca, 1,
        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        occupied_ptr);

    *handled = true;
    return !ctx->has_error;
}

bool
llvm_mir_try_emit_source_resource_let(const MIRInstruction *inst,
                                      LLVMValueRef alloca,
                                      LLVMGenCtx *ctx,
                                      const char *expected_type_name,
                                      bool *handled)
{
    bool local_handled = false;

    if (handled != NULL)
        *handled = false;
    if (!mir_instruction_uses_source_local_decl_emit(inst))
        return true;
    if (!llvm_mir_try_emit_source_claim_let(inst, alloca, ctx,
            expected_type_name, &local_handled)) {
        return false;
    }
    if (local_handled) {
        if (handled != NULL)
            *handled = true;
        return true;
    }
    if (!llvm_mir_try_emit_source_secure_slot_sugar_let(inst, alloca, ctx,
            expected_type_name, &local_handled)) {
        return false;
    }
    if (local_handled) {
        if (handled != NULL)
            *handled = true;
        return true;
    }
    if (!llvm_mir_try_emit_source_channel_let(inst, alloca, ctx,
            expected_type_name, &local_handled)) {
        return false;
    }
    if (handled != NULL)
        *handled = local_handled;
    return true;
}

#endif
