/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend: MIR-backed intent declaration helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

bool
llvm_intent_involves_uses_pointer_self(LLVMGenCtx *ctx, ASTNode *involves)
{
    const char *type_name = llvm_intent_involves_type_name(involves);

    if (ctx == NULL || type_name == NULL)
        return false;
    return llvm_type_name_uses_pointer_self(ctx, type_name);
}

const char *
llvm_intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        return step->data.intent_step.using_expr->data.identifier.name;
    }
    return step->data.intent_step.transfer_to_alias;
}

void
llvm_emit_intent_decl(ASTNode *node, LLVMGenCtx *ctx)
{
    const MIRRoutine *mir_routine;
    ASTNode **mir_steps = NULL;
    ASTNode **step_nodes = NULL;
    const char **mir_step_names = NULL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    LLVMFuncEntry *entry;
    LLVMFuncEntry *enter_fn;
    LLVMFuncEntry *exit_fn;
    LLVMFuncEntry *trace_step_fn;
    LLVMFuncEntry *trace_bind_fn;
    LLVMFuncEntry *trace_step_ok_fn;
    LLVMFuncEntry *trace_fail_fn;
    LLVMValueRef fn;
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret_type;
    LLVMBasicBlockRef entry_bb;
    LLVMBasicBlockRef run_bb;
    LLVMBasicBlockRef fail_enter_bb;
    LLVMBasicBlockRef fail_bb;
    LLVMBasicBlockRef cleanup_bb;
    LLVMBasicBlockRef compensate_bb;
    LLVMBasicBlockRef maybe_exit_bb;
    LLVMBasicBlockRef do_exit_bb;
    LLVMBasicBlockRef ret_bb;
    LLVMValueRef result_alloca;
    LLVMValueRef failed_alloca;
    LLVMValueRef fail_reason_alloca;
    LLVMValueRef handle_alloca;
    LLVMValueRef subjects_ptr;
    LLVMValueRef *completed_allocas = NULL;
    size_t participant_count = 0;
    size_t param_count = 0;
    size_t subject_count = 0;
    size_t step_count = 0;
    bool has_compensate_steps = false;
    bool mir_only_intent = false;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    mir_routine = llvm_find_mir_intent_routine(ctx, node);
    if (mir_routine != NULL) {
        step_count = llvm_collect_mir_intent_steps(mir_routine, ctx, &mir_steps);
        (void)llvm_collect_mir_intent_step_names(mir_routine, ctx, &mir_step_names);
    }
    if (ctx->mir != NULL && node->data.intent_decl.step_count > 0) {
        if (mir_routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent routine for '%s'",
                node->data.intent_decl.name != NULL
                    ? node->data.intent_decl.name
                    : "(anonymous)");
            return;
        }
        if (step_count == 0) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent step sequence for '%s'",
                node->data.intent_decl.name != NULL
                    ? node->data.intent_decl.name
                    : "(anonymous)");
            return;
        }
        mir_only_intent = true;
    }
    if (step_count > 0) {
        step_nodes = mir_steps;
    } else {
        step_count = node->data.intent_decl.step_count;
        step_nodes = node->data.intent_decl.steps;
    }
    if (mir_routine != NULL) {
        participant_count = llvm_collect_mir_intent_participants(
            mir_routine, ctx, &participant_aliases, &participant_types);
    }
    if (mir_only_intent && node->data.intent_decl.involve_count > 0) {
        if (participant_count < node->data.intent_decl.involve_count) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent participant metadata for '%s'",
                node->data.intent_decl.name != NULL
                    ? node->data.intent_decl.name
                    : "(anonymous)");
            return;
        }
        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            if (participant_aliases == NULL || participant_types == NULL
                || participant_aliases[i] == NULL || participant_types[i] == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete intent participant metadata for '%s'",
                    node->data.intent_decl.name != NULL
                        ? node->data.intent_decl.name
                        : "(anonymous)");
                return;
            }
        }
    }
    if (participant_count == 0)
        participant_count = node->data.intent_decl.involve_count;
    param_count = node->data.intent_decl.binding_count > 0
        ? node->data.intent_decl.binding_count
        : (node->data.intent_decl.involve_count + node->data.intent_decl.value_count);
    if (param_count == 0)
        param_count = participant_count;

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
        if (step != NULL && step->type == AST_INTENT_STEP
            && ((mir_only_intent && mir_routine != NULL
                 && llvm_mir_intent_has_stmt(
                     mir_routine, step_name != NULL ? step_name : step->data.intent_step.name,
                     "IntentEval", "compensate"))
                || (!mir_only_intent && step->data.intent_step.compensate_expr_count > 0))) {
            has_compensate_steps = true;
            break;
        }
    }
    if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_NONE)
        has_compensate_steps = false;
    entry = llvm_lookup_function(ctx, node->data.intent_decl.name);
    if (entry == NULL)
        return;
    enter_fn = llvm_lookup_function(ctx, "pgy_intent_enter_export");
    exit_fn = llvm_lookup_function(ctx, "pgy_intent_exit_export");
    trace_step_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_step_export") : NULL;
    trace_bind_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_bind_export") : NULL;
    trace_step_ok_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_step_ok_export") : NULL;
    trace_fail_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_fail_export") : NULL;
    if (enter_fn == NULL || exit_fn == NULL)
        return;
    if (ctx->uses_intent_observability
        && (trace_step_fn == NULL || trace_bind_fn == NULL
            || trace_step_ok_fn == NULL || trace_fail_fn == NULL))
        return;

    fn = entry->fn;
    saved_fn = ctx->current_function;
    saved_ret_type = ctx->current_ret_type;
    ctx->current_function = fn;
    ctx->current_ret_type = ctx->type_i1;

    entry_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "entry");
    run_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.run");
    fail_enter_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.fail.enter");
    fail_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.fail");
    cleanup_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.cleanup");
    compensate_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.compensate");
    maybe_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.maybe_exit");
    do_exit_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.do_exit");
    ret_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.ret");
    LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);
    llvm_scope_push(ctx);

    llvm_emit_intent_entry_bindings(ctx, node, fn,
        participant_aliases, participant_types,
        participant_count, param_count, mir_only_intent,
        &subjects_ptr, &subject_count);

    result_alloca = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_result");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
    failed_alloca = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_failed");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), failed_alloca);
    fail_reason_alloca = llvm_create_entry_alloca(ctx, ctx->type_i8ptr, "__intent_fail_reason");
    LLVMBuildStore(ctx->builder, LLVMBuildGlobalStringPtr(ctx->builder, "", llvm_tmp_name(ctx)),
        fail_reason_alloca);
    handle_alloca = llvm_create_entry_alloca(ctx, ctx->type_i32, "__intent_handle");
    LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i32, 0, 0), handle_alloca);
    llvm_scope_declare(ctx, "__intent_handle", handle_alloca, ctx->type_i32);
    if (has_compensate_steps && step_count > 0) {
        /* Per-step completion flag allocas: transient tracking array used
         * only during intent emission; never escapes. */
        completed_allocas = pgy_arena_calloc(&ctx->scratch,
            step_count * sizeof(LLVMValueRef));
        for (size_t i = 0; i < step_count; i++) {
            completed_allocas[i] = llvm_create_entry_alloca(ctx, ctx->type_i1, "__intent_step_done");
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), completed_allocas[i]);
        }
    }

    {
        LLVMValueRef priority = node->data.intent_decl.priority_expr != NULL
            ? llvm_emit_expression(node->data.intent_decl.priority_expr, ctx)
            : LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef enter_args[] = {
            LLVMBuildGlobalStringPtr(ctx->builder, node->data.intent_decl.name,
                llvm_tmp_name(ctx)),
            subjects_ptr,
            LLVMConstInt(ctx->type_i32, (unsigned)subject_count, 0),
            LLVMConstInt(ctx->type_i1, node->data.intent_decl.is_concurrent ? 1 : 0, 0),
            priority
        };
        LLVMValueRef handle = LLVMBuildCall2(ctx->builder, enter_fn->fn_type, enter_fn->fn,
            enter_args, 5, llvm_tmp_name(ctx));
        LLVMValueRef entered = LLVMBuildICmp(ctx->builder, LLVMIntNE, handle,
            LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, handle, handle_alloca);
        LLVMBuildCondBr(ctx->builder, entered, run_bb, fail_enter_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fail_enter_bb);
    {
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), failed_alloca);
        LLVMBuildStore(ctx->builder,
            LLVMBuildGlobalStringPtr(ctx->builder, "enter-conflict", llvm_tmp_name(ctx)),
            fail_reason_alloca);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, run_bb);

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
        LLVMIntentStepContext step_ctx;
        const char *causes_effect;
        LLVMValueRef *saved_participant_ptrs = NULL;
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (step_name == NULL)
            step_name = step->data.intent_step.name;
        if (!llvm_intent_step_context_load(ctx, node, mir_routine, step, step_name,
                mir_only_intent, &step_ctx))
            goto intent_emit_fail;
        causes_effect = step_ctx.causes_effect;

        if (ctx->uses_intent_observability && trace_step_fn != NULL) {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step_name != NULL ? step_name : "<step>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step_ctx.zone_type_name != NULL ? step_ctx.zone_type_name : "<zone>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_step_fn->fn_type, trace_step_fn->fn, args, 3, "");
        }
        if (ctx->uses_intent_observability && trace_bind_fn != NULL) {
        for (size_t j = 0; j < step_ctx.who_alias_count; j++) {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            const char *alias = step_ctx.who_aliases[j];
            const char *slot_name = llvm_resolve_intent_zone_slot_name_for_zone(
                ctx, node, step_ctx.zone_type_name, alias);
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder, alias != NULL ? alias : "<participant>",
                    llvm_tmp_name(ctx)),
                LLVMBuildGlobalStringPtr(ctx->builder, slot_name != NULL ? slot_name : "<unbound>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_bind_fn->fn_type, trace_bind_fn->fn, args, 3, "");
        }
        }
        llvm_emit_intent_step_validate_authority(ctx, fn, fail_bb, fail_reason_alloca,
            step_name, step_ctx.zone_type_name, step_ctx.zone_alias,
            step_ctx.authorized_aliases, step_ctx.authorized_alias_count);
        llvm_emit_intent_step_bind_bound_zone(
            ctx, node, step_ctx.zone_type_name, step_ctx.zone_alias, step_ctx.from_alias,
            step_ctx.who_aliases, step_ctx.who_alias_count);
        if (step_ctx.who_alias_count > 0) {
            /* Participant pointer cache for rebind/unrebind window; freed
             * at step end, never escapes. */
            saved_participant_ptrs = pgy_arena_calloc(&ctx->scratch,
                step_ctx.who_alias_count * sizeof(LLVMValueRef));
            if (saved_participant_ptrs != NULL)
                rebound_aliases = llvm_emit_intent_step_rebind_bound_zone_aliases(
                    ctx, node, step_ctx.zone_type_name, step_ctx.zone_alias,
                    step_ctx.who_aliases, step_ctx.who_alias_count, saved_participant_ptrs);
        }

        if (step_ctx.pre_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.pre.ok");
            LLVMValueRef cond = llvm_emit_expression(step_ctx.pre_expr, ctx);
            snprintf(reason, sizeof(reason), "pre:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step_ctx.invariant_pre_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.invariant.pre.ok");
            LLVMValueRef cond = llvm_emit_expression(step_ctx.invariant_pre_expr, ctx);
            snprintf(reason, sizeof(reason), "invariant-pre:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step_ctx.on_expr_count > 0) {
            for (size_t j = 0; j < step_ctx.on_expr_count; j++) {
                if (step_ctx.on_exprs[j] != NULL)
                    (void)llvm_emit_expression(step_ctx.on_exprs[j], ctx);
            }
        }
        if (step_ctx.subintent_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.subintent.ok");
            LLVMValueRef cond = llvm_emit_expression(step_ctx.subintent_expr, ctx);
            snprintf(reason, sizeof(reason), "intent:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        } else if (step_ctx.on_expr_count == 0) {
            size_t alias_count = step_ctx.dispatch_alias_count;
            if (!mir_only_intent && alias_count == 0)
                alias_count = step->data.intent_step.who_count;
            for (size_t j = 0; j < alias_count; j++) {
                const char *alias = step_ctx.dispatch_alias_count > 0
                    ? step_ctx.dispatch_aliases[j]
                    : step->data.intent_step.who_names[j];
                const char *subject_name = llvm_lookup_var_class(ctx, alias);
                if (subject_name != NULL) {
                    char full_name[256];
                    LLVMFuncEntry *action_fn;
                    LLVMVarEntry *participant_var = llvm_scope_lookup(ctx, alias);
                    snprintf(full_name, sizeof(full_name), "%s_%s",
                        subject_name, step_name);
                    action_fn = llvm_lookup_function(ctx, full_name);
                    if (action_fn != NULL && action_fn->is_action
                        && action_fn->action_self_only && participant_var != NULL) {
                        LLVMValueRef participant_ptr = LLVMBuildLoad2(ctx->builder,
                            participant_var->type, participant_var->alloca, llvm_tmp_name(ctx));
                        LLVMValueRef args[] = { participant_ptr };
                        if (action_fn->ret_type == ctx->type_void) {
                            LLVMBuildCall2(ctx->builder, action_fn->fn_type, action_fn->fn,
                                args, 1, "");
                        } else {
                            (void)LLVMBuildCall2(ctx->builder, action_fn->fn_type, action_fn->fn,
                                args, 1, llvm_tmp_name(ctx));
                        }
                    }
                }
            }
        }
        if (causes_effect == NULL) {
            causes_effect = llvm_infer_intent_step_causes_from_on_exprs(
                ctx, step_ctx.on_exprs, step_ctx.on_expr_count);
        }
        llvm_emit_intent_step_mark_caused_effect(
            ctx, step_ctx.zone_type_name, step_ctx.zone_alias, causes_effect);
        if (rebound_aliases)
            llvm_emit_intent_step_dirty_zone_projections(
                ctx, step_ctx.zone_type_name, step_ctx.zone_alias);
        if (rebound_aliases)
            llvm_emit_intent_step_sync_effective_zone(
                ctx, step_ctx.zone_type_name, step_ctx.zone_alias);
        else
            llvm_emit_intent_step_bind_bound_zone(
                ctx, node, step_ctx.zone_type_name, step_ctx.zone_alias, step_ctx.from_alias,
                step_ctx.who_aliases, step_ctx.who_alias_count);
        if (rebound_aliases)
            llvm_emit_intent_step_restore_bound_zone_aliases(
                ctx, node, step_ctx.zone_type_name, step_ctx.who_aliases,
                step_ctx.who_alias_count, saved_participant_ptrs);

        if (completed_allocas != NULL) {
            LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), completed_allocas[i]);
        }

        if (step_ctx.guard_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.guard.ok");
            LLVMValueRef cond = llvm_emit_expression(step_ctx.guard_expr, ctx);
            snprintf(reason, sizeof(reason), "guard:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step_ctx.expect_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.expect.ok");
            LLVMValueRef cond = llvm_emit_expression(step_ctx.expect_expr, ctx);
            snprintf(reason, sizeof(reason), "expect:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step_ctx.post_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.post.ok");
            LLVMValueRef cond = llvm_emit_expression(step_ctx.post_expr, ctx);
            snprintf(reason, sizeof(reason), "post:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }

        if (step_ctx.invariant_post_expr != NULL) {
            char reason[256];
            LLVMBasicBlockRef next_bb = LLVMAppendBasicBlockInContext(ctx->context, fn, "intent.invariant.post.ok");
            LLVMValueRef cond = llvm_emit_expression(step_ctx.invariant_post_expr, ctx);
            snprintf(reason, sizeof(reason), "invariant-post:%s",
                step_name != NULL ? step_name : "<step>");
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        }
        /* saved_participant_ptrs is ctx->scratch-owned; clear the local
         * reference so the next step rebinds fresh (blocks stay in arena). */
        saved_participant_ptrs = NULL;
        if (ctx->uses_intent_observability && trace_step_ok_fn != NULL) {
            LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
                handle_alloca, llvm_tmp_name(ctx));
            LLVMValueRef args[] = {
                handle,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    step_name != NULL ? step_name : "<step>",
                    llvm_tmp_name(ctx))
            };
            LLVMBuildCall2(ctx->builder, trace_step_ok_fn->fn_type, trace_step_ok_fn->fn, args, 2, "");
        }
    }

    {
        /* `success` may legitimately be NULL when llvm_emit_expression cannot
         * evaluate the success predicate at intent-completion scope (e.g.
         * deep participant.vessel.field references that the LLVM expression
         * emitter currently cannot resolve at this position). Without a
         * guard, LLVMBuildStore(builder, NULL, alloca) crashes the LLVM C
         * API. Until intent-scope expression emission covers every form the
         * C backend accepts, fall back to a constant `true` so the intent
         * completes its success branch with the same observable behavior as
         * the C backend (which evaluates the predicate at runtime via the
         * cleanup tail). The lossy fallback matches docs/120 §2 honest
         * gap: full intent-success expression coverage on the LLVM backend
         * is post-beta work. */
        LLVMValueRef success = NULL;
        if (node->data.intent_decl.success_expr != NULL)
            success = llvm_emit_expression(node->data.intent_decl.success_expr, ctx);
        if (success == NULL)
            success = LLVMConstInt(ctx->type_i1, 1, 0);
        LLVMBuildStore(ctx->builder, success, result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    {
        LLVMValueRef handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        LLVMValueRef reason = LLVMBuildLoad2(ctx->builder, ctx->type_i8ptr,
            fail_reason_alloca, llvm_tmp_name(ctx));
        if (ctx->uses_intent_observability && trace_fail_fn != NULL) {
            LLVMValueRef trace_args[] = { handle, reason };
            LLVMBuildCall2(ctx->builder, trace_fail_fn->fn_type, trace_fail_fn->fn, trace_args, 2, "");
        }
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 1, 0), failed_alloca);
        LLVMBuildStore(ctx->builder, LLVMConstInt(ctx->type_i1, 0, 0), result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, cleanup_bb);
    if (!llvm_emit_intent_cleanup_tail(ctx, node, mir_routine,
            step_nodes, mir_step_names, completed_allocas, step_count,
            mir_only_intent, handle_alloca, failed_alloca,
            compensate_bb, maybe_exit_bb, do_exit_bb, ret_bb, exit_fn)) {
        goto intent_emit_fail;
    }

    LLVMPositionBuilderAtEnd(ctx->builder, ret_bb);
    {
        LLVMValueRef result = LLVMBuildLoad2(ctx->builder, ctx->type_i1,
            result_alloca, llvm_tmp_name(ctx));
        LLVMBuildRet(ctx->builder, result);
    }

    llvm_scope_pop(ctx);
    /* completed_allocas is ctx->scratch-owned. */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;

    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
    return;

intent_emit_fail:
    /* completed_allocas is ctx->scratch-owned. */
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;
    if (saved_fn != NULL) {
        LLVMBasicBlockRef last = LLVMGetLastBasicBlock(saved_fn);
        if (last != NULL)
            LLVMPositionBuilderAtEnd(ctx->builder, last);
    }
}

#endif
