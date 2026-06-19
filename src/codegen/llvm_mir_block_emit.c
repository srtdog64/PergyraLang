/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_block_emit.h"
#include "llvm_mir_await_emit.h"
#include "llvm_mir_host_field.h"
#include "llvm_mir_local_emit.h"
#include "llvm_mir_scope_bind.h"
#include "llvm_mir_source_def_copy.h"
#include "llvm_mir_source_resource_defs.h"

#include <stdio.h>
#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_mir_resource_view.h"
#include "../parser/ast_api.h"

static bool
llvm_mir_def_uses_source_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_source_statement_emit(inst);
}

static bool
llvm_mir_def_uses_source_local_decl_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_source_local_decl_emit(inst);
}

static bool
llvm_mir_def_uses_channel_receive_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_channel_receive_statement_emit(inst);
}

static bool
llvm_mir_def_uses_select_receive_statement_emit(const MIRInstruction *inst)
{
    return mir_instruction_uses_select_receive_statement_emit(inst);
}

static const char *
llvm_mir_instruction_expected_type_name(const MIRRoutine *routine,
                                        const MIRInstruction *inst)
{
    const char *type_name;
    char base_name[128];

    if (inst == NULL)
        return NULL;
    if (inst->abi_type_name != NULL && inst->abi_type_name[0] != '\0')
        return inst->abi_type_name;
    if (routine == NULL)
        return NULL;
    if (inst->arg0 != NULL) {
        type_name = mir_routine_source_local_type_name(routine, inst->arg0);
        if (type_name != NULL && type_name[0] != '\0')
            return type_name;
    }
    if (inst->result_name != NULL
        && llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        type_name = mir_routine_source_local_type_name(routine, base_name);
        if (type_name != NULL && type_name[0] != '\0')
            return type_name;
    }
    return NULL;
}

static ASTNode *
llvm_mir_return_callable_type(LLVMGenCtx *ctx, const MIRRoutine *routine)
{
    ASTNode *return_type = llvm_mir_routine_return_type(routine);

    if (return_type != NULL) {
        if (return_type->type == AST_EVENT_HANDLER_TYPE)
            return return_type;
        return NULL;
    }
    return llvm_stmt_current_return_callable_type(ctx);
}

static bool
llvm_mir_stmt_instruction_is_cfg_container(const MIRInstruction *inst)
{
    return mir_instruction_source_is_cfg_container(inst);
}

static void
llvm_mir_bind_versioned_local_scope(LLVMGenCtx *ctx,
                                    LLVMMirVar *vars,
                                    size_t var_count,
                                    const char *versioned_name,
                                    const char *type_name)
{
    char base_name[128];
    LLVMMirVar *entry;

    if (ctx == NULL || versioned_name == NULL)
        return;
    if (!llvm_mir_base_name_from_versioned(versioned_name, base_name,
            sizeof(base_name))) {
        return;
    }
    entry = llvm_mir_get_var_entry(vars, var_count, versioned_name);
    if (entry == NULL) {
        /* Closure #88: phi-result versioned name has no alloca of its
         * own. MIR routine reports `phi=N` in its meta header but
         * doesn't emit explicit phi instructions, so the bb-entry
         * scope-seed step finds nothing to bind. If we leave the
         * previous block's binding in place, the next consumer of the
         * base identifier loads from a sibling-block-only alloca that
         * the current path never wrote to — that's the uninitialized
         * stack memory that shows up as `"findings":[<garbage>]}` in
         * the air_graph_json_validator self-host CI smoke. Rebind the
         * base name to the FIRST SSA-versioned alloca for that slot
         * (e.g. `findings.1`); upstream MIR keeps that first version's
         * alloca in sync with the merged value at every branch
         * exit (each branch stores its new value to `%findings.1`
         * before copying to its versioned snapshot). */
        char first_ver[128];
        int written = snprintf(first_ver, sizeof(first_ver), "%s.1",
            base_name);
        if (written > 0 && (size_t)written < sizeof(first_ver)) {
            LLVMMirVar *base_entry =
                llvm_mir_get_var_entry(vars, var_count, first_ver);
            if (base_entry != NULL && base_entry->alloca != NULL) {
                llvm_mir_bind_base_local_scope(ctx, base_name,
                    base_entry->alloca, base_entry->type, type_name);
            }
        }
        return;
    }
    if (strcmp(base_name, versioned_name) != 0
        && llvm_lookup_channel_inner(ctx, base_name) != NULL) {
        return;
    }
    if (strcmp(base_name, versioned_name) != 0
        && llvm_lookup_slot_inner(ctx, base_name) != NULL
        && llvm_lookup_slot_inner(ctx, versioned_name) != NULL) {
        return;
    }
    llvm_mir_bind_base_local_scope(ctx, base_name, entry->alloca,
        entry->type, type_name);
}

static void
llvm_mir_seed_block_entry_scope(const MIRBasicBlock *mir_block,
                                LLVMGenCtx *ctx,
                                LLVMMirVar *vars,
                                size_t var_count)
{
    if (mir_block == NULL)
        return;
    for (size_t i = 0; i < mir_block->ssa_entry_value_count; i++) {
        llvm_mir_bind_versioned_local_scope(ctx, vars, var_count,
            mir_block->ssa_entry_values[i], NULL);
    }
}

static void
llvm_mir_seed_block_phi_scope(const MIRBasicBlock *mir_block,
                              LLVMGenCtx *ctx,
                              LLVMMirVar *vars,
                              size_t var_count)
{
    if (mir_block == NULL)
        return;
    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        if (inst->kind == MIR_INST_PHI && inst->result_name != NULL)
            llvm_mir_bind_versioned_local_scope(ctx, vars, var_count,
                inst->result_name, inst->arg1);
    }
}

void
llvm_emit_mir_block_with_exprs(const MIRBasicBlock *mir_block,
                               const MIRRoutine *routine,
                               LLVMGenCtx *ctx,
                               LLVMBasicBlockRef *llvm_block_heads,
                               LLVMBasicBlockRef *llvm_block_tails,
                               LLVMMirVar *vars, size_t var_count,
                               LLVMClassTypeEntry *owner_cls, LLVMFuncEntry *owner_sync,
                               const char *owner_name)
{
    if (mir_block == NULL || routine == NULL || ctx == NULL
        || llvm_block_heads == NULL || llvm_block_tails == NULL)
        return;
    if (mir_block->id >= routine->block_count) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR block emission failed: block id %llu is outside routine block inventory",
            (unsigned long long)mir_block->id);
        return;
    }

    if (mir_block->instruction_count > 0 && mir_block->instructions == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR block emission failed: block %llu has instruction count without instruction inventory",
            (unsigned long long) mir_block->id);
        return;
    }

    LLVMBasicBlockRef llvm_block = llvm_block_heads[mir_block->id];
    LLVMPositionBuilderAtEnd(ctx->builder, llvm_block);
    bool emitted_terminator = false;
    LLVMTypeRef function_ret_type = ctx->current_function_ret_type;
    if (function_ret_type == NULL)
        function_ret_type = ctx->current_ret_type;
    llvm_block_tails[mir_block->id] = llvm_block;

    if (!llvm_mir_emit_pin_enter(mir_block, ctx))
        return;
    llvm_mir_seed_block_entry_scope(mir_block, ctx, vars, var_count);
    if (ctx->has_error)
        return;
    llvm_mir_seed_block_phi_scope(mir_block, ctx, vars, var_count);
    if (ctx->has_error)
        return;
    if (!llvm_mir_emit_for_loop_body_binding(routine, mir_block, ctx))
        return;
    if (!llvm_mir_emit_for_in_body_binding(routine, mir_block, ctx))
        return;
    if (!llvm_mir_remap_active_match_bindings(routine, mir_block, ctx))
        return;
    if (!llvm_mir_emit_match_case_body_binding(routine, mir_block, ctx))
        return;

    for (size_t i = 0; i < mir_block->instruction_count; i++) {
        const MIRInstruction *inst = &mir_block->instructions[i];
        ASTNode *source_payload = mir_instruction_source_payload(inst);
        llvm_debug_set_line(ctx, mir_instruction_source_line(inst));
        if (llvm_debug_detail_enabled()) {
            fprintf(stderr,
                "[llvm inst] block=%zu inst=%zu kind=%d line=%u result=%s\n",
                mir_block->id, i, (int)inst->kind,
                mir_instruction_source_line(inst),
                inst->result_name != NULL ? inst->result_name : "-");
        }
        switch (inst->kind) {
        case MIR_INST_RESOURCE_OP:
            if (mir_instruction_is_with_slot_claim(inst)) {
                llvm_mir_emit_with_claim_only(inst, ctx);
            }
            if (!llvm_mir_emit_borrow_view_alias(inst, ctx))
                return;
            break;
        case MIR_INST_DEF:
            if (inst->result_name != NULL) {
                if (llvm_mir_def_uses_channel_receive_statement_emit(inst)) {
                    LLVMValueRef mir_alloca =
                        llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (llvm_debug_detail_enabled()
                        && llvm_mir_def_uses_select_receive_statement_emit(inst)) {
                        fprintf(stderr, "[llvm inst] emit_select_receive_def\n");
                    }
                    if (!llvm_mir_emit_channel_receive_def(inst, ctx,
                                                           mir_alloca))
                        return;
                    break;
                }
                if (llvm_mir_def_uses_source_statement_emit(inst)) {
                    bool await_def_handled = false;
                    LLVMValueRef alloca;
                    LLVMValueRef val;
                    if (!llvm_mir_try_emit_await_local_def(inst, mir_block,
                            routine, ctx, vars, var_count,
                            &await_def_handled)) {
                        return;
                    }
                    if (await_def_handled)
                        break;
                    if (llvm_debug_detail_enabled())
                        fprintf(stderr, llvm_mir_def_uses_source_local_decl_emit(inst)
                            ? "[llvm inst] emit_source_local_decl\n"
                            : "[llvm inst] emit_source_statement\n");
                    alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (alloca == NULL) {
                        llvm_set_mir_inventory_missing(ctx,
                            "LLVM MIR source-statement DEF '%s' has no MIR local storage",
                            inst->result_name != NULL
                                ? inst->result_name
                                : "(anonymous-local)");
                        return;
                    }
                    if (inst->expr0 == NULL) {
                        llvm_set_mir_topology_invalid(ctx,
                            "LLVM MIR source-statement DEF is missing expression fact");
                        return;
                    }
                    {
                        bool resource_let_handled = false;
                        const char *expected_type_name =
                            llvm_mir_instruction_expected_type_name(
                                routine, inst);
                        if (!llvm_mir_try_emit_source_resource_let(inst,
                                source_payload, alloca, ctx,
                                expected_type_name,
                                &resource_let_handled)) {
                            return;
                        }
                        if (resource_let_handled)
                            break;
                    }
                    {
                        const char *saved_expected_type_name =
                            ctx->expected_type_name;
                        LLVMTypeRef saved_current_ret_type =
                            ctx->current_ret_type;
                        const char *expected_type_name =
                            llvm_mir_instruction_expected_type_name(
                                routine, inst);
                        LLVMMirVar *target_entry =
                            llvm_mir_get_var_entry(vars, var_count,
                                inst->result_name);
                        ASTNode *emit_expr = source_payload != NULL
                            && source_payload->type == AST_ASSIGNMENT
                                ? source_payload
                                : inst->expr0;
                        if (expected_type_name != NULL
                            && expected_type_name[0] != '\0') {
                            ctx->expected_type_name = expected_type_name;
                        }
                        if (target_entry != NULL
                            && target_entry->type != NULL) {
                            ctx->current_ret_type = target_entry->type;
                        }
                        if (!llvm_stmt_require_non_void_value(ctx, emit_expr,
                                "LLVM MIR source-statement DEF cannot consume a Void expression value")) {
                            ctx->current_ret_type = saved_current_ret_type;
                            ctx->expected_type_name = saved_expected_type_name;
                            return;
                        }
                        val = llvm_emit_expression(emit_expr, ctx);
                        if (val != NULL
                            && emit_expr != NULL
                            && emit_expr->type == AST_CALL) {
                            llvm_stmt_emit_zone_action_effect_runtime(
                                emit_expr, ctx);
                        }
                        ctx->current_ret_type = saved_current_ret_type;
                        ctx->expected_type_name = saved_expected_type_name;
                    }
                    if (val == NULL) {
                        if (!ctx->has_error) {
                            llvm_set_mir_topology_invalid(ctx,
                                "LLVM MIR source-statement DEF expression could not be lowered");
                        }
                        return;
                    }
                    LLVMBuildStore(ctx->builder, val, alloca);
                    if (!llvm_mir_copy_source_def_to_versioned_local(
                            inst, mir_block, ctx, vars, var_count)) {
                        return;
                    }
                } else {
                    LLVMValueRef alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (llvm_debug_detail_enabled())
                        fprintf(stderr, "[llvm inst] emit_expression_store\n");
                    LLVMValueRef val = NULL;
                    if (inst->expr0 != NULL) {
                        const char *saved_expected_type_name =
                            ctx->expected_type_name;
                        LLVMTypeRef saved_current_ret_type =
                            ctx->current_ret_type;
                        const char *expected_type_name =
                            llvm_mir_instruction_expected_type_name(
                                routine, inst);
                        LLVMMirVar *target_entry =
                            llvm_mir_get_var_entry(vars, var_count,
                                inst->result_name);
                        if (expected_type_name != NULL
                            && expected_type_name[0] != '\0') {
                            ctx->expected_type_name = expected_type_name;
                        }
                        if (target_entry != NULL
                            && target_entry->type != NULL) {
                            ctx->current_ret_type = target_entry->type;
                        }
                        if (!llvm_stmt_require_non_void_value(ctx, inst->expr0,
                                "LLVM MIR DEF cannot consume a Void expression value")) {
                            ctx->current_ret_type = saved_current_ret_type;
                            ctx->expected_type_name = saved_expected_type_name;
                            return;
                        }
                        val = llvm_emit_expression(inst->expr0, ctx);
                        ctx->current_ret_type = saved_current_ret_type;
                        ctx->expected_type_name = saved_expected_type_name;
                    }
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
            if (mir_instruction_has_required_branch_lowering_fact(inst)
                && mir_block->has_succ_true
                && mir_block->has_succ_false) {
                LLVMValueRef cond;
                if (inst->branch_shape == MIR_BRANCH_FOR_RANGE
                    || inst->branch_shape == MIR_BRANCH_FOR_IN) {
                    cond = llvm_mir_emit_for_loop_condition(inst, ctx);
                } else if (inst->branch_shape == MIR_BRANCH_MATCH_CASE) {
                    cond = llvm_mir_emit_match_case_condition(inst, ctx);
                } else if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH) {
                    cond = llvm_mir_emit_select_dispatch_condition(
                        inst, routine, mir_block->succ_true, ctx);
                    if (cond == NULL && ctx->has_error)
                        return;
                } else {
                    if (!llvm_stmt_require_non_void_value(ctx, inst->expr0,
                            "LLVM MIR branch cannot consume a Void expression as condition")) {
                        return;
                    }
                    cond = llvm_emit_expression(inst->expr0, ctx);
                }
                if (cond == NULL) {
                    if (ctx->has_error)
                        return;
                    llvm_set_mir_topology_invalid(ctx,
                        "LLVM MIR branch condition could not be lowered");
                    return;
                }
                if (cond != NULL) {
                    LLVMBasicBlockRef true_bb = llvm_block_heads[mir_block->succ_true];
                    LLVMBasicBlockRef false_bb = llvm_block_heads[mir_block->succ_false];
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    llvm_block_tails[mir_block->id] =
                        LLVMGetInsertBlock(ctx->builder);
                    LLVMBuildCondBr(ctx->builder, cond, true_bb, false_bb);
                    emitted_terminator = true;
                }
            } else if (mir_block->has_succ_true) {
                if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                    return;
                llvm_block_tails[mir_block->id] =
                    LLVMGetInsertBlock(ctx->builder);
                LLVMBuildBr(ctx->builder, llvm_block_heads[mir_block->succ_true]);
                emitted_terminator = true;
            }
            break;
        case MIR_INST_RETURN:
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (inst->expr0 != NULL) {
                ASTNode *return_expr = inst->expr0;
                const char *saved_expected_type_name = ctx->expected_type_name;
                ASTNode *saved_expected_callable_type =
                    ctx->expected_callable_type;
                const char *mir_return_type_name =
                    llvm_mir_routine_return_type_name(routine);
                ASTNode *mir_return_type =
                    llvm_mir_routine_return_type(routine);
                ASTNode *mir_callable_type =
                    llvm_mir_return_callable_type(ctx, routine);
                LLVMValueRef val;
                if (function_ret_type == ctx->type_void) {
                    llvm_set_error_at_with_hints(ctx, return_expr,
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_INSPECT_MIR_INVENTORY,
                        "LLVM MIR void function return must not carry a value expression");
                    return;
                }
                if (mir_return_type_name != NULL) {
                    ctx->expected_type_name = mir_return_type_name;
                } else if (mir_return_type != NULL) {
                    ctx->expected_type_name =
                        llvm_stmt_render_type_annotation_copy(ctx, mir_return_type);
                }
                if (mir_callable_type != NULL)
                    ctx->expected_callable_type = mir_callable_type;
                if (!llvm_stmt_require_non_void_value(ctx, return_expr,
                        "LLVM MIR return cannot consume a Void expression value")) {
                    ctx->expected_callable_type = saved_expected_callable_type;
                    ctx->expected_type_name = saved_expected_type_name;
                    return;
                }
                val = llvm_emit_expression(return_expr, ctx);
                ctx->expected_callable_type = saved_expected_callable_type;
                ctx->expected_type_name = saved_expected_type_name;
                if (val != NULL) {
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    llvm_emit_mut_ref_writebacks(ctx);
                    LLVMBuildRet(ctx->builder, val);
                    emitted_terminator = true;
                } else {
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    if (!ctx->has_error) {
                        llvm_set_error_at_with_hints(ctx, return_expr,
                            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                            PGY_FIX_INSPECT_MIR_INVENTORY,
                            "LLVM MIR return could not lower value expression");
                    }
                    if (LLVMGetBasicBlockTerminator(
                            LLVMGetInsertBlock(ctx->builder)) == NULL) {
                        LLVMBuildUnreachable(ctx->builder);
                    }
                    return;
                }
            } else {
                if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                    return;
                llvm_emit_mut_ref_writebacks(ctx);
                if (function_ret_type == ctx->type_void) {
                    LLVMBuildRetVoid(ctx->builder);
                } else {
                    llvm_set_mir_topology_invalid(ctx,
                        "LLVM MIR non-Void return requires a value expression");
                    LLVMBuildUnreachable(ctx->builder);
                }
            }
            emitted_terminator = true;
            break;
        case MIR_INST_CLEANUP_EDGE:
            break;
        case MIR_INST_LOOP_INIT:
            if (!llvm_mir_emit_for_loop_init(inst, ctx))
                return;
            break;
        case MIR_INST_DESTRUCTURE:
            if (source_payload == NULL
                || source_payload->type != AST_LET_DESTRUCTURE) {
                llvm_set_mir_topology_invalid(ctx,
                    "LLVM MIR DESTRUCTURE instruction missing destructure payload");
                return;
            }
            llvm_emit_let_destructure_stmt(source_payload, ctx);
            break;
        case MIR_INST_ASSIGN:
            if (source_payload == NULL || source_payload->type != AST_ASSIGNMENT) {
                llvm_set_mir_topology_invalid(ctx,
                    "LLVM MIR ASSIGN instruction missing assignment payload");
                return;
            }
            {
                LLVMValueRef assigned = llvm_emit_expression(source_payload, ctx);
                if (assigned == NULL) {
                    if (!ctx->has_error) {
                        llvm_set_mir_topology_invalid(ctx,
                            "LLVM MIR ASSIGN expression could not be lowered");
                    }
                    return;
                }
            }
            break;
        case MIR_INST_STMT:
            if (mir_instruction_source_is_defer_stmt(inst)) {
                if (inst->expr0 != NULL)
                    llvm_register_defer(inst->expr0, ctx);
            } else if (mir_instruction_has_source_statement_order(inst)) {
                if (llvm_mir_stmt_instruction_is_cfg_container(inst))
                    break;
                if (mir_instruction_source_matches_ast_type(inst, AST_CALL)
                    && inst->expr0 != NULL) {
                    LLVMValueRef ignored = llvm_emit_expression(inst->expr0, ctx);
                    if (ignored == NULL && ctx->has_error)
                        return;
                    if (ignored != NULL)
                        llvm_stmt_emit_zone_action_effect_runtime(
                            inst->expr0, ctx);
                    break;
                }
                if (inst->expr0 != NULL
                    && (mir_instruction_source_matches_ast_type(inst, AST_SPAWN_EXPR)
                        || mir_instruction_source_matches_ast_type(inst, AST_AWAIT_EXPR)
                        || mir_instruction_source_matches_ast_type(inst, AST_CHANNEL_SEND)
                        || mir_instruction_source_matches_ast_type(inst, AST_CHANNEL_RECV)
                        || mir_instruction_source_matches_ast_type(inst, AST_EVENT_SUBSCRIBE)
                        || mir_instruction_source_matches_ast_type(inst, AST_EVENT_UNSUBSCRIBE)
                        || mir_instruction_source_matches_ast_type(inst, AST_EVENT_INVOKE)
                        || mir_instruction_source_matches_ast_type(inst, AST_PARALLEL_BLOCK)
                        || mir_instruction_source_matches_ast_type(inst, AST_ASYNC_BLOCK)
                        || mir_instruction_source_matches_ast_type(inst, AST_UNSAFE_BLOCK)
                        || mir_instruction_source_matches_ast_type(inst, AST_TRANSACTION_BLOCK))) {
                    llvm_emit_statement(inst->expr0, ctx);
                    if (ctx->has_error)
                        return;
                    break;
                }
                llvm_set_mir_topology_invalid(ctx,
                    "LLVM MIR STMT source-payload emission is retired; lower this statement to MIR facts");
                return;
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
            llvm_block_tails[mir_block->id] =
                LLVMGetInsertBlock(ctx->builder);
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR block %llu has two successors without a branch condition terminator",
                (unsigned long long)mir_block->id);
            if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(ctx->builder)) == NULL)
                LLVMBuildUnreachable(ctx->builder);
            return;
        } else if (mir_block->has_succ_true) {
            if (!llvm_mir_emit_loop_backedge_increment(routine, mir_block, ctx))
                return;
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            llvm_block_tails[mir_block->id] =
                LLVMGetInsertBlock(ctx->builder);
            LLVMBuildBr(ctx->builder, llvm_block_heads[mir_block->succ_true]);
        } else {
            llvm_emit_defers_from(ctx, 0);
            llvm_mir_emit_owner_sync_exit(ctx, owner_cls, owner_sync, owner_name);
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            llvm_emit_mut_ref_writebacks(ctx);
            if (function_ret_type == ctx->type_void) {
                LLVMBuildRetVoid(ctx->builder);
            } else {
                /* Closure #74: a non-void block with no successors and no
                 * return instruction is either (a) the dead post-merge of
                 * an exhaustive match where every case returned, or (b) a
                 * genuinely missing return in user code. The two cases are
                 * not distinguishable from this position alone because the
                 * exhaustive-match merge has the same shape as the missing-
                 * return case once MIR has lowered match dispatch into
                 * BRANCH-shaped predecessors. We emit `unreachable` so the
                 * exhaustive-match case compiles; for the missing-return
                 * case the LLVM verifier may or may not complain depending
                 * on how the path is reached. See the risk audit entry in
                 * this doc for the trade-off and the planned MIR-side fix
                 * (mark dead post-match blocks unreachable upstream so this
                 * site can be strict again). */
                LLVMBuildUnreachable(ctx->builder);
            }
        }
    }
}

#endif
