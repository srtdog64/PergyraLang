#ifndef PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H
#define PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H

#include "transpiler.h"
#include "transpiler_intent_context.h"

/* C backend intent signature and runtime entry emission owner. */

bool transpiler_emit_intent_signature_and_entry(ASTNode *node,
                                                TranspilerCtx *ctx,
                                                bool has_compensate_steps,
                                                size_t step_count,
                                                const IntentBindingMetadataView *bindings,
                                                bool emit_cleanup_from_mir,
                                                const MIRRoutine *mir_routine);

#endif /* PGY_TRANSPILER_INTENT_PROLOGUE_EMIT_H */
