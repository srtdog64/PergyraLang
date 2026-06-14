/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend MIR-backed intent flow and signature helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

static bool
llvm_intent_flow_reason_name(LLVMGenCtx *ctx, char *out, size_t out_size,
                             const char *step_name)
{
    int written;

    if (out == NULL || out_size == 0)
        return false;
    written = snprintf(out, out_size, "authority:%s",
        step_name != NULL ? step_name : "<step>");
    if (written >= 0 && (size_t)written < out_size)
        return true;
    llvm_set_error_with_hints(ctx,
        PGY_CODE_LLVM_SPEC_LIMIT,
        PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
        PGY_FIX_REFACTOR_OR_RAISE_LIMIT,
        "intent authority failure reason is too long for step '%s'",
        step_name != NULL ? step_name : "<step>");
    return false;
}

const MIRRoutine *
llvm_find_mir_intent_routine(const LLVMGenCtx *ctx, ASTNode *intent_decl)
{
    LLVMMIRRoutineInventory routine_inventory;
    const char *intent_name;

    if (ctx == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL) {
        return NULL;
    }

    intent_name = ast_intent_decl_name(intent_decl);
    if (intent_name == NULL)
        return NULL;

    llvm_active_routine_inventory(ctx, &routine_inventory);
    for (size_t i = 0; i < routine_inventory.count; i++) {
        const MIRRoutine *routine =
            llvm_routine_inventory_get(&routine_inventory, i);
        const char *routine_name = llvm_mir_routine_name(routine);
        if (routine == NULL)
            continue;
        if (llvm_mir_routine_kind(routine) != MIR_SCOPE_INTENT
            || routine_name == NULL)
            continue;
        if (strcmp(routine_name, intent_name) == 0)
            return routine;
    }

    return NULL;
}

static bool
llvm_mir_intent_routine_has_instructions(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (routine->blocks[i].instruction_count > 0)
            return true;
    }
    return false;
}

bool
llvm_require_mir_intent_source_decl(LLVMGenCtx *ctx,
                                    const MIRRoutine *routine,
                                    ASTNode **intent_decl_out)
{
    ASTNode *intent_decl;

    intent_decl = mir_routine_source_decl_of_type(
        routine, MIR_SCOPE_INTENT, AST_INTENT_DECL);
    if (intent_decl_out != NULL)
        *intent_decl_out = intent_decl;
    if (intent_decl != NULL)
        return true;
    if (routine != NULL
        && llvm_mir_routine_kind(routine) == MIR_SCOPE_INTENT
        && llvm_mir_intent_routine_has_instructions(routine)) {
        const char *routine_name = llvm_mir_routine_name(routine);
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing intent source declaration for routine '%s'",
            routine_name != NULL ? routine_name : "(anonymous)");
        return false;
    }
    return true;
}

void
llvm_emit_mir_resource_hook(LLVMGenCtx *ctx,
                            const MIRInstruction *inst,
                            LLVMValueRef handle,
                            bool cleanup_hook)
{
    const char *helper_name = cleanup_hook
        ? "pgy_mir_cleanup_op_export"
        : "pgy_mir_resource_op_export";
    const char *op_name = "unknown";
    const char *slot_anchor = "";
    const char *arg_name = "";
    LLVMFuncEntry *helper;

    if (ctx == NULL || inst == NULL)
        return;

    helper = llvm_lookup_function(ctx, helper_name);
    if (helper == NULL)
        return;

    if (inst->name != NULL)
        op_name = inst->name;
    if (inst->rir_op != NULL && inst->rir_op->slot_anchor != NULL)
        slot_anchor = inst->rir_op->slot_anchor;
    else if (inst->arg0 != NULL)
        slot_anchor = inst->arg0;
    if (inst->arg1 != NULL)
        arg_name = inst->arg1;
    else if (inst->rir_op != NULL && inst->rir_op->arg0 != NULL)
        arg_name = inst->rir_op->arg0;

    {
        LLVMValueRef args[] = {
            handle,
            LLVMBuildGlobalStringPtr(ctx->builder, op_name, llvm_tmp_name(ctx)),
            LLVMBuildGlobalStringPtr(ctx->builder, slot_anchor, llvm_tmp_name(ctx)),
            LLVMBuildGlobalStringPtr(ctx->builder, arg_name, llvm_tmp_name(ctx))
        };
        LLVMBuildCall2(ctx->builder, helper->fn_type, helper->fn, args, 4, "");
    }
}

size_t
llvm_collect_mir_intent_step_names(const MIRRoutine *routine,
                                   LLVMGenCtx *ctx,
                                   const char ***names_out)
{
    const char **names = NULL;
    size_t count = 0;

    if (names_out != NULL)
        *names_out = NULL;
    if (routine == NULL || names_out == NULL || ctx == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];

            if (mir_instruction_intent_step_name(inst) == NULL)
                continue;
            count++;
        }
    }

    if (count == 0)
        return 0;
    names = pgy_arena_calloc(&ctx->scratch, count * sizeof(const char *));
    if (names == NULL)
        return 0;

    count = 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (mir_instruction_intent_step_name(inst) == NULL)
                continue;
            names[count++] = mir_instruction_intent_step_name(inst);
        }
    }

    *names_out = names;
    return count;
}

ASTNode *
llvm_find_mir_intent_check_expr(const MIRRoutine *routine,
                                const char *step_name,
                                const char *phase_name)
{
    if (routine == NULL || phase_name == NULL)
        return NULL;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->expr0 == NULL)
                continue;
            if (!mir_instruction_is_intent_stmt(inst, "IntentCheck"))
                continue;
            if (!mir_instruction_intent_phase_matches(inst, phase_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            return inst->expr0;
        }
    }
    return NULL;
}

size_t
llvm_collect_mir_intent_eval_exprs(const MIRRoutine *routine,
                                   LLVMGenCtx *ctx,
                                   const char *step_name,
                                   const char *phase_name,
                                   ASTNode ***exprs_out)
{
    ASTNode **exprs = NULL;
    size_t count = 0;

    if (exprs_out != NULL)
        *exprs_out = NULL;
    if (routine == NULL || phase_name == NULL || exprs_out == NULL || ctx == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];

            if (inst->expr0 == NULL)
                continue;
            if (!mir_instruction_is_intent_stmt(inst, "IntentEval"))
                continue;
            if (!mir_instruction_intent_phase_matches(inst, phase_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            count++;
        }
    }

    if (count == 0)
        return 0;
    exprs = pgy_arena_calloc(&ctx->scratch, count * sizeof(ASTNode *));
    if (exprs == NULL)
        return 0;

    count = 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->expr0 == NULL)
                continue;
            if (!mir_instruction_is_intent_stmt(inst, "IntentEval"))
                continue;
            if (!mir_instruction_intent_phase_matches(inst, phase_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            exprs[count++] = inst->expr0;
        }
    }

    *exprs_out = exprs;
    return count;
}

ASTNode *
llvm_find_mir_intent_eval_expr(const MIRRoutine *routine,
                               LLVMGenCtx *ctx,
                               const char *step_name,
                               const char *phase_name)
{
    ASTNode **exprs = NULL;
    ASTNode *result = NULL;
    size_t count = llvm_collect_mir_intent_eval_exprs(
        routine, ctx, step_name, phase_name, &exprs);
    if (count > 0)
        result = exprs[0];
    return result;
}

size_t
llvm_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                         LLVMGenCtx *ctx,
                                         const char *step_name,
                                         const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL || ctx == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, "IntentDispatch"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            count++;
        }
    }

    if (count == 0)
        return 0;
    aliases = pgy_arena_calloc(&ctx->scratch, count * sizeof(const char *));
    if (aliases == NULL)
        return 0;

    count = 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);

            if (!mir_instruction_is_intent_stmt(inst, "IntentDispatch"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}

static LLVMValueRef
llvm_emit_intent_presence_flag(LLVMGenCtx *ctx, const char *alias)
{
    LLVMVarEntry var;
    LLVMValueRef value;

    if (ctx == NULL)
        return NULL;
    if (alias == NULL)
        return LLVMConstInt(ctx->type_i1, 0, 0);

    if (!llvm_scope_lookup_snapshot(ctx, alias, &var)
        || LLVMGetTypeKind(var.type) != LLVMPointerTypeKind)
        return LLVMConstInt(ctx->type_i1, 0, 0);

    value = LLVMBuildLoad2(ctx->builder, var.type, var.alloca, llvm_tmp_name(ctx));
    return LLVMBuildICmp(ctx->builder, LLVMIntNE, value,
        LLVMConstPointerNull(var.type), llvm_tmp_name(ctx));
}

void
llvm_emit_intent_step_validate_authority(LLVMGenCtx *ctx,
                                         LLVMValueRef fn,
                                         LLVMBasicBlockRef fail_bb,
                                         LLVMValueRef fail_reason_alloca,
                                         const char *step_name,
                                         const char *zone_type_name,
                                         const char *zone_alias,
                                         const char **authorized_aliases,
                                         size_t authorized_alias_count)
{
    LLVMFuncEntry *validate_fn;

    if (ctx == NULL || fn == NULL || fail_bb == NULL || fail_reason_alloca == NULL
        || zone_type_name == NULL || authorized_aliases == NULL
        || authorized_alias_count == 0) {
        return;
    }

    validate_fn = llvm_lookup_function(ctx, "pgy_zone_authority_validate_flags_export");
    if (validate_fn == NULL)
        return;

    for (size_t i = 0; i < authorized_alias_count; i++) {
        const char *alias = authorized_aliases[i];
        char reason[256];
        LLVMBasicBlockRef ok_bb;
        LLVMValueRef args[4];
        LLVMValueRef ok;

        if (alias == NULL)
            continue;

        ok_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.authority.ok");
        args[0] = llvm_emit_intent_presence_flag(ctx, zone_alias);
        args[1] = llvm_emit_intent_presence_flag(ctx, alias);
        args[2] = LLVMBuildGlobalStringPtr(ctx->builder, zone_type_name, llvm_tmp_name(ctx));
        args[3] = LLVMBuildGlobalStringPtr(ctx->builder, alias, llvm_tmp_name(ctx));
        ok = LLVMBuildCall2(ctx->builder, validate_fn->fn_type, validate_fn->fn,
            args, 4, llvm_tmp_name(ctx));
        if (!llvm_intent_flow_reason_name(ctx, reason, sizeof(reason), step_name))
            return;
        LLVMBuildStore(ctx->builder,
            LLVMBuildGlobalStringPtr(ctx->builder, reason, llvm_tmp_name(ctx)),
            fail_reason_alloca);
        LLVMBuildCondBr(ctx->builder, ok, ok_bb, fail_bb);
        LLVMPositionBuilderAtEnd(ctx->builder, ok_bb);
    }
}

#endif
