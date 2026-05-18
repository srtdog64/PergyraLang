#ifndef PGY_SRC_CODEGEN_TRANSPILER_DEFER_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_DEFER_EMIT_H

#include "transpiler.h"

void transpiler_defer_scope_push(TranspilerCtx *ctx);
void transpiler_defer_scope_pop(TranspilerCtx *ctx);
void transpiler_register_defer(ASTNode *body, TranspilerCtx *ctx);
void transpiler_emit_defers_from(TranspilerCtx *ctx, int start_depth);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_DEFER_EMIT_H */
