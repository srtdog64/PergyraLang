#ifndef PGY_SRC_CODEGEN_TRANSPILER_DEFER_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_DEFER_EMIT_H

#include "transpiler.h"

void transpiler_defer_scope_push(TranspilerCtx *ctx);
void transpiler_defer_scope_pop(TranspilerCtx *ctx);
void transpiler_register_defer(ASTNode *body, TranspilerCtx *ctx);
void transpiler_emit_defers_from(TranspilerCtx *ctx, int start_depth);

/* Active inout value-parameter tracking for copy-in / copy-out lowering. */
void transpiler_mut_ref_params_reset(TranspilerCtx *ctx);
void transpiler_register_mut_ref_param(TranspilerCtx *ctx, const char *name,
    const char *ctype);
void transpiler_emit_mut_ref_copyins(TranspilerCtx *ctx);
void transpiler_emit_mut_ref_writebacks(TranspilerCtx *ctx);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_DEFER_EMIT_H */
