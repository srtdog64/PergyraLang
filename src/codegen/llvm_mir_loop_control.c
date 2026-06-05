/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR range/for-in loop control lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "parser/ast_api.h"

static bool
llvm_mir_loop_bound_error(LLVMGenCtx *ctx, const char *bound_name)
{
    if (ctx != NULL && !ctx->has_error) {
        llvm_set_error_at_with_hints(ctx, NULL,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR for-range could not lower %s expression",
            bound_name != NULL ? bound_name : "bound");
    }
    return false;
}

static void
llvm_mir_for_loop_alloca_name(const MIRInstruction *inst,
                              const char *binding,
                              char *buffer,
                              size_t buffer_size)
{
    uint32_t stable_id;

    if (buffer == NULL || buffer_size == 0)
        return;
    buffer[0] = '\0';
    if (binding == NULL) {
        snprintf(buffer, buffer_size, "mir.for.binding");
        return;
    }
    stable_id = mir_instruction_source_stable_id(inst);
    if (stable_id == 0) {
        snprintf(buffer, buffer_size, "%s.mir.for", binding);
        return;
    }
    snprintf(buffer, buffer_size, "%s.mir.for.%u", binding, stable_id);
}

static bool
llvm_mir_lookup_for_loop_var_snapshot(const MIRInstruction *inst,
                                      LLVMGenCtx *ctx,
                                      const char *variable,
                                      LLVMVarEntry *out)
{
    char alloca_name[256];

    llvm_mir_for_loop_alloca_name(inst, variable,
                                  alloca_name, sizeof(alloca_name));
    return llvm_scope_lookup_snapshot(ctx, alloca_name, out)
        || llvm_scope_lookup_snapshot(ctx, variable, out);
}

bool
llvm_mir_emit_for_loop_init(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    LLVMValueRef var_alloca;
    LLVMValueRef start;
    const char *variable;
    char alloca_name[256];

    if (inst == NULL || ctx == NULL)
        return true;
    if (inst->kind != MIR_INST_LOOP_INIT)
        return true;
    if (inst->branch_shape != MIR_BRANCH_FOR_RANGE
        && inst->branch_shape != MIR_BRANCH_FOR_IN)
        return false;
    variable = inst->arg0;
    if (variable == NULL)
        return true;
    if (inst->branch_shape == MIR_BRANCH_FOR_IN)
        return llvm_mir_emit_for_in_loop_init(inst, ctx);

    llvm_mir_for_loop_alloca_name(inst, variable,
                                  alloca_name, sizeof(alloca_name));
    if (llvm_scope_contains(ctx, alloca_name))
        return true;
    var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, alloca_name);
    start = llvm_emit_expression(inst->expr0, ctx);
    if (start == NULL)
        return llvm_mir_loop_bound_error(ctx, "start");
    LLVMBuildStore(ctx->builder, start, var_alloca);
    llvm_scope_declare(ctx, pergyra_strdup(variable), var_alloca,
                       ctx->type_i32);
    llvm_scope_declare(ctx, pergyra_strdup(alloca_name), var_alloca,
                       ctx->type_i32);
    return true;
}

LLVMValueRef
llvm_mir_emit_for_loop_condition(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    LLVMVarEntry loop_snapshot;
    LLVMValueRef current;
    LLVMValueRef end;
    const char *variable;

    if (inst == NULL || ctx == NULL)
        return NULL;
    if (inst->branch_shape != MIR_BRANCH_FOR_RANGE
        && inst->branch_shape != MIR_BRANCH_FOR_IN)
        return NULL;
    variable = inst->arg0;
    if (variable == NULL)
        return NULL;
    if (inst->branch_shape == MIR_BRANCH_FOR_IN)
        return llvm_mir_emit_for_in_loop_condition(inst, ctx);

    if (!llvm_mir_lookup_for_loop_var_snapshot(inst, ctx, variable,
                                               &loop_snapshot)
        || loop_snapshot.alloca == NULL) {
        char alloca_name[256];
        LLVMValueRef var_alloca;
        LLVMValueRef start = llvm_emit_expression(inst->expr0, ctx);
        llvm_mir_for_loop_alloca_name(inst, variable,
                                      alloca_name, sizeof(alloca_name));
        var_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32,
                                              alloca_name);
        if (start == NULL) {
            llvm_mir_loop_bound_error(ctx, "start");
            return NULL;
        }
        LLVMBuildStore(ctx->builder, start, var_alloca);
        llvm_scope_declare(ctx, pergyra_strdup(variable), var_alloca,
                           ctx->type_i32);
        llvm_scope_declare(ctx, pergyra_strdup(alloca_name), var_alloca,
                           ctx->type_i32);
        (void)llvm_mir_lookup_for_loop_var_snapshot(inst, ctx, variable,
                                                    &loop_snapshot);
    }
    if (loop_snapshot.alloca == NULL)
        return NULL;

    current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                             loop_snapshot.alloca, llvm_tmp_name(ctx));
    end = llvm_emit_expression(inst->expr1, ctx);
    if (end == NULL) {
        llvm_mir_loop_bound_error(ctx, "end");
        return NULL;
    }
    return LLVMBuildICmp(ctx->builder, LLVMIntSLT, current, end,
                         llvm_tmp_name(ctx));
}

static bool
llvm_mir_emit_for_loop_increment(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    LLVMVarEntry loop_snapshot;
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

    if (!llvm_mir_lookup_for_loop_var_snapshot(inst, ctx, variable,
                                               &loop_snapshot)
        || loop_snapshot.alloca == NULL)
        return true;

    current = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                             loop_snapshot.alloca, llvm_tmp_name(ctx));
    next = LLVMBuildAdd(ctx->builder, current,
                        LLVMConstInt(ctx->type_i32, 1, 0),
                        llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, next, loop_snapshot.alloca);
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

static const MIRInstruction *
llvm_mir_find_incoming_range_loop_branch(const MIRRoutine *routine,
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
        const MIRInstruction *inst = llvm_mir_find_loop_branch_inst(pred);
        if (inst != NULL && inst->branch_shape == MIR_BRANCH_FOR_RANGE)
            return inst;
    }
    return NULL;
}

static const MIRInstruction *
llvm_mir_find_backedge_range_loop_branch(const MIRRoutine *routine,
                                         const MIRBasicBlock *block)
{
    const MIRBasicBlock *target;
    const MIRInstruction *inst;

    if (routine == NULL || block == NULL)
        return NULL;
    if (!block->has_succ_true || block->succ_true >= routine->block_count)
        return NULL;
    if (block->id <= block->succ_true)
        return NULL;

    target = &routine->blocks[block->succ_true];
    if (target == block)
        return NULL;
    inst = llvm_mir_find_loop_branch_inst(target);
    if (inst != NULL && inst->branch_shape == MIR_BRANCH_FOR_RANGE)
        return inst;
    return NULL;
}

bool
llvm_mir_emit_for_loop_body_binding(const MIRRoutine *routine,
                                    const MIRBasicBlock *block,
                                    LLVMGenCtx *ctx)
{
    const MIRInstruction *branch_inst;
    const char *variable;
    char alloca_name[256];

    if (routine == NULL || block == NULL || ctx == NULL)
        return true;
    branch_inst = llvm_mir_find_incoming_range_loop_branch(routine, block);
    if (branch_inst == NULL) {
        branch_inst = llvm_mir_find_backedge_range_loop_branch(routine,
                                                               block);
    }
    if (branch_inst == NULL)
        return true;

    variable = branch_inst->arg0;
    if (variable == NULL)
        return true;
    llvm_mir_for_loop_alloca_name(branch_inst, variable,
                                  alloca_name, sizeof(alloca_name));
    {
        LLVMVarEntry loop_snapshot;
        if (!llvm_scope_lookup_snapshot(ctx, alloca_name, &loop_snapshot)
            || loop_snapshot.alloca == NULL) {
            return true;
        }
        llvm_scope_declare(ctx, pergyra_strdup(variable),
                           loop_snapshot.alloca, loop_snapshot.type);
        return true;
    }
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
