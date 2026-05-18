#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_OP_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_OP_EMIT_H

#include "transpiler_mir_ssa_map.h"

/* C backend MIR resource-op statement emission owner. */
typedef enum
{
    TRANSPILE_MIR_INST_NOT_HANDLED,
    TRANSPILE_MIR_INST_HANDLED,
    TRANSPILE_MIR_INST_FAILED
} TranspilerMIRInstEmitResult;

TranspilerMIRInstEmitResult
transpiler_emit_mir_resource_op_inst(CodeBuf *buf,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     const MIRInstruction *inst,
                                     size_t inst_index,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *ssa_map_out,
                                     char *reason,
                                     size_t reason_cap);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_OP_EMIT_H */
