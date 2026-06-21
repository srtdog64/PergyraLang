#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_STMT_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_STMT_EMIT_H

#include "transpiler.h"
#include "transpiler_mir_ssa_map.h"

bool transpiler_mir_stmt_is_mirrored_resource(TranspilerCtx *ctx,
                                              const MIRBasicBlock *block,
                                              const MIRInstruction *stmt_inst);

/* True when `resource_inst` has a paired MIR_INST_STMT with the same source
 * statement index inside the same `block`. Used to gate concrete C emission of
 * resource ops: when the paired stmt lives in a different block (e.g. the
 * SSA def-block carries a use-block's resource flow), the resource op must
 * stay observability-only so the actual runtime call fires exactly once,
 * inside the owning stmt block. */
bool transpiler_mir_resource_has_mirroring_stmt_in_block(
    const MIRBasicBlock *block,
    const MIRInstruction *resource_inst);

bool transpiler_emit_mir_call_statement(CodeBuf *buf,
                                        const MIRBasicBlock *block,
                                        const MIRInstruction *inst,
                                        ASTNode *stmt,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *ssa_map,
                                        char *reason,
                                        size_t reason_cap);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_STMT_EMIT_H */
