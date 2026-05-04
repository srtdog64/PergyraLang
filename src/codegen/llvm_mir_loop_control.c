/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR range/for-in loop control lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

bool
llvm_mir_emit_for_loop_init(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    ASTNode *node;
    LLVMValueRef var_alloca;
    LLVMValueRef start;
    const char *variable;

    if (inst == NULL || ctx == NULL)
        return true;
    if (inst->kind != MIR_INST_LOOP_INIT)
        return true;
    node = inst->ast;
    if (node == NULL
        || (inst->branch_shape != MIR_BRANCH_FOR_RANGE
            && inst->branch_shape != MIR_BRANCH_FOR_IN))
        return false;
    variable = inst->arg0;
    if (variable == NULL)
        return true;
    if (inst->branch_shape == MIR_BRANCH_FOR_IN)
        return llvm_mir_emit_for_in_loop_init(inst, ctx);
    if (llvm_scope_lookup(ctx, variable) != NULL)
        return true;

    var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, variable);
    start = llvm_emit_expression(inst->expr0, ctx);
    if (start == NULL)
        start = LLVMConstInt(ctx->type_i32, 0, 0);
    LLVMBuildStore(ctx->builder, start, var_alloca);
    llvm_scope_declare(ctx, variable, var_alloca, ctx->type_i32);
    return true;
}

LLVMValueRef
llvm_mir_emit_for_loop_condition(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    ASTNode *node;
    LLVMVarEntry *loop_var;
    LLVMValueRef current;
    LLVMValueRef end;
    const char *variable;

    if (inst == NULL || ctx == NULL)
        return NULL;
    node = inst->ast;
    if (node == NULL
        || (inst->branch_shape != MIR_BRANCH_FOR_RANGE
            && inst->branch_shape != MIR_BRANCH_FOR_IN))
        return NULL;
    variable = inst->arg0;
    if (variable == NULL)
        return NULL;
    if (inst->branch_shape == MIR_BRANCH_FOR_IN)
        return llvm_mir_emit_for_in_loop_condition(inst, ctx);

    loop_var = llvm_scope_lookup(ctx, variable);
    if (loop_var == NULL || loop_var->alloca == NULL) {
        LLVMValueRef var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                                           variable);
        LLVMValueRef start = llvm_emit_expression(inst->expr0, ctx);
        if (start == NULL)
            start = LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMBuildStore(ctx->builder, start, var_alloca);
        llvm_scope_declare(ctx, variable, var_alloca, ctx->type_i32);
        loop_var = llvm_scope_lookup(ctx, variable);
    }
    if (loop_var == NULL || loop_var->alloca == NULL)
        return NULL;

    current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                             loop_var->alloca, llvm_tmp_name(ctx));
    end = llvm_emit_expression(inst->expr1, ctx);
    if (end == NULL)
        end = LLVMConstInt(ctx->type_i32, 0, 0);
    return LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, end,
                         llvm_tmp_name(ctx));
}

static bool
llvm_mir_emit_for_loop_increment(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    LLVMVarEntry *loop_var;
    LLVMValueRef current;
    LLVMValueRef next;
    const char *variable;

    if (inst == NULL || ctx == NULL)
        return true;
    if (inst->branch_shape != MIR_BRANCH_FOR_RANGE
        && inst->branch_shape != MIR_BRANCH_FOR_IN)
        return true;
    variable = inst->arg0;
    if (variable == NULL)
        return true;
    if (inst->branch_shape == MIR_BRANCH_FOR_IN)
        return llvm_mir_emit_for_in_loop_increment(inst, ctx);

    loop_var = llvm_scope_lookup(ctx, variable);
    if (loop_var == NULL || loop_var->alloca == NULL)
        return true;

    current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                             loop_var->alloca, llvm_tmp_name(ctx));
    next = LLVMBuildAdd(ctx->builder, current,
                        LLVMConstInt(ctx->type_i32, 1, 0),
                        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, loop_var->alloca);
    return true;
}

static const MIRInstruction *
llvm_mir_find_loop_branch_inst(const MIRBasicBlock *block)
{
    if (block == NULL)
        return NULL;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_BRANCH
            && (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                || inst->branch_shape == MIR_BRANCH_FOR_IN)) {
            return inst;
        }
    }
    return NULL;
}

bool
llvm_mir_emit_loop_backedge_increment(const MIRRoutine *routine,
                                      const MIRBasicBlock *mir_block,
                                      LLVMGenCtx *ctx)
{
    const MIRBasicBlock *target;
    const MIRInstruction *branch_inst;

    if (routine == NULL || mir_block == NULL || ctx == NULL)
        return true;
    if (!mir_block->has_succ_true || mir_block->succ_true >= routine->block_count)
        return true;
    if (mir_block->id <= mir_block->succ_true)
        return true;

    target = &routine->blocks[mir_block->succ_true];
    if (target == mir_block)
        return true;
    branch_inst = llvm_mir_find_loop_branch_inst(target);
    if (branch_inst == NULL)
        return true;

    return llvm_mir_emit_for_loop_increment(branch_inst, ctx);
}

#endif /* PGY_LLVM_ENABLED */
