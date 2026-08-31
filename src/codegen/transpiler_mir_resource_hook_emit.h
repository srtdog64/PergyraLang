#ifndef PERGYRA_TRANSPILER_MIR_RESOURCE_HOOK_EMIT_H
#define PERGYRA_TRANSPILER_MIR_RESOURCE_HOOK_EMIT_H

#include "transpiler.h"

bool transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                       CodeBuf *out,
                                       int indent,
                                       const MIRBasicBlock *owning_block,
                                       const MIRInstruction *inst,
                                       const char *handle_expr,
                                       bool cleanup_hook);

bool transpiler_emit_mir_embedded_zone_local_guard_decl(
    TranspilerCtx *ctx,
    CodeBuf *out,
    int indent,
    const char *versioned_name,
    const char *type_name,
    const char *c_name);

bool transpiler_emit_mir_embedded_zone_local_init(
    TranspilerCtx *ctx,
    CodeBuf *out,
    int indent,
    const char *versioned_name,
    const char *type_name,
    const char *c_name);

bool transpiler_emit_mir_embedded_zone_local_cleanups(
    TranspilerCtx *ctx,
    CodeBuf *out,
    int indent);

#endif /* PERGYRA_TRANSPILER_MIR_RESOURCE_HOOK_EMIT_H */
