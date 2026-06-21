/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_lifecycle_emit.h"

#include "llvm_internal_api.h"
#include "../parser/ast_api.h"

/* Emit the domain-lifecycle runtime guard carried by this MIR call fact
 * (doc/12 section 2.3). LC_GUARD_CHECK is the fail-closed ambiguous-state
 * guard; LC_GUARD_SET records a proven transition. Construction state defaults
 * to the initial index (absent == state 0) in the runtime side-map. */
void
llvm_mir_emit_lifecycle_guard(const MIRInstruction *inst,
                              ASTNode *stmt,
                              LLVMGenCtx *ctx)
{
    ASTNode      *callee;
    ASTNode      *obj;
    LLVMVarEntry  entry;
    LLVMValueRef  recv_ptr;
    LLVMFuncEntry *fn;

    if (!mir_instruction_has_lifecycle_guard(inst)
        || stmt == NULL
        || stmt->type != AST_CALL) {
        return;
    }
    callee = ast_call_callee(stmt);
    obj = callee != NULL ? ast_member_object(callee) : NULL;
    if (obj == NULL || obj->type != AST_IDENTIFIER)
        return;
    /* The receiver local's own alloca is the stable per-instance key (the same
     * storage both the SET and CHECK on this variable resolve to), matching the
     * C backend keying on &<ssa-local>. */
    if (!llvm_scope_lookup_snapshot(ctx, ast_identifier_name(obj), &entry)
        || entry.alloca == NULL)
        return;
    recv_ptr = LLVMBuildBitCast(ctx->builder, entry.alloca, ctx->type_i8ptr,
                                llvm_tmp_name(ctx));
    if (mir_instruction_lifecycle_guard_kind(inst)
            == MIR_LIFECYCLE_GUARD_CHECK) {
        LLVMValueRef args[5];
        fn = llvm_lookup_function(ctx, "pgy_runtime_lifecycle_guard_export");
        if (fn == NULL)
            return;
        args[0] = recv_ptr;
        args[1] = LLVMConstInt(ctx->type_i32,
            (unsigned)mir_instruction_lifecycle_valid_mask(inst), 0);
        args[2] = LLVMConstInt(ctx->type_i32,
            (unsigned long long)mir_instruction_lifecycle_to_state(inst), 0);
        args[3] = LLVMBuildGlobalStringPtr(ctx->builder,
            mir_instruction_lifecycle_op(inst) != NULL
                ? mir_instruction_lifecycle_op(inst)
                : "",
            llvm_tmp_name(ctx));
        args[4] = LLVMBuildGlobalStringPtr(ctx->builder,
            mir_instruction_lifecycle_subject(inst) != NULL
                ? mir_instruction_lifecycle_subject(inst)
                : "",
            llvm_tmp_name(ctx));
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 5, "");
    } else {
        LLVMValueRef args[2];
        fn = llvm_lookup_function(ctx, "pgy_runtime_lifecycle_set_export");
        if (fn == NULL)
            return;
        args[0] = recv_ptr;
        args[1] = LLVMConstInt(ctx->type_i32,
            (unsigned long long)mir_instruction_lifecycle_to_state(inst), 0);
        LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, args, 2, "");
    }
}

#endif
