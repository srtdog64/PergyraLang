#ifndef PERGYRA_TRANSPILER_MIR_DESTRUCTURE_EMIT_H
#define PERGYRA_TRANSPILER_MIR_DESTRUCTURE_EMIT_H

#include <stdbool.h>
#include <stddef.h>

#include "transpiler_mir_expr_ssa.h"

bool transpiler_emit_mir_let_destructure_stmt(CodeBuf *buf,
                                              const MIRBasicBlock *block,
                                              const MIRInstruction *inst,
                                              TranspilerCtx *ctx,
                                              TranspilerSSANameMap *ssa_map_out,
                                              char *reason,
                                              size_t reason_cap);

#endif /* PERGYRA_TRANSPILER_MIR_DESTRUCTURE_EMIT_H */
