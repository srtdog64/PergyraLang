#ifndef PGY_TRANSPILER_WORLD_SELECT_EVENT_EMIT_H
#define PGY_TRANSPILER_WORLD_SELECT_EVENT_EMIT_H

#include "transpiler_context.h"

void emit_world_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_world_decl_from_mir_header(const MIRDeclHeader *header,
                                     TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_WORLD_SELECT_EVENT_EMIT_H */
