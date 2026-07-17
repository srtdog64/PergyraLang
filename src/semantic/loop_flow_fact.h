#ifndef PERGYRA_LOOP_FLOW_FACT_H
#define PERGYRA_LOOP_FLOW_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Immutable loop-transfer evidence exported after semantic scope teardown.
 * A state row is keyed by the function-local ResourceFlowUniverse stable
 * index; Symbol * pointers and AST recovery are deliberately absent.
 */
typedef struct
{
    size_t   stable_index;
    bool     is_consumed;
    bool     is_used;
    uint8_t  access_mask;
    int32_t  slot_state;
    int32_t  semantic_state;
    int32_t  pool_id;
} PgyLoopFlowStateFact;

typedef struct
{
    uint32_t function_syntax_id;
    uint32_t loop_syntax_id;
    uint32_t kind; /* 0 = while, 1 = for */
    uint32_t effect_base;
    uint32_t effect_delta;
    uint32_t flags;
    size_t   entry_state_start;
    size_t   entry_state_count;
    size_t   exit_state_start;
    size_t   exit_state_count;
} PgyLoopFlowSummaryFact;

#endif /* PERGYRA_LOOP_FLOW_FACT_H */
