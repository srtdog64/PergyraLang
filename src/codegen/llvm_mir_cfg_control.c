/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR CFG control helpers for loop/select/match lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "../parser/ast_api.h"

static ASTNode *
llvm_mir_recv_expr_channel(ASTNode *node)
{
    if (node == NULL || node->type != AST_CHANNEL_RECV)
        return NULL;
    return ast_channel_recv_channel(node);
}

static ASTNode *
llvm_mir_assignment_recv_channel(ASTNode *node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    if (ast_assignment_value(node) == NULL
        || ast_assignment_value(node)->type != AST_CHANNEL_RECV) {
        return NULL;
    }
    return llvm_mir_recv_expr_channel(ast_assignment_value(node));
}

static ASTNode *
llvm_mir_select_case_channel(ASTNode *node)
{
    ASTNode *first;

    if (node == NULL || node->type != AST_BLOCK
        || ast_block_statement_count(node) == 0)
        return NULL;

    first = ast_block_statement(node, 0);
    if (first == NULL)
        return NULL;
    if (first->type == AST_CHANNEL_RECV)
        return ast_channel_recv_channel(first);
    return llvm_mir_assignment_recv_channel(first);
}

static LLVMFuncEntry *
llvm_mir_required_channel_ready_function(ASTNode *channel, LLVMGenCtx *ctx,
                                         const char *function_name)
{
    LLVMFuncEntry *ready_fn = function_name != NULL
        ? llvm_lookup_function(ctx, function_name)
        : NULL;
    if (ready_fn == NULL || ready_fn->fn == NULL) {
        llvm_set_error_at_with_hints(ctx, channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM MIR select readiness requires registered runtime function '%s'",
            function_name != NULL ? function_name : "<missing>");
        return NULL;
    }
    return ready_fn;
}

static LLVMValueRef
llvm_mir_emit_channel_ready_condition(ASTNode *channel, LLVMGenCtx *ctx)
{
    LLVMChannelTarget target;
    char fn_name[128];
    LLVMFuncEntry *ready_fn;
    LLVMValueRef args[1];

    if (channel == NULL || ctx == NULL || channel->type != AST_IDENTIFIER)
        return NULL;

    if (!llvm_resolve_channel_target(ctx, channel, channel,
            "select readiness", &target))
        return NULL;

    if (target.inner == NULL || target.inner[0] == '\0'
        || target.ptr == NULL)
        return NULL;

    snprintf(fn_name, sizeof(fn_name), "pgy_channel_ready_%s", target.inner);
    ready_fn = llvm_mir_required_channel_ready_function(channel, ctx, fn_name);
    if (ready_fn == NULL)
        return NULL;

    args[0] = target.ptr;
    return LLVMBuildCall2(ctx->builder, ready_fn->fn_type, ready_fn->fn,
                          args, 1, llvm_tmp_name(ctx));
}

static LLVMValueRef
llvm_mir_emit_select_case_condition(const MIRRoutine *routine,
                                    size_t target_block,
                                    LLVMGenCtx *ctx)
{
    const MIRBasicBlock *target;

    if (routine == NULL || ctx == NULL || target_block >= routine->block_count)
        return NULL;

    target = &routine->blocks[target_block];
    for (size_t i = 0; i < target->instruction_count; i++) {
        const MIRInstruction *inst = &target->instructions[i];
        ASTNode *channel;
        if (inst->kind != MIR_INST_DEF)
            continue;
        channel = llvm_mir_recv_expr_channel(inst->expr0);
        if (channel != NULL)
            return llvm_mir_emit_channel_ready_condition(channel, ctx);
    }
    return NULL;
}

LLVMValueRef
llvm_mir_emit_select_dispatch_condition(ASTNode *case_node,
                                        const MIRRoutine *routine,
                                        size_t target_block,
                                        LLVMGenCtx *ctx)
{
    ASTNode *channel = llvm_mir_select_case_channel(case_node);
    if (channel != NULL)
        return llvm_mir_emit_channel_ready_condition(channel, ctx);
    return llvm_mir_emit_select_case_condition(routine, target_block, ctx);
}

bool
llvm_mir_declare_recv_target(const char *target_name,
                             ASTNode *recv_expr,
                             LLVMGenCtx *ctx)
{
    ASTNode *channel;
    LLVMChannelTarget target;
    LLVMTypeRef value_ty;
    LLVMValueRef alloca_val;

    if (ctx == NULL)
        return true;
    if (target_name == NULL || llvm_scope_contains(ctx, target_name))
        return true;

    channel = llvm_mir_recv_expr_channel(recv_expr);
    if (channel == NULL || channel->type != AST_IDENTIFIER)
        return true;

    if (!llvm_resolve_channel_target(ctx, recv_expr, channel,
            "channel receive target", &target)) {
        return false;
    }

    value_ty = pergyra_type_to_llvm(ctx, target.inner);
    if (ctx->has_error || value_ty == NULL)
        return false;
    alloca_val = llvm_create_entry_alloca(ctx, value_ty, target_name);
    llvm_scope_declare(ctx, pergyra_strdup(target_name), alloca_val, value_ty);
    return true;
}

bool
llvm_mir_emit_channel_receive_def(const MIRInstruction *inst,
                                  LLVMGenCtx *ctx,
                                  LLVMValueRef mir_alloca)
{
    ASTNode *channel;
    LLVMChannelTarget channel_target;
    LLVMVarEntry target_var;
    LLVMFuncEntry *recv_fn;
    LLVMValueRef args[1];
    LLVMValueRef value;
    char fn_name[128];

    if (inst == NULL || ctx == NULL)
        return false;
    if (inst->arg0 == NULL) {
        llvm_set_error_at_with_hints(ctx, inst->expr0,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM channel receive DEF requires a materialized target name");
        return false;
    }
    if (!llvm_mir_declare_recv_target(inst->arg0, inst->expr0, ctx))
        return false;

    channel = llvm_mir_recv_expr_channel(inst->expr0);
    if (channel == NULL || channel->type != AST_IDENTIFIER) {
        llvm_set_error_at_with_hints(ctx, inst->expr0,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM channel receive DEF requires an identifier channel");
        return false;
    }
    if (!llvm_resolve_channel_target(ctx, inst->expr0, channel,
            "channel receive DEF", &channel_target))
        return false;

    if (!llvm_scope_lookup_snapshot(ctx, inst->arg0, &target_var)
        || target_var.alloca == NULL) {
        llvm_set_error_at_with_hints(ctx, inst->expr0,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM channel receive DEF requires materialized target '%s'",
            inst->arg0);
        return false;
    }

    if (channel_target.inner == NULL || channel_target.inner[0] == '\0'
        || channel_target.ptr == NULL) {
        llvm_set_error_at_with_hints(ctx, channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM channel receive DEF '%s' requires concrete Channel<T> metadata",
            inst->arg0 != NULL ? inst->arg0 : "<target>");
        return false;
    }

    snprintf(fn_name, sizeof(fn_name), "pgy_channel_recv_val_%s",
             channel_target.inner);
    recv_fn = llvm_lookup_function(ctx, fn_name);
    if (recv_fn == NULL || recv_fn->fn == NULL) {
        llvm_set_error_at_with_hints(ctx, channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
            PGY_FIX_INSPECT_MIR_INVENTORY,
            "LLVM channel receive DEF requires registered runtime function '%s'",
            fn_name);
        return false;
    }

    args[0] = channel_target.ptr;
    value = LLVMBuildCall2(ctx->builder, recv_fn->fn_type, recv_fn->fn,
                           args, 1, llvm_tmp_name(ctx));
    LLVMBuildStore(ctx->builder, value, target_var.alloca);
    if (mir_alloca != NULL && mir_alloca != target_var.alloca)
        LLVMBuildStore(ctx->builder, value, mir_alloca);
    return true;
}

#endif /* PGY_LLVM_ENABLED */
