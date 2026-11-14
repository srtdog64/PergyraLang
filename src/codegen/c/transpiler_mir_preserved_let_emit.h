#ifndef PERGYRA_TRANSPILER_MIR_PRESERVED_LET_EMIT_H
#define PERGYRA_TRANSPILER_MIR_PRESERVED_LET_EMIT_H

#include "transpiler_mir_expr_ssa.h"

typedef enum TranspilerMIRLocalLetEmitResult {
    TRANSPILE_MIR_LOCAL_LET_NOT_HANDLED = 0,
    TRANSPILE_MIR_LOCAL_LET_HANDLED,
    TRANSPILE_MIR_LOCAL_LET_FAILED
} TranspilerMIRLocalLetEmitResult;

TranspilerMIRLocalLetEmitResult
transpiler_emit_mir_source_local_let_def_inst(
    CodeBuf *buf,
    const MIRRoutine *mir_routine,
    const MIRBasicBlock *block,
    const MIRInstruction *inst,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map_out,
    char *reason,
    size_t reason_cap);

#endif /* PERGYRA_TRANSPILER_MIR_PRESERVED_LET_EMIT_H */
