#ifndef PGY_TRANSPILER_MIR_CFG_CONTROL_EMIT_H
#define PGY_TRANSPILER_MIR_CFG_CONTROL_EMIT_H

#include <stdbool.h>

#include "transpiler.h"
#include "transpiler_mir_expr_ssa.h"

bool transpiler_mir_emit_for_loop_init_inst(
    CodeBuf *buf,
    const MIRInstruction *inst,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map);
bool transpiler_mir_emit_for_in_body_binding(
    CodeBuf *buf,
    const MIRRoutine *routine,
    const MIRBasicBlock *block,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map);
bool transpiler_mir_emit_loop_backedge_increment(CodeBuf *buf,
                                                 TranspilerCtx *ctx,
                                                 const MIRRoutine *routine,
                                                 const MIRBasicBlock *block);
char *transpiler_mir_render_branch_condition(
    ASTNode *func_decl,
    const MIRRoutine *routine,
    const MIRInstruction *inst,
    size_t target_block,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map);

#endif /* PGY_TRANSPILER_MIR_CFG_CONTROL_EMIT_H */
