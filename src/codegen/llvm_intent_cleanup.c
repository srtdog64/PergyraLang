/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM intent cleanup / rollback / invalidation tail emission.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

static bool
llvm_intent_cleanup_carrier_fail(LLVMGenCtx *ctx, const char *msg)
{
    llvm_set_mir_intent_carrier_missing(ctx, "%s", msg);
    return false;
}

static void
llvm_emit_intent_mir_resource_block(LLVMGenCtx *ctx,
                                    const MIRBasicBlock *block,
                                    LLVMValueRef handle)
{
    if (ctx == NULL || block == NULL)
        return;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_CLEANUP_EDGE
            || inst->kind == MIR_INST_RESOURCE_OP) {
            llvm_emit_mir_resource_hook(ctx, inst, handle, true);
        }
    }
}

bool
llvm_emit_intent_cleanup_tail(LLVMGenCtx *ctx,
                              ASTNode *node,
                              const MIRRoutine *mir_routine,
                              ASTNode **step_nodes,
                              const char **mir_step_names,
                              LLVMValueRef *completed_allocas,
                              size_t step_count,
                              bool mir_only_intent,
                              LLVMValueRef handle_alloca,
                              LLVMValueRef failed_alloca,
                              LLVMBasicBlockRef compensate_bb,
                              LLVMBasicBlockRef maybe_exit_bb,
                              LLVMBasicBlockRef do_exit_bb,
                              LLVMBasicBlockRef ret_bb,
                              LLVMFuncEntry *exit_fn)
{
    if (ctx == NULL || node == NULL)
        return false;

    if (mir_routine != NULL && mir_routine->has_cleanup_block) {
        const MIRBasicBlock *cleanup_block =
            &mir_routine->blocks[mir_routine->cleanup_block];
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        llvm_emit_intent_mir_resource_block(ctx, cleanup_block, handle);
    }
    {
        LLVMValueRef failed = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            failed_alloca, llvm_tmp_name(ctx));
        if (mir_routine != NULL && mir_routine->has_rollback_block)
            LLVMBuildCondBr(ctx->builder, failed, compensate_bb, maybe_exit_bb);
        else
            LLVMBuildBr(ctx->builder, maybe_exit_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, compensate_bb);
    if (mir_routine != NULL && mir_routine->has_rollback_block) {
        const MIRBasicBlock *rollback_block =
            &mir_routine->blocks[mir_routine->rollback_block];
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        llvm_emit_intent_mir_resource_block(ctx, rollback_block, handle);
    }
    if (completed_allocas != NULL) {
        for (size_t i = step_count; i-- > 0;) {
            ASTNode *step = step_nodes[i];
            const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
            ASTNode **compensate_exprs = NULL;
            size_t compensate_expr_count = 0;
            const char *zone_type_name = NULL;
            const char *zone_alias = NULL;
            const char *from_alias = NULL;
            const char **who_aliases = NULL;
            size_t who_alias_count = 0;
            bool has_compensate = false;
            if (step != NULL && step->type == AST_INTENT_STEP) {
                if (step_name == NULL)
                    step_name = ast_intent_step_name(step);
                has_compensate = mir_only_intent
                    ? llvm_mir_intent_has_stmt(
                        mir_routine, step_name,
                        "IntentEval", "compensate")
                    : ast_intent_step_compensate_expr_count(step) > 0;
            }
            if (step == NULL || step->type != AST_INTENT_STEP || !has_compensate)
                continue;
            if (mir_routine != NULL) {
                compensate_expr_count = llvm_collect_mir_intent_eval_exprs(
                    mir_routine, ctx, step_name, "compensate", &compensate_exprs);
                zone_type_name = llvm_find_mir_intent_meta_arg(
                    mir_routine, step_name, "IntentZoneWhere");
                zone_alias = llvm_find_mir_intent_meta_arg(
                    mir_routine, step_name, "IntentZoneAlias");
                from_alias = llvm_find_mir_intent_meta_arg(
                    mir_routine, step_name, "IntentZoneFrom");
                who_alias_count = llvm_collect_mir_intent_who_aliases(
                    mir_routine, ctx, step_name, &who_aliases);
            }
            if (mir_only_intent
                && llvm_mir_intent_has_stmt(mir_routine, step_name,
                                            "IntentEval", "compensate")
                && compensate_expr_count == 0) {
                return llvm_intent_cleanup_carrier_fail(ctx,
                    "MIR-only LLVM path missing intent compensate eval carrier");
            }
            if (!mir_only_intent && compensate_expr_count == 0) {
                compensate_expr_count = ast_intent_step_compensate_expr_count(step);
                compensate_exprs = ast_intent_step_compensate_exprs(step, NULL);
            }
            if (mir_only_intent) {
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneWhere", NULL)
                    && zone_type_name == NULL) {
                    return llvm_intent_cleanup_carrier_fail(ctx,
                        "MIR-only LLVM path missing intent zone where metadata");
                }
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneAlias", NULL)
                    && zone_alias == NULL) {
                    return llvm_intent_cleanup_carrier_fail(ctx,
                        "MIR-only LLVM path missing intent zone alias metadata");
                }
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneFrom", NULL)
                    && from_alias == NULL) {
                    return llvm_intent_cleanup_carrier_fail(ctx,
                        "MIR-only LLVM path missing intent transfer-from metadata");
                }
                if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentWho", NULL)
                    && who_alias_count == 0) {
                    return llvm_intent_cleanup_carrier_fail(ctx,
                        "MIR-only LLVM path missing intent who metadata");
                }
            } else {
                if (zone_type_name == NULL
                    && ast_intent_step_where_type(step) != NULL
                    && ast_intent_step_where_type(step)->type == AST_TYPE) {
                    zone_type_name = ast_intent_step_where_type(step)->data.type.name;
                }
                if (zone_alias == NULL)
                    zone_alias = llvm_intent_step_effective_zone_alias(step);
                if (from_alias == NULL)
                    from_alias = ast_intent_step_transfer_from_alias(step);
                if (who_alias_count == 0) {
                    who_alias_count = ast_intent_step_who_count(step);
                    who_aliases = (const char **)ast_intent_step_who_names(step, NULL);
                }
            }
            {
                LLVMBasicBlockRef do_bb =
                    LLVMAppendBasicBlockInContext(ctx->context,
                                                  ctx->current_function,
                                                  "intent.comp.do");
                LLVMBasicBlockRef next_bb =
                    LLVMAppendBasicBlockInContext(ctx->context,
                                                  ctx->current_function,
                                                  "intent.comp.next");
                LLVMValueRef done = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
                    completed_allocas[i], llvm_tmp_name(ctx));
                LLVMBuildCondBr(ctx->builder, done, do_bb, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, do_bb);
                for (size_t j = compensate_expr_count; j-- > 0;) {
                    if (compensate_exprs[j] != NULL)
                        (void)llvm_emit_expression(compensate_exprs[j], ctx);
                }
                llvm_emit_intent_step_bind_bound_zone(
                    ctx, node, zone_type_name, zone_alias, from_alias,
                    who_aliases, who_alias_count);
                if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT)
                    LLVMBuildBr(ctx->builder, maybe_exit_bb);
                else
                    LLVMBuildBr(ctx->builder, next_bb);
                LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
            }
        }
    }
    LLVMBuildBr(ctx->builder, maybe_exit_bb);

    LLVMPositionBuilderAtEnd(ctx->builder, maybe_exit_bb);
    if (mir_routine != NULL && mir_routine->has_invalidation_block) {
        const MIRBasicBlock *invalidation_block =
            &mir_routine->blocks[mir_routine->invalidation_block];
        LLVMValueRef hook_handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        llvm_emit_intent_mir_resource_block(ctx, invalidation_block, hook_handle);
    }
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef entered = LLVMBuildICmp(ctx->builder, LLVMIntNE, handle,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, entered, do_exit_bb, ret_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, do_exit_bb);
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef exit_args[] = { handle };
        LLVMBuildCall2(ctx->builder, exit_fn->fn_type, exit_fn->fn, exit_args, 1, "");
        LLVMBuildBr(ctx->builder, ret_bb);
    }
    return true;
}

#endif
