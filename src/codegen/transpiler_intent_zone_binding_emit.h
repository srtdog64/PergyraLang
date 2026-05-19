#ifndef PGY_TRANSPILER_INTENT_ZONE_BINDING_EMIT_H
#define PGY_TRANSPILER_INTENT_ZONE_BINDING_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

void emit_intent_step_restore_bound_zone_aliases(CodeBuf *out,
                                                 TranspilerCtx *ctx,
                                                 ASTNode *intent,
                                                 ASTNode *step,
                                                 size_t step_index);
void emit_intent_step_restore_bound_zone_aliases_with_metadata(
    CodeBuf *out,
    TranspilerCtx *ctx,
    ASTNode *intent,
    const char *zone_type,
    const char **who_aliases,
    size_t who_alias_count,
    size_t step_index);
bool intent_action_has_only_self(ASTNode *action_decl);
void emit_intent_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx);
bool transpiler_can_forward_declare_intent_early(TranspilerCtx *ctx,
                                                 ASTNode *intent);

#endif /* PGY_TRANSPILER_INTENT_ZONE_BINDING_EMIT_H */
