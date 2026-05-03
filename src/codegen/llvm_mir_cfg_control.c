/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM MIR CFG control helpers for loop/select/match lowering.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"

bool
llvm_mir_stmt_is_cfg_container(ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_WITH_STMT:
    case AST_UNSAFE_BLOCK:
    case AST_DEFER_STMT:
    case AST_IF_STMT:
    case AST_WHILE_LOOP:
    case AST_FOR_LOOP:
    case AST_SELECT_STMT:
    case AST_MATCH_STMT:
    case AST_BREAK:
    case AST_CONTINUE:
    case AST_RETURN:
        return true;
    default:
        return false;
    }
}

static ASTNode *
llvm_mir_find_match_subject_for_case(ASTNode *node, ASTNode *case_node)
{
    if (node == NULL || case_node == NULL)
        return NULL;

    if (node->type == AST_MATCH_STMT) {
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            if (node->data.match_stmt.cases[i] == case_node)
                return node->data.match_stmt.subject;
        }
        if (node->data.match_stmt.default_body != NULL) {
            ASTNode *found = llvm_mir_find_match_subject_for_case(
                node->data.match_stmt.default_body, case_node);
            if (found != NULL)
                return found;
        }
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            ASTNode *found = llvm_mir_find_match_subject_for_case(
                node->data.block.statements[i], case_node);
            if (found != NULL)
                return found;
        }
        break;
    case AST_IF_STMT: {
        ASTNode *found = llvm_mir_find_match_subject_for_case(
            node->data.if_stmt.then_branch, case_node);
        if (found != NULL)
            return found;
        return llvm_mir_find_match_subject_for_case(
            node->data.if_stmt.else_branch, case_node);
    }
    case AST_FOR_LOOP:
        return llvm_mir_find_match_subject_for_case(
            node->data.for_loop.body, case_node);
    case AST_WHILE_LOOP:
        return llvm_mir_find_match_subject_for_case(
            node->data.while_loop.body, case_node);
    case AST_WITH_STMT:
        return llvm_mir_find_match_subject_for_case(
            node->data.with_stmt.body, case_node);
    default:
        break;
    }
    return NULL;
}

static bool
llvm_mir_is_option_destructor(ASTNode *pat, const char **kind,
                              const char **binding)
{
    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            if (kind != NULL)
                *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        if (kind != NULL)
            *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        if (kind != NULL)
            *kind = "Some";
        if (binding != NULL
            && pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

static bool
llvm_mir_is_result_destructor(ASTNode *pat, const char **kind,
                              const char **binding)
{
    if (kind != NULL)
        *kind = NULL;
    if (binding != NULL)
        *binding = NULL;
    if (pat == NULL || pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if ((strcmp(name, "Ok") == 0 || strcmp(name, "Err") == 0)
        && pat->data.call.arg_count == 1) {
        if (kind != NULL)
            *kind = name;
        if (binding != NULL
            && pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

LLVMValueRef
llvm_mir_emit_match_case_condition(ASTNode *func_decl, ASTNode *case_node,
                                   LLVMGenCtx *ctx)
{
    ASTNode *subject_node;
    LLVMValueRef subject;
    LLVMValueRef cmp = NULL;

    if (func_decl == NULL || case_node == NULL || ctx == NULL
        || case_node->type != AST_MATCH_CASE) {
        return NULL;
    }

    subject_node = func_decl->type == AST_FUNC_DECL
        ? llvm_mir_find_match_subject_for_case(func_decl->data.func_decl.body, case_node)
        : NULL;
    if (subject_node == NULL)
        return NULL;

    subject = llvm_emit_expression(subject_node, ctx);
    if (subject == NULL)
        return NULL;

    if (case_node->data.match_case.patterns != NULL
        && case_node->data.match_case.pattern_count > 1) {
        for (size_t i = 0; i < case_node->data.match_case.pattern_count; i++) {
            LLVMValueRef pattern = llvm_emit_expression(
                case_node->data.match_case.patterns[i], ctx);
            LLVMValueRef one_cmp;
            if (pattern == NULL)
                continue;
            one_cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, subject, pattern,
                                    llvm_tmp_name(ctx));
            cmp = cmp == NULL ? one_cmp
                              : LLVMBuildOr(ctx->builder, cmp, one_cmp,
                                            llvm_tmp_name(ctx));
        }
        return cmp;
    }

    if (case_node->data.match_case.pattern == NULL)
        return NULL;

    {
        const char *option_kind = NULL;
        const char *result_kind = NULL;
        const char *binding = NULL;
        if (llvm_mir_is_option_destructor(case_node->data.match_case.pattern,
                                          &option_kind, &binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            if (binding != NULL) {
                LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                    subject, 1, llvm_tmp_name(ctx));
                LLVMTypeRef payload_ty = LLVMTypeOf(payload);
                LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx,
                    payload_ty, binding);
                LLVMBuildStore(ctx->builder, payload, payload_alloca);
                llvm_scope_declare(ctx, pergyra_strdup(binding),
                    payload_alloca, payload_ty);
            }
            return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    strcmp(option_kind, "Some") == 0 ? 0 : 1, 0),
                llvm_tmp_name(ctx));
        }
        if (llvm_mir_is_result_destructor(case_node->data.match_case.pattern,
                                          &result_kind, &binding)) {
            LLVMValueRef tag = LLVMBuildExtractValue(ctx->builder, subject, 0,
                llvm_tmp_name(ctx));
            if (binding != NULL) {
                unsigned payload_index =
                    (strcmp(result_kind, "Err") == 0) ? 2 : 1;
                LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                    subject, payload_index, llvm_tmp_name(ctx));
                LLVMTypeRef payload_ty = LLVMTypeOf(payload);
                LLVMValueRef payload_alloca = llvm_create_entry_alloca(ctx,
                    payload_ty, binding);
                LLVMBuildStore(ctx->builder, payload, payload_alloca);
                llvm_scope_declare(ctx, pergyra_strdup(binding),
                    payload_alloca, payload_ty);
            }
            return LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    strcmp(result_kind, "Ok") == 0 ? 0 : 1, 0),
                llvm_tmp_name(ctx));
        }
        LLVMValueRef pattern = llvm_emit_expression(case_node->data.match_case.pattern, ctx);
        if (pattern == NULL)
            return NULL;
        return LLVMBuildICmp(ctx->builder, LLVMIntEQ, subject, pattern,
                             llvm_tmp_name(ctx));
    }
}

static ASTNode *
llvm_mir_assignment_recv_channel(ASTNode *node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    if (node->data.assignment.value == NULL
        || node->data.assignment.value->type != AST_CHANNEL_RECV) {
        return NULL;
    }
    return node->data.assignment.value->data.channel_recv.channel;
}

static ASTNode *
llvm_mir_select_case_channel(ASTNode *node)
{
    ASTNode *first;

    if (node == NULL || node->type != AST_BLOCK || node->data.block.count == 0)
        return NULL;

    first = node->data.block.statements[0];
    if (first == NULL)
        return NULL;
    if (first->type == AST_CHANNEL_RECV)
        return first->data.channel_recv.channel;
    return llvm_mir_assignment_recv_channel(first);
}

static LLVMValueRef
llvm_mir_emit_channel_ready_condition(ASTNode *channel, LLVMGenCtx *ctx)
{
    const char *channel_name;
    const char *inner;
    LLVMVarEntry *ch_var;
    char fn_name[128];
    LLVMFuncEntry *ready_fn;
    LLVMValueRef args[1];

    if (channel == NULL || ctx == NULL || channel->type != AST_IDENTIFIER)
        return NULL;

    channel_name = channel->data.identifier.name;
    if (channel_name == NULL)
        return NULL;

    ch_var = llvm_scope_lookup(ctx, channel_name);
    if (ch_var == NULL || ch_var->alloca == NULL)
        return NULL;

    inner = llvm_lookup_channel_inner(ctx, channel_name);
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM select channel '%s' requires concrete Channel<T> metadata",
            channel_name);
        return NULL;
    }

    snprintf(fn_name, sizeof(fn_name), "pgy_channel_ready_%s", inner);
    ready_fn = llvm_lookup_function(ctx, fn_name);
    if (ready_fn == NULL || ready_fn->fn == NULL)
        return NULL;

    args[0] = ch_var->alloca;
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
        if (inst->kind != MIR_INST_DEF || inst->ast == NULL)
            continue;
        channel = llvm_mir_assignment_recv_channel(inst->ast);
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
llvm_mir_declare_assignment_recv_target(ASTNode *node, LLVMGenCtx *ctx)
{
    ASTNode *channel;
    const char *target_name;
    const char *channel_name;
    const char *inner;
    LLVMTypeRef value_ty;
    LLVMValueRef alloca_val;

    if (node == NULL || ctx == NULL || node->type != AST_ASSIGNMENT)
        return true;
    if (node->data.assignment.target == NULL
        || node->data.assignment.target->type != AST_IDENTIFIER) {
        return true;
    }
    target_name = node->data.assignment.target->data.identifier.name;
    if (target_name == NULL || llvm_scope_lookup(ctx, target_name) != NULL)
        return true;

    channel = llvm_mir_assignment_recv_channel(node);
    if (channel == NULL || channel->type != AST_IDENTIFIER)
        return true;

    channel_name = channel->data.identifier.name;
    if (channel_name == NULL)
        return true;

    inner = llvm_lookup_channel_inner(ctx, channel_name);
    if (inner == NULL || inner[0] == '\0') {
        llvm_set_error_at_with_hints(ctx, channel,
            PGY_CODE_LLVM_TYPE_UNSUPPORTED,
            PGY_CAUSE_LLVM_SLOT_INNER_TYPE_MISSING,
            PGY_FIX_ANNOTATE_CONCRETE_TYPE,
            "LLVM channel receive target '%s' requires concrete Channel<T> metadata",
            target_name);
        return false;
    }

    value_ty = pergyra_type_to_llvm(ctx, inner);
    alloca_val = llvm_create_entry_alloca(ctx, value_ty, target_name);
    llvm_scope_declare(ctx, pergyra_strdup(target_name), alloca_val, value_ty);
    return true;
}

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
