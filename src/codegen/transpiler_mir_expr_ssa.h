/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR SSA expression emission seam.
 */

#ifndef PERGYRA_TRANSPILER_MIR_EXPR_SSA_H
#define PERGYRA_TRANSPILER_MIR_EXPR_SSA_H

#include "transpiler.h"
#include "transpiler_mir_ssa_map.h"

char *emit_expression_with_ssa_map(ASTNode *node,
                                   TranspilerCtx *ctx,
                                   const TranspilerSSANameMap *ssa_map);

#endif /* PERGYRA_TRANSPILER_MIR_EXPR_SSA_H */
