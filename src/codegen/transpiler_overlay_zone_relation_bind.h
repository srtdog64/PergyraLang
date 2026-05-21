#ifndef PGY_TRANSPILER_OVERLAY_ZONE_RELATION_BIND_H
#define PGY_TRANSPILER_OVERLAY_ZONE_RELATION_BIND_H

#include "transpiler.h"

void emit_zone_bind_relation_layer(CodeBuf *out,
                                   ASTNode *zone,
                                   const char *layer_slot_name,
                                   const char *left_slot_name,
                                   const char *right_slot_name,
                                   TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_OVERLAY_ZONE_RELATION_BIND_H */
