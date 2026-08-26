#include "common/intent_observability_abi.h"

#include <stdio.h>
#include <string.h>

int
main(void)
{
    const PgyIntentObservabilityAbiRow *row;
    size_t count = pgy_intent_observability_abi_row_count();

    if (count != 51 || pgy_intent_observability_abi_row_at(count) != NULL)
        return 1;
    row = pgy_intent_observability_abi_row_by_source("IntentHistoryCount");
    if (row == NULL || row->runtime_call_abi_id != 25
        || row->parameter_shape != PGY_INTENT_OBSERVABILITY_PARAMS_NONE
        || pgy_intent_observability_argument_count(row) != 0
        || pgy_intent_observability_argument_kind_at(row, 0)
            != PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID
        || row->return_kind != PGY_INTENT_OBSERVABILITY_RETURN_INT
        || strcmp(row->runtime_name, "pgy_intent_history_count_export") != 0)
        return 2;
    if (pgy_intent_observability_abi_row_by_source("Intent") != NULL
        || pgy_intent_observability_abi_row_by_source("IntentMissing") != NULL)
        return 3;
    if (pgy_intent_observability_abi_row_by_id(25) != row
        || pgy_intent_observability_abi_row_by_id(0) != NULL
        || pgy_intent_observability_abi_row_by_id(999) != NULL
        || pgy_intent_observability_abi_row_for_carried_identity(
            25, "IntentHistoryCount") != row
        || pgy_intent_observability_abi_row_for_carried_identity(
            25, "IntentActiveConcurrent") != NULL
        || pgy_intent_observability_abi_row_for_carried_identity(
            0, "IntentHistoryCount") != NULL) {
        return 8;
    }
    row = pgy_intent_observability_abi_row_by_source("IntentActiveConcurrent");
    if (row == NULL || row->parameter_shape != PGY_INTENT_OBSERVABILITY_PARAMS_INT
        || pgy_intent_observability_argument_count(row) != 1
        || pgy_intent_observability_argument_kind_at(row, 0)
            != PGY_INTENT_OBSERVABILITY_ARGUMENT_INT
        || pgy_intent_observability_argument_kind_at(row, 1)
            != PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID)
        return 4;
    row = pgy_intent_observability_abi_row_by_source("IntentActiveStepFailure");
    if (row == NULL
        || row->parameter_shape != PGY_INTENT_OBSERVABILITY_PARAMS_INT_INT
        || pgy_intent_observability_argument_count(row) != 2
        || pgy_intent_observability_argument_kind_at(row, 0)
            != PGY_INTENT_OBSERVABILITY_ARGUMENT_INT
        || pgy_intent_observability_argument_kind_at(row, 1)
            != PGY_INTENT_OBSERVABILITY_ARGUMENT_INT
        || pgy_intent_observability_argument_kind_at(row, 2)
            != PGY_INTENT_OBSERVABILITY_ARGUMENT_INVALID)
        return 5;
    for (size_t i = 0; i < count; i++) {
        const PgyIntentObservabilityAbiRow *current =
            pgy_intent_observability_abi_row_at(i);
        if (current == NULL || current->runtime_call_abi_id == 0)
            return 6;
        for (size_t j = 0; j < i; j++) {
            const PgyIntentObservabilityAbiRow *previous =
                pgy_intent_observability_abi_row_at(j);
            if (previous == NULL || previous->runtime_call_abi_id
                    == current->runtime_call_abi_id)
                return 7;
        }
    }
    puts("intent observability ABI registry probe: ok");
    return 0;
}
