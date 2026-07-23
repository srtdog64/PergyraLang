#ifndef PERGYRA_SEMANTIC_REGION_RETENTION_SUMMARY_H
#define PERGYRA_SEMANTIC_REGION_RETENTION_SUMMARY_H

/*
 * Semantic owner for the bounded callee-retention facts used by region escape
 * admission. Unknown or missing summaries are never treated as borrowed.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum PgyRegionRetentionKind {
    PGY_REGION_RETENTION_UNKNOWN = 0,
    PGY_REGION_RETENTION_BORROWED_FOR_CALL = 1
} PgyRegionRetentionKind;

struct ASTNode;

typedef bool (*PgyRegionRetentionSummaryLookup)(
    const struct ASTNode *call,
    size_t argument_index,
    PgyRegionRetentionKind *kind_out,
    void *userdata);

/* Return the owner-derived retention summary for one builtin argument. */
bool semantic_region_retention_summary_for_builtin(
    uint32_t builtin_kind,
    size_t argument_index,
    PgyRegionRetentionKind *kind_out);

/* Resolve one user-callee parameter through the semantic owner. */
bool semantic_region_retention_summary_for_user_call(
    const struct ASTNode *call,
    size_t argument_index,
    PgyRegionRetentionKind *kind_out,
    void *semantic_context);

#endif /* PERGYRA_SEMANTIC_REGION_RETENTION_SUMMARY_H */
