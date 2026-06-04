#ifndef PGY_TRANSPILER_BLOCK_INTENT_HELPERS_H
#define PGY_TRANSPILER_BLOCK_INTENT_HELPERS_H

#include "transpiler.h"
#include "transpiler_block_intent_rebind_helpers.h"
#include "transpiler_intent_context.h"

void emit_intent_step_bind_bound_zone(CodeBuf *out, TranspilerCtx *ctx,
                                      ASTNode *intent, ASTNode *step);
void emit_intent_step_bind_bound_zone_with_metadata(
    CodeBuf *out,
    TranspilerCtx *ctx,
    ASTNode *intent,
    const char *zone_type,
    const char *zone_alias,
    const char *from_alias,
    const char **who_aliases,
    size_t who_alias_count,
    const IntentBindingMetadataView *bindings);
void emit_intent_step_mark_caused_effect(CodeBuf *out, TranspilerCtx *ctx,
                                         const char *zone_type,
                                         const char *zone_alias,
                                         const char *causes_effect);
void emit_intent_step_validate_authority(CodeBuf *out,
                                         TranspilerCtx *ctx,
                                         const char *intent_name,
                                         const char *step_name,
                                         const char *zone_type,
                                         const char *zone_alias,
                                         const char **authorized_aliases,
                                         size_t authorized_alias_count,
                                         bool emit_cleanup_from_mir,
                                         size_t cleanup_block);
void emit_intent_step_sync_effective_zone(CodeBuf *out, TranspilerCtx *ctx,
                                          ASTNode *step);
void emit_intent_step_sync_effective_zone_with_metadata(CodeBuf *out,
                                                        TranspilerCtx *ctx,
                                                        const char *zone_type,
                                                        const char *zone_alias);

#endif /* PGY_TRANSPILER_BLOCK_INTENT_HELPERS_H */
