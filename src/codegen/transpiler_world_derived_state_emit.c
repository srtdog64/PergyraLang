#include "transpiler_world_derived_state_emit.h"

#include <string.h>

#include "../parser/ast_api.h"
#include "domain_frontier_policy.h"
#include "transpiler_domain_provenance_emit.h"

bool
transpiler_world_zone_slot_view_contains(
    const TranspilerHostedWorldZoneSlotView *view,
    const char *slot_name)
{
    if (view == NULL || slot_name == NULL)
        return false;
    for (size_t i = 0; i < view->count; i++) {
        const char *candidate =
            transpiler_hosted_world_zone_slot_view_name(view, i);
        if (candidate != NULL && strcmp(candidate, slot_name) == 0)
            return true;
    }
    return false;
}

bool
transpiler_emit_world_derived_state_pass(
    TranspilerCtx *ctx,
    const char *world_name,
    const MIRDeclHeader *world_header,
    ASTNode **states,
    size_t state_count,
    bool use_mir_world_states,
    const TranspilerHostedWorldZoneSlotView *zone_view)
{
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world derived pass */\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_world_needs_derived) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_world_pass = 0;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_world_pass_limit = %zu;\n",
        pgy_domain_world_derived_frontier_pass_limit_from_count(state_count));
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_continue = true;\n");
    write_indent(ctx);
    codebuf_write(ctx->out,
        "while (_pgy_world_continue && _pgy_world_pass < _pgy_world_pass_limit) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_continue = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_pass++;\n");
    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = NULL;
        const MIRDeclWorldState *state_meta = NULL;
        const char *state_name = NULL;
        const char *zone_slot_name = NULL;
        const char *detail_name = NULL;
        WorldStateSourceKind source_kind = WORLD_STATE_SOURCE_ZONE;
        size_t input_count = 0;
        if (use_mir_world_states) {
            state_meta = mir_decl_header_world_state(world_header, i);
            state_name = mir_decl_world_state_name(state_meta);
            zone_slot_name = mir_decl_world_state_zone_slot_name(state_meta);
            source_kind = mir_decl_world_state_source_kind(state_meta);
            detail_name = mir_decl_world_state_detail_name(state_meta);
            input_count = mir_decl_world_state_input_count(state_meta);
        } else {
            state = states[i];
            state_name = ast_world_state_name(state);
            zone_slot_name = ast_world_state_zone_slot_name(state);
            source_kind = ast_world_state_source_kind(state);
            detail_name = ast_world_state_detail_name(state);
            input_count = ast_world_state_input_count(state);
        }
        if (state_name == NULL
            || ((source_kind == WORLD_STATE_SOURCE_ZONE
                    || source_kind == WORLD_STATE_SOURCE_PROJECTION
                    || source_kind == WORLD_STATE_SOURCE_LAYER
                    || source_kind == WORLD_STATE_SOURCE_STATE)
                && zone_slot_name == NULL)
            || ((source_kind == WORLD_STATE_SOURCE_PROJECTION
                    || source_kind == WORLD_STATE_SOURCE_LAYER
                    || source_kind == WORLD_STATE_SOURCE_STATE)
                && detail_name == NULL)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path has incomplete world-state metadata for world '%s' state[%zu]",
                world_name != NULL ? world_name : "(anonymous-world)", i);
            return false;
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "bool _pgy_prev_zone_state_%s = self->__zone_state_%s;\n",
            state_name, state_name);
        write_indent(ctx);
        switch (source_kind) {
        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY: {
            bool first = true;
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                state_name);
            codebuf_write(ctx->out, "(");
            for (size_t input_i = 0;
                 input_i < input_count;
                 input_i++) {
                const char *input_name = use_mir_world_states
                    ? mir_decl_world_state_input_name(state_meta, input_i)
                    : ast_world_state_input_name(state, input_i);
                if (input_name == NULL) {
                    transpiler_set_mir_inventory_missing(
                        ctx,
                        "MIR-only C path has incomplete world-state input metadata for world '%s' state[%zu]",
                        world_name != NULL ? world_name : "(anonymous-world)",
                        i);
                    return false;
                }
                if (!first) {
                    codebuf_write(ctx->out,
                        source_kind == WORLD_STATE_SOURCE_ALL
                            ? " && " : " || ");
                }
                if (transpiler_world_zone_slot_view_contains(
                        zone_view, input_name)) {
                    codebuf_write(ctx->out, "self->__zone_active_%s",
                        input_name);
                } else {
                    codebuf_write(ctx->out, "self->__zone_state_%s",
                        input_name);
                }
                first = false;
            }
            if (first) {
                codebuf_write(ctx->out,
                    source_kind == WORLD_STATE_SOURCE_ALL
                        ? "true" : "false");
            }
            codebuf_write(ctx->out, ");\n");
            break;
        }
        case WORLD_STATE_SOURCE_PROJECTION:
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                state_name);
            codebuf_write(ctx->out,
                "(self->__zone_active_%s && self->%s.__projection_ready_%s)",
                zone_slot_name, zone_slot_name, detail_name);
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_LAYER:
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                state_name);
            codebuf_write(ctx->out,
                "(self->__zone_active_%s && self->%s.__layer_active_%s)",
                zone_slot_name, zone_slot_name, detail_name);
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_STATE:
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                state_name);
            codebuf_write(ctx->out,
                "(self->__zone_active_%s && self->%s.__state_%s)",
                zone_slot_name, zone_slot_name, detail_name);
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_ZONE:
        default:
            codebuf_write(ctx->out,
                "self->__zone_state_%s = self->__zone_active_%s;\n",
                state_name, zone_slot_name);
            break;
        }
        emit_hidden_provenance_stamp(ctx, "self", "zone_state",
            state_name, PGY_PROP_CAUSE_WORLD_DERIVED);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__zone_state_%s != _pgy_prev_zone_state_%s) {\n",
            state_name, state_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_continue = true;\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_derived_changed_any = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_world_continue) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_PANIC(\"%s\");\n",
        PGY_FRONTIER_REASON_WORLD_DERIVED_OVERFLOW);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "self->__world_derived_dirty = false;\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    return true;
}
