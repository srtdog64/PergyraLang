#ifndef PGY_SRC_CODEGEN_TRANSPILER_BLOCK_INTENT_REBIND_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_BLOCK_INTENT_REBIND_HELPERS_H

#include "transpiler.h"
#include "transpiler_intent_context.h"

bool emit_intent_step_rebind_bound_zone_aliases(CodeBuf *out,
                                                TranspilerCtx *ctx,
                                                ASTNode *intent,
                                                ASTNode *step,
                                                size_t step_index);
bool emit_intent_step_rebind_bound_zone_aliases_with_metadata(
    CodeBuf *out,
    TranspilerCtx *ctx,
    ASTNode *intent,
    const char *zone_type,
    const char *zone_alias,
    const char **who_aliases,
    size_t who_alias_count,
    size_t step_index,
    const IntentBindingMetadataView *bindings);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_BLOCK_INTENT_REBIND_HELPERS_H */
