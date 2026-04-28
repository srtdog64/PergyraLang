/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Intent observability builtin typing.
 */

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

Type *
type_check_intent_observability_builtin(ASTNode *call, BuiltinKind kind,
                                        SemanticContext *ctx,
                                        bool *handled_out)
{
    if (handled_out != NULL)
        *handled_out = true;

    switch (kind) {
    case BUILTIN_INTENT_LAST_TRACE:
        check_call_arity(call, 0, "IntentLastTrace", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_FAILURE:
        check_call_arity(call, 0, "IntentLastFailure", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_NAME:
        check_call_arity(call, 0, "IntentLastName", ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_LAST_HANDLE:
        check_call_arity(call, 0, "IntentLastHandle", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_TRACE_ID:
        check_call_arity(call, 0, "IntentLastTraceId", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_STEP_COUNT:
        check_call_arity(call, 0, "IntentLastStepCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_LAST_FAILED:
        check_call_arity(call, 0, "IntentLastFailed", ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_HISTORY_COUNT:
        check_call_arity(call, 0, "IntentHistoryCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_HISTORY_STEP_NAME:
        check_call_arity(call, 1, "IntentHistoryStepName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_PHASE:
        check_call_arity(call, 1, "IntentHistoryStepPhase", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT:
        check_call_arity(call, 1, "IntentHistoryStepParticipant", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepFromZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepFromSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_TO_ZONE:
        check_call_arity(call, 1, "IntentHistoryStepToZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_TO_SLOT:
        check_call_arity(call, 1, "IntentHistoryStepToSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_HISTORY_STEP_OK:
        check_call_arity(call, 1, "IntentHistoryStepOk", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_HISTORY_STEP_FAILURE:
        check_call_arity(call, 1, "IntentHistoryStepFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_COUNT:
        check_call_arity(call, 0, "IntentActiveCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_NAME:
        check_call_arity(call, 1, "IntentActiveName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_HANDLE:
        check_call_arity(call, 1, "IntentActiveHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_PARENT_HANDLE:
        check_call_arity(call, 1, "IntentActiveParentHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_TRACE_ID:
        check_call_arity(call, 1, "IntentActiveTraceId", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_PRIORITY:
        check_call_arity(call, 1, "IntentActivePriority", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT:
        check_call_arity(call, 1, "IntentActiveSubjectCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_STEP_COUNT:
        check_call_arity(call, 1, "IntentActiveStepCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_ACTIVE_CONCURRENT:
        check_call_arity(call, 1, "IntentActiveConcurrent", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_FAILED:
        check_call_arity(call, 1, "IntentActiveFailed", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_FAILURE:
        check_call_arity(call, 1, "IntentActiveFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_TRACE:
        check_call_arity(call, 1, "IntentActiveTrace", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_NAME:
        check_call_arity(call, 2, "IntentActiveStepName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_ZONE:
        check_call_arity(call, 2, "IntentActiveStepZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_PHASE:
        check_call_arity(call, 2, "IntentActiveStepPhase", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT:
        check_call_arity(call, 2, "IntentActiveStepParticipant", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_SLOT:
        check_call_arity(call, 2, "IntentActiveStepSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE:
        check_call_arity(call, 2, "IntentActiveStepFromZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT:
        check_call_arity(call, 2, "IntentActiveStepFromSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE:
        check_call_arity(call, 2, "IntentActiveStepToZone", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT:
        check_call_arity(call, 2, "IntentActiveStepToSlot", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_ACTIVE_STEP_OK:
        check_call_arity(call, 2, "IntentActiveStepOk", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_BOOL;
    case BUILTIN_INTENT_ACTIVE_STEP_FAILURE:
        check_call_arity(call, 2, "IntentActiveStepFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        require_assignable(type_check_expression(call->data.call.arguments[1], ctx),
            TYPE_INT, call->data.call.arguments[1], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_CURRENT_HANDLE:
        check_call_arity(call, 0, "IntentCurrentHandle", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_COUNT:
        check_call_arity(call, 0, "IntentRecentCount", ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_HANDLE:
        check_call_arity(call, 1, "IntentRecentHandle", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_TRACE_ID:
        check_call_arity(call, 1, "IntentRecentTraceId", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_NAME:
        check_call_arity(call, 1, "IntentRecentName", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_TRACE:
        check_call_arity(call, 1, "IntentRecentTrace", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_FAILURE:
        check_call_arity(call, 1, "IntentRecentFailure", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_STRING;
    case BUILTIN_INTENT_RECENT_STEP_COUNT:
        check_call_arity(call, 1, "IntentRecentStepCount", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_INT;
    case BUILTIN_INTENT_RECENT_FAILED:
        check_call_arity(call, 1, "IntentRecentFailed", ctx);
        require_assignable(type_check_expression(call->data.call.arguments[0], ctx),
            TYPE_INT, call->data.call.arguments[0], ctx);
        return TYPE_BOOL;
    default:
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }
}
