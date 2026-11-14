#ifndef PGY_TRANSPILER_ZONE_METHODS_EMIT_H
#define PGY_TRANSPILER_ZONE_METHODS_EMIT_H

#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"

void transpiler_emit_zone_hosted_methods_bridge(
    const char *name,
    const TranspilerHostedMethodView *method_view,
    TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_ZONE_METHODS_EMIT_H */
