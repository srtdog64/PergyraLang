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
/*
 * `array_preserved` is the formal's Array storage verdict; zero is unproved.
 * DESCRIPTOR: the callee neither keeps, hands off nor releases the storage
 * the caller passed. ELEMENTS, set only with DESCRIPTOR: the call also
 * leaves the elements as owned as they were, and keeps none of them.
 */
#define PGY_FUNCTION_PARAM_ARRAY_DESCRIPTOR_PRESERVED 0x1u
#define PGY_FUNCTION_PARAM_ARRAY_ELEMENTS_PRESERVED   0x2u
#define PGY_FUNCTION_PARAM_ARRAY_PRESERVED_ALL        0x3u

typedef struct
{
    uint32_t function_syntax_id;
    size_t   parameter_index;
    uint32_t mask;
    uint32_t array_preserved;
} PgyFunctionParamFlowFact;

void pgy_function_param_flow_facts_destroy(PgyFunctionParamFlowFact *facts);

#endif /* PERGYRA_FUNCTION_PARAM_FLOW_FACT_H */
