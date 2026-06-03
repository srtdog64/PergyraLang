#ifndef PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H
#define PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H

#include "transpiler.h"

/* C backend intent signature and runtime entry emission owner. */

bool transpiler_emit_intent_signature_and_entry(ASTNode *node,
                                                TranspilerCtx *ctx,
                                                bool mir_only_intent,
                                                bool has_compensate_steps,
                                                size_t step_count,
                                                const char **participant_aliases,
                                                const char **participant_types,
                                                size_t participant_count,
                                                const char **value_aliases,
                                                const char **value_types,
                                                size_t mir_value_count,
                                                bool emit_cleanup_from_mir,
                                                const MIRRoutine *mir_routine);

#endif /* PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H */
