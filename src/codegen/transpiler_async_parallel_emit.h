#ifndef PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H
#define PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H

#include "transpiler.h"

void emit_parallel_block(ASTNode *node, TranspilerCtx *ctx);
void emit_async_block(ASTNode *node, TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_ASYNC_PARALLEL_EMIT_H */
