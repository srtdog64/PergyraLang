/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend zone action-cause reactivation emission.
 */

#include "transpiler_zone_action_cause_emit.h"

#include <stddef.h>
#include <string.h>

#include "transpiler_context.h"
#include "transpiler_domain_provenance_emit.h"

void
transpiler_emit_zone_action_causes(
    TranspilerCtx *ctx,
    const TranspilerHostedZoneLayerSlotView *layer_view,
    const TranspilerHostedZoneStateView *state_view)
{
    for (size_t i = 0; i < layer_view->count; i++) {
        const char *layer_name;
        if (transpiler_hosted_zone_layer_slot_view_is_relation(layer_view, i)
            || transpiler_hosted_zone_layer_slot_view_name(
                layer_view, i) == NULL) {
            continue;
        }
        layer_name =
            transpiler_hosted_zone_layer_slot_view_name(layer_view, i);
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__layer_cause_%s == %d) {\n",
            layer_name, PGY_PROP_CAUSE_ACTION);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            layer_name);
        for (size_t j = 0; j < state_view->count; j++) {
            const char *state_name =
                transpiler_hosted_zone_state_view_name(state_view, j);
            const char *state_layer =
                transpiler_hosted_zone_state_view_layer_slot_name(
                    state_view, j);
            if (transpiler_hosted_zone_state_view_is_relation(state_view, j)
                || state_layer == NULL
                || state_name == NULL
                || strcmp(state_layer, layer_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                state_name);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}
