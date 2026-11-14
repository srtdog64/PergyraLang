#ifndef PGY_TRANSPILER_ROSTER_DECL_EMIT_H
#define PGY_TRANSPILER_ROSTER_DECL_EMIT_H

#include "transpiler_context.h"

void emit_roster_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_roster_decl_from_mir_header(const MIRDeclHeader *header,
                                      TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_ROSTER_DECL_EMIT_H */
