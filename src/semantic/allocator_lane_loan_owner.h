#ifndef PGY_ALLOCATOR_LANE_LOAN_OWNER_H
#define PGY_ALLOCATOR_LANE_LOAN_OWNER_H

#include <stdbool.h>

typedef struct SemanticContext SemanticContext;

/* A String finished with an allocator that is not a result lane is a loan
 * of that allocator (docs/truth/decisions/
 * 2026-10-19_finished_text_borrows_its_allocator.md). Runs once the
 * parameter-flow summaries are final and refuses each loan of a local
 * allocator that leaves the routine or may be read after the allocator is
 * destroyed. */
bool semantic_allocator_lane_loan_finalize(SemanticContext *ctx);

#endif
