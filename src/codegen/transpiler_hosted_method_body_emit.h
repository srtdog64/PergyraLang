#ifndef PGY_TRANSPILER_HOSTED_METHOD_BODY_EMIT_H
#define PGY_TRANSPILER_HOSTED_METHOD_BODY_EMIT_H

#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"

void transpiler_emit_hosted_methods_from_mir_or_error(
    const char *host_name,
    const char *anonymous_host_name,
    const char *host_kind,
    const TranspilerHostedMethodView *method_view,
    TranspilerCtx *ctx);

#endif /* PGY_TRANSPILER_HOSTED_METHOD_BODY_EMIT_H */
