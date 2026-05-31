/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR PHI lowering owner.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_phi.h"

static bool
llvm_mir_ssa_name_is_pin_view(const MIRBasicBlock *pred_block,
                              const MIRBasicBlock *target_block,
                              const char *name)
{
    char base[128];

    if (name == NULL)
        return false;
    if (!llvm_mir_base_name_from_versioned(name, base, sizeof(base)))
        snprintf(base, sizeof(base), "%s", name);

    return (pred_block != NULL
            && pred_block->pin_view_name != NULL
            && strcmp(base, pred_block->pin_view_name) == 0)
        || (target_block != NULL
            && target_block->pin_view_name != NULL
            && strcmp(base, target_block->pin_view_name) == 0);
}

static bool
llvm_mir_phi_is_lowerable(const MIRRoutine *routine,
                          const MIRBasicBlock *target_block,
                          const MIRInstruction *inst,
                          LLVMMirVar *vars,
                          size_t var_count,
                          LLVMTypeRef *phi_type_out,
                          LLVMValueRef *phi_alloca_out)
{
    LLVMMirVar *target;

    if (routine == NULL || target_block == NULL || inst == NULL
        || inst->kind != MIR_INST_PHI || inst->result_name == NULL
        || inst->phi_incoming_count == 0) {
        return false;
    }
    if (llvm_mir_ssa_name_is_pin_view(NULL, target_block, inst->result_name))
        return false;

    target = llvm_mir_get_var_entry(vars, var_count, inst->result_name);
    if (target == NULL || target->alloca == NULL || target->type == NULL)
        return false;

    for (size_t i = 0; i < inst->phi_incoming_count; i++) {
        const MIRPhiIncoming *incoming = &inst->phi_incomings[i];
        LLVMMirVar *source;
        const MIRBasicBlock *pred_block;

        if (incoming->predecessor_block >= routine->block_count
            || incoming->value_name == NULL) {
            return false;
        }
        pred_block = &routine->blocks[incoming->predecessor_block];
        if (llvm_mir_ssa_name_is_pin_view(pred_block, target_block,
                                          incoming->value_name)) {
            return false;
        }
        source = llvm_mir_get_var_entry(vars, var_count, incoming->value_name);
        if (source == NULL || source->alloca == NULL || source->type != target->type)
            return false;
    }

    if (phi_type_out != NULL)
        *phi_type_out = target->type;
    if (phi_alloca_out != NULL)
        *phi_alloca_out = target->alloca;
    return true;
}

static void
llvm_mir_position_before_original_first(LLVMGenCtx *ctx,
                                        LLVMBasicBlockRef block,
                                        LLVMValueRef first_inst)
{
    if (ctx == NULL || block == NULL)
        return;
    if (first_inst != NULL)
        LLVMPositionBuilderBefore(ctx->builder, first_inst);
    else
        LLVMPositionBuilderAtEnd(ctx->builder, block);
}

void
llvm_mir_emit_true_phi_nodes(const MIRRoutine *routine,
                             LLVMGenCtx *ctx,
                             LLVMBasicBlockRef *llvm_blocks,
                             LLVMBasicBlockRef *llvm_block_heads,
                             LLVMMirVar *vars,
                             size_t var_count)
{
    LLVMBasicBlockRef saved_block;

    if (routine == NULL || ctx == NULL || llvm_blocks == NULL
        || llvm_block_heads == NULL)
        return;

    saved_block = LLVMGetInsertBlock(ctx->builder);

    for (size_t b = 0; b < routine->block_count; b++) {
        const MIRBasicBlock *target_block = &routine->blocks[b];
        LLVMValueRef first_inst;
        LLVMValueRef *phis = NULL;
        LLVMValueRef *allocas = NULL;
        size_t phi_count = 0;

        if (!target_block->is_reachable || target_block->is_cleanup)
            continue;

        /*
         * PHI nodes must be inserted at the original block head, not the
         * tail. llvm_blocks[b] may have been updated by block emit to point
         * at a split tail block (Coalesce / short-circuit lowering). The
         * head map preserves the entry-block identity that matches MIR's
         * logical predecessor edges.
         */
        first_inst = LLVMGetFirstInstruction(llvm_block_heads[b]);
        for (size_t i = 0; i < target_block->instruction_count; i++) {
            const MIRInstruction *inst = &target_block->instructions[i];
            LLVMTypeRef phi_type = NULL;
            LLVMValueRef phi_alloca = NULL;
            LLVMValueRef phi;
            LLVMValueRef *incoming_values;
            LLVMBasicBlockRef *incoming_blocks;

            if (!llvm_mir_phi_is_lowerable(routine, target_block, inst, vars,
                                           var_count, &phi_type, &phi_alloca)) {
                continue;
            }

            incoming_values = pgy_arena_calloc(&ctx->scratch,
                inst->phi_incoming_count * sizeof(LLVMValueRef));
            incoming_blocks = pgy_arena_calloc(&ctx->scratch,
                inst->phi_incoming_count * sizeof(LLVMBasicBlockRef));
            if (incoming_values == NULL || incoming_blocks == NULL)
                return;

            llvm_mir_position_before_original_first(ctx, llvm_block_heads[b], first_inst);
            phi = LLVMBuildPhi(ctx->builder, phi_type, inst->result_name);

            for (size_t j = 0; j < inst->phi_incoming_count; j++) {
                const MIRPhiIncoming *incoming = &inst->phi_incomings[j];
                LLVMMirVar *source = llvm_mir_get_var_entry(
                    vars, var_count, incoming->value_name);
                LLVMBasicBlockRef pred_bb = llvm_blocks[incoming->predecessor_block];
                LLVMValueRef terminator = LLVMGetBasicBlockTerminator(pred_bb);

                if (terminator != NULL)
                    LLVMPositionBuilderBefore(ctx->builder, terminator);
                else
                    LLVMPositionBuilderAtEnd(ctx->builder, pred_bb);

                incoming_values[j] = LLVMBuildLoad2(ctx->builder, source->type,
                    source->alloca, llvm_tmp_name(ctx));
                incoming_blocks[j] = pred_bb;
            }

            LLVMAddIncoming(phi, incoming_values, incoming_blocks,
                (unsigned)inst->phi_incoming_count);

            if (phi_count == 0) {
                phis = pgy_arena_calloc(&ctx->scratch,
                    target_block->instruction_count * sizeof(LLVMValueRef));
                allocas = pgy_arena_calloc(&ctx->scratch,
                    target_block->instruction_count * sizeof(LLVMValueRef));
                if (phis == NULL || allocas == NULL)
                    return;
            }
            phis[phi_count] = phi;
            allocas[phi_count] = phi_alloca;
            phi_count++;
        }

        if (phi_count == 0)
            continue;

        llvm_mir_position_before_original_first(ctx, llvm_blocks[b], first_inst);
        for (size_t i = 0; i < phi_count; i++) {
            if (phis[i] != NULL && allocas[i] != NULL)
                LLVMBuildStore(ctx->builder, phis[i], allocas[i]);
        }
    }

    if (saved_block != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_block);
}

#endif /* PGY_LLVM_ENABLED */
