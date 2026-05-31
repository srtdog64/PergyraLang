#include "transpiler_zone_decl_emit.h"

#include <stddef.h>
#include <string.h>

#include "domain_frontier_policy.h"
#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_provenance_emit.h"
#include "transpiler_overlay_zone_bind.h"
#include "transpiler_overlay_zone_relation_bind.h"
#include "transpiler_zone_specialization_emit.h"
#include "transpiler_zone_methods_emit.h"
#include "transpiler_zone_struct_emit.h"
#include "transpiler_zone_frontier_emit.h"

void
emit_zone_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_ZONE_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;

    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for zone '%s'",
            name != NULL ? name : "(anonymous-zone)");
        return;
    }

    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(node, &slot_count);
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, name, node);
    TranspilerHostedZoneLayerSlotView layer_view =
        transpiler_hosted_zone_layer_slot_view_from_decl(ctx, name, node);
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(node, &state_count);
    size_t refresh_count = 0;
    ASTNode **refreshes = ast_zone_refreshes(node, &refresh_count);
    size_t apply_count = 0;
    ASTNode **applies = ast_zone_applies(node, &apply_count);
    size_t link_count = 0;
    ASTNode **links = ast_zone_links(node, &link_count);
    size_t detach_count = 0;
    ASTNode **detaches = ast_zone_detaches(node, &detach_count);
    size_t unlink_count = 0;
    ASTNode **unlinks = ast_zone_unlinks(node, &unlink_count);
    size_t maintained_effect_count = 0;
    ASTNode **maintained_effects = ast_zone_maintained_effects(
        node, &maintained_effect_count);
    size_t maintained_relation_count = 0;
    ASTNode **maintained_relations = ast_zone_maintained_relations(
        node, &maintained_relation_count);
    size_t maintained_state_count = 0;
    ASTNode **maintained_states = ast_zone_maintained_states(
        node, &maintained_state_count);

    if (transpiler_hosted_shared_field_view_missing_mir_metadata(
            &shared_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing shared-field declaration metadata for zone '%s'",
            name != NULL ? name : "(anonymous-zone)");
        return;
    }
    if (transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing zone layer-slot declaration metadata for '%s'",
            name != NULL ? name : "(anonymous-zone)");
        return;
    }
    for (size_t i = 0; i < layer_view.count; i++) {
        ASTNode *slot =
            transpiler_hosted_zone_layer_slot_view_source_ast(
                &layer_view, i);
        const char *layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || layer_name == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing zone layer-slot source payload for '%s'",
                name != NULL ? name : "(anonymous-zone)");
            return;
        }
    }

    transpiler_emit_zone_required_specializations(ctx,
        slots, slot_count,
        &shared_view,
        &method_view);

    if (!transpiler_emit_zone_struct_decl(ctx, node, name))
        return;
    transpiler_emit_zone_layer_accessors(ctx, node, name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;

    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_ZONE_WRLOCK(self);\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_ZONE_GENERATION_INC(self);\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_zone_frontier_pass = 0;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_zone_frontier_pass_limit = %zu;\n",
        pgy_domain_zone_frontier_pass_limit_from_counts(
            state_count, layer_view.count));
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_zone_frontier_continue = true;\n");
    write_indent(ctx);
    codebuf_write(ctx->out,
        "while (_pgy_zone_frontier_continue && _pgy_zone_frontier_pass < _pgy_zone_frontier_pass_limit) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_zone_frontier_continue = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_zone_frontier_pass++;\n");

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_state_%s = self->__state_%s;\n",
            ast_zone_state_name(state),
            ast_zone_state_name(state));
    }
    for (size_t i = 0; i < layer_view.count; i++) {
        const char *layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (layer_name == NULL)
            continue;
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_layer_%s = self->__layer_active_%s;\n",
            layer_name,
            layer_name);
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = false;\n",
            ast_zone_state_name(state));
    }
    for (size_t i = 0; i < layer_view.count; i++) {
        ASTNode *slot =
            transpiler_hosted_zone_layer_slot_view_source_ast(
                &layer_view, i);
        const char *layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        if (layer_name == NULL)
            continue;
        if (ast_zone_layer_slot_is_pool(slot)) {
            write_indent(ctx);
            codebuf_write(ctx->out, "PGY_EFFECT_POOL_INIT(self->%s);\n",
                layer_name);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            layer_name);
    }

    emit_domain_projection_sync_loop(ctx,
        slots,
        slot_count,
        refreshes,
        refresh_count,
        "zone_projection",
        false);

    for (size_t i = 0; i < layer_view.count; i++) {
        ASTNode *slot =
            transpiler_hosted_zone_layer_slot_view_source_ast(
                &layer_view, i);
        const char *layer_name;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_is_relation(slot)
            || transpiler_hosted_zone_layer_slot_view_name(
                &layer_view, i) == NULL) {
            continue;
        }
        layer_name =
            transpiler_hosted_zone_layer_slot_view_name(&layer_view, i);
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__layer_cause_%s == %d) {\n",
            layer_name, PGY_PROP_CAUSE_ACTION);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n", layer_name);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (state == NULL || state->type != AST_ZONE_STATE
                || ast_zone_state_is_relation(state)
                || ast_zone_state_layer_slot_name(state) == NULL
                || ast_zone_state_name(state) == NULL
                || strcmp(ast_zone_state_layer_slot_name(state), layer_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                ast_zone_state_name(state));
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    for (size_t i = 0; i < apply_count; i++) {
        ASTNode *apply = applies[i];
        if (ast_zone_directive_state_name(apply) != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                ast_zone_directive_state_name(apply));
            emit_hidden_provenance_stamp(ctx, "self", "state",
                ast_zone_directive_state_name(apply), PGY_PROP_CAUSE_APPLY);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (!ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              ast_zone_directive_state_name(apply)) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        ast_zone_state_layer_slot_name(state));
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        ast_zone_state_layer_slot_name(state),
                        PGY_PROP_CAUSE_APPLY);
                    emit_zone_bind_effect_layer(ctx->out, node,
                        ast_zone_state_layer_slot_name(state),
                        ast_zone_state_left_or_target_slot_name(state), ctx);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            ast_zone_effect_slot_name(apply));
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            ast_zone_effect_slot_name(apply), PGY_PROP_CAUSE_APPLY);
        emit_zone_bind_effect_layer(ctx->out, node,
            ast_zone_effect_slot_name(apply),
            ast_zone_effect_target_slot_name(apply), ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (!ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          ast_zone_effect_slot_name(apply)) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          ast_zone_effect_target_slot_name(apply)) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    ast_zone_state_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    ast_zone_state_name(state), PGY_PROP_CAUSE_APPLY);
            }
        }
    }

    for (size_t i = 0; i < maintained_effect_count; i++) {
        ASTNode *maintain = maintained_effects[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            ast_zone_effect_slot_name(maintain));
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            ast_zone_effect_slot_name(maintain),
            PGY_PROP_CAUSE_MAINTAIN);
        emit_zone_bind_effect_layer(ctx->out, node,
            ast_zone_effect_slot_name(maintain),
            ast_zone_effect_target_slot_name(maintain), ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (!ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          ast_zone_effect_slot_name(maintain)) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          ast_zone_effect_target_slot_name(maintain)) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    ast_zone_state_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    ast_zone_state_name(state),
                    PGY_PROP_CAUSE_MAINTAIN);
            }
        }
    }

    for (size_t i = 0; i < maintained_state_count; i++) {
        ASTNode *maintain = maintained_states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = true;\n",
            ast_zone_directive_state_name(maintain));
        emit_hidden_provenance_stamp(ctx, "self", "state",
            ast_zone_directive_state_name(maintain),
            PGY_PROP_CAUSE_MAINTAIN);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (strcmp(ast_zone_state_name(state),
                       ast_zone_directive_state_name(maintain)) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                    ast_zone_state_layer_slot_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "layer",
                    ast_zone_state_layer_slot_name(state),
                    PGY_PROP_CAUSE_MAINTAIN);
                if (!ast_zone_state_is_relation(state)) {
                    emit_zone_bind_effect_layer(ctx->out, node,
                        ast_zone_state_layer_slot_name(state),
                        ast_zone_state_left_or_target_slot_name(state), ctx);
                } else {
                    emit_zone_bind_relation_layer(ctx->out, node,
                        ast_zone_state_layer_slot_name(state),
                        ast_zone_state_left_or_target_slot_name(state),
                        ast_zone_state_right_slot_name(state), ctx);
                }
            }
        }
    }

    for (size_t i = 0; i < detach_count; i++) {
        ASTNode *detach = detaches[i];
        if (ast_zone_directive_state_name(detach) != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                ast_zone_directive_state_name(detach));
            emit_hidden_provenance_stamp(ctx, "self", "state",
                ast_zone_directive_state_name(detach), PGY_PROP_CAUSE_DETACH);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (!ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              ast_zone_directive_state_name(detach)) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        ast_zone_state_layer_slot_name(state));
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        ast_zone_state_layer_slot_name(state),
                        PGY_PROP_CAUSE_DETACH);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            ast_zone_effect_slot_name(detach));
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            ast_zone_effect_slot_name(detach), PGY_PROP_CAUSE_DETACH);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (!ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          ast_zone_effect_slot_name(detach)) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          ast_zone_effect_target_slot_name(detach)) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    ast_zone_state_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    ast_zone_state_name(state),
                    PGY_PROP_CAUSE_DETACH);
            }
        }
    }

    for (size_t i = 0; i < link_count; i++) {
        ASTNode *link = links[i];
        if (ast_zone_directive_state_name(link) != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                ast_zone_directive_state_name(link));
            emit_hidden_provenance_stamp(ctx, "self", "state",
                ast_zone_directive_state_name(link), PGY_PROP_CAUSE_LINK);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              ast_zone_directive_state_name(link)) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        ast_zone_state_layer_slot_name(state));
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        ast_zone_state_layer_slot_name(state),
                        PGY_PROP_CAUSE_LINK);
                    emit_zone_bind_relation_layer(ctx->out, node,
                        ast_zone_state_layer_slot_name(state),
                        ast_zone_state_left_or_target_slot_name(state),
                        ast_zone_state_right_slot_name(state), ctx);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            ast_zone_relation_slot_name(link));
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            ast_zone_relation_slot_name(link), PGY_PROP_CAUSE_LINK);
        emit_zone_bind_relation_layer(ctx->out, node,
            ast_zone_relation_slot_name(link),
            ast_zone_relation_left_slot_name(link),
            ast_zone_relation_right_slot_name(link), ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          ast_zone_relation_slot_name(link)) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          ast_zone_relation_left_slot_name(link)) == 0
                && strcmp(ast_zone_state_right_slot_name(state),
                          ast_zone_relation_right_slot_name(link)) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    ast_zone_state_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    ast_zone_state_name(state), PGY_PROP_CAUSE_LINK);
            }
        }
    }

    for (size_t i = 0; i < maintained_relation_count; i++) {
        ASTNode *maintain = maintained_relations[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            ast_zone_relation_slot_name(maintain));
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            ast_zone_relation_slot_name(maintain),
            PGY_PROP_CAUSE_MAINTAIN);
        emit_zone_bind_relation_layer(ctx->out, node,
            ast_zone_relation_slot_name(maintain),
            ast_zone_relation_left_slot_name(maintain),
            ast_zone_relation_right_slot_name(maintain), ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          ast_zone_relation_slot_name(maintain)) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          ast_zone_relation_left_slot_name(maintain)) == 0
                && strcmp(ast_zone_state_right_slot_name(state),
                          ast_zone_relation_right_slot_name(maintain)) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    ast_zone_state_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    ast_zone_state_name(state),
                    PGY_PROP_CAUSE_MAINTAIN);
            }
        }
    }

    for (size_t i = 0; i < unlink_count; i++) {
        ASTNode *unlink = unlinks[i];
        if (ast_zone_directive_state_name(unlink) != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                ast_zone_directive_state_name(unlink));
            emit_hidden_provenance_stamp(ctx, "self", "state",
                ast_zone_directive_state_name(unlink), PGY_PROP_CAUSE_UNLINK);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              ast_zone_directive_state_name(unlink)) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        ast_zone_state_layer_slot_name(state));
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        ast_zone_state_layer_slot_name(state),
                        PGY_PROP_CAUSE_UNLINK);
                }
            }
            continue;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            ast_zone_relation_slot_name(unlink));
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            ast_zone_relation_slot_name(unlink), PGY_PROP_CAUSE_UNLINK);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          ast_zone_relation_slot_name(unlink)) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          ast_zone_relation_left_slot_name(unlink)) == 0
                && strcmp(ast_zone_state_right_slot_name(state),
                          ast_zone_relation_right_slot_name(unlink)) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    ast_zone_state_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    ast_zone_state_name(state),
                    PGY_PROP_CAUSE_UNLINK);
            }
        }
    }

    transpiler_emit_zone_frontier_change_checks(ctx,
        states, state_count, &layer_view);

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    transpiler_emit_zone_frontier_overflow_guard(ctx);

    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_ZONE_UNLOCK(self);\n");

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    transpiler_emit_zone_hosted_methods_bridge(name, &method_view, ctx);
}
