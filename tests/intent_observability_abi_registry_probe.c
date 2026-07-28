#include "common/intent_observability_abi.h"

#include <stdio.h>
#include <string.h>

int
main(void)
{
    const PgyIntentObservabilityAbiRow *row;

    if (pgy_intent_observability_abi_row_count() != 51)
        return 1;
    row = pgy_intent_observability_abi_row_by_source("IntentHistoryCount");
    if (row == NULL || row->runtime_call_abi_id != 25 || row->arg_count != 0
        || row->return_kind != PGY_INTENT_OBSERVABILITY_RETURN_INT
        || strcmp(row->runtime_name, "pgy_intent_history_count_export") != 0)
        return 2;
    if (pgy_intent_observability_abi_row_by_source("Intent") != NULL
        || pgy_intent_observability_abi_row_by_source("IntentMissing") != NULL)
        return 3;
    puts("intent observability ABI registry probe: ok");
    return 0;
}
