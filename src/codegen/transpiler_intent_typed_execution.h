#ifndef PGY_TRANSPILER_INTENT_TYPED_EXECUTION_H
#define PGY_TRANSPILER_INTENT_TYPED_EXECUTION_H

#include "transpiler.h"
#include "transpiler_intent_context.h"

/* Last consumer for an admitted MIR typed-intent execution plan. */
bool transpiler_emit_typed_intent_execution(
    ASTNode *node,
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    const IntentBindingMetadataView *bindings);

#endif /* PGY_TRANSPILER_INTENT_TYPED_EXECUTION_H */
