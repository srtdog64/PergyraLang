#include "function_param_flow_summary_internal.h"

#include <stdint.h>
#include <stdlib.h>

void
pgy_function_param_flow_facts_destroy(PgyFunctionParamFlowFact *facts)
{
    free(facts);
}

bool
function_param_flow_summary_snapshot(SemanticContext *ctx)
{
    FunctionParamFlowSummaryStore *store;
    PgyFunctionParamFlowFact *facts;
    size_t count = 0;

    if (ctx == NULL)
        return false;
    store = ctx->function_param_flow_summaries;
    if (store == NULL)
        return true;
    if (store->failed)
        return false;
    if (store->count > SIZE_MAX / sizeof(*facts))
        return false;

    facts = store->count == 0
        ? NULL
        : calloc(store->count, sizeof(*facts));
    if (store->count != 0 && facts == NULL)
        return false;
    for (size_t i = 0; i < store->count; i++) {
        const FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (entry->state != FUNCTION_PARAM_FLOW_COMPLETE)
            continue;
        facts[count].function_syntax_id = entry->function_id;
        facts[count].parameter_index = entry->param_index;
        facts[count].mask = entry->mask;
        count++;
    }
    if (count == 0) {
        free(facts);
        facts = NULL;
    }
    ctx->function_param_flow_facts = facts;
    ctx->function_param_flow_fact_count = count;
    ctx->function_param_flow_fact_capacity = count;
    return true;
}
