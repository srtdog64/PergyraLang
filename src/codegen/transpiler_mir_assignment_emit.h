#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_ASSIGNMENT_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_ASSIGNMENT_EMIT_H

#include "transpiler_mir_ssa_map.h"

typedef enum TranspilerMIRAssignmentEmitResult {
    TRANSPILE_MIR_ASSIGNMENT_NOT_HANDLED = 0,
    TRANSPILE_MIR_ASSIGNMENT_HANDLED,
    TRANSPILE_MIR_ASSIGNMENT_FAILED
} TranspilerMIRAssignmentEmitResult;

TranspilerMIRAssignmentEmitResult
transpiler_emit_mir_assignment_def_inst(CodeBuf *buf,
                                        const ASTNode *func_decl,
                                        const MIRRoutine *mir_routine,
                                        const MIRBasicBlock *block,
                                        const MIRInstruction *inst,
                                        size_t inst_index,
                                        ASTNode *stmt,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *ssa_map_out,
                                        char *reason,
                                        size_t reason_cap);

bool transpiler_emit_mir_assignment_expr_stmt(CodeBuf *buf,
                                             const MIRBasicBlock *block,
                                             const MIRInstruction *inst,
                                             ASTNode *stmt,
                                             TranspilerCtx *ctx,
                                             TranspilerSSANameMap *ssa_map_out,
                                             char *reason,
                                             size_t reason_cap);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_ASSIGNMENT_EMIT_H */
