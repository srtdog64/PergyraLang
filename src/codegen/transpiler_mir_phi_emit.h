#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_PHI_EMIT_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_PHI_EMIT_H

#include "transpiler.h"

bool transpiler_emit_mir_phi_copies(CodeBuf *buf,
                                    TranspilerCtx *ctx,
                                    int indent,
                                    size_t pred_block_index,
                                    const MIRBasicBlock *pred_block,
                                    const MIRBasicBlock *target_block);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_PHI_EMIT_H */
