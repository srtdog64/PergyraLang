#ifndef PERGYRA_RESOURCE_FLOW_FACT_H
#define PERGYRA_RESOURCE_FLOW_FACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * A copied, function-scoped ResourceFlowUniverse row.  The semantic context
 * owns the live Symbol pointer; this carrier owns only stable identity and
 * declaration metadata so later IR stages do not recover flow identity from a
 * dead scope or allocation address.
 */
typedef struct
{
    uint32_t function_syntax_id;
    size_t   stable_index;
    uint32_t declaration_syntax_id;
    uint32_t line;
    uint32_t column;
    uint32_t symbol_kind;
    bool     is_parameter;
    size_t   parameter_index;
    char    *name;
} PgyResourceFlowFact;

void pgy_resource_flow_facts_destroy(PgyResourceFlowFact *facts,
                                     size_t count);

#endif /* PERGYRA_RESOURCE_FLOW_FACT_H */
