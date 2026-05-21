#ifndef PGY_TRANSPILER_OVERLAY_PROJECTION_H
#define PGY_TRANSPILER_OVERLAY_PROJECTION_H

#include "transpiler.h"

char *emit_current_overlay_projection_invalidation(
    TranspilerCtx *ctx,
    const char *source_slot_name,
    const char *source_field_name);
char *emit_assignment_projection_invalidation(TranspilerCtx *ctx,
                                              ASTNode *target);
char *emit_world_embedded_assignment_sync(TranspilerCtx *ctx,
                                          ASTNode *target);
char *emit_world_embedded_receiver_projection_sync(TranspilerCtx *ctx,
                                                  ASTNode *receiver);

#endif /* PGY_TRANSPILER_OVERLAY_PROJECTION_H */
