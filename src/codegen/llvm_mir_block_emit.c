/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_block_emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "llvm_internal_api.h"

static bool
llvm_mir_instruction_has_source_ast_payload(const MIRInstruction *inst)
{
    return mir_instruction_source_payload(inst) != NULL;
}

static bool
llvm_mir_branch_has_required_condition_fact(const MIRInstruction *inst)
{
    if (inst == NULL)
        return false;
    if (mir_instruction_branch_requires_source_emit(inst))
        return llvm_mir_instruction_has_source_ast_payload(inst);
    return inst->expr0 != NULL;
}

static bool
llvm_mir_def_uses_source_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_source_statement_emit(inst)
        && llvm_mir_instruction_has_source_ast_payload(inst);
}

static bool
llvm_mir_def_uses_source_local_decl_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_source_local_decl_emit(inst)
        && llvm_mir_instruction_has_source_ast_payload(inst);
}

static bool
llvm_mir_def_uses_channel_receive_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_channel_receive_statement_emit(inst)
        && llvm_mir_instruction_has_source_ast_payload(inst);
}

static bool
llvm_mir_def_uses_select_receive_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_select_receive_statement_emit(inst)
        && llvm_mir_instruction_has_source_ast_payload(inst);
}

static bool
llvm_mir_stmt_instruction_is_cfg_container(const MIRInstruction *inst)
{
    return mir_instruction_source_is_cfg_container(inst);
}

void
llvm_emit_mir_block_with_exprs(const MIRBasicBlock *mir_block, const MIRRoutine *routine,
                               LLVMGenCtx *ctx, LLVMBasicBlockRef *llvm_blocks,
                               LLVMMirVar *vars, size_t var_count, ASTNode *func_decl,
                               LLVMClassTypeEntry *owner_cls, LLVMFuncEntry *owner_sync,
                               const char *owner_name)
{
    (void)routine;
    (void)func_decl;

    if (mir_block->instruction_count > 0 && mir_block->instructions == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR block emission failed: block %llu has instruction count without instruction inventory",
            (unsigned long long) mir_block->id);
        return;
    }

    LLVMBasicBlockRef llvm_block = llvm_blocks[mir_block->id];
    LLVMPositionBuilderAtEnd(ctx->builder, llvm_block);
    bool emitted_terminator = false;

    if (!llvm_mir_emit_pin_enter(mir_block, ctx))
        return;
    if (!llvm_mir_emit_for_in_body_binding(routine, mir_block, ctx))
        return;

    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        ASTNode *source_payload = mir_instruction_source_payload(inst);
        if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL) {
            fprintf(stderr,
                "[llvm inst] block=%zu inst=%zu kind=%d ast=%d result=%s\n",
                mir_block->id, i, (int)inst->kind,
                mir_instruction_source_ast_type_or(inst, -1),
                inst->result_name != NULL ? inst->result_name : "-");
        }
        switch (inst->kind) {
        case MIR_INST_RESOURCE_OP:
            if (mir_instruction_is_with_slot_claim(inst)) {
                llvm_mir_emit_with_claim_only(inst, ctx);
            }
            llvm_mir_emit_borrow_view_alias(inst, ctx);
            break;
        case MIR_INST_DEF:
            if (inst->result_name != NULL) {
                if (llvm_mir_def_uses_channel_receive_statement_emit(inst)) {
                    LLVMValueRef mir_alloca =
                        llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL
                        && llvm_mir_def_uses_select_receive_statement_emit(inst)) {
                        fprintf(stderr, "[llvm inst] emit_select_receive_def\n");
                    }
                    if (!llvm_mir_emit_channel_receive_def(inst, ctx,
                                                           mir_alloca))
                        return;
                    break;
                }
                if (llvm_mir_def_uses_source_statement_emit(inst)) {
                    if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
                        fprintf(stderr, llvm_mir_def_uses_source_local_decl_emit(inst)
                            ? "[llvm inst] emit_source_local_decl\n"
                            : "[llvm inst] emit_source_statement\n");
                    llvm_emit_statement(source_payload, ctx);
                } else {
                    LLVMValueRef alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (getenv("PGY_DEBUG_LLVM_DETAIL") != NULL)
                        fprintf(stderr, "[llvm inst] emit_expression_store\n");
                    LLVMValueRef val = inst->expr0 != NULL
                        ? llvm_emit_expression(inst->expr0, ctx)
                        : NULL;
                    if (val != NULL && alloca != NULL) {
                        LLVMBuildStore(ctx->builder, val, alloca);
                    }
                }
            }
            break;
        case MIR_INST_PHI:
            break;
        case MIR_INST_BRANCH:
            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) != NULL) {
                emitted_terminator = true;
                break;
            }
            if (llvm_mir_branch_has_required_condition_fact(inst)
                && mir_block->has_succ_true
                && mir_block->has_succ_false) {
                LLVMValueRef cond;
                if (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                    || inst->branch_shape == MIR_BRANCH_FOR_IN) {
                    cond = llvm_mir_emit_for_loop_condition(inst, ctx);
                } else if (inst->branch_shape == MIR_BRANCH_MATCH_CASE) {
                    cond = llvm_mir_emit_match_case_condition(func_decl,
                                                              source_payload,
                                                              ctx);
                } else if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH) {
                    cond = llvm_mir_emit_select_dispatch_condition(
                        source_payload, routine, mir_block->succ_true, ctx);
                    if (cond == NULL)
                        cond = LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, 0);
                } else {
                    cond = llvm_emit_expression(inst->expr0, ctx);
                }
                if (cond != NULL) {
                    LLVMBasicBlockRef true_bb = llvm_blocks[mir_block->succ_true];
                    LLVMBasicBlockRef false_bb = llvm_blocks[mir_block->succ_false];
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    LLVMBuildCondBr(ctx->builder, cond, true_bb, false_bb);
                    emitted_terminator = true;
                }
            } else if (mir_block->has_succ_true) {
                if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                    return;
                LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
                emitted_terminator = true;
            }
            break;
        case MIR_INST_RETURN:
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (inst->expr0 != NULL) {
                ASTNode *return_expr = inst->expr0;
                const char *saved_expected_type_name = ctx->expected_type_name;
                LLVMValueRef val;
                if (ctx->current_func_decl != NULL
                    && ctx->current_func_decl->type == AST_FUNC_DECL
                    && ctx->current_func_decl->data.func_decl.return_type != NULL) {
                    ctx->expected_type_name = llvm_stmt_render_type_annotation_static(
                        ctx->current_func_decl->data.func_decl.return_type);
                }
                val = llvm_emit_expression(return_expr, ctx);
                ctx->expected_type_name = saved_expected_type_name;
                if (val != NULL) {
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    LLVMBuildRet(ctx->builder, val);
                    emitted_terminator = true;
                } else {
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    LLVMBuildRetVoid(ctx->builder);
                }
            } else {
                if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                    return;
                LLVMBuildRetVoid(ctx->builder);
            }
            emitted_terminator = true;
            break;
        case MIR_INST_CLEANUP_EDGE:
            break;
        case MIR_INST_LOOP_INIT:
            if (!llvm_mir_emit_for_loop_init(inst, ctx))
                return;
            break;
        case MIR_INST_STMT:
            if (mir_instruction_source_is_defer_stmt(inst)) {
                if (inst->expr0 != NULL)
                    llvm_register_defer(inst->expr0, ctx);
            } else if (source_payload != NULL) {
                if (!mir_instruction_source_stmt_fallback_is_allowed(inst)) {
                    llvm_set_error_at_with_hints(
                        ctx,
                        source_payload,
                        PGY_CODE_MIR_TOPOLOGY_INVALID,
                        PGY_CAUSE_MIR_TOPOLOGY_INVALID,
                        PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
                        "LLVM MIR block emission rejected STMT fallback outside allowed residual statement policy");
                    return;
                }
                if (llvm_mir_stmt_instruction_is_cfg_container(inst))
                    break;
                llvm_emit_statement(source_payload, ctx);
            }
            break;
        default:
            break;
        }
    }

    if (!emitted_terminator) {
        if (mir_block->has_succ_true && mir_block->has_succ_false) {
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            LLVMBuildCondBr(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(
                                LLVMGetModuleContext(ctx->module)), 1, false),
                            llvm_blocks[mir_block->succ_true],
                            llvm_blocks[mir_block->succ_false]);
        } else if (mir_block->has_succ_true) {
            if (!llvm_mir_emit_loop_backedge_increment(routine, mir_block, ctx))
                return;
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            LLVMBuildBr(ctx->builder, llvm_blocks[mir_block->succ_true]);
        } else {
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            if (ctx->current_ret_type == ctx->type_void) {
                LLVMBuildRetVoid(ctx->builder);
            } else {
                LLVMBuildRet(ctx->builder, LLVMConstNull(ctx->current_ret_type));
            }
        }
    }
}

#endif
