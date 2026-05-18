#ifndef PERGYRA_TRANSPILER_MIR_RESOURCE_HOOK_EMIT_H
#define PERGYRA_TRANSPILER_MIR_RESOURCE_HOOK_EMIT_H

#include "transpiler.h"

bool transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                       CodeBuf *out,
                                       int indent,
                                       const MIRInstruction *inst,
                                       const char *handle_expr,
                                       bool cleanup_hook);

#endif /* PERGYRA_TRANSPILER_MIR_RESOURCE_HOOK_EMIT_H */
