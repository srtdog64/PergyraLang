#ifndef PGY_TRANSPILER_MIR_PENDING_USES_H
#define PGY_TRANSPILER_MIR_PENDING_USES_H

#include "transpiler.h"
#include "transpiler_mir_ssa_map.h"

bool transpiler_materialize_pending_inst_uses(CodeBuf *buf,
                                              TranspilerCtx *ctx,
                                              const ASTNode *func_decl,
                                              const MIRBasicBlock *block,
                                              const MIRInstruction *inst,
                                              TranspilerSSANameMap *ssa_map_out,
                                              int indent,
                                              bool emit_assignments,
                                              char *reason,
                                              size_t reason_cap);

#endif /* PGY_TRANSPILER_MIR_PENDING_USES_H */
