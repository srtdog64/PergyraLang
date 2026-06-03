#ifndef PGY_TRANSPILER_ZONE_SPECIALIZATION_EMIT_H
#define PGY_TRANSPILER_ZONE_SPECIALIZATION_EMIT_H

#include <stddef.h>

#include "transpiler.h"
#include "transpiler_decl_lookup.h"

void transpiler_emit_zone_required_specializations(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const TranspilerHostedSharedFieldView *shared_view,
    const TranspilerHostedMethodView *method_view);

#endif /* PGY_TRANSPILER_ZONE_SPECIALIZATION_EMIT_H */
