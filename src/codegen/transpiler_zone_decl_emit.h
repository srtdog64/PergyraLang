#ifndef PGY_TRANSPILER_ZONE_DECL_EMIT_H
#define PGY_TRANSPILER_ZONE_DECL_EMIT_H

#include "transpiler_zone_struct_emit.h"
#include "transpiler_zone_specialization_emit.h"
#include "transpiler_zone_methods_emit.h"

void
emit_zone_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = ast_zone_name(node);
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
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
    size_t shared_count = 0;
    ASTNode **shared_fields = ast_zone_shared_fields(node, &shared_count);
    size_t state_count = 0;
    ASTNode **states = ast_zone_states(node, &state_count);
    size_t layer_slot_count = 0;
    ASTNode **layer_slots = ast_zone_layer_slots(node, &layer_slot_count);
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

    transpiler_emit_zone_required_specializations(ctx,
        slots, slot_count,
        shared_fields, shared_count,
        &method_view);

    if (!transpiler_emit_zone_struct_decl(ctx, node, name))
        return;
    transpiler_emit_zone_layer_accessors(ctx, node, name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;

    /* Acquire write lock + bump generation for stale-state detection */
    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_ZONE_WRLOCK(self);\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_ZONE_GENERATION_INC(self);\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_zone_frontier_pass = 0;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "size_t _pgy_zone_frontier_pass_limit = %zu;\n",
        pgy_domain_zone_frontier_pass_limit(node));
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
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_layer_%s = self->__layer_active_%s;\n",
            ast_zone_layer_slot_name(slot),
            ast_zone_layer_slot_name(slot));
    }

    for (size_t i = 0; i < state_count; i++) {
        ASTNode *state = states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = false;\n",
            ast_zone_state_name(state));
    }
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        if (ast_zone_layer_slot_is_pool(slot)) {
            write_indent(ctx);
            codebuf_write(ctx->out, "PGY_EFFECT_POOL_INIT(self->%s);\n",
                ast_zone_layer_slot_name(slot));
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            ast_zone_layer_slot_name(slot));
    }

    emit_domain_projection_sync_loop(ctx,
        slots,
        slot_count,
        refreshes,
        refresh_count,
        "zone_projection",
        false);

    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        const char *layer_name;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || ast_zone_layer_slot_is_relation(slot)
            || ast_zone_layer_slot_name(slot) == NULL) {
            continue;
        }
        layer_name = ast_zone_layer_slot_name(slot);
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
        if (apply->data.zone_apply.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                apply->data.zone_apply.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                apply->data.zone_apply.state_name, PGY_PROP_CAUSE_APPLY);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (!ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              apply->data.zone_apply.state_name) == 0) {
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
            apply->data.zone_apply.effect_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            apply->data.zone_apply.effect_slot_name, PGY_PROP_CAUSE_APPLY);
        emit_zone_bind_effect_layer(ctx->out, node,
            apply->data.zone_apply.effect_slot_name,
            apply->data.zone_apply.target_slot_name, ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (!ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          apply->data.zone_apply.effect_slot_name) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          apply->data.zone_apply.target_slot_name) == 0) {
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
            maintain->data.zone_maintain_effect.effect_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            maintain->data.zone_maintain_effect.effect_slot_name,
            PGY_PROP_CAUSE_MAINTAIN);
        emit_zone_bind_effect_layer(ctx->out, node,
            maintain->data.zone_maintain_effect.effect_slot_name,
            maintain->data.zone_maintain_effect.target_slot_name, ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (!ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          maintain->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          maintain->data.zone_maintain_effect.target_slot_name) == 0) {
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
            maintain->data.zone_maintain_state.state_name);
        emit_hidden_provenance_stamp(ctx, "self", "state",
            maintain->data.zone_maintain_state.state_name,
            PGY_PROP_CAUSE_MAINTAIN);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (strcmp(ast_zone_state_name(state),
                       maintain->data.zone_maintain_state.state_name) == 0) {
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
        if (detach->data.zone_detach.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                detach->data.zone_detach.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                detach->data.zone_detach.state_name, PGY_PROP_CAUSE_DETACH);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (!ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              detach->data.zone_detach.state_name) == 0) {
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
            detach->data.zone_detach.effect_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            detach->data.zone_detach.effect_slot_name, PGY_PROP_CAUSE_DETACH);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (!ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          detach->data.zone_detach.effect_slot_name) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          detach->data.zone_detach.target_slot_name) == 0) {
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
        if (link->data.zone_link.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                link->data.zone_link.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                link->data.zone_link.state_name, PGY_PROP_CAUSE_LINK);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              link->data.zone_link.state_name) == 0) {
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
            link->data.zone_link.relation_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            link->data.zone_link.relation_slot_name, PGY_PROP_CAUSE_LINK);
        emit_zone_bind_relation_layer(ctx->out, node,
            link->data.zone_link.relation_slot_name,
            link->data.zone_link.left_slot_name,
            link->data.zone_link.right_slot_name, ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          link->data.zone_link.relation_slot_name) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          link->data.zone_link.left_slot_name) == 0
                && strcmp(ast_zone_state_right_slot_name(state),
                          link->data.zone_link.right_slot_name) == 0) {
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
            maintain->data.zone_maintain_relation.relation_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            maintain->data.zone_maintain_relation.relation_slot_name,
            PGY_PROP_CAUSE_MAINTAIN);
        emit_zone_bind_relation_layer(ctx->out, node,
            maintain->data.zone_maintain_relation.relation_slot_name,
            maintain->data.zone_maintain_relation.left_slot_name,
            maintain->data.zone_maintain_relation.right_slot_name, ctx);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          maintain->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          maintain->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(ast_zone_state_right_slot_name(state),
                          maintain->data.zone_maintain_relation.right_slot_name) == 0) {
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
        if (unlink->data.zone_unlink.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                unlink->data.zone_unlink.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                unlink->data.zone_unlink.state_name, PGY_PROP_CAUSE_UNLINK);
            for (size_t j = 0; j < state_count; j++) {
                ASTNode *state = states[j];
                if (ast_zone_state_is_relation(state)
                    && strcmp(ast_zone_state_name(state),
                              unlink->data.zone_unlink.state_name) == 0) {
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
            unlink->data.zone_unlink.relation_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            unlink->data.zone_unlink.relation_slot_name, PGY_PROP_CAUSE_UNLINK);
        for (size_t j = 0; j < state_count; j++) {
            ASTNode *state = states[j];
            if (ast_zone_state_is_relation(state)
                && strcmp(ast_zone_state_layer_slot_name(state),
                          unlink->data.zone_unlink.relation_slot_name) == 0
                && strcmp(ast_zone_state_left_or_target_slot_name(state),
                          unlink->data.zone_unlink.left_slot_name) == 0
                && strcmp(ast_zone_state_right_slot_name(state),
                          unlink->data.zone_unlink.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    ast_zone_state_name(state));
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    ast_zone_state_name(state),
                    PGY_PROP_CAUSE_UNLINK);
            }
        }
    }

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
    for (size_t i = 0; i < layer_slot_count; i++) {
        ASTNode *slot = layer_slots[i];
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__layer_active_%s != _pgy_prev_layer_%s) {\n",
            ast_zone_layer_slot_name(slot),
            ast_zone_layer_slot_name(slot));
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_zone_frontier_continue = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (_pgy_zone_frontier_continue) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out,
        "PGY_PANIC(\"%s\");\n", PGY_FRONTIER_REASON_ZONE_OVERFLOW);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    /* Release write lock */
    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_ZONE_UNLOCK(self);\n");

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    transpiler_emit_zone_hosted_methods(name, &method_view, ctx);
}

#endif /* PGY_TRANSPILER_ZONE_DECL_EMIT_H */
