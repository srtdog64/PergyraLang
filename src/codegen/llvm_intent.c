/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend: MIR-backed intent declaration helpers.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

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
    const char *intent_name = NULL;
    ASTNode **decl_steps = NULL;
    size_t decl_step_count = 0;
    size_t involve_count = 0;
    size_t binding_count = 0;
    size_t value_count = 0;
    IntentRollbackPolicy rollback_policy = INTENT_ROLLBACK_NONE;
    ASTNode *priority_expr = NULL;
    ASTNode *success_expr = NULL;
    bool is_concurrent = false;

    if (node == NULL || node->type != AST_INTENT_DECL || ctx == NULL)
        return;
    intent_name = ast_intent_decl_name(node);
    decl_steps = ast_intent_decl_steps(node, &decl_step_count);
    involve_count = ast_intent_decl_involve_count(node);
    binding_count = ast_intent_decl_binding_count(node);
    value_count = ast_intent_decl_value_count(node);
    rollback_policy = ast_intent_decl_rollback_policy(node);
    priority_expr = ast_intent_decl_priority_expr(node);
    success_expr = ast_intent_decl_success_expr(node);
    is_concurrent = ast_intent_decl_is_concurrent(node);
    mir_routine = llvm_find_mir_intent_routine(ctx, node);
    if (mir_routine != NULL) {
        step_count = llvm_collect_mir_intent_step_names(
            mir_routine, ctx, &mir_step_names);
        mir_steps = llvm_build_mir_intent_step_sources(
            node, mir_step_names, step_count, ctx);
    }
    if (ctx->mir != NULL && decl_step_count > 0) {
        if (mir_routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent routine for '%s'",
                intent_name != NULL ? intent_name : "(anonymous)");
            return;
        }
        if (step_count == 0) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent step sequence for '%s'",
                intent_name != NULL ? intent_name : "(anonymous)");
            return;
        }
        for (size_t i = 0; i < step_count; i++) {
            if (mir_steps != NULL && mir_steps[i] != NULL)
                continue;
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent step source mapping for '%s'",
                mir_step_names != NULL && mir_step_names[i] != NULL
                    ? mir_step_names[i]
                    : "(anonymous-step)");
            return;
        }
        mir_only_intent = true;
    }
    if (step_count > 0) {
        step_nodes = mir_steps;
    } else {
        step_count = decl_step_count;
        step_nodes = decl_steps;
    }
    if (mir_routine != NULL) {
        participant_count = llvm_collect_mir_intent_participants(
            mir_routine, ctx, &participant_aliases, &participant_types);
    }
    if (mir_only_intent && involve_count > 0) {
        if (participant_count < involve_count) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing intent participant metadata for '%s'",
                intent_name != NULL ? intent_name : "(anonymous)");
            return;
        }
        for (size_t i = 0; i < involve_count; i++) {
            if (participant_aliases == NULL || participant_types == NULL
                || participant_aliases[i] == NULL || participant_types[i] == NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "MIR-only LLVM path has incomplete intent participant metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous)");
                return;
            }
        }
    }
    if (participant_count == 0)
        participant_count = involve_count;
    param_count = binding_count > 0 ? binding_count : (involve_count + value_count);
    if (param_count == 0)
        param_count = participant_count;

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
        if (step != NULL && step->type == AST_INTENT_STEP
            && ((mir_only_intent && mir_routine != NULL
                 && llvm_mir_intent_has_stmt(
                     mir_routine, step_name != NULL ? step_name : ast_intent_step_name(step),
                     "IntentEval", "compensate"))
                || (!mir_only_intent && ast_intent_step_compensate_expr_count(step) > 0))) {
            has_compensate_steps = true;
            break;
        }
    }
    if (rollback_policy == INTENT_ROLLBACK_NONE)
        has_compensate_steps = false;
    entry = llvm_lookup_function(ctx, intent_name);
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
        LLVMValueRef priority = priority_expr != NULL
            ? llvm_emit_expression(priority_expr, ctx)
            : LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef enter_args[] = {
            LLVMBuildGlobalStringPtr(ctx->builder, intent_name,
                llvm_tmp_name(ctx)),
            subjects_ptr,
            LLVMConstInt(ctx->type_i32, (unsigned)subject_count, 0),
            LLVMConstInt(ctx->type_i1, is_concurrent ? 1 : 0, 0),
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
            step_name = ast_intent_step_name(step);
        if (!llvm_intent_step_context_load(ctx, node, mir_routine, step, step_name,
                mir_only_intent, &step_ctx))
            goto intent_emit_fail;
        causes_effect = step_ctx.causes_effect;

        if (ctx->uses_intent_observability)
            llvm_emit_intent_trace_step(ctx, trace_step_fn, handle_alloca,
                step_name, step_ctx.zone_type_name);
        if (ctx->uses_intent_observability)
            llvm_emit_intent_trace_bindings(ctx, node, trace_bind_fn,
                handle_alloca, &step_ctx);
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

        if (!llvm_emit_intent_predicate_check(ctx, fn, fail_bb,
                fail_reason_alloca, step_ctx.pre_expr, "pre", step_name,
                "intent.pre.ok")) {
            goto intent_emit_fail;
        }
        if (!llvm_emit_intent_predicate_check(ctx, fn, fail_bb,
                fail_reason_alloca, step_ctx.invariant_pre_expr,
                "invariant-pre", step_name, "intent.invariant.pre.ok")) {
            goto intent_emit_fail;
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
            if (!llvm_intent_reason_name(ctx, reason, sizeof(reason),
                    "intent", step_name))
                return;
            LLVMBuildStore(ctx->builder,
                LLVMBuildGlobalStringPtr(ctx->builder,
                    reason,
                    llvm_tmp_name(ctx)),
                fail_reason_alloca);
            LLVMBuildCondBr(ctx->builder, cond, next_bb, fail_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, next_bb);
        } else if (step_ctx.on_expr_count == 0) {
            size_t alias_count = step_ctx.dispatch_alias_count;
            for (size_t j = 0; j < alias_count; j++) {
                const char *alias = step_ctx.dispatch_aliases[j];
                const char *subject_name = llvm_lookup_var_class(ctx, alias);
                if (subject_name != NULL) {
                    char full_name[256];
                    LLVMFuncEntry *action_fn;
                    LLVMVarEntry *participant_var = llvm_scope_lookup(ctx, alias);
                    if (!llvm_intent_action_function_name(ctx, full_name,
                            sizeof(full_name), subject_name, step_name))
                        return;
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

        if (!llvm_emit_intent_predicate_check(ctx, fn, fail_bb,
                fail_reason_alloca, step_ctx.guard_expr, "guard", step_name,
                "intent.guard.ok")) {
            goto intent_emit_fail;
        }
        if (!llvm_emit_intent_predicate_check(ctx, fn, fail_bb,
                fail_reason_alloca, step_ctx.expect_expr, "expect", step_name,
                "intent.expect.ok")) {
            goto intent_emit_fail;
        }
        if (!llvm_emit_intent_predicate_check(ctx, fn, fail_bb,
                fail_reason_alloca, step_ctx.post_expr, "post", step_name,
                "intent.post.ok")) {
            goto intent_emit_fail;
        }
        if (!llvm_emit_intent_predicate_check(ctx, fn, fail_bb,
                fail_reason_alloca, step_ctx.invariant_post_expr,
                "invariant-post", step_name, "intent.invariant.post.ok")) {
            goto intent_emit_fail;
        }
        /* saved_participant_ptrs is ctx->scratch-owned; clear the local
         * reference so the next step rebinds fresh (blocks stay in arena). */
        saved_participant_ptrs = NULL;
        if (ctx->uses_intent_observability)
            llvm_emit_intent_trace_step_ok(ctx, trace_step_ok_fn,
                handle_alloca, step_name);
    }

    {
        /* Do not silently turn an unsupported success predicate into `true`.
         * Backend parity is stricter than preserving a best-effort executable:
         * unsupported LLVM intent-success expressions must fail with a
         * diagnostic until the predicate can lower through real LLVM facts. */
        LLVMValueRef success = NULL;
        if (success_expr != NULL)
            success = llvm_emit_expression(success_expr, ctx);
        else
            success = LLVMConstInt(ctx->type_i1, 1, 0);
        if (success == NULL) {
            llvm_set_error_at_with_hints(ctx,
                success_expr,
                PGY_CODE_LLVM_TYPE_UNSUPPORTED,
                PGY_CAUSE_LLVM_TYPE_UNSUPPORTED,
                PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
                "LLVM intent '%s' cannot lower its success predicate; silent true fallback is disabled",
                intent_name != NULL ? intent_name : "<intent>");
            goto intent_emit_fail;
        }
        LLVMBuildStore(ctx->builder, success, result_alloca);
        LLVMBuildBr(ctx->builder, cleanup_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, fail_bb);
    {
        if (ctx->uses_intent_observability)
            llvm_emit_intent_trace_failure(ctx, trace_fail_fn,
                handle_alloca, fail_reason_alloca);
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
