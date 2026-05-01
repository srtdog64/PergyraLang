typedef struct LLVMIntentObservabilityBuiltin {
    const char *name;
    const char *runtime_name;
    size_t arg_count;
} LLVMIntentObservabilityBuiltin;

static int
llvm_intent_observability_builtin_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const LLVMIntentObservabilityBuiltin *builtin =
        (const LLVMIntentObservabilityBuiltin *)entry;

    return strcmp(name, builtin->name);
}

static bool
llvm_emit_intent_observability_call(ASTNode *node, LLVMGenCtx *ctx,
                                    const char *callee_name, LLVMValueRef *out)
{
    static const LLVMIntentObservabilityBuiltin builtins[] = {
        { "IntentActiveConcurrent", "pgy_intent_active_concurrent_export", 1 },
        { "IntentActiveCount", "pgy_intent_active_count_export", 0 },
        { "IntentActiveFailed", "pgy_intent_active_failed_export", 1 },
        { "IntentActiveFailure", "pgy_intent_active_failure_export", 1 },
        { "IntentActiveHandle", "pgy_intent_active_handle_export", 1 },
        { "IntentActiveName", "pgy_intent_active_name_export", 1 },
        { "IntentActiveParentHandle", "pgy_intent_active_parent_handle_export", 1 },
        { "IntentActivePriority", "pgy_intent_active_priority_export", 1 },
        { "IntentActiveStepCount", "pgy_intent_active_step_count_export", 1 },
        { "IntentActiveStepFailure", "pgy_intent_active_step_failure_export", 2 },
        { "IntentActiveStepFromSlot", "pgy_intent_active_step_from_slot_export", 2 },
        { "IntentActiveStepFromZone", "pgy_intent_active_step_from_zone_export", 2 },
        { "IntentActiveStepName", "pgy_intent_active_step_name_export", 2 },
        { "IntentActiveStepOk", "pgy_intent_active_step_ok_export", 2 },
        { "IntentActiveStepParticipant", "pgy_intent_active_step_participant_export", 2 },
        { "IntentActiveStepPhase", "pgy_intent_active_step_phase_export", 2 },
        { "IntentActiveStepSlot", "pgy_intent_active_step_slot_export", 2 },
        { "IntentActiveStepToSlot", "pgy_intent_active_step_to_slot_export", 2 },
        { "IntentActiveStepToZone", "pgy_intent_active_step_to_zone_export", 2 },
        { "IntentActiveStepZone", "pgy_intent_active_step_zone_export", 2 },
        { "IntentActiveSubjectCount", "pgy_intent_active_subject_count_export", 1 },
        { "IntentActiveTrace", "pgy_intent_active_trace_export", 1 },
        { "IntentActiveTraceId", "pgy_intent_active_trace_id_export", 1 },
        { "IntentCurrentHandle", "pgy_intent_current_handle_export", 0 },
        { "IntentHistoryCount", "pgy_intent_history_count_export", 0 },
        { "IntentHistoryStepFailure", "pgy_intent_history_step_failure_export", 1 },
        { "IntentHistoryStepFromSlot", "pgy_intent_history_step_from_slot_export", 1 },
        { "IntentHistoryStepFromZone", "pgy_intent_history_step_from_zone_export", 1 },
        { "IntentHistoryStepName", "pgy_intent_history_step_name_export", 1 },
        { "IntentHistoryStepOk", "pgy_intent_history_step_ok_export", 1 },
        { "IntentHistoryStepParticipant", "pgy_intent_history_step_participant_export", 1 },
        { "IntentHistoryStepPhase", "pgy_intent_history_step_phase_export", 1 },
        { "IntentHistoryStepSlot", "pgy_intent_history_step_slot_export", 1 },
        { "IntentHistoryStepToSlot", "pgy_intent_history_step_to_slot_export", 1 },
        { "IntentHistoryStepToZone", "pgy_intent_history_step_to_zone_export", 1 },
        { "IntentHistoryStepZone", "pgy_intent_history_step_zone_export", 1 },
        { "IntentLastFailed", "pgy_intent_last_failed_export", 0 },
        { "IntentLastFailure", "pgy_intent_last_failure_export", 0 },
        { "IntentLastHandle", "pgy_intent_last_handle_export", 0 },
        { "IntentLastName", "pgy_intent_last_name_export", 0 },
        { "IntentLastStepCount", "pgy_intent_last_step_count_export", 0 },
        { "IntentLastTrace", "pgy_intent_last_trace_export", 0 },
        { "IntentLastTraceId", "pgy_intent_last_trace_id_export", 0 },
        { "IntentRecentCount", "pgy_intent_recent_count_export", 0 },
        { "IntentRecentFailed", "pgy_intent_recent_failed_export", 1 },
        { "IntentRecentFailure", "pgy_intent_recent_failure_export", 1 },
        { "IntentRecentHandle", "pgy_intent_recent_handle_export", 1 },
        { "IntentRecentName", "pgy_intent_recent_name_export", 1 },
        { "IntentRecentStepCount", "pgy_intent_recent_step_count_export", 1 },
        { "IntentRecentTrace", "pgy_intent_recent_trace_export", 1 },
        { "IntentRecentTraceId", "pgy_intent_recent_trace_id_export", 1 },
    };
    const LLVMIntentObservabilityBuiltin *builtin;

    if (out == NULL)
        return false;

    builtin = (const LLVMIntentObservabilityBuiltin *)bsearch(
        callee_name, builtins, sizeof(builtins) / sizeof(builtins[0]),
        sizeof(builtins[0]), llvm_intent_observability_builtin_compare);
    if (builtin == NULL || node->data.call.arg_count != builtin->arg_count)
        return false;

    ctx->uses_intent_observability = true;
    LLVMFuncEntry *fn = llvm_lookup_function(ctx, builtin->runtime_name);
    if (fn == NULL)
        return false;

    if (builtin->arg_count == 0)
        *out = LLVMBuildCall2(ctx->builder, fn->fn_type, fn->fn, NULL, 0,
            llvm_tmp_name(ctx));
    else
        *out = llvm_emit_function_call_args(ctx, fn,
            node->data.call.arguments, builtin->arg_count);

    return true;
}
