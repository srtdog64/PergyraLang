#ifndef PGY_TRANSPILER_INTENT_CLEANUP_EMIT_H
#define PGY_TRANSPILER_INTENT_CLEANUP_EMIT_H

#include "transpiler.h"

/* C backend intent cleanup / rollback tail emission owner. */

bool transpiler_emit_intent_cleanup_tail(ASTNode *node,
                                         TranspilerCtx *ctx,
                                         bool mir_only_intent,
                                         bool emit_cleanup_from_mir,
                                         bool has_compensate_steps,
                                         bool needs_cleanup_done_label,
                                         ASTNode **step_nodes,
                                         size_t step_count,
                                         const char **mir_step_names,
                                         const MIRRoutine *mir_routine,
                                         const char **participant_aliases,
                                         const char **participant_types,
                                         size_t participant_count);

#endif /* PGY_TRANSPILER_INTENT_CLEANUP_EMIT_H */
