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
#include "llvm_mir_resource_view.h"
#include "../parser/ast_api.h"

static bool
llvm_mir_instruction_has_source_ast_payload(const MIRInstruction *inst)
{
    return mir_instruction_source_payload(inst) != NULL;
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
llvm_mir_bind_base_local_scope(LLVMGenCtx *ctx,
                               const char *base_name,
                               LLVMValueRef alloca,
                               LLVMTypeRef type,
                               const char *type_name)
{
    char *owned_base;
    LLVMClassTypeEntry *class_entry;

    if (ctx == NULL || base_name == NULL || alloca == NULL || type == NULL)
        return;

    owned_base = pgy_arena_strdup(&ctx->persistent, base_name);
    if (owned_base == NULL) {
        llvm_set_mir_topology_invalid(ctx,
            "LLVM MIR block emission out of memory binding local scope");
        return;
    }
    llvm_scope_declare(ctx, owned_base, alloca, type);
    if (type_name != NULL && type_name[0] != '\0'
        && llvm_lookup_class(ctx, type_name) != NULL) {
        llvm_register_var_class(ctx, owned_base, type_name);
    } else {
        class_entry = llvm_lookup_class_by_struct_type(ctx, type);
        if (class_entry != NULL && class_entry->class_name != NULL)
            llvm_register_var_class(ctx, owned_base, class_entry->class_name);
    }
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
    if (entry == NULL)
        return;
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

static bool
llvm_mir_copy_source_def_to_versioned_local(const MIRInstruction *inst,
                                            const MIRBasicBlock *mir_block,
                                            LLVMGenCtx *ctx,
                                            LLVMMirVar *vars,
                                            size_t var_count)
{
    char base_name[128];
    LLVMVarEntry source;
    bool has_source;
    LLVMMirVar *target;
    LLVMValueRef loaded;
    ASTNode *source_payload;
    ASTNode *type_ann;
    LLVMValueRef active_alloca;
    const char *source_future_inner;
    const char *source_channel_inner;
    bool source_future_is_remote = false;

    if (inst == NULL || ctx == NULL || inst->result_name == NULL)
        return true;
    if (!llvm_mir_base_name_from_versioned(inst->result_name, base_name,
            sizeof(base_name))) {
        return true;
    }
    if (llvm_mir_def_is_resource_view_alias(inst))
        return llvm_mir_bind_resource_view_def_alias(inst, mir_block, ctx, vars,
            var_count);
    has_source = llvm_scope_lookup_snapshot(ctx, base_name, &source);
    target = llvm_mir_get_var_entry(vars, var_count, inst->result_name);
    if (target == NULL || target->alloca == NULL)
        return true;
    if (llvm_mir_copy_host_field_to_versioned_local(ctx, base_name,
            target)) {
        llvm_mir_bind_base_local_scope(ctx, base_name, target->alloca,
            target->type, inst->arg1);
        return !ctx->has_error;
    }
    if (!has_source) {
        return !ctx->has_error;
    }
    if (source.alloca == NULL) {
        if (llvm_lookup_projection_borrow(ctx, base_name) != NULL)
            return true;
        llvm_set_mir_inventory_missing(ctx,
            "LLVM MIR source local '%s' has no backing storage",
            base_name);
        return false;
    }
    source_future_inner = llvm_lookup_future_inner(ctx, base_name);
    if (source_future_inner != NULL)
        source_future_is_remote = llvm_lookup_future_is_remote(ctx, base_name);
    source_channel_inner = llvm_lookup_channel_inner(ctx, base_name);
    active_alloca = target->alloca;
    if (source_channel_inner != NULL) {
        active_alloca = source.alloca;
    } else if (source.alloca != target->alloca
        && source.type != NULL
        && target->type != NULL
        && source.type == target->type) {
        loaded = LLVMBuildLoad2(ctx->builder, source.type, source.alloca,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, loaded, target->alloca);
    } else if (source.alloca != target->alloca
               && source.type != NULL
               && target->type != NULL
               && source.type != target->type) {
        active_alloca = source.alloca;
    }
    llvm_mir_bind_base_local_scope(ctx, base_name, active_alloca,
        active_alloca == source.alloca ? source.type : target->type,
        inst->arg1);
    source_payload = mir_instruction_source_payload(inst);
    type_ann = source_payload != NULL && source_payload->type == AST_LET_DECL
        ? ast_let_type(source_payload) : NULL;
    if (type_ann != NULL)
        llvm_register_typed_var_binding(ctx, base_name, active_alloca,
            type_ann);
    else if (inst->type_layout != NULL
             && inst->type_layout->abi_type_name != NULL) {
        llvm_register_typed_var_abi_binding(ctx, base_name, active_alloca,
            inst->type_layout->abi_type_name);
    } else if (source_future_inner != NULL
               && source_future_inner[0] != '\0') {
        llvm_register_future_var_binding(ctx, base_name, active_alloca,
            source_future_inner, source_future_is_remote);
    } else if (source_channel_inner != NULL
               && source_channel_inner[0] != '\0') {
        llvm_register_channel_var_binding(ctx, base_name, active_alloca,
            source_channel_inner);
    }
    return !ctx->has_error;
}

bool
llvm_mir_copy_host_field_to_versioned_local(LLVMGenCtx *ctx,
                                            const char *field_name,
                                            LLVMMirVar *target)
{
    const char *host_name;
    LLVMClassTypeEntry *cls;
    LLVMValueRef base_ptr;
    LLVMValueRef gep;
    LLVMValueRef loaded;
    LLVMTypeRef field_type;
    int field_idx;

    if (ctx == NULL || field_name == NULL || target == NULL
        || target->alloca == NULL || target->type == NULL) {
        return false;
    }

    host_name = llvm_current_host_class_name(ctx);
    if (host_name == NULL)
        return false;
    cls = llvm_lookup_class(ctx, host_name);
    field_idx = cls != NULL ? llvm_class_field_index(cls, field_name) : -1;
    if (field_idx < 0)
        return false;
    field_type = llvm_class_field_type_at_index(cls, field_idx);
    if (field_type == NULL)
        return false;
    base_ptr = llvm_current_self_base_ptr(ctx, cls);
    if (base_ptr == NULL)
        return false;

    gep = LLVMBuildStructGEP2(ctx->builder, cls->struct_type, base_ptr,
        (unsigned)field_idx, llvm_tmp_name(ctx));
    loaded = LLVMBuildLoad2(ctx->builder, field_type, gep, llvm_tmp_name(ctx));
    if (LLVMTypeOf(loaded) != target->type) {
        if ((target->type == ctx->type_i32 || target->type == ctx->type_i64)
            && (LLVMTypeOf(loaded) == ctx->type_i32
                || LLVMTypeOf(loaded) == ctx->type_i64)) {
            loaded = LLVMGetIntTypeWidth(target->type)
                    > LLVMGetIntTypeWidth(LLVMTypeOf(loaded))
                ? LLVMBuildSExt(ctx->builder, loaded, target->type,
                    llvm_tmp_name(ctx))
                : LLVMBuildTrunc(ctx->builder, loaded, target->type,
                    llvm_tmp_name(ctx));
        } else {
            return false;
        }
    }
    LLVMBuildStore(ctx->builder, loaded, target->alloca);
    return true;
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
        if (llvm_debug_detail_enabled()) {
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
                    if (llvm_debug_detail_enabled())
                        fprintf(stderr, llvm_mir_def_uses_source_local_decl_emit(inst)
                            ? "[llvm inst] emit_source_local_decl\n"
                            : "[llvm inst] emit_source_statement\n");
                    llvm_emit_statement(source_payload, ctx);
                    if (!llvm_mir_copy_source_def_to_versioned_local(
                            inst, mir_block, ctx, vars, var_count)) {
                        return;
                    }
                } else {
                    LLVMValueRef alloca = llvm_mir_get_var(vars, var_count, inst->result_name);
                    if (llvm_debug_detail_enabled())
                        fprintf(stderr, "[llvm inst] emit_expression_store\n");
                    if (inst->expr0 != NULL
                        && !llvm_stmt_require_non_void_value(ctx, inst->expr0,
                            "LLVM MIR DEF cannot consume a Void expression value")) {
                        return;
                    }
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
            if (mir_instruction_has_required_branch_condition_fact(inst)
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
                        source_payload, routine, mir_block->succ_true, ctx);
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
                if (function_ret_type == ctx->type_void) {
                    LLVMBuildRetVoid(ctx->builder);
                } else {
                    llvm_set_error_at_with_hints(ctx,
                        mir_instruction_source_payload(inst),
                        PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                        PGY_FIX_ADD_RETURN_ON_ALL_PATHS,
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
            llvm_emit_statement(source_payload, ctx);
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
