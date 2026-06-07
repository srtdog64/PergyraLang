#include "transpiler_world_select_event_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "domain_frontier_policy.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_provenance_emit.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_hosted_method_body_emit.h"
#include "transpiler_projection.h"
#include "transpiler_type_require.h"

static size_t
transpiler_frontier_zone_member_count(void *ctx, const char *zone_name)
{
    TranspilerCtx *transpiler_ctx = (TranspilerCtx *)ctx;
    ASTNode *zone_decl;
    size_t state_count = 0;
    TranspilerHostedZoneLayerSlotView layer_view;

    if (transpiler_ctx == NULL || zone_name == NULL)
        return 0;

    zone_decl = transpiler_find_named_decl_local(
        transpiler_ctx, AST_ZONE_DECL, zone_name);
    if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
        return 0;

    (void)ast_zone_states(zone_decl, &state_count);
    layer_view = transpiler_hosted_zone_layer_slot_view_from_decl(
        transpiler_ctx, zone_name, zone_decl);
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(transpiler_ctx,
            "MIR-only C path missing embedded zone layer-slot metadata for world frontier '%s'",
            zone_name);
        return 0;
    }

    return pgy_frontier_embedded_zone_member_count(
        state_count, layer_view.count);
}

static const char *
transpiler_frontier_world_zone_type_name(void *ctx, size_t index)
{
    return transpiler_hosted_world_zone_slot_view_type_name(
        (const TranspilerHostedWorldZoneSlotView *)ctx,
        index);
}

void
emit_world_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;
    size_t embedded_frontier_count;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_named_decl_local(
        ctx, AST_WORLD_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;
    TranspilerHostedWorldRosterSlotView roster_view =
        transpiler_hosted_world_roster_slot_view_from_decl(ctx, name, node);
    size_t roster_count = roster_view.count;
    TranspilerHostedWorldZoneSlotView zone_view =
        transpiler_hosted_world_zone_slot_view_from_decl(ctx, name, node);
    size_t zone_count = zone_view.count;
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, name, node);
    if (transpiler_hosted_world_roster_slot_view_missing_mir_metadata(
            &roster_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing roster-slot declaration metadata for world '%s'",
            name != NULL ? name : "(anonymous-world)");
        return;
    }
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(&shared_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing shared field metadata for world '%s'",
            name != NULL ? name : "(anonymous-world)");
        return;
    }
    if (transpiler_hosted_world_zone_slot_view_missing_mir_metadata(
            &zone_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing zone-slot declaration metadata for world '%s'",
            name != NULL ? name : "(anonymous-world)");
        return;
    }
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for world '%s'",
            name != NULL ? name : "(anonymous-world)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for world '%s'",
            name != NULL ? name : "(anonymous-world)")) {
        return;
    }
    size_t state_count = 0;
    ASTNode **states = ast_world_states(node, &state_count);
    size_t activate_count = 0;
    ASTNode **activations = ast_world_activations(node, &activate_count);
    size_t maintained_zone_count = 0;
    ASTNode **maintained_zones =
        ast_world_maintained_zones(node, &maintained_zone_count);
    size_t deactivate_count = 0;
    ASTNode **deactivations = ast_world_deactivations(node, &deactivate_count);

    embedded_frontier_count =
        pgy_domain_world_embedded_frontier_count_from_zone_types(
            zone_view.count,
            transpiler_frontier_world_zone_type_name,
            &zone_view,
            transpiler_frontier_zone_member_count,
            ctx);
    if (ctx->backend_error != NULL)
        return;

    codebuf_write(ctx->out, "\n/* World: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < roster_count; i++) {
        const char *roster_type =
            transpiler_hosted_world_roster_slot_view_type_name(
                &roster_view, i);
        const char *slot_name =
            transpiler_hosted_world_roster_slot_view_name(&roster_view, i);
        if (roster_type == NULL || slot_name == NULL)
            continue;
        codebuf_write(ctx->out, "    %s %s;\n", roster_type, slot_name);
    }

    for (size_t i = 0; i < zone_count; i++) {
        const char *zone_type =
            transpiler_hosted_world_zone_slot_view_type_name(&zone_view, i);
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (zone_type == NULL || slot_name == NULL)
            continue;
        codebuf_write(ctx->out, "    %s %s;\n", zone_type, slot_name);
        codebuf_write(ctx->out, "    bool __zone_active_%s;\n", slot_name);
        codebuf_write(ctx->out, "    bool __zone_dirty_%s;\n", slot_name);
        codebuf_write(ctx->out, "    uint32_t __zone_seen_generation_%s;\n",
            slot_name);
        emit_hidden_provenance_fields(ctx, "zone", slot_name);
    }

    for (size_t i = 0; i < shared_view.count; i++) {
        const char *shared_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        ASTNode *shared_type =
            transpiler_hosted_shared_field_view_type(&shared_view, i);
        char ft[256];
        char surface_desc[256];
        if (shared_name == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C backend: world '%s' shared field[%zu] is missing declaration field metadata",
                name != NULL ? name : "(anonymous-world)",
                i);
            return;
        }
        snprintf(surface_desc, sizeof(surface_desc),
            "world shared field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            shared_name);
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                shared_type,
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared_name);
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        codebuf_write(ctx->out, "    bool __zone_state_%s;\n",
            ast_world_state_name(state));
        emit_hidden_provenance_fields(ctx, "zone_state",
            ast_world_state_name(state));
    }
    codebuf_write(ctx->out, "    bool __world_derived_dirty;\n");

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: reset */\n");
    for (size_t i = 0; i < zone_count; i++) {
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (slot_name == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "bool _pgy_prev_active_%s = self->__zone_active_%s;\n",
            slot_name, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_active_%s = false;\n",
            slot_name);
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: directives */\n");
    for (size_t i = 0; i < activate_count; i++) {
        ASTNode *act = activations[i];
        const char *slot_name = ast_world_directive_zone_slot_name(act);
        if (slot_name == NULL && ast_world_directive_state_name(act) != NULL) {
            ASTNode *state = transpiler_find_world_state_decl(
                node, ast_world_directive_state_name(act));
            if (state != NULL)
                slot_name = ast_world_state_zone_slot_name(state);
            else if (transpiler_world_has_zone_slot(
                         ctx, node, ast_world_directive_state_name(act))) {
                slot_name = ast_world_directive_state_name(act);
            }
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n",
                slot_name);
            emit_hidden_provenance_stamp(ctx, "self", "zone", slot_name,
                PGY_PROP_CAUSE_WORLD_ACTIVATE);
        }
    }
    for (size_t i = 0; i < maintained_zone_count; i++) {
        ASTNode *mnt = maintained_zones[i];
        const char *slot_name = ast_world_directive_zone_slot_name(mnt);
        if (slot_name == NULL && ast_world_directive_state_name(mnt) != NULL) {
            ASTNode *state = transpiler_find_world_state_decl(
                node, ast_world_directive_state_name(mnt));
            if (state != NULL)
                slot_name = ast_world_state_zone_slot_name(state);
            else if (transpiler_world_has_zone_slot(
                         ctx, node, ast_world_directive_state_name(mnt))) {
                slot_name = ast_world_directive_state_name(mnt);
            }
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n",
                slot_name);
            emit_hidden_provenance_stamp(ctx, "self", "zone", slot_name,
                PGY_PROP_CAUSE_WORLD_MAINTAIN);
        }
    }
    for (size_t i = 0; i < deactivate_count; i++) {
        ASTNode *act = deactivations[i];
        const char *slot_name = ast_world_directive_zone_slot_name(act);
        if (slot_name == NULL && ast_world_directive_state_name(act) != NULL) {
            ASTNode *state = transpiler_find_world_state_decl(
                node, ast_world_directive_state_name(act));
            if (state != NULL)
                slot_name = ast_world_state_zone_slot_name(state);
            else if (transpiler_world_has_zone_slot(
                         ctx, node, ast_world_directive_state_name(act))) {
                slot_name = ast_world_directive_state_name(act);
            }
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = false;\n",
                slot_name);
            emit_hidden_provenance_stamp(ctx, "self", "zone", slot_name,
                PGY_PROP_CAUSE_WORLD_DEACTIVATE);
        }
    }
    for (size_t i = 0; i < zone_count; i++) {
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (slot_name == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__zone_active_%s != _pgy_prev_active_%s) {\n",
            slot_name, slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_dirty_%s = true;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__world_derived_dirty = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_world_frontier_pass = 0;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_world_frontier_pass_limit = %zu;\n",
        pgy_domain_world_transitive_frontier_pass_limit_from_counts(
            zone_count, state_count, embedded_frontier_count));
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_frontier_continue = true;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_derived_changed_any = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out,
        "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_derived_changed_any = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_needs_derived = self->__world_derived_dirty;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_frontier_continue = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_frontier_pass++;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world zone sync pass */\n");
    for (size_t i = 0; i < zone_count; i++) {
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        const char *zone_type =
            transpiler_hosted_world_zone_slot_view_type_name(&zone_view, i);
        if (slot_name == NULL || zone_type == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (PGY_ZONE_GENERATION_LOAD(&self->%s) != self->__zone_seen_generation_%s) {\n",
            slot_name, slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_dirty_%s = true;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__world_derived_dirty = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__zone_dirty_%s) {\n", slot_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s_sync(&self->%s);\n", zone_type, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "self->__zone_seen_generation_%s = PGY_ZONE_GENERATION_LOAD(&self->%s);\n",
            slot_name, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_dirty_%s = false;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_needs_derived = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
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
        ASTNode *state = states[i];
        const char *expr_fmt = "self->__zone_active_%s";
        const char *detail_name = ast_world_state_detail_name(state);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "bool _pgy_prev_zone_state_%s = self->__zone_state_%s;\n",
            ast_world_state_name(state), ast_world_state_name(state));
        write_indent(ctx);
        switch (ast_world_state_source_kind(state)) {
        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY: {
            bool first = true;
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                ast_world_state_name(state));
            codebuf_write(ctx->out, "(");
            for (size_t input_i = 0;
                 input_i < ast_world_state_input_count(state);
                 input_i++) {
                const char *input_name =
                    ast_world_state_input_name(state, input_i);
                if (input_name == NULL)
                    continue;
                if (!first) {
                    codebuf_write(ctx->out,
                        ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ALL
                            ? " && " : " || ");
                }
                if (transpiler_world_has_zone_slot(ctx, node, input_name))
                    codebuf_write(ctx->out, "self->__zone_active_%s", input_name);
                else
                    codebuf_write(ctx->out, "self->__zone_state_%s", input_name);
                first = false;
            }
            if (first) {
                codebuf_write(ctx->out,
                    ast_world_state_source_kind(state) == WORLD_STATE_SOURCE_ALL
                        ? "true" : "false");
            }
            codebuf_write(ctx->out, ");\n");
            break;
        }
        case WORLD_STATE_SOURCE_PROJECTION:
            expr_fmt = "(self->__zone_active_%s && self->%s.__projection_ready_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                ast_world_state_name(state));
            codebuf_write(ctx->out, expr_fmt,
                ast_world_state_zone_slot_name(state),
                ast_world_state_zone_slot_name(state),
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_LAYER:
            expr_fmt = "(self->__zone_active_%s && self->%s.__layer_active_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                ast_world_state_name(state));
            codebuf_write(ctx->out, expr_fmt,
                ast_world_state_zone_slot_name(state),
                ast_world_state_zone_slot_name(state),
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_STATE:
            expr_fmt = "(self->__zone_active_%s && self->%s.__state_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                ast_world_state_name(state));
            codebuf_write(ctx->out, expr_fmt,
                ast_world_state_zone_slot_name(state),
                ast_world_state_zone_slot_name(state),
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_ZONE:
        default:
            codebuf_write(ctx->out,
                "self->__zone_state_%s = self->__zone_active_%s;\n",
                ast_world_state_name(state),
                ast_world_state_zone_slot_name(state));
            break;
        }
        emit_hidden_provenance_stamp(ctx, "self", "zone_state",
            ast_world_state_name(state), PGY_PROP_CAUSE_WORLD_DERIVED);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__zone_state_%s != _pgy_prev_zone_state_%s) {\n",
            ast_world_state_name(state), ast_world_state_name(state));
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
    write_indent(ctx);
    codebuf_write(ctx->out,
        "if (_pgy_world_derived_changed_any || self->__world_derived_dirty");
    for (size_t i = 0; i < zone_count; i++) {
        const char *slot_name =
            transpiler_hosted_world_zone_slot_view_name(&zone_view, i);
        if (slot_name == NULL)
            continue;
        codebuf_write(ctx->out, " || self->__zone_dirty_%s", slot_name);
    }
    codebuf_write(ctx->out, ") {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_frontier_continue = true;\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_world_frontier_continue) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_PANIC(\"%s\");\n",
        PGY_FRONTIER_REASON_WORLD_TRANSITIVE_OVERFLOW);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (transpiler_hosted_method_view_missing_mir_method_row(&method_view, i)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path has invalid method declaration metadata row for world '%s'",
                name != NULL ? name : "(anonymous-world)");
            return;
        }
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-world)",
        "world", &method_view, ctx);
}
