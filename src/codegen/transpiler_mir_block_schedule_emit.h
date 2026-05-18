#ifndef PERGYRA_TRANSPILER_MIR_BLOCK_SCHEDULE_EMIT_H
#define PERGYRA_TRANSPILER_MIR_BLOCK_SCHEDULE_EMIT_H

#include "transpiler.h"

bool transpiler_mir_block_has_source_order_metadata(
    const MIRBasicBlock *block);
bool transpiler_mir_block_build_source_order(const MIRBasicBlock *block,
                                             size_t **inst_order_out,
                                             char *reason,
                                             size_t reason_cap);
bool transpiler_emit_mir_claim_prepass(CodeBuf *buf,
                                       const MIRBasicBlock *block,
                                       TranspilerCtx *ctx,
                                       char *reason,
                                       size_t reason_cap);

#endif /* PERGYRA_TRANSPILER_MIR_BLOCK_SCHEDULE_EMIT_H */
