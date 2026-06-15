/*
 * Copyright (c) 2026 Pergyra Language Project
 * Small builtin registry for backend-facing facts.
 */

#include "transpiler_builtin_type_table.h"

#include "../common/intent_observability_names.h"

#include <stdlib.h>
#include <string.h>

static int
pgy_builtin_entry_compare(const void *key, const void *entry)
{
    const char *name = (const char *)key;
    const PgyBuiltinInfo *candidate = (const PgyBuiltinInfo *)entry;

    return strcmp(name, candidate->name);
}

static const PgyBuiltinInfo *
pgy_builtin_entries(size_t *count)
{
    static const PgyBuiltinInfo entries[] = {
        { "Acos", "Float", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorDebug", "Allocator", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorDestroy", "Void", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorPersistent", "Allocator", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorPool", "Allocator", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorResult", "Allocator", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorScratch", "Allocator", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorSystem", "Allocator", PGY_BUILTIN_FLAG_NONE },
        { "AllocatorTracing", "Allocator", PGY_BUILTIN_FLAG_NONE },
        { "Args", "Array<String>", PGY_BUILTIN_FLAG_NONE },
        { "ArrayPop", "Void", PGY_BUILTIN_FLAG_NONE },
        { "ArrayPush", "Void", PGY_BUILTIN_FLAG_NONE },
        { "ArraySet", "Void", PGY_BUILTIN_FLAG_NONE },
        { "Asin", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Atan", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Atan2", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Cancel", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "Ceil", "Float", PGY_BUILTIN_FLAG_NONE },
        { "ChannelCapacity", "Int", PGY_BUILTIN_FLAG_NONE },
        { "ChannelClose", "Void", PGY_BUILTIN_FLAG_NONE },
        { "ChannelClosed", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "ChannelFull", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "ChannelLength", "Int", PGY_BUILTIN_FLAG_NONE },
        { "ChannelReady", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "ChannelSpace", "Int", PGY_BUILTIN_FLAG_NONE },
        { "ClaimQubit", "QubitSlot", PGY_BUILTIN_FLAG_NONE },
        { "Concat", "String", PGY_BUILTIN_FLAG_NONE },
        { "Contains", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "CooldownNew", "Cooldown", PGY_BUILTIN_FLAG_NONE },
        { "Cos", "Float", PGY_BUILTIN_FLAG_NONE },
        { "DirWalk", "Array<String>", PGY_BUILTIN_FLAG_NONE },
        { "E", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Exit", "Void", PGY_BUILTIN_FLAG_NONE },
        { "Exp", "Float", PGY_BUILTIN_FLAG_NONE },
        { "FileClose", "Void", PGY_BUILTIN_FLAG_NONE },
        { "FileExists", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "FileOpen", "Int", PGY_BUILTIN_FLAG_NONE },
        { "FileRead", "String", PGY_BUILTIN_FLAG_NONE },
        { "FileWrite", "Void", PGY_BUILTIN_FLAG_NONE },
        { "Floor", "Float", PGY_BUILTIN_FLAG_NONE },
        { "FsmNew", "Fsm", PGY_BUILTIN_FLAG_NONE },
        { "H", "Void", PGY_BUILTIN_FLAG_NONE },
        { "HasLayer", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "HasProjection", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "HasState", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "HasZone", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "HasZoneLayer", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "HasZoneProjection", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "HasZoneState", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "Input", "String", PGY_BUILTIN_FLAG_NONE },
        { "IntentActiveConcurrent", "Bool", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveCount", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveFailed", "Bool", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveFailure", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveHandle", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveName", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveParentHandle", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActivePriority", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepCount", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepFailure", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepFromSlot", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepFromZone", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepName", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepOk", "Bool", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepParticipant", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepPhase", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepSlot", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepToSlot", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepToZone", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveStepZone", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveSubjectCount", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveTrace", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentActiveTraceId", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentCurrentHandle", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryCount", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepFailure", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepFromSlot", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepFromZone", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepName", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepOk", "Bool", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepParticipant", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepPhase", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepSlot", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepToSlot", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepToZone", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentHistoryStepZone", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentLastFailed", "Bool", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentLastFailure", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentLastHandle", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentLastName", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentLastStepCount", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentLastTrace", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentLastTraceId", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentCount", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentFailed", "Bool", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentFailure", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentHandle", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentName", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentStepCount", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentTrace", "String", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntentRecentTraceId", "Int", PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY },
        { "IntoClassical", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "IsCancelled", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "IsCollapsed", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "IsErr", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "IsNone", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "IsOk", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "IsSome", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "Join", "String", PGY_BUILTIN_FLAG_NONE },
        { "Length", "Int", PGY_BUILTIN_FLAG_NONE },
        { "ListNew", "List", PGY_BUILTIN_FLAG_NONE },
        { "ListSize", "Int", PGY_BUILTIN_FLAG_NONE },
        { "Log10", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Log2", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Lower", "String", PGY_BUILTIN_FLAG_NONE },
        { "MapHas", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "MapNew", "HashMap", PGY_BUILTIN_FLAG_NONE },
        { "MapSize", "Int", PGY_BUILTIN_FLAG_NONE },
        { "MathLog", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Measure", "Int", PGY_BUILTIN_FLAG_NONE },
        { "PI", "Float", PGY_BUILTIN_FLAG_NONE },
        { "Pow", "Float", PGY_BUILTIN_FLAG_NONE },
        { "QubitState", "Int", PGY_BUILTIN_FLAG_NONE },
        { "QueueEmpty", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "QueueNew", "Queue", PGY_BUILTIN_FLAG_NONE },
        { "QueueSize", "Int", PGY_BUILTIN_FLAG_NONE },
        { "Random", "Int", PGY_BUILTIN_FLAG_NONE },
        { "ReadFile", "String", PGY_BUILTIN_FLAG_NONE },
        { "Replace", "String", PGY_BUILTIN_FLAG_NONE },
        { "Round", "Float", PGY_BUILTIN_FLAG_NONE },
        { "SeedRandom", "Void", PGY_BUILTIN_FLAG_NONE },
        { "SendTimeout", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "SetHas", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "SetNew", "Set", PGY_BUILTIN_FLAG_NONE },
        { "SetSize", "Int", PGY_BUILTIN_FLAG_NONE },
        { "Sin", "Float", PGY_BUILTIN_FLAG_NONE },
        { "SliceCopy", "Array", PGY_BUILTIN_FLAG_NONE },
        { "Split", "Array<String>", PGY_BUILTIN_FLAG_NONE },
        { "Sqrt", "Float", PGY_BUILTIN_FLAG_NONE },
        { "StringConcat", "String", PGY_BUILTIN_FLAG_NONE },
        { "StringContains", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "StringIndexOf", "Int", PGY_BUILTIN_FLAG_NONE },
        { "StringJoin", "String", PGY_BUILTIN_FLAG_NONE },
        { "StringLength", "Int", PGY_BUILTIN_FLAG_NONE },
        { "StringReplace", "String", PGY_BUILTIN_FLAG_NONE },
        { "StringSplit", "Array<String>", PGY_BUILTIN_FLAG_NONE },
        { "StringTrim", "String", PGY_BUILTIN_FLAG_NONE },
        { "Substring", "String", PGY_BUILTIN_FLAG_NONE },
        { "Tan", "Float", PGY_BUILTIN_FLAG_NONE },
        { "TimerNew", "Timer", PGY_BUILTIN_FLAG_NONE },
        { "ToFloat", "Float", PGY_BUILTIN_FLAG_NONE },
        { "ToInt", "Int", PGY_BUILTIN_FLAG_NONE },
        { "ToLower", "String", PGY_BUILTIN_FLAG_NONE },
        { "ToString", "String", PGY_BUILTIN_FLAG_NONE },
        { "ToUpper", "String", PGY_BUILTIN_FLAG_NONE },
        { "Trim", "String", PGY_BUILTIN_FLAG_NONE },
        { "TrySend", "Bool", PGY_BUILTIN_FLAG_NONE },
        { "Upper", "String", PGY_BUILTIN_FLAG_NONE },
        { "WriteFile", "Void", PGY_BUILTIN_FLAG_NONE },
    };

    if (count != NULL)
        *count = sizeof(entries) / sizeof(entries[0]);
    return entries;
}

const PgyBuiltinInfo *
pgy_builtin_lookup(const char *name)
{
    size_t count;
    const PgyBuiltinInfo *entries;

    if (name == NULL)
        return NULL;

    entries = pgy_builtin_entries(&count);
    return (const PgyBuiltinInfo *)bsearch(
        name, entries, count, sizeof(entries[0]), pgy_builtin_entry_compare);
}

const char *
pgy_builtin_simple_return_type(const char *name)
{
    const PgyBuiltinInfo *entry = pgy_builtin_lookup(name);
    return entry != NULL ? entry->type_name : NULL;
}

bool
pgy_builtin_is_intent_observability(const char *name)
{
    const PgyBuiltinInfo *entry;

    if (!pgy_intent_observability_name_is_builtin(name))
        return false;

    entry = pgy_builtin_lookup(name);
    return entry != NULL
        && (entry->flags & PGY_BUILTIN_FLAG_INTENT_OBSERVABILITY) != 0;
}
