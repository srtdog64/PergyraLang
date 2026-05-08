/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR for-in lowering. MIR owns the loop index, condition, body binding,
 * and backedge increment; this file keeps iterable loop details out of the
 * generic CFG control owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

static void
llvm_mir_for_in_index_name(const char *variable, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
        return;
    snprintf(buf, buf_size, "__pgy_idx_%s", variable != NULL ? variable : "it");
}

static LLVMFuncEntry *
llvm_mir_for_in_required_runtime(LLVMGenCtx *ctx,
                                 const MIRInstruction *inst,
                                 const char *function_name)
{
    LLVMFuncEntry *fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;

    if (fn == NULL && ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, inst != NULL ? inst->ast : NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR for-in lowering requires registered runtime function '%s'",
            function_name != NULL ? function_name : "<missing>");
    }
    return fn;
}

bool
llvm_mir_emit_for_in_loop_init(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    const char *variable;
    LLVMValueRef idx_alloca;
    char idx_name[256];

    if (inst == NULL || ctx == NULL)
        return true;
    if (inst->branch_shape != MIR_BRANCH_FOR_IN) {
        return true;
    }
    variable = inst->arg0;
    if (variable == NULL)
        return true;

    llvm_mir_for_in_index_name(variable, idx_name, sizeof(idx_name));
    if (llvm_scope_lookup(ctx, idx_name) != NULL)
        return true;
    idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, idx_name);
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), idx_alloca);
    llvm_scope_declare(ctx, pergyra_strdup(idx_name), idx_alloca, ctx->type_i32);
    return true;
}

LLVMValueRef
llvm_mir_emit_for_in_loop_condition(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    ASTNode *iterable;
    const char *variable;
    LLVMVarEntry *idx_var;
    LLVMValueRef current;
    LLVMValueRef size_call;
    char idx_name[256];

    if (inst == NULL || ctx == NULL)
        return NULL;
    if (inst->branch_shape != MIR_BRANCH_FOR_IN) {
        return NULL;
    }
    variable = inst->arg0;
    if (variable == NULL)
        return NULL;

    llvm_mir_for_in_index_name(variable, idx_name, sizeof(idx_name));
    idx_var = llvm_scope_lookup(ctx, idx_name);
    if (idx_var == NULL || idx_var->alloca == NULL) {
        LLVMValueRef idx_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                           idx_name);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0),
                       idx_alloca);
        llvm_scope_declare(ctx, pergyra_strdup(idx_name), idx_alloca,
                           ctx->type_i32);
        idx_var = llvm_scope_lookup(ctx, idx_name);
    }
    if (idx_var == NULL || idx_var->alloca == NULL)
        return NULL;

    size_call = LLVMConstInt(ctx->type_i32, 0, 0);
    iterable = inst->expr0;
    if (iterable != NULL && iterable->type == AST_IDENTIFIER) {
        const char *iter_name = iterable->data.identifier.name;
        LLVMVarEntry *list_var = llvm_scope_lookup(ctx, iter_name);
        LLVMFuncEntry *size_fn = llvm_mir_for_in_required_runtime(ctx, inst,
            "pgy_list_size_raw_export");
        if (list_var != NULL && size_fn != NULL) {
            LLVMValueRef args[] = {
                LLVMBuildBitCast(ctx->builder, list_var->alloca,
                                 ctx->type_i8ptr, llvm_tmp_name(ctx))
            };
            size_call = LLVMBuildCall2(ctx->builder, size_fn->fn_type,
                                       size_fn->fn, args, 1,
                                       llvm_tmp_name(ctx));
        }
    }

    current = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_var->alloca,
                             llvm_tmp_name(ctx));
    return LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, size_call,
                         llvm_tmp_name(ctx));
}

bool
llvm_mir_emit_for_in_loop_increment(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    const char *variable;
    LLVMVarEntry *idx_var;
    LLVMValueRef current;
    LLVMValueRef next;
    char idx_name[256];

    if (inst == NULL || ctx == NULL)
        return true;
    if (inst->branch_shape != MIR_BRANCH_FOR_IN) {
        return true;
    }
    variable = inst->arg0;
    if (variable == NULL)
        return true;

    llvm_mir_for_in_index_name(variable, idx_name, sizeof(idx_name));
    idx_var = llvm_scope_lookup(ctx, idx_name);
    if (idx_var == NULL || idx_var->alloca == NULL)
        return true;

    current = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_var->alloca,
                             llvm_tmp_name(ctx));
    next = LLVMBuildAdd(ctx->builder, current,
                        LLVMConstInt(ctx->type_i32, 1, 0),
                        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, idx_var->alloca);
    return true;
}

static const MIRInstruction *
llvm_mir_find_incoming_for_in_branch(const MIRRoutine *routine,
                                     const MIRBasicBlock *block)
{
    size_t target_id = SIZE_MAX;

    if (routine == NULL || block == NULL)
        return NULL;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (&routine->blocks[i] == block) {
            target_id = i;
            break;
        }
    }
    if (target_id == SIZE_MAX)
        return NULL;

    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *pred = &routine->blocks[i];
        if (!pred->has_succ_true || pred->succ_true != target_id)
            continue;
        for (size_t j = 0; j < pred->instruction_count; j++) {
            const MIRInstruction *inst = &pred->instructions[j];
            if (inst->kind == MIR_INST_BRANCH
                && inst->branch_shape == MIR_BRANCH_FOR_IN) {
                return inst;
            }
        }
    }
    return NULL;
}

bool
llvm_mir_emit_for_in_body_binding(const MIRRoutine *routine,
                                  const MIRBasicBlock *block,
                                  LLVMGenCtx *ctx)
{
    const MIRInstruction *branch_inst;
    ASTNode *iterable;
    const char *variable;
    const char *iter_name;
    const char *list_inner;
    LLVMVarEntry *list_var;
    LLVMVarEntry *loop_var;
    LLVMVarEntry *idx_var;
    LLVMFuncEntry *get_fn;
    LLVMTypeRef elem_ty;
    LLVMValueRef idx;
    char idx_name[256];

    if (routine == NULL || block == NULL || ctx == NULL)
        return true;
    branch_inst = llvm_mir_find_incoming_for_in_branch(routine, block);
    if (branch_inst == NULL)
        return true;
    iterable = branch_inst->expr0;
    variable = branch_inst->arg0;
    if (iterable == NULL || iterable->type != AST_IDENTIFIER || variable == NULL)
        return true;

    iter_name = iterable->data.identifier.name;
    list_inner = llvm_lookup_list_inner(ctx, iter_name);
    list_var = llvm_scope_lookup(ctx, iter_name);
    if (list_inner == NULL || list_var == NULL)
        return true;

    elem_ty = pergyra_type_to_llvm(ctx, list_inner);
    if (ctx->has_error || elem_ty == NULL)
        return false;
    loop_var = llvm_scope_lookup(ctx, variable);
    if (loop_var == NULL) {
        LLVMValueRef loop_alloca = llvm_create_entry_alloca(ctx, elem_ty,
                                                            variable);
        llvm_scope_declare(ctx, pergyra_strdup(variable), loop_alloca, elem_ty);
        {
            LLVMClassTypeEntry *cls = llvm_stmt_lookup_class_by_type(ctx, elem_ty);
            if (cls != NULL)
                llvm_register_var_class(ctx, pergyra_strdup(variable),
                                        cls->class_name);
        }
        loop_var = llvm_scope_lookup(ctx, variable);
    }
    llvm_mir_for_in_index_name(variable, idx_name, sizeof(idx_name));
    idx_var = llvm_scope_lookup(ctx, idx_name);
    get_fn = llvm_mir_for_in_required_runtime(ctx, branch_inst,
        "pgy_list_get_raw_export");
    if (loop_var == NULL || loop_var->alloca == NULL
        || idx_var == NULL || idx_var->alloca == NULL || get_fn == NULL) {
        return true;
    }

    idx = LLVMBuildLoad2(ctx->builder, ctx->type_i32, idx_var->alloca,
                         llvm_tmp_name(ctx));
    {
        LLVMValueRef args[] = {
            LLVMBuildBitCast(ctx->builder, list_var->alloca, ctx->type_i8ptr,
                             llvm_tmp_name(ctx)),
            idx,
            LLVMBuildBitCast(ctx->builder, loop_var->alloca, ctx->type_i8ptr,
                             llvm_tmp_name(ctx)),
            llvm_sizeof_type_i64(ctx, elem_ty)
        };
        LLVMBuildCall2(ctx->builder, get_fn->fn_type, get_fn->fn,
                       args, 4, "");
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
