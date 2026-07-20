#ifndef PGY_TRANSPILER_ZONE_DECL_EMIT_H
#define PGY_TRANSPILER_ZONE_DECL_EMIT_H

#include "transpiler.h"

void emit_zone_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_zone_decl_from_mir_header(const MIRDeclHeader *header,
                                    TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_ZONE_DECL_EMIT_H */
