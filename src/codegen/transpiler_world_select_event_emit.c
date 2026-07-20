#include "transpiler_world_select_event_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "domain_frontier_policy.h"
#include "domain_frontier_graph.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_provenance_emit.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_hosted_method_body_emit.h"
#include "transpiler_projection.h"
#include "transpiler_type_require.h"
#include "transpiler_world_derived_state_emit.h"
#include "transpiler_world_frontier_inputs.h"

static const char *
transpiler_world_resolve_mir_directive_slot(
    TranspilerCtx *ctx,
    const MIRDeclHeader *header,
    const TranspilerHostedWorldZoneSlotView *zone_view,
    const MIRDeclWorldDirective *directive,
    const char *world_name)
{
    const char *slot_name;
    const char *state_name;

    if (header == NULL || zone_view == NULL || directive == NULL)
        return NULL;
    slot_name = mir_decl_world_directive_zone_slot_name(directive);
    state_name = mir_decl_world_directive_state_name(directive);
    if (slot_name == NULL && state_name != NULL) {
        for (size_t i = 0; i < mir_decl_header_world_state_count(header); i++) {
            const MIRDeclWorldState *state =
                mir_decl_header_world_state(header, i);
            const char *state_decl_name = mir_decl_world_state_name(state);
            if (state_decl_name != NULL
                && strcmp(state_decl_name, state_name) == 0) {
                slot_name = mir_decl_world_state_zone_slot_name(state);
                break;
            }
        }
        if (slot_name == NULL
            && transpiler_world_zone_slot_view_contains(zone_view,
                state_name)) {
            slot_name = state_name;
        }
    }
    if (slot_name == NULL
        || !transpiler_world_zone_slot_view_contains(zone_view, slot_name)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path world '%s' directive has no validated zone slot",
            world_name != NULL ? world_name : "(anonymous-world)");
        return NULL;
    }
    return slot_name;
}

void
transpiler_emit_world_decl_impl(ASTNode *node,
                                const MIRDeclHeader *header,
                                const char *name,
                                TranspilerCtx *ctx)
{
    size_t embedded_frontier_count;

    if (name == NULL || node == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx, "C world emitter received incomplete declaration dispatch");
        return;
    }
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
    const MIRDeclHeader *world_header = header != NULL
        ? header : transpiler_active_decl_header_of_type(
            ctx, AST_WORLD_DECL, name);
    bool use_mir_world_states = transpiler_active_has_mir(ctx);
    if (use_mir_world_states && world_header == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration header for world '%s'",
            name != NULL ? name : "(anonymous-world)");
        return;
    }
    size_t state_count = 0;
    ASTNode **states = NULL;
    if (use_mir_world_states) {
        state_count = mir_decl_header_world_state_count(world_header);
        if (state_count != mir_decl_header_world_state_declared_count(
                world_header)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path world '%s' has inconsistent world-state metadata count",
                name != NULL ? name : "(anonymous-world)");
            return;
        }
    } else {
        states = ast_world_states(node, &state_count);
    }
    size_t activate_count = 0;
    ASTNode **activations = NULL;
    size_t maintained_zone_count = 0;
    ASTNode **maintained_zones = NULL;
    size_t deactivate_count = 0;
    ASTNode **deactivations = NULL;
    size_t directive_count = 0;
    if (use_mir_world_states) {
        directive_count = mir_decl_header_world_directive_count(world_header);
        if (directive_count
            != mir_decl_header_world_directive_declared_count(world_header)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path world '%s' has inconsistent directive metadata count",
                name != NULL ? name : "(anonymous-world)");
            return;
        }
        if (directive_count > 0) {
            if (mir_decl_header_world_directive(world_header, 0) == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path world '%s' has no directive metadata storage",
                    name != NULL ? name : "(anonymous-world)");
                return;
            }
        }
    } else {
        activations = ast_world_activations(node, &activate_count);
        maintained_zones = ast_world_maintained_zones(
            node, &maintained_zone_count);
        deactivations = ast_world_deactivations(node, &deactivate_count);
    }

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
        const MIRDeclField *shared_meta =
            transpiler_hosted_shared_field_view_metadata(&shared_view, i);
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
        {
            const char *shared_type_name =
                transpiler_mir_decl_field_type_name(shared_meta);
            if (shared_type_name != NULL) {
                if (!transpiler_require_type_name_c_type_copy(
                        ctx, shared_type_name, surface_desc, ft, sizeof(ft)))
                    return;
            } else if (shared_view.requires_mir_metadata) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing world shared-field type-name metadata for '%s.%s'",
                    name != NULL ? name : "(anonymous-world)",
                    shared_name);
                return;
            } else if (!transpiler_require_ast_c_type_copy(
                    ctx,
                    transpiler_hosted_shared_field_view_type(&shared_view, i),
                    surface_desc,
                    ft,
                    sizeof(ft))) {
                return;
            }
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared_name);
    }

    for (size_t i = 0; i < state_count; i++) {
        const char *state_name;
        if (use_mir_world_states) {
            const MIRDeclWorldState *state_meta =
                mir_decl_header_world_state(world_header, i);
            state_name = mir_decl_world_state_name(state_meta);
            if (state_name == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing world-state name metadata for world '%s'",
                    name != NULL ? name : "(anonymous-world)");
                return;
            }
        } else {
            ASTNode *state = states[i];
            state_name = ast_world_state_name(state);
        }
        if (state_name == NULL) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "C backend: world '%s' state[%zu] is missing a name",
                name != NULL ? name : "(anonymous-world)", i);
            return;
        }
        codebuf_write(ctx->out, "    bool __zone_state_%s;\n",
            state_name);
        emit_hidden_provenance_fields(ctx, "zone_state",
            state_name);
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
    if (use_mir_world_states) {
        for (size_t i = 0; i < directive_count; i++) {
            const MIRDeclWorldDirective *directive =
                mir_decl_header_world_directive(world_header, i);
            const char *slot_name =
                transpiler_world_resolve_mir_directive_slot(
                    ctx, world_header, &zone_view, directive, name);
            bool active;
            int cause;
            if (directive == NULL || slot_name == NULL)
                return;
            switch (mir_decl_world_directive_kind(directive)) {
            case MIR_DECL_WORLD_DIRECTIVE_ACTIVATE:
                active = true;
                cause = PGY_PROP_CAUSE_WORLD_ACTIVATE;
                break;
            case MIR_DECL_WORLD_DIRECTIVE_MAINTAIN:
                active = true;
                cause = PGY_PROP_CAUSE_WORLD_MAINTAIN;
                break;
            case MIR_DECL_WORLD_DIRECTIVE_DEACTIVATE:
                active = false;
                cause = PGY_PROP_CAUSE_WORLD_DEACTIVATE;
                break;
            default:
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path world '%s' has unknown directive kind",
                    name != NULL ? name : "(anonymous-world)");
                return;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = %s;\n",
                slot_name, active ? "true" : "false");
            emit_hidden_provenance_stamp(ctx, "self", "zone", slot_name,
                cause);
        }
    } else {
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
        pgy_codegen_world_frontier_graph_pass_limit(node,
            name,
            pgy_domain_world_transitive_frontier_pass_limit_from_counts(
                zone_count, state_count, embedded_frontier_count)));
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
    if (!transpiler_emit_world_derived_state_pass(
            ctx, name, world_header, states, state_count,
            use_mir_world_states, &zone_view)) {
        return;
    }
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
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for world '%s'",
                name != NULL ? name : "(anonymous-world)");
            return;
        }
        if (method_meta == NULL) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            NULL, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-world)",
        "world", &method_view, ctx);
}
