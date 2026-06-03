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
llvm_mir_copy_host_field_to_versioned_local(LLVMGenCtx *ctx,
                                            const char *field_name,
                                            LLVMMirVar *target);

static bool
llvm_mir_copy_source_def_to_versioned_local(const MIRInstruction *inst,
                                            const MIRBasicBlock *mir_block,
                                            LLVMGenCtx *ctx,
                                            LLVMMirVar *vars,
                                            size_t var_count)
{
    char base_name[128];
    LLVMVarEntry *source;
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
    source = llvm_scope_lookup(ctx, base_name);
    target = llvm_mir_get_var_entry(vars, var_count, inst->result_name);
    if (target == NULL || target->alloca == NULL)
        return true;
    if (llvm_mir_copy_host_field_to_versioned_local(ctx, base_name,
            target)) {
        llvm_mir_bind_base_local_scope(ctx, base_name, target->alloca,
            target->type, inst->arg1);
        return !ctx->has_error;
    }
    if (source == NULL) {
        return !ctx->has_error;
    }
    if (source->alloca == NULL) {
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
        active_alloca = source->alloca;
    } else if (source->alloca != target->alloca
        && source->type != NULL
        && target->type != NULL
        && source->type == target->type) {
        loaded = LLVMBuildLoad2(ctx->builder, source->type, source->alloca,
            llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, loaded, target->alloca);
    } else if (source->alloca != target->alloca
               && source->type != NULL
               && target->type != NULL
               && source->type != target->type) {
        active_alloca = source->alloca;
    }
    llvm_mir_bind_base_local_scope(ctx, base_name, active_alloca,
        active_alloca == source->alloca ? source->type : target->type,
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

static bool
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
                               LLVMBasicBlockRef *llvm_blocks,
                               LLVMBasicBlockRef *llvm_block_heads,
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
    if (!llvm_mir_remap_active_match_bindings(routine, mir_block, func_decl,
                                              ctx))
        return;
    if (!llvm_mir_emit_match_case_body_binding(routine, mir_block, func_decl,
                                               ctx))
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
            llvm_mir_emit_borrow_view_alias(inst, ctx);
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
                    cond = llvm_mir_emit_match_case_condition(func_decl,
                                                              inst,
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
                    LLVMBasicBlockRef true_bb = llvm_block_heads[mir_block->succ_true];
                    LLVMBasicBlockRef false_bb = llvm_block_heads[mir_block->succ_false];
                    if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                        return;
                    /*
                     * Condition emission may have split this MIR block across
                     * multiple LLVM basic blocks (e.g. Coalesce lowering
                     * creates extra blocks). The actual predecessor of
                     * succ_true/succ_false is the current insert block, not
                     * the entry block recorded at llvm_blocks[mir_block->id].
                     * Update the entry so the subsequent PHI lowering wires
                     * incoming entries to the real predecessor.
                     *
                     * Note: if this MIR block has its own PHI to lower, the
                     * PHI lowering will now target the tail block. This is a
                     * trade-off: PHI insertion at the original entry would
                     * leave succ predecessors pointing to a stale block. The
                     * proper fix is a separate llvm_block_tails[] array
                     * threaded through PHI lowering; deferred until the
                     * block-split lowering footprint grows.
                     */
                    llvm_blocks[mir_block->id] =
                        LLVMGetInsertBlock(ctx->builder);
                    LLVMBuildCondBr(ctx->builder, cond, true_bb, false_bb);
                    emitted_terminator = true;
                }
            } else if (mir_block->has_succ_true) {
                if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                    return;
                llvm_blocks[mir_block->id] =
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
                LLVMValueRef val;
                if (ctx->current_func_decl != NULL
                    && ctx->current_func_decl->type == AST_FUNC_DECL
                    && ast_func_return_type(ctx->current_func_decl) != NULL) {
                    ctx->expected_type_name =
                        llvm_stmt_render_type_annotation_copy(ctx,
                            ast_func_return_type(ctx->current_func_decl));
                    ctx->expected_callable_type =
                        llvm_stmt_current_return_callable_type(ctx);
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
            llvm_blocks[mir_block->id] =
                LLVMGetInsertBlock(ctx->builder);
            LLVMBuildCondBr(ctx->builder,
                            LLVMConstInt(LLVMInt1TypeInContext(
                                LLVMGetModuleContext(ctx->module)), 1, false),
                            llvm_block_heads[mir_block->succ_true],
                            llvm_block_heads[mir_block->succ_false]);
        } else if (mir_block->has_succ_true) {
            if (!llvm_mir_emit_loop_backedge_increment(routine, mir_block, ctx))
                return;
            if (!llvm_mir_emit_pin_exit(mir_block, ctx))
                return;
            llvm_blocks[mir_block->id] =
                LLVMGetInsertBlock(ctx->builder);
            LLVMBuildBr(ctx->builder, llvm_block_heads[mir_block->succ_true]);
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
