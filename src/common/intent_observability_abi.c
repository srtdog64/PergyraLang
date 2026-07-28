#include "intent_observability_abi.h"

#include <stdlib.h>
#include <string.h>

static const PgyIntentObservabilityAbiRow kIntentObservabilityAbiRows[] = {
#define PGY_INTENT_OBSERVABILITY_ABI(abi_id, source, runtime, argc, result) \
    { abi_id, source, runtime, argc, result },
#include "intent_observability_abi.def"
#undef PGY_INTENT_OBSERVABILITY_ABI
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
