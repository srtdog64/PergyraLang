#ifndef PGY_SRC_CODEGEN_TRANSPILER_ZONE_STRUCT_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_ZONE_STRUCT_EMIT_H

#include <stdbool.h>

#include "transpiler.h"

bool transpiler_emit_zone_struct_decl(TranspilerCtx *ctx,
                                      ASTNode *node,
                                      const char *name);
void transpiler_emit_zone_layer_accessors(TranspilerCtx *ctx,
                                          ASTNode *node,
                                          const char *name);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_ZONE_STRUCT_EMIT_H */
