#ifndef PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H
#define PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H

#include "transpiler.h"
#include "transpiler_mir_expr_ssa.h"

char *transpiler_mir_render_match_case_condition(
    ASTNode *func_decl,
    const MIRInstruction *inst,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map);
bool transpiler_mir_emit_match_case_body_binding(
    CodeBuf *buf,
    ASTNode *func_decl,
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map);
bool transpiler_mir_remap_active_match_bindings(
    ASTNode *func_decl,
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map);

#endif /* PGY_TRANSPILER_MIR_MATCH_CONDITION_EMIT_H */
