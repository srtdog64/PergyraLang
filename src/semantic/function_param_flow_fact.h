#ifndef PERGYRA_FUNCTION_PARAM_FLOW_FACT_H
#define PERGYRA_FUNCTION_PARAM_FLOW_FACT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Stable semantic snapshot of a demanded interprocedural parameter-flow row.
 * The semantic owner keeps the recursive hash store private; later stages
 * consume this immutable function/parameter identity instead of reopening a
 * callee body or joining by parameter spelling.
 */
typedef struct
{
    uint32_t function_syntax_id;
    size_t   parameter_index;
    uint32_t mask;
} PgyFunctionParamFlowFact;

void pgy_function_param_flow_facts_destroy(PgyFunctionParamFlowFact *facts);

#endif /* PERGYRA_FUNCTION_PARAM_FLOW_FACT_H */
