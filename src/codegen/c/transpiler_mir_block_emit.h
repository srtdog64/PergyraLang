#ifndef PGY_TRANSPILER_MIR_BLOCK_EMIT_H
#define PGY_TRANSPILER_MIR_BLOCK_EMIT_H

#include "transpiler_mir_ssa_map.h"

bool
transpiler_emit_mir_block_statements(CodeBuf *buf, const ASTNode *func_decl,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *out_ssa_map,
                                     char *reason,
                                     size_t reason_cap);
#endif /* PGY_TRANSPILER_MIR_BLOCK_EMIT_H */
