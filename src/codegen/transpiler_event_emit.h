#ifndef PGY_SRC_CODEGEN_TRANSPILER_EVENT_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_EVENT_EMIT_H

#include "transpiler.h"

void emit_event_decl(ASTNode *node, TranspilerCtx *ctx);
void emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx);
void emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_EVENT_EMIT_H */
