#ifndef PGY_TRANSPILER_OVERLAY_ZONE_BIND_H
#define PGY_TRANSPILER_OVERLAY_ZONE_BIND_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_find_zone_layer_slot_local(TranspilerCtx *ctx,
                                           ASTNode *zone,
                                           const char *layer_slot_name,
                                           bool is_relation,
                                           const char **layer_type_out,
                                           bool *is_pool_out);
void emit_zone_bind_effect_layer(CodeBuf *out,
                                 ASTNode *zone,
                                 const char *layer_slot_name,
                                 const char *target_slot_name,
                                 TranspilerCtx *ctx);

#endif
