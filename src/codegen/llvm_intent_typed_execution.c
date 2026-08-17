#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_typed_execution.h"

#include <string.h>

#include "llvm_intent_internal.h"

static const MIRIntentStepTransitionFact *
llvm_intent_transition_by_id(const MIRRoutine *routine,
                             uint32_t transition_id,
                             size_t *index_out)
{
    size_t count = mir_routine_intent_step_transition_count(routine);

    for (size_t i = 0; i < count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        if (row != NULL && row->transition_id == transition_id) {
            if (index_out != NULL)
                *index_out = i;
            return row;
        }
    }
    return NULL;
}

static const MIRIntentStepTransitionFact *
llvm_intent_transition_child(const MIRRoutine *routine,
                             uint32_t predecessor_transition_id)
{
    const MIRIntentStepTransitionFact *child = NULL;
    size_t count = mir_routine_intent_step_transition_count(routine);

    for (size_t i = 0; i < count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        if (row == NULL || !row->has_predecessor
            || row->predecessor_transition_id != predecessor_transition_id) {
            continue;
        }
        if (child != NULL)
            return NULL;
        child = row;
    }
    return child;
}

static const MIRIntentTerminalTransitionFact *
llvm_intent_terminal_for(const MIRRoutine *routine,
                         uint32_t source_transition_id,
                         MIRIntentTerminalRole role)
{
    const MIRIntentTerminalTransitionFact *terminal = NULL;
    size_t count = mir_routine_intent_terminal_transition_count(routine);

    for (size_t i = 0; i < count; i++) {
        const MIRIntentTerminalTransitionFact *row =
            mir_routine_intent_terminal_transition_at(routine, i);
        if (row == NULL || row->source_transition_id != source_transition_id
            || row->role != role) {
            continue;
        }
        if (terminal != NULL)
            return NULL;
        terminal = row;
    }
    return terminal;
}

static bool
llvm_intent_emit_typed_predecessor_compensation(
    LLVMGenCtx *ctx,
    const MIRRoutine *routine,
    const MIRIntentStepTransitionFact *failed,
    LLVMValueRef *completed_allocas)
{
    const MIRIntentStepTransitionFact *cursor = failed;
    size_t step_count = mir_routine_intent_step_transition_count(routine);
    size_t depth = 0;

    while (cursor != NULL && cursor->has_predecessor) {
        size_t predecessor_index = 0;
        const MIRIntentStepTransitionFact *predecessor =
            llvm_intent_transition_by_id(routine,
                cursor->predecessor_transition_id, &predecessor_index);
        if (predecessor == NULL
            || (predecessor->compensation_count > 0
                && predecessor->compensations == NULL)) {
            llvm_set_mir_inventory_missing(ctx,
                "admitted typed intent has invalid predecessor compensation evidence");
            return false;
        }
        if (predecessor->compensation_count > 0) {
            LLVMBasicBlockRef compensate_bb = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function,
                "intent.typed.compensate");
            LLVMBasicBlockRef continue_bb = LLVMAppendBasicBlockInContext(
                ctx->context, ctx->current_function,
                "intent.typed.compensate.continue");
            LLVMValueRef completed = LLVMBuildLoad2(ctx->builder,
                ctx->type_i1, completed_allocas[predecessor_index],
                llvm_tmp_name(ctx));
            LLVMBuildCondBr(ctx->builder, completed,
                compensate_bb, continue_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, compensate_bb);
            for (size_t i = predecessor->compensation_count; i > 0; i--) {
                const MIRIntentCompensationFact *compensation =
                    &predecessor->compensations[i - 1];

                if (compensation->expression == NULL) {
                    llvm_set_mir_inventory_missing(ctx,
                        "admitted typed intent has missing predecessor compensation expression");
                    return false;
                }
                (void)llvm_emit_expression(compensation->expression, ctx);
                if (ctx->has_error)
                    return false;
            }
            LLVMBuildBr(ctx->builder, continue_bb);
            LLVMPositionBuilderAtEnd(ctx->builder, continue_bb);
        }
        cursor = predecessor;
        depth++;
        if (depth > step_count) {
            llvm_set_mir_inventory_missing(ctx,
                "admitted typed intent predecessor chain is cyclic");
            return false;
        }
    }
    return true;
}

bool
llvm_emit_typed_intent_execution(ASTNode *node,
                                 LLVMGenCtx *ctx,
                                 const MIRRoutine *routine,
                                 ASTNode *priority_expr,
                                 bool is_concurrent)
{
    const MIRIntentStepTransitionFact *root = NULL;
    IntentBindingMetadataView binding_metadata = {0};
    LLVMFuncEntry *entry;
    LLVMFuncEntry *enter_fn;
    LLVMFuncEntry *exit_fn;
    LLVMFuncEntry *trace_step_fn;
    LLVMFuncEntry *trace_step_ok_fn;
    LLVMFuncEntry *trace_fail_fn;
    LLVMFuncEntry *panic_fn;
    LLVMValueRef fn;
    LLVMValueRef saved_fn;
    LLVMTypeRef saved_ret_type;
    LLVMTypeRef saved_function_ret_type;
    const char *saved_return_type_name;
    ASTNode *saved_return_callable_type;
    LLVMBasicBlockRef saved_bb;
    LLVMLexicalRegistrySnapshot lexical_snapshot;
    bool scope_pushed = false;
    LLVMTypeRef return_type;
    LLVMValueRef handle_alloca;
    LLVMValueRef subjects_ptr = NULL;
    size_t subject_count = 0;
    size_t binding_count;
    size_t step_count = mir_routine_intent_step_transition_count(routine);
    size_t terminal_count =
        mir_routine_intent_terminal_transition_count(routine);
    LLVMBasicBlockRef entry_bb;
    LLVMBasicBlockRef admission_fail_bb;
    LLVMBasicBlockRef invalid_tag_bb;
    LLVMBasicBlockRef *transition_bbs;
    LLVMBasicBlockRef *success_bbs;
    LLVMBasicBlockRef *failure_test_bbs;
    LLVMBasicBlockRef *failure_bbs;
    LLVMBasicBlockRef *terminal_bbs;
    LLVMValueRef *outcome_allocas;
    LLVMValueRef *success_payload_allocas;
    LLVMValueRef *failure_payload_allocas;
    LLVMValueRef *completed_allocas;
    const char *return_type_name;
    size_t root_index = 0;

    if (node == NULL || ctx == NULL || routine == NULL
        || step_count == 0 || terminal_count == 0) {
        return false;
    }
    return_type_name = mir_routine_return_type_name(routine);
    return_type = pergyra_type_to_llvm(ctx, return_type_name);
    if (ctx->has_error || return_type == NULL)
        return false;

    binding_count = llvm_collect_mir_intent_bindings(
        routine, ctx, &binding_metadata);
    for (size_t i = 0; i < binding_count; i++) {
        if (!intent_binding_metadata_view_has_supported_row(
                &binding_metadata, i)) {
            llvm_set_mir_inventory_missing(ctx,
                "admitted typed intent has invalid ordered binding metadata");
            return false;
        }
    }
    for (size_t i = 0; i < step_count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        if (row == NULL || !row->sealed || row->transition_id == 0
            || row->outcome_expression == NULL
            || row->outcome_result_name == NULL
            || row->outcome_type_name == NULL
            || row->success.payload_name == NULL
            || row->success.payload_type_name == NULL
            || row->failure.payload_name == NULL
            || row->failure.payload_type_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "admitted typed intent has an incomplete step transition");
            return false;
        }
        if (!row->has_predecessor) {
            if (root != NULL) {
                llvm_set_mir_inventory_missing(ctx,
                    "admitted typed intent has multiple root transitions");
                return false;
            }
            root = row;
            root_index = i;
        }
    }
    if (root == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "admitted typed intent has no root transition");
        return false;
    }
    for (size_t i = 0; i < terminal_count; i++) {
        const MIRIntentTerminalTransitionFact *terminal =
            mir_routine_intent_terminal_transition_at(routine, i);
        if (terminal == NULL || !terminal->sealed
            || terminal->expression == NULL
            || llvm_intent_transition_by_id(
                routine, terminal->source_transition_id, NULL) == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "admitted typed intent has an incomplete terminal transition");
            return false;
        }
    }

    entry = llvm_lookup_function(ctx, mir_routine_name(routine));
    enter_fn = llvm_lookup_function(ctx, "pgy_intent_enter_export");
    exit_fn = llvm_lookup_function(ctx, "pgy_intent_exit_export");
    trace_step_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_step_export") : NULL;
    trace_step_ok_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_step_ok_export") : NULL;
    trace_fail_fn = ctx->uses_intent_observability
        ? llvm_lookup_function(ctx, "pgy_intent_trace_fail_export") : NULL;
    panic_fn = llvm_lookup_function(
        ctx, "pgy_runtime_panic_internal_invariant_export");
    if (entry == NULL || enter_fn == NULL || exit_fn == NULL
        || panic_fn == NULL || (ctx->uses_intent_observability
            && (trace_step_fn == NULL || trace_step_ok_fn == NULL
                || trace_fail_fn == NULL))) {
        llvm_set_mir_inventory_missing(ctx,
            "typed intent runtime function inventory is incomplete");
        return false;
    }

    transition_bbs = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*transition_bbs));
    success_bbs = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*success_bbs));
    failure_test_bbs = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*failure_test_bbs));
    failure_bbs = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*failure_bbs));
    terminal_bbs = pgy_arena_calloc(&ctx->scratch,
        terminal_count * sizeof(*terminal_bbs));
    outcome_allocas = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*outcome_allocas));
    success_payload_allocas = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*success_payload_allocas));
    failure_payload_allocas = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*failure_payload_allocas));
    completed_allocas = pgy_arena_calloc(&ctx->scratch,
        step_count * sizeof(*completed_allocas));
    if (transition_bbs == NULL || success_bbs == NULL
        || failure_test_bbs == NULL || failure_bbs == NULL
        || terminal_bbs == NULL || outcome_allocas == NULL
        || success_payload_allocas == NULL
        || failure_payload_allocas == NULL || completed_allocas == NULL) {
        llvm_set_error_at_with_hints(ctx, node,
            PGY_CODE_LLVM_OOM,
            PGY_CAUSE_LLVM_MEMORY_EXHAUSTED,
            PGY_FIX_REDUCE_UNIT_SIZE_OR_RAISE_LIMIT,
            "LLVM typed intent plan allocation failed");
        return false;
    }

    fn = entry->fn;
    saved_fn = ctx->current_function;
    saved_ret_type = ctx->current_ret_type;
    saved_function_ret_type = ctx->current_function_ret_type;
    saved_return_type_name = ctx->current_return_type_name;
    saved_return_callable_type = ctx->current_return_callable_type;
    saved_bb = LLVMGetInsertBlock(ctx->builder);
    lexical_snapshot = llvm_lexical_registry_snapshot(ctx);
    ctx->current_function = fn;
    ctx->current_ret_type = return_type;
    ctx->current_function_ret_type = return_type;
    ctx->current_return_type_name = return_type_name;
    ctx->current_return_callable_type = NULL;

    entry_bb = LLVMAppendBasicBlockInContext(ctx->context, fn,
        "intent.typed.entry");
    admission_fail_bb = LLVMAppendBasicBlockInContext(ctx->context, fn,
        "intent.typed.admission.fail");
    invalid_tag_bb = LLVMAppendBasicBlockInContext(ctx->context, fn,
        "intent.typed.invalid.tag");
    for (size_t i = 0; i < step_count; i++) {
        transition_bbs[i] = LLVMAppendBasicBlockInContext(ctx->context, fn,
            "intent.typed.transition");
        success_bbs[i] = LLVMAppendBasicBlockInContext(ctx->context, fn,
            "intent.typed.success");
        failure_test_bbs[i] = LLVMAppendBasicBlockInContext(ctx->context, fn,
            "intent.typed.failure.test");
        failure_bbs[i] = LLVMAppendBasicBlockInContext(ctx->context, fn,
            "intent.typed.failure");
    }
    for (size_t i = 0; i < terminal_count; i++) {
        terminal_bbs[i] = LLVMAppendBasicBlockInContext(ctx->context, fn,
            "intent.typed.terminal");
    }

    LLVMPositionBuilderAtEnd(ctx->builder, entry_bb);
    llvm_scope_push(ctx);
    if (ctx->has_error)
        goto typed_intent_fail;
    scope_pushed = true;
    llvm_emit_intent_entry_bindings(ctx, node, fn, &binding_metadata,
        binding_count, true, &subjects_ptr, &subject_count);
    if (ctx->has_error)
        goto typed_intent_fail;
    handle_alloca = llvm_create_entry_alloca(
        ctx, ctx->type_i32, "__intent_handle");
    llvm_scope_declare(ctx, "__intent_handle", handle_alloca,
        ctx->type_i32);

    for (size_t i = 0; i < step_count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        LLVMTypeRef outcome_type = pergyra_type_to_llvm(
            ctx, row->outcome_type_name);
        LLVMTypeRef success_type = pergyra_type_to_llvm(
            ctx, row->success.payload_type_name);
        LLVMTypeRef failure_type = pergyra_type_to_llvm(
            ctx, row->failure.payload_type_name);
        LLVMClassTypeEntry *success_class;
        LLVMClassTypeEntry *failure_class;
        if (ctx->has_error || outcome_type == NULL
            || success_type == NULL || failure_type == NULL)
            goto typed_intent_fail;
        outcome_allocas[i] = llvm_create_entry_alloca(
            ctx, outcome_type, row->outcome_result_name);
        success_payload_allocas[i] = llvm_create_entry_alloca(
            ctx, success_type, row->success.payload_name);
        failure_payload_allocas[i] = llvm_create_entry_alloca(
            ctx, failure_type, row->failure.payload_name);
        completed_allocas[i] = llvm_create_entry_alloca(
            ctx, ctx->type_i1, "__intent_step_done");
        LLVMBuildStore(ctx->builder,
            LLVMConstInt(ctx->type_i1, 0, 0), completed_allocas[i]);
        llvm_scope_declare(ctx, row->outcome_result_name,
            outcome_allocas[i], outcome_type);
        llvm_scope_declare(ctx, row->success.payload_name,
            success_payload_allocas[i], success_type);
        llvm_scope_declare(ctx, row->failure.payload_name,
            failure_payload_allocas[i], failure_type);
        success_class = llvm_lookup_class_by_type(ctx, success_type);
        failure_class = llvm_lookup_class_by_type(ctx, failure_type);
        if (success_class != NULL)
            llvm_register_var_class(ctx, row->success.payload_name,
                success_class->class_name);
        if (failure_class != NULL)
            llvm_register_var_class(ctx, row->failure.payload_name,
                failure_class->class_name);
        if (ctx->has_error)
            goto typed_intent_fail;
    }

    {
        LLVMValueRef priority = priority_expr != NULL
            ? llvm_emit_expression(priority_expr, ctx)
            : LLVMConstInt(ctx->type_i32, 0, 0);
        LLVMValueRef enter_args[] = {
            LLVMBuildGlobalStringPtr(ctx->builder,
                mir_routine_name(routine), llvm_tmp_name(ctx)),
            subjects_ptr,
            LLVMConstInt(ctx->type_i32, (unsigned)subject_count, 0),
            LLVMConstInt(ctx->type_i1, is_concurrent ? 1 : 0, 0),
            priority
        };
        LLVMValueRef handle = LLVMBuildCall2(ctx->builder,
            enter_fn->fn_type, enter_fn->fn, enter_args, 5,
            llvm_tmp_name(ctx));
        LLVMValueRef entered = LLVMBuildICmp(ctx->builder, LLVMIntNE,
            handle, LLVMConstInt(ctx->type_i32, 0, 0), llvm_tmp_name(ctx));
        LLVMBuildStore(ctx->builder, handle, handle_alloca);
        LLVMBuildCondBr(ctx->builder, entered,
            transition_bbs[root_index], admission_fail_bb);
    }

    LLVMPositionBuilderAtEnd(ctx->builder, admission_fail_bb);
    {
        LLVMValueRef reason = LLVMBuildGlobalStringPtr(ctx->builder,
            "typed intent runtime admission failed", llvm_tmp_name(ctx));
        LLVMValueRef args[] = { reason };
        LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
            args, 1, "");
        LLVMBuildUnreachable(ctx->builder);
    }
    LLVMPositionBuilderAtEnd(ctx->builder, invalid_tag_bb);
    {
        LLVMValueRef reason = LLVMBuildGlobalStringPtr(ctx->builder,
            "typed intent outcome tag escaped admitted branches",
            llvm_tmp_name(ctx));
        LLVMValueRef args[] = { reason };
        LLVMBuildCall2(ctx->builder, panic_fn->fn_type, panic_fn->fn,
            args, 1, "");
        LLVMBuildUnreachable(ctx->builder);
    }

    for (size_t i = 0; i < step_count; i++) {
        const MIRIntentStepTransitionFact *row =
            mir_routine_intent_step_transition_at(routine, i);
        const MIRIntentStepTransitionFact *child =
            llvm_intent_transition_child(routine, row->transition_id);
        const MIRIntentTerminalTransitionFact *success_terminal =
            llvm_intent_terminal_for(routine, row->transition_id,
                MIR_INTENT_TERMINAL_SUCCESS);
        const MIRIntentTerminalTransitionFact *failure_terminal =
            llvm_intent_terminal_for(routine, row->transition_id,
                MIR_INTENT_TERMINAL_FAILURE);
        size_t child_index = 0;
        size_t success_terminal_index = 0;
        size_t failure_terminal_index = 0;
        LLVMTypeRef outcome_type = pergyra_type_to_llvm(
            ctx, row->outcome_type_name);
        LLVMValueRef outcome;
        LLVMValueRef loaded;
        LLVMValueRef tag;
        LLVMValueRef success_match;

        if (ctx->has_error || outcome_type == NULL)
            goto typed_intent_fail;
        if ((child == NULL) == (success_terminal == NULL)
            || failure_terminal == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "admitted typed intent successor/terminal topology is invalid");
            goto typed_intent_fail;
        }
        if (child != NULL
            && llvm_intent_transition_by_id(routine,
                child->transition_id, &child_index) == NULL) {
            goto typed_intent_fail;
        }
        for (size_t j = 0; j < terminal_count; j++) {
            const MIRIntentTerminalTransitionFact *terminal =
                mir_routine_intent_terminal_transition_at(routine, j);
            if (terminal == success_terminal)
                success_terminal_index = j;
            if (terminal == failure_terminal)
                failure_terminal_index = j;
        }

        LLVMPositionBuilderAtEnd(ctx->builder, transition_bbs[i]);
        if (ctx->uses_intent_observability) {
            llvm_emit_intent_trace_step(ctx, trace_step_fn, handle_alloca,
                row->step_name, row->where_zone_name);
        }
        {
            const char *saved_expected = ctx->expected_type_name;
            ctx->expected_type_name = row->outcome_type_name;
            outcome = llvm_emit_expression(row->outcome_expression, ctx);
            ctx->expected_type_name = saved_expected;
        }
        if (outcome == NULL || ctx->has_error)
            goto typed_intent_fail;
        LLVMBuildStore(ctx->builder, outcome, outcome_allocas[i]);
        loaded = LLVMBuildLoad2(ctx->builder, outcome_type,
            outcome_allocas[i], llvm_tmp_name(ctx));
        tag = LLVMBuildExtractValue(ctx->builder, loaded, 0,
            llvm_tmp_name(ctx));
        success_match = LLVMBuildICmp(ctx->builder, LLVMIntEQ, tag,
            LLVMConstInt(ctx->type_i32,
                (unsigned long long)row->success.variant_index, 0),
            llvm_tmp_name(ctx));
        LLVMBuildCondBr(ctx->builder, success_match,
            success_bbs[i], failure_test_bbs[i]);

        LLVMPositionBuilderAtEnd(ctx->builder, failure_test_bbs[i]);
        {
            LLVMValueRef failure_match = LLVMBuildICmp(ctx->builder,
                LLVMIntEQ, tag,
                LLVMConstInt(ctx->type_i32,
                    (unsigned long long)row->failure.variant_index, 0),
                llvm_tmp_name(ctx));
            LLVMBuildCondBr(ctx->builder, failure_match,
                failure_bbs[i], invalid_tag_bb);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, success_bbs[i]);
        {
            LLVMValueRef variant_payload = LLVMBuildExtractValue(
                ctx->builder, loaded,
                (unsigned)(row->success.variant_index + 1),
                llvm_tmp_name(ctx));
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                variant_payload, 0, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, payload,
                success_payload_allocas[i]);
            LLVMBuildStore(ctx->builder,
                LLVMConstInt(ctx->type_i1, 1, 0), completed_allocas[i]);
            if (ctx->uses_intent_observability) {
                llvm_emit_intent_trace_step_ok(ctx, trace_step_ok_fn,
                    handle_alloca, row->step_name);
            }
            LLVMBuildBr(ctx->builder,
                child != NULL
                    ? transition_bbs[child_index]
                    : terminal_bbs[success_terminal_index]);
        }

        LLVMPositionBuilderAtEnd(ctx->builder, failure_bbs[i]);
        {
            LLVMValueRef variant_payload = LLVMBuildExtractValue(
                ctx->builder, loaded,
                (unsigned)(row->failure.variant_index + 1),
                llvm_tmp_name(ctx));
            LLVMValueRef payload = LLVMBuildExtractValue(ctx->builder,
                variant_payload, 0, llvm_tmp_name(ctx));
            LLVMBuildStore(ctx->builder, payload,
                failure_payload_allocas[i]);
            if (ctx->uses_intent_observability) {
                const char *reason = pgy_arena_fmt(&ctx->scratch,
                    "outcome:%s", row->step_name);
                LLVMValueRef handle = LLVMBuildLoad2(ctx->builder,
                    ctx->type_i32, handle_alloca, llvm_tmp_name(ctx));
                LLVMValueRef args[] = {
                    handle,
                    LLVMBuildGlobalStringPtr(ctx->builder, reason,
                        llvm_tmp_name(ctx))
                };
                LLVMBuildCall2(ctx->builder, trace_fail_fn->fn_type,
                    trace_fail_fn->fn, args, 2, "");
            }
            LLVMBuildBr(ctx->builder,
                terminal_bbs[failure_terminal_index]);
        }
    }

    for (size_t i = 0; i < terminal_count; i++) {
        const MIRIntentTerminalTransitionFact *terminal =
            mir_routine_intent_terminal_transition_at(routine, i);
        const MIRIntentStepTransitionFact *source =
            llvm_intent_transition_by_id(
                routine, terminal->source_transition_id, NULL);
        LLVMValueRef result;
        LLVMValueRef handle;
        LLVMValueRef exit_args[1];

        LLVMPositionBuilderAtEnd(ctx->builder, terminal_bbs[i]);
        if (terminal->role == MIR_INTENT_TERMINAL_FAILURE
            && !llvm_intent_emit_typed_predecessor_compensation(
                ctx, routine, source, completed_allocas)) {
            goto typed_intent_fail;
        }
        {
            const char *saved_expected = ctx->expected_type_name;
            ctx->expected_type_name = terminal->result_type_name;
            result = llvm_emit_expression(terminal->expression, ctx);
            ctx->expected_type_name = saved_expected;
        }
        if (result == NULL || ctx->has_error)
            goto typed_intent_fail;
        handle = LLVMBuildLoad2(ctx->builder, ctx->type_i32,
            handle_alloca, llvm_tmp_name(ctx));
        exit_args[0] = handle;
        LLVMBuildCall2(ctx->builder, exit_fn->fn_type, exit_fn->fn,
            exit_args, 1, "");
        LLVMBuildRet(ctx->builder, result);
    }

    llvm_scope_pop(ctx);
    scope_pushed = false;
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;
    ctx->current_function_ret_type = saved_function_ret_type;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    intent_binding_metadata_view_dispose(&binding_metadata);
    return true;

typed_intent_fail:
    if (scope_pushed)
        llvm_scope_pop(ctx);
    llvm_lexical_registry_restore(ctx, lexical_snapshot);
    ctx->current_function = saved_fn;
    ctx->current_ret_type = saved_ret_type;
    ctx->current_function_ret_type = saved_function_ret_type;
    ctx->current_return_type_name = saved_return_type_name;
    ctx->current_return_callable_type = saved_return_callable_type;
    if (saved_bb != NULL)
        LLVMPositionBuilderAtEnd(ctx->builder, saved_bb);
    intent_binding_metadata_view_dispose(&binding_metadata);
    return false;
}


#endif /* PGY_LLVM_ENABLED */
