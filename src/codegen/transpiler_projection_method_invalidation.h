#ifndef PGY_TRANSPILER_PROJECTION_METHOD_INVALIDATION_H
#define PGY_TRANSPILER_PROJECTION_METHOD_INVALIDATION_H

#include "../compiler/mir_decl.h"
#include "transpiler.h"

char *emit_current_overlay_method_projection_invalidation(
    TranspilerCtx *ctx,
    const char *source_slot_name,
    const char *host_type_name,
    const MIRDeclMethod *method_meta,
    ASTNode *method_decl);

#endif /* PGY_TRANSPILER_PROJECTION_METHOD_INVALIDATION_H */
