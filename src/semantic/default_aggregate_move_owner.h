#ifndef PGY_DEFAULT_AGGREGATE_MOVE_OWNER_H
#define PGY_DEFAULT_AGGREGATE_MOVE_OWNER_H

#include <stdbool.h>

typedef struct SemanticContext SemanticContext;

/* A default-mode aggregate argument is moved by a call whose callee changes
 * or lets out the parameter's storage
 * (docs/truth/decisions/2026-10-18_default_aggregate_move.md). Runs once the
 * parameter-flow summaries are final, and refuses each later use of a moved
 * access path until an assignment re-initializes it. */
bool semantic_default_aggregate_move_finalize(SemanticContext *ctx);

#endif
