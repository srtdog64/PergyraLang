#ifndef PGY_INTENT_OBSERVABILITY_ABI_H
#define PGY_INTENT_OBSERVABILITY_ABI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum PgyIntentObservabilityReturnKind {
    PGY_INTENT_OBSERVABILITY_RETURN_INT,
    PGY_INTENT_OBSERVABILITY_RETURN_BOOL,
    PGY_INTENT_OBSERVABILITY_RETURN_STRING
} PgyIntentObservabilityReturnKind;

typedef enum PgyIntentObservabilityArgumentKind {
    PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID,
    PGY_INTENT_OBSERVABILITY_ARGUMENT_INT
} PgyIntentObservabilityArgumentKind;

typedef struct PgyIntentObservabilityAbiRow {
    /* Append-only identity: never derive this value from the sorted row index. */
    uint32_t runtime_call_abi_id;
    const char *source_name;
    const char *runtime_name;
    size_t arg_count;
    PgyIntentObservabilityReturnKind return_kind;
} PgyIntentObservabilityAbiRow;

size_t pgy_intent_observability_abi_row_count(void);
const PgyIntentObservabilityAbiRow *pgy_intent_observability_abi_row_at(
    size_t index);
const PgyIntentObservabilityAbiRow *pgy_intent_observability_abi_row_by_source(
    const char *source_name);
const char *pgy_intent_observability_return_type_name(
    PgyIntentObservabilityReturnKind kind);
PgyIntentObservabilityArgumentKind
pgy_intent_observability_argument_kind_at(
    const PgyIntentObservabilityAbiRow *row, size_t index);
const char *pgy_intent_observability_argument_type_name(
    PgyIntentObservabilityArgumentKind kind);
bool pgy_intent_observability_name_is_builtin(const char *name);

#endif /* PGY_INTENT_OBSERVABILITY_ABI_H */
