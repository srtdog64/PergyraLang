#include "intent_observability_names.h"

#include <stdlib.h>
#include <string.h>

static const char *const kIntentObservabilityNames[] = {
    "IntentActiveConcurrent",
    "IntentActiveCount",
    "IntentActiveFailed",
    "IntentActiveFailure",
    "IntentActiveHandle",
    "IntentActiveName",
    "IntentActiveParentHandle",
    "IntentActivePriority",
    "IntentActiveStepCount",
    "IntentActiveStepFailure",
    "IntentActiveStepFromSlot",
    "IntentActiveStepFromZone",
    "IntentActiveStepName",
    "IntentActiveStepOk",
    "IntentActiveStepParticipant",
    "IntentActiveStepPhase",
    "IntentActiveStepSlot",
    "IntentActiveStepToSlot",
    "IntentActiveStepToZone",
    "IntentActiveStepZone",
    "IntentActiveSubjectCount",
    "IntentActiveTrace",
    "IntentActiveTraceId",
    "IntentCurrentHandle",
    "IntentHistoryCount",
    "IntentHistoryStepFailure",
    "IntentHistoryStepFromSlot",
    "IntentHistoryStepFromZone",
    "IntentHistoryStepName",
    "IntentHistoryStepOk",
    "IntentHistoryStepParticipant",
    "IntentHistoryStepPhase",
    "IntentHistoryStepSlot",
    "IntentHistoryStepToSlot",
    "IntentHistoryStepToZone",
    "IntentHistoryStepZone",
    "IntentLastFailed",
    "IntentLastFailure",
    "IntentLastHandle",
    "IntentLastName",
    "IntentLastStepCount",
    "IntentLastTrace",
    "IntentLastTraceId",
    "IntentRecentCount",
    "IntentRecentFailed",
    "IntentRecentFailure",
    "IntentRecentHandle",
    "IntentRecentName",
    "IntentRecentStepCount",
    "IntentRecentTrace",
    "IntentRecentTraceId",
};

static int
intent_observability_name_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const char *const *candidate = (const char *const *)entry;

    return strcmp(name, *candidate);
}

bool
pgy_intent_observability_name_is_builtin(const char *name)
{
    if (name == NULL || strncmp(name, "Intent", 6) != 0)
        return false;

    return bsearch(name, kIntentObservabilityNames,
                   sizeof(kIntentObservabilityNames)
                       / sizeof(kIntentObservabilityNames[0]),
                   sizeof(kIntentObservabilityNames[0]),
                   intent_observability_name_compare) != NULL;
}
