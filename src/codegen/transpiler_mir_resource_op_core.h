#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_OP_CORE_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_OP_CORE_H

#include "transpiler.h"

bool transpiler_emit_mir_resource_op(TranspilerCtx *ctx,
                                     CodeBuf *out,
                                     int indent,
                                     const MIRInstruction *inst,
                                     const MIRTypeLayout *layout,
                                     const char *ssa_result_name);

#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_RESOURCE_OP_CORE_H */
