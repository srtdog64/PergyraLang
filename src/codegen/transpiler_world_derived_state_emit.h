#ifndef PGY_TRANSPILER_WORLD_DERIVED_STATE_EMIT_H
#define PGY_TRANSPILER_WORLD_DERIVED_STATE_EMIT_H

#include <stdbool.h>

#include "../compiler/mir_decl_headers.h"
#include "transpiler_context.h"
#include "transpiler_projection.h"

bool transpiler_world_zone_slot_view_contains(
    const TranspilerHostedWorldZoneSlotView *view,
    const char *slot_name);

bool transpiler_emit_world_derived_state_pass(
    TranspilerCtx *ctx,
    const char *world_name,
    const MIRDeclHeader *world_header,
    ASTNode **states,
    size_t state_count,
    bool use_mir_world_states,
    const TranspilerHostedWorldZoneSlotView *zone_view);

#endif /* PGY_TRANSPILER_WORLD_DERIVED_STATE_EMIT_H */
