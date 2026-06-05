/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA expression emission seam.
 */

#include "transpiler_mir_expr_ssa.h"

char *
emit_expression_with_ssa_map(ASTNode *node,
                             TranspilerCtx *ctx,
                             const TranspilerSSANameMap *ssa_map)
{
    const void *saved_active_ssa_map;
    char *result;

    if (node == NULL)
        return NULL;
    if (ctx == NULL)
        return emit_expression(node, ctx);

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map;
    result = emit_expression(node, ctx);
    ctx->active_ssa_map = saved_active_ssa_map;
    return result;
}
