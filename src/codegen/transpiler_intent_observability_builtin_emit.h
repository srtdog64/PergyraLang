static const char *
intent_observability_zero_export(BuiltinKind bk)
{
    switch (bk) {
    case BUILTIN_INTENT_LAST_TRACE:
        return "pgy_intent_last_trace_export";
    case BUILTIN_INTENT_LAST_FAILURE:
        return "pgy_intent_last_failure_export";
    case BUILTIN_INTENT_LAST_NAME:
        return "pgy_intent_last_name_export";
    case BUILTIN_INTENT_LAST_HANDLE:
        return "pgy_intent_last_handle_export";
    case BUILTIN_INTENT_LAST_TRACE_ID:
        return "pgy_intent_last_trace_id_export";
    case BUILTIN_INTENT_LAST_STEP_COUNT:
        return "pgy_intent_last_step_count_export";
    case BUILTIN_INTENT_LAST_FAILED:
        return "pgy_intent_last_failed_export";
    case BUILTIN_INTENT_HISTORY_COUNT:
        return "pgy_intent_history_count_export";
    case BUILTIN_INTENT_ACTIVE_COUNT:
        return "pgy_intent_active_count_export";
    case BUILTIN_INTENT_CURRENT_HANDLE:
        return "pgy_intent_current_handle_export";
    case BUILTIN_INTENT_RECENT_COUNT:
        return "pgy_intent_recent_count_export";
    default:
        return NULL;
    }
}

static const char *
intent_observability_one_arg_export(BuiltinKind bk)
{
    switch (bk) {
    case BUILTIN_INTENT_HISTORY_STEP_NAME:
        return "pgy_intent_history_step_name_export";
    case BUILTIN_INTENT_HISTORY_STEP_ZONE:
        return "pgy_intent_history_step_zone_export";
    case BUILTIN_INTENT_HISTORY_STEP_PHASE:
        return "pgy_intent_history_step_phase_export";
    case BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT:
        return "pgy_intent_history_step_participant_export";
    case BUILTIN_INTENT_HISTORY_STEP_SLOT:
        return "pgy_intent_history_step_slot_export";
    case BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE:
        return "pgy_intent_history_step_from_zone_export";
    case BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT:
        return "pgy_intent_history_step_from_slot_export";
    case BUILTIN_INTENT_HISTORY_STEP_TO_ZONE:
        return "pgy_intent_history_step_to_zone_export";
    case BUILTIN_INTENT_HISTORY_STEP_TO_SLOT:
        return "pgy_intent_history_step_to_slot_export";
    case BUILTIN_INTENT_HISTORY_STEP_OK:
        return "pgy_intent_history_step_ok_export";
    case BUILTIN_INTENT_HISTORY_STEP_FAILURE:
        return "pgy_intent_history_step_failure_export";
    case BUILTIN_INTENT_ACTIVE_NAME:
        return "pgy_intent_active_name_export";
    case BUILTIN_INTENT_ACTIVE_HANDLE:
        return "pgy_intent_active_handle_export";
    case BUILTIN_INTENT_ACTIVE_PARENT_HANDLE:
        return "pgy_intent_active_parent_handle_export";
    case BUILTIN_INTENT_ACTIVE_TRACE_ID:
        return "pgy_intent_active_trace_id_export";
    case BUILTIN_INTENT_ACTIVE_PRIORITY:
        return "pgy_intent_active_priority_export";
    case BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT:
        return "pgy_intent_active_subject_count_export";
    case BUILTIN_INTENT_ACTIVE_STEP_COUNT:
        return "pgy_intent_active_step_count_export";
    case BUILTIN_INTENT_ACTIVE_CONCURRENT:
        return "pgy_intent_active_concurrent_export";
    case BUILTIN_INTENT_ACTIVE_FAILED:
        return "pgy_intent_active_failed_export";
    case BUILTIN_INTENT_ACTIVE_FAILURE:
        return "pgy_intent_active_failure_export";
    case BUILTIN_INTENT_ACTIVE_TRACE:
        return "pgy_intent_active_trace_export";
    case BUILTIN_INTENT_RECENT_HANDLE:
        return "pgy_intent_recent_handle_export";
    case BUILTIN_INTENT_RECENT_TRACE_ID:
        return "pgy_intent_recent_trace_id_export";
    case BUILTIN_INTENT_RECENT_NAME:
        return "pgy_intent_recent_name_export";
    case BUILTIN_INTENT_RECENT_TRACE:
        return "pgy_intent_recent_trace_export";
    case BUILTIN_INTENT_RECENT_FAILURE:
        return "pgy_intent_recent_failure_export";
    case BUILTIN_INTENT_RECENT_STEP_COUNT:
        return "pgy_intent_recent_step_count_export";
    case BUILTIN_INTENT_RECENT_FAILED:
        return "pgy_intent_recent_failed_export";
    default:
        return NULL;
    }
}

static const char *
intent_observability_two_arg_export(BuiltinKind bk)
{
    switch (bk) {
    case BUILTIN_INTENT_ACTIVE_STEP_NAME:
        return "pgy_intent_active_step_name_export";
    case BUILTIN_INTENT_ACTIVE_STEP_ZONE:
        return "pgy_intent_active_step_zone_export";
    case BUILTIN_INTENT_ACTIVE_STEP_PHASE:
        return "pgy_intent_active_step_phase_export";
    case BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT:
        return "pgy_intent_active_step_participant_export";
    case BUILTIN_INTENT_ACTIVE_STEP_SLOT:
        return "pgy_intent_active_step_slot_export";
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE:
        return "pgy_intent_active_step_from_zone_export";
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT:
        return "pgy_intent_active_step_from_slot_export";
    case BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE:
        return "pgy_intent_active_step_to_zone_export";
    case BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT:
        return "pgy_intent_active_step_to_slot_export";
    case BUILTIN_INTENT_ACTIVE_STEP_OK:
        return "pgy_intent_active_step_ok_export";
    case BUILTIN_INTENT_ACTIVE_STEP_FAILURE:
        return "pgy_intent_active_step_failure_export";
    default:
        return NULL;
    }
}

static char *
emit_builtin_intent_observability(ASTNode *call, BuiltinKind bk,
                                  TranspilerCtx *ctx)
{
    const char *zero_export = intent_observability_zero_export(bk);
    const char *one_export = NULL;
    const char *two_export = NULL;

    if (ctx != NULL)
        ctx->uses_intent_observability = true;

    if (zero_export != NULL)
        return strdup_fmt("%s()", zero_export);

    one_export = intent_observability_one_arg_export(bk);
    if (one_export != NULL) {
        char *index = emit_expression(call->data.call.arguments[0], ctx);
        char *result = strdup_fmt("%s(%s)", one_export, index);
        free(index);
        return result;
    }

    two_export = intent_observability_two_arg_export(bk);
    if (two_export != NULL) {
        char *intent_index = emit_expression(call->data.call.arguments[0], ctx);
        char *step_index = emit_expression(call->data.call.arguments[1], ctx);
        char *result = strdup_fmt("%s(%s, %s)", two_export, intent_index,
                                  step_index);
        free(intent_index);
        free(step_index);
        return result;
    }

    return NULL;
}
