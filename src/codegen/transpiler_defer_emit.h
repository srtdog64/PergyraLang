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
/* True when `name` is the copy-in value local of a registered inout
 * parameter: the body sees a value, not the `<name>__mutref` pointer. */
bool transpiler_mut_ref_param_is_copy_in_local(const TranspilerCtx *ctx,
    const char *name);
void transpiler_emit_mut_ref_copyins(TranspilerCtx *ctx);
void transpiler_emit_mut_ref_writebacks(TranspilerCtx *ctx);
const char *transpiler_emit_mut_ref_return_capture(
    TranspilerCtx *ctx, const char *return_expr,
    char *temp_name, size_t temp_name_size);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_DEFER_EMIT_H */
