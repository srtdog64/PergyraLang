#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

#include <stdio.h>
#include <string.h>

#include "codegen_slot_type_policy.h"

static const char *
llvm_destructure_binding_name(const MIRInstruction *inst,
                              ASTNode *node,
                              size_t index)
{
    if (inst != NULL)
        return mir_instruction_destructure_binding_name_at(inst, index);
    return ast_let_destructure_name(node, index);
}

static const char *
llvm_destructure_claim_abi_name(const MIRInstruction *inst)
{
    if (inst == NULL)
        return NULL;
    if (inst->abi_type_name != NULL && inst->abi_type_name[0] != '\0')
        return inst->abi_type_name;
    return inst->type_layout != NULL ? inst->type_layout->abi_type_name : NULL;
}

static bool
llvm_destructure_claim_inner_name(const MIRInstruction *inst,
                                  char *out,
                                  size_t out_size)
{
    const char *abi_name = llvm_destructure_claim_abi_name(inst);

    if (abi_name == NULL)
        return false;
    return llvm_constructed_arg_name_copy(abi_name, 0, out, out_size)
        && out[0] != '\0'
        && strcmp(out, "Unknown") != 0;
}

static bool
llvm_emit_destructure_claim_token(LLVMGenCtx *ctx,
                                  ASTNode *diagnostic_node,
                                  const char *slot_name,
                                  const char *token_binding_name,
                                  const char *inner,
                                  LLVMTypeRef slot_ty,
                                  LLVMValueRef slot_alloca)
{
    char slot_token_name[256];
    char token_type_name[256];
    int written;
    LLVMTypeRef token_ty;
    LLVMValueRef token_alloca;
    LLVMValueRef slot_ptr_i64;
    LLVMValueRef token_id;
    LLVMValueRef slot_token_ptr;
    LLVMValueRef token_id_ptr;
    LLVMValueRef token_write_ptr;
    LLVMValueRef token_read_ptr;

    if (ctx == NULL || slot_name == NULL || token_binding_name == NULL
        || inner == NULL || slot_ty == NULL || slot_alloca == NULL) {
        return false;
    }

    written = snprintf(slot_token_name, sizeof(slot_token_name), "%s_token",
                       slot_name);
    if (written < 0 || (size_t)written >= sizeof(slot_token_name)) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM ClaimSecureSlot destructuring token name is too long for '%s'",
            slot_name);
        return false;
    }
    written = snprintf(token_type_name, sizeof(token_type_name), "Token<%s>",
                       inner);
    if (written < 0 || (size_t)written >= sizeof(token_type_name)) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM ClaimSecureSlot destructuring token type is too long for '%s'",
            inner);
        return false;
    }

    token_ty = llvm_secure_token_type(ctx, inner);
    token_alloca = llvm_create_entry_alloca(ctx, token_ty,
        token_binding_name);
    LLVMBuildStore(ctx->builder, LLVMConstNull(token_ty), token_alloca);

    slot_ptr_i64 = LLVMBuildPtrToInt(ctx->builder, slot_alloca, ctx->type_i64,
        llvm_tmp_name(ctx));
    token_id = LLVMBuildXor(ctx->builder, slot_ptr_i64,
        LLVMConstInt(ctx->type_i64, 0xDEADBEEFCAFEBABEULL, 0),
        llvm_tmp_name(ctx));
    slot_token_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, slot_alloca,
        2, llvm_tmp_name(ctx));
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

    llvm_scope_declare(ctx, pergyra_strdup(token_binding_name), token_alloca,
                       token_ty);
    if (strcmp(token_binding_name, slot_token_name) != 0) {
        llvm_scope_declare(ctx, pergyra_strdup(slot_token_name), token_alloca,
                           token_ty);
    }
    llvm_register_typed_var_abi_binding(ctx, token_binding_name, token_alloca,
                                        token_type_name);
    return !ctx->has_error;
}

static bool
llvm_try_emit_destructure_claim(const MIRInstruction *inst,
                                ASTNode *diagnostic_node,
                                LLVMGenCtx *ctx,
                                bool *handled)
{
    const char *abi_name;
    const char *slot_name;
    char inner[128];
    char slot_type_name[256];
    int written;
    size_t binding_count;
    bool is_secure;
    bool is_plain_slot;
    LLVMTypeRef slot_ty;
    LLVMValueRef slot_alloca;
    LLVMValueRef claimed_ptr;

    if (handled != NULL)
        *handled = false;
    if (inst == NULL || ctx == NULL || handled == NULL)
        return true;

    abi_name = llvm_destructure_claim_abi_name(inst);
    is_secure = pgy_codegen_type_name_is_secure_slot(abi_name);
    is_plain_slot = pgy_codegen_type_name_is_slot(abi_name);
    if (!is_secure && !is_plain_slot)
        return true;

    binding_count = mir_instruction_destructure_binding_count(inst);
    if ((is_secure && binding_count != 2)
        || (is_plain_slot && binding_count != 1)) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s destructuring requires %zu binding(s)",
            is_secure ? "ClaimSecureSlot" : "ClaimSlot",
            is_secure ? (size_t)2 : (size_t)1);
        return false;
    }
    if (!llvm_destructure_claim_inner_name(inst, inner, sizeof(inner))) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM %s destructuring requires concrete MIR-owned %s<T> metadata",
            is_secure ? "ClaimSecureSlot" : "ClaimSlot",
            is_secure ? "SecureSlot" : "Slot");
        return false;
    }

    slot_name = mir_instruction_destructure_binding_name_at(inst, 0);
    if (slot_name == NULL || slot_name[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM slot claim destructuring is missing slot binding metadata");
        return false;
    }

    slot_ty = is_secure
        ? llvm_secure_slot_struct_type(ctx, inner)
        : llvm_slot_struct_type(ctx, inner);
    slot_alloca = llvm_create_entry_alloca(ctx, slot_ty, slot_name);
    LLVMBuildStore(ctx->builder, LLVMConstNull(slot_ty), slot_alloca);
    claimed_ptr = LLVMBuildStructGEP2(ctx->builder, slot_ty, slot_alloca, 1,
                                      llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder,
        LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 1, 0),
        claimed_ptr);

    if (is_secure) {
        const char *token_name =
            mir_instruction_destructure_binding_name_at(inst, 1);
        if (token_name == NULL || token_name[0] == '\0') {
            llvm_set_error_at_with_hints(ctx, diagnostic_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM ClaimSecureSlot destructuring is missing token binding metadata");
            return false;
        }
        if (!llvm_emit_destructure_claim_token(ctx, diagnostic_node,
                slot_name, token_name, inner, slot_ty, slot_alloca)) {
            return false;
        }
    }

    written = snprintf(slot_type_name, sizeof(slot_type_name), "%s<%s>",
                       is_secure ? "SecureSlot" : "Slot", inner);
    if (written < 0 || (size_t)written >= sizeof(slot_type_name)) {
        llvm_set_error_at_with_hints(ctx, diagnostic_node,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM slot claim destructuring type name is too long for '%s'",
            inner);
        return false;
    }
    llvm_scope_declare(ctx, pergyra_strdup(slot_name), slot_alloca, slot_ty);
    llvm_register_slot_var_binding(ctx, slot_name, slot_alloca, inner,
                                   is_secure);
    llvm_register_typed_var_abi_binding(ctx, slot_name, slot_alloca,
                                        slot_type_name);
    *handled = true;
    return !ctx->has_error;
}

static void
llvm_emit_destructure_parts(ASTNode *init,
                            ASTNode *diagnostic_node,
                            const MIRInstruction *inst,
                            ASTNode *node,
                            size_t binding_count,
                            LLVMGenCtx *ctx)
{
    /* let (a, b, c) = expr;
     * Two shapes supported:
     *   1) Tuple: struct { T0, T1, ... } -> ExtractValue per field.
     *   2) Array-like: struct { T* data, i64 size, i64 cap } -> GEP + load. */
    if (init == NULL)
        return;
    if (inst != NULL) {
        bool handled = false;
        if (!llvm_try_emit_destructure_claim(inst, diagnostic_node, ctx,
                &handled)) {
            return;
        }
        if (handled)
            return;
    }
    LLVMValueRef rhs_val = llvm_emit_expression(init, ctx);
    if (rhs_val == NULL) {
        if (ctx != NULL && !ctx->has_error) {
            llvm_set_error_at_with_hints(ctx, init,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM destructuring let could not lower initializer expression");
        }
        return;
    }
    LLVMTypeRef rhs_ty = LLVMTypeOf(rhs_val);
    if (LLVMGetTypeKind(rhs_ty) != LLVMStructTypeKind) {
        llvm_set_error_with_hints(ctx, PGY_CODE_LLVM_TYPE_UNSUPPORTED, PGY_CAUSE_LLVM_TYPE_UNSUPPORTED, PGY_FIX_ANNOTATE_CONCRETE_TYPE, "destructuring requires an Array-like or tuple struct initializer");
        return;
    }

    /* Heuristic: tuple if struct field count equals the binding count
     * AND the first field is not a pointer (array-like has pointer as
     * the first field for `data`). */
    unsigned field_count = LLVMCountStructElementTypes(rhs_ty);
    bool is_tuple = false;
    if (field_count == (unsigned)binding_count) {
        LLVMTypeRef f0 = LLVMStructGetTypeAtIndex(rhs_ty, 0);
        if (f0 != NULL && LLVMGetTypeKind(f0) != LLVMPointerTypeKind)
            is_tuple = true;
    }

    if (is_tuple) {
        for (size_t i = 0; i < binding_count; i++) {
            const char *bname =
                llvm_destructure_binding_name(inst, node, i);
            if (bname == NULL) {
                llvm_set_error_at_with_hints(ctx, diagnostic_node,
                    PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                    PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "LLVM destructuring let binding name metadata is missing");
                return;
            }
            LLVMTypeRef ft = LLVMStructGetTypeAtIndex(rhs_ty, (unsigned)i);
            LLVMValueRef v = LLVMBuildExtractValue(ctx->builder, rhs_val,
                (unsigned)i, llvm_tmp_name(ctx));
            LLVMValueRef alloca = llvm_create_entry_alloca(ctx, ft, bname);
            LLVMBuildStore(ctx->builder, v, alloca);
            llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, ft);
        }
        return;
    }

    /* Array-like path (unchanged) */
    LLVMValueRef data_ptr = LLVMBuildExtractValue(ctx->builder,
        rhs_val, 0, llvm_tmp_name(ctx));
    LLVMTypeRef elem_type = llvm_stmt_resolve_array_elem_type(
        ctx, init, data_ptr);
    if (elem_type == NULL)
        return;
    for (size_t i = 0; i < binding_count; i++) {
        const char *bname = llvm_destructure_binding_name(inst, node, i);
        if (bname == NULL) {
            llvm_set_error_at_with_hints(ctx, diagnostic_node,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "LLVM destructuring let binding name metadata is missing");
            return;
        }
        LLVMValueRef idx = LLVMConstInt(ctx->type_i64,
            (unsigned long long)i, 0);
        LLVMValueRef gep = LLVMBuildGEP2(ctx->builder,
            elem_type, data_ptr, &idx, 1, llvm_tmp_name(ctx));
        LLVMValueRef val = LLVMBuildLoad2(ctx->builder, elem_type,
            gep, llvm_tmp_name(ctx));
        LLVMValueRef alloca = llvm_create_entry_alloca(
            ctx, elem_type, bname);
        LLVMBuildStore(ctx->builder, val, alloca);
        llvm_scope_declare(ctx, pergyra_strdup(bname), alloca, elem_type);
    }
}

void
llvm_emit_let_destructure_stmt(ASTNode *node, LLVMGenCtx *ctx)
{
    if (node == NULL || node->type != AST_LET_DESTRUCTURE)
        return;
    llvm_emit_destructure_parts(ast_let_destructure_initializer(node),
                                node,
                                NULL,
                                node,
                                ast_let_destructure_name_count(node),
                                ctx);
}

void
llvm_emit_mir_destructure_inst(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    if (inst == NULL || inst->kind != MIR_INST_DESTRUCTURE) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR DESTRUCTURE instruction missing MIR destructure facts");
        return;
    }
    if (inst->expr0 == NULL
        || mir_instruction_destructure_binding_count(inst) == 0) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR DESTRUCTURE instruction missing MIR destructure facts");
        return;
    }
    llvm_emit_destructure_parts(inst->expr0,
                                inst->expr0,
                                inst,
                                NULL,
                                mir_instruction_destructure_binding_count(inst),
                                ctx);
}
#endif /* PGY_LLVM_ENABLED */
