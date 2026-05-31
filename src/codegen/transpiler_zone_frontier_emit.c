/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend bounded zone frontier guard emission.
 */

#include "transpiler_zone_frontier_emit.h"

#include "domain_frontier_policy.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"

void
transpiler_emit_zone_frontier_change_checks(TranspilerCtx *ctx,
                                            ASTNode **states,
                                            size_t state_count,
                                            const TranspilerHostedZoneLayerSlotView *layer_view)
{
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__state_%s != _pgy_prev_state_%s) {\n",
            ast_zone_state_name(state),
            ast_zone_state_name(state));
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_zone_frontier_continue = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    if (layer_view == NULL)
        return;
    for (size_t i = 0; i < layer_view->count; i++) {
        const char *slot_name =
            transpiler_hosted_zone_layer_slot_view_name(layer_view, i);
        if (slot_name == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__layer_active_%s != _pgy_prev_layer_%s) {\n",
            slot_name,
            slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_zone_frontier_continue = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

void
transpiler_emit_zone_frontier_overflow_guard(TranspilerCtx *ctx)
{
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_zone_frontier_continue) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "PGY_PANIC(\"%s\");\n", PGY_FRONTIER_REASON_ZONE_OVERFLOW);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}
