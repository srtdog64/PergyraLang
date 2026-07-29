/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone action-cause reactivation declarations.
 */

#ifndef PGY_TRANSPILER_ZONE_ACTION_CAUSE_EMIT_H
#define PGY_TRANSPILER_ZONE_ACTION_CAUSE_EMIT_H

#include "transpiler.h"
#include "transpiler_decl_lookup.h"

void transpiler_emit_zone_action_causes(
    TranspilerCtx *ctx,
    const TranspilerHostedZoneLayerSlotView *layer_view,
    const TranspilerHostedZoneStateView *state_view);

#endif /* PGY_TRANSPILER_ZONE_ACTION_CAUSE_EMIT_H */
