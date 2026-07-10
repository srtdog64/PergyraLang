#include "intent_observability_abi.h"

#include <stdlib.h>
#include <string.h>

static const PgyIntentObservabilityAbiRow kIntentObservabilityAbiRows[] = {
    { 1, "IntentActiveConcurrent", "pgy_intent_active_concurrent_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_BOOL },
    { 2, "IntentActiveCount", "pgy_intent_active_count_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 3, "IntentActiveFailed", "pgy_intent_active_failed_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_BOOL },
    { 4, "IntentActiveFailure", "pgy_intent_active_failure_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 5, "IntentActiveHandle", "pgy_intent_active_handle_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 6, "IntentActiveName", "pgy_intent_active_name_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 7, "IntentActiveParentHandle", "pgy_intent_active_parent_handle_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 8, "IntentActivePriority", "pgy_intent_active_priority_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 9, "IntentActiveStepCount", "pgy_intent_active_step_count_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 10, "IntentActiveStepFailure", "pgy_intent_active_step_failure_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 11, "IntentActiveStepFromSlot", "pgy_intent_active_step_from_slot_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 12, "IntentActiveStepFromZone", "pgy_intent_active_step_from_zone_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 13, "IntentActiveStepName", "pgy_intent_active_step_name_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 14, "IntentActiveStepOk", "pgy_intent_active_step_ok_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_BOOL },
    { 15, "IntentActiveStepParticipant", "pgy_intent_active_step_participant_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 16, "IntentActiveStepPhase", "pgy_intent_active_step_phase_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 17, "IntentActiveStepSlot", "pgy_intent_active_step_slot_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 18, "IntentActiveStepToSlot", "pgy_intent_active_step_to_slot_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 19, "IntentActiveStepToZone", "pgy_intent_active_step_to_zone_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 20, "IntentActiveStepZone", "pgy_intent_active_step_zone_export", 2,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 21, "IntentActiveSubjectCount", "pgy_intent_active_subject_count_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 22, "IntentActiveTrace", "pgy_intent_active_trace_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 23, "IntentActiveTraceId", "pgy_intent_active_trace_id_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 24, "IntentCurrentHandle", "pgy_intent_current_handle_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 25, "IntentHistoryCount", "pgy_intent_history_count_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 26, "IntentHistoryStepFailure", "pgy_intent_history_step_failure_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 27, "IntentHistoryStepFromSlot", "pgy_intent_history_step_from_slot_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 28, "IntentHistoryStepFromZone", "pgy_intent_history_step_from_zone_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 29, "IntentHistoryStepName", "pgy_intent_history_step_name_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 30, "IntentHistoryStepOk", "pgy_intent_history_step_ok_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_BOOL },
    { 31, "IntentHistoryStepParticipant", "pgy_intent_history_step_participant_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 32, "IntentHistoryStepPhase", "pgy_intent_history_step_phase_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 33, "IntentHistoryStepSlot", "pgy_intent_history_step_slot_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 34, "IntentHistoryStepToSlot", "pgy_intent_history_step_to_slot_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 35, "IntentHistoryStepToZone", "pgy_intent_history_step_to_zone_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 36, "IntentHistoryStepZone", "pgy_intent_history_step_zone_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 37, "IntentLastFailed", "pgy_intent_last_failed_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_BOOL },
    { 38, "IntentLastFailure", "pgy_intent_last_failure_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 39, "IntentLastHandle", "pgy_intent_last_handle_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 40, "IntentLastName", "pgy_intent_last_name_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 41, "IntentLastStepCount", "pgy_intent_last_step_count_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 42, "IntentLastTrace", "pgy_intent_last_trace_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 43, "IntentLastTraceId", "pgy_intent_last_trace_id_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 44, "IntentRecentCount", "pgy_intent_recent_count_export", 0,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 45, "IntentRecentFailed", "pgy_intent_recent_failed_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_BOOL },
    { 46, "IntentRecentFailure", "pgy_intent_recent_failure_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 47, "IntentRecentHandle", "pgy_intent_recent_handle_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 48, "IntentRecentName", "pgy_intent_recent_name_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 49, "IntentRecentStepCount", "pgy_intent_recent_step_count_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
    { 50, "IntentRecentTrace", "pgy_intent_recent_trace_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_STRING },
    { 51, "IntentRecentTraceId", "pgy_intent_recent_trace_id_export", 1,
      PGY_INTENT_OBSERVABILITY_RETURN_INT },
};

static int
intent_observability_abi_row_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const PgyIntentObservabilityAbiRow *row =
        (const PgyIntentObservabilityAbiRow *)entry;

    return strcmp(name, row->source_name);
}

size_t
pgy_intent_observability_abi_row_count(void)
{
    return sizeof(kIntentObservabilityAbiRows)
        / sizeof(kIntentObservabilityAbiRows[0]);
}

const PgyIntentObservabilityAbiRow *
pgy_intent_observability_abi_row_at(size_t index)
{
    if (index >= pgy_intent_observability_abi_row_count())
        return NULL;
    return &kIntentObservabilityAbiRows[index];
}

const PgyIntentObservabilityAbiRow *
pgy_intent_observability_abi_row_by_source(const char *source_name)
{
    if (source_name == NULL || strncmp(source_name, "Intent", 6) != 0)
        return NULL;
    return (const PgyIntentObservabilityAbiRow *)bsearch(
        source_name,
        kIntentObservabilityAbiRows,
        pgy_intent_observability_abi_row_count(),
        sizeof(kIntentObservabilityAbiRows[0]),
        intent_observability_abi_row_compare);
}

const char *
pgy_intent_observability_return_type_name(
    PgyIntentObservabilityReturnKind kind)
{
    switch (kind) {
    case PGY_INTENT_OBSERVABILITY_RETURN_INT:
        return "Int";
    case PGY_INTENT_OBSERVABILITY_RETURN_BOOL:
        return "Bool";
    case PGY_INTENT_OBSERVABILITY_RETURN_STRING:
        return "String";
    }
    return NULL;
}

PgyIntentObservabilityArgumentKind
pgy_intent_observability_argument_kind_at(
    const PgyIntentObservabilityAbiRow *row, size_t index)
{
    if (row == NULL || index >= row->arg_count)
        return PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID;

    /* The current observability ABI uses Int handles and indexes only. */
    return PGY_INTENT_OBSERVABILITY_ARGUMENT_INT;
}

const char *
pgy_intent_observability_argument_type_name(
    PgyIntentObservabilityArgumentKind kind)
{
    switch (kind) {
    case PGY_INTENT_OBSERVABILITY_ARGUMENT_INT:
        return "Int";
    case PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID:
        break;
    }
    return NULL;
}

bool
pgy_intent_observability_name_is_builtin(const char *name)
{
    return pgy_intent_observability_abi_row_by_source(name) != NULL;
}
