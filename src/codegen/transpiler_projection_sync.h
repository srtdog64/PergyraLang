#ifndef PGY_TRANSPILER_PROJECTION_SYNC_H
#define PGY_TRANSPILER_PROJECTION_SYNC_H

#include "transpiler.h"

void emit_zone_action_effect_runtime(CodeBuf *out, ASTNode *call,
                                     TranspilerCtx *ctx);
char *emit_world_embedded_action_effect_sync(TranspilerCtx *ctx,
                                             ASTNode *receiver,
                                             const MIRDeclMethod *method_meta);

#endif /* PGY_TRANSPILER_PROJECTION_SYNC_H */
