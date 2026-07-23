#ifndef PERGYRA_SEMANTIC_REGION_ESCAPE_FACT_H
#define PERGYRA_SEMANTIC_REGION_ESCAPE_FACT_H

/*
 * Semantic owner for the bounded region-safe string allocation facts.
 *
 * The collector runs after type checking has annotated call targets. It emits
 * only stable identities and scope ownership; downstream IR and backends do
 * not walk the AST to recover a lifetime decision.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "region_retention_summary.h"

struct ASTNode;

typedef struct PgyRegionEscapeFact {
    uint32_t allocation_site_id;
    uint32_t scope_id;
    uint32_t function_syntax_id;
} PgyRegionEscapeFact;

/*
 * Collect the currently certified synchronous-consumer concat facts.
 * `facts_out` and `count_out` are always initialized on entry. A false return means the
 * semantic owner could not produce a complete fact set (invalid stable id or
 * allocation failure); callers must reject the fact set, not use a prefix.
 */
bool semantic_region_escape_collect(
    const struct ASTNode *root,
    PgyRegionRetentionSummaryLookup retention_lookup,
    void *retention_userdata,
    PgyRegionEscapeFact **facts_out,
    size_t *count_out);

void semantic_region_escape_facts_free(PgyRegionEscapeFact *facts);

#endif /* PERGYRA_SEMANTIC_REGION_ESCAPE_FACT_H */
