#ifndef PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H
#define PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H

#include "transpiler.h"
#include "transpiler_mir_expr_ssa.h"

char *transpiler_mir_render_match_case_condition(
    ASTNode *func_decl,
    ASTNode *case_node,
    TranspilerCtx *ctx,
    const TranspilerSSANameMap *ssa_map);

#endif /* PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H */
