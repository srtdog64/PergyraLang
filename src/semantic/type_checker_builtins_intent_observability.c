/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Intent observability builtin typing.
 */

#include <stddef.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

typedef enum
{
    INTENT_OBS_RETURN_INT,
    INTENT_OBS_RETURN_BOOL,
    INTENT_OBS_RETURN_STRING
} IntentObservabilityReturnKind;

typedef struct
{
    BuiltinKind kind;
    const char *name;
    size_t arity;
    IntentObservabilityReturnKind return_kind;
} IntentObservabilityBuiltinSpec;

static const IntentObservabilityBuiltinSpec intent_observability_specs[] = {
    { BUILTIN_INTENT_LAST_TRACE, "IntentLastTrace", 0, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_LAST_FAILURE, "IntentLastFailure", 0, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_LAST_NAME, "IntentLastName", 0, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_LAST_HANDLE, "IntentLastHandle", 0, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_LAST_TRACE_ID, "IntentLastTraceId", 0, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_LAST_STEP_COUNT, "IntentLastStepCount", 0, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_LAST_FAILED, "IntentLastFailed", 0, INTENT_OBS_RETURN_BOOL },
    { BUILTIN_INTENT_HISTORY_COUNT, "IntentHistoryCount", 0, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_HISTORY_STEP_NAME, "IntentHistoryStepName", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_ZONE, "IntentHistoryStepZone", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_PHASE, "IntentHistoryStepPhase", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_PARTICIPANT, "IntentHistoryStepParticipant", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_SLOT, "IntentHistoryStepSlot", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_FROM_ZONE, "IntentHistoryStepFromZone", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_FROM_SLOT, "IntentHistoryStepFromSlot", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_TO_ZONE, "IntentHistoryStepToZone", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_TO_SLOT, "IntentHistoryStepToSlot", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_HISTORY_STEP_OK, "IntentHistoryStepOk", 1, INTENT_OBS_RETURN_BOOL },
    { BUILTIN_INTENT_HISTORY_STEP_FAILURE, "IntentHistoryStepFailure", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_COUNT, "IntentActiveCount", 0, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_ACTIVE_NAME, "IntentActiveName", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_HANDLE, "IntentActiveHandle", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_ACTIVE_PARENT_HANDLE, "IntentActiveParentHandle", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_ACTIVE_TRACE_ID, "IntentActiveTraceId", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_ACTIVE_PRIORITY, "IntentActivePriority", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_ACTIVE_SUBJECT_COUNT, "IntentActiveSubjectCount", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_ACTIVE_STEP_COUNT, "IntentActiveStepCount", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_ACTIVE_CONCURRENT, "IntentActiveConcurrent", 1, INTENT_OBS_RETURN_BOOL },
    { BUILTIN_INTENT_ACTIVE_FAILED, "IntentActiveFailed", 1, INTENT_OBS_RETURN_BOOL },
    { BUILTIN_INTENT_ACTIVE_FAILURE, "IntentActiveFailure", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_TRACE, "IntentActiveTrace", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_NAME, "IntentActiveStepName", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_ZONE, "IntentActiveStepZone", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_PHASE, "IntentActiveStepPhase", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_PARTICIPANT, "IntentActiveStepParticipant", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_SLOT, "IntentActiveStepSlot", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_FROM_ZONE, "IntentActiveStepFromZone", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_FROM_SLOT, "IntentActiveStepFromSlot", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_TO_ZONE, "IntentActiveStepToZone", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_TO_SLOT, "IntentActiveStepToSlot", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_ACTIVE_STEP_OK, "IntentActiveStepOk", 2, INTENT_OBS_RETURN_BOOL },
    { BUILTIN_INTENT_ACTIVE_STEP_FAILURE, "IntentActiveStepFailure", 2, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_CURRENT_HANDLE, "IntentCurrentHandle", 0, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_RECENT_COUNT, "IntentRecentCount", 0, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_RECENT_HANDLE, "IntentRecentHandle", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_RECENT_TRACE_ID, "IntentRecentTraceId", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_RECENT_NAME, "IntentRecentName", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_RECENT_TRACE, "IntentRecentTrace", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_RECENT_FAILURE, "IntentRecentFailure", 1, INTENT_OBS_RETURN_STRING },
    { BUILTIN_INTENT_RECENT_STEP_COUNT, "IntentRecentStepCount", 1, INTENT_OBS_RETURN_INT },
    { BUILTIN_INTENT_RECENT_FAILED, "IntentRecentFailed", 1, INTENT_OBS_RETURN_BOOL },
};

static const IntentObservabilityBuiltinSpec *
intent_observability_lookup_spec(BuiltinKind kind)
{
    for (size_t i = 0;
         i < sizeof(intent_observability_specs) / sizeof(intent_observability_specs[0]);
         i++) {
        if (intent_observability_specs[i].kind == kind)
            return &intent_observability_specs[i];
    }
    return NULL;
}

static Type *
intent_observability_return_type(IntentObservabilityReturnKind kind)
{
    switch (kind) {
    case INTENT_OBS_RETURN_INT:
        return TYPE_INT;
    case INTENT_OBS_RETURN_BOOL:
        return TYPE_BOOL;
    case INTENT_OBS_RETURN_STRING:
        return TYPE_STRING;
    }
    return TYPE_UNKNOWN;
}

Type *
type_check_intent_observability_builtin(ASTNode *call, BuiltinKind kind,
                                        SemanticContext *ctx,
                                        bool *handled_out)
{
    const IntentObservabilityBuiltinSpec *spec =
        intent_observability_lookup_spec(kind);

    if (handled_out != NULL)
        *handled_out = spec != NULL;

    if (spec == NULL) {
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }

    check_call_arity(call, spec->arity, spec->name, ctx);
    for (size_t i = 0;
         call != NULL && i < spec->arity && i < ast_call_arg_count(call);
         i++) {
        ASTNode *arg = ast_call_argument(call, i);
        require_assignable(type_check_expression(arg, ctx), TYPE_INT, arg, ctx);
    }
    return intent_observability_return_type(spec->return_kind);
}
