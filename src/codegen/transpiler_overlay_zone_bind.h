#ifndef PGY_TRANSPILER_OVERLAY_ZONE_BIND_H
#define PGY_TRANSPILER_OVERLAY_ZONE_BIND_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler.h"

ASTNode *find_nth_bindable_domain_slot_local(ASTNode **slots,
                                             size_t slot_count,
                                             ASTNode **refreshes,
                                             size_t refresh_count,
                                             size_t nth);
bool transpiler_find_zone_layer_slot_local(TranspilerCtx *ctx,
                                           ASTNode *zone,
                                           const char *layer_slot_name,
                                           bool is_relation,
                                           ASTNode **slot_out,
                                           const char **layer_type_out,
                                           bool *is_pool_out);
void emit_zone_bind_effect_layer(CodeBuf *out,
                                 ASTNode *zone,
                                 const char *layer_slot_name,
                                 const char *target_slot_name,
                                 TranspilerCtx *ctx);

#endif
