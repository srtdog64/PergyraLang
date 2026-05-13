#ifndef PGY_TRANSPILER_ZONE_DECL_EMIT_H
#define PGY_TRANSPILER_ZONE_DECL_EMIT_H

#include "transpiler_zone_struct_emit.h"

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

    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        if (slot != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out,
                slot->data.domain_slot.type);
    }
    for (size_t i = 0; i < node->data.zone_decl.shared_count; i++) {
        ASTNode *shared = node->data.zone_decl.shared_fields[i];
        if (shared != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out,
                shared->data.party_shared.type);
    }
    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out,
            method);
    }

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

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_state_%s = self->__state_%s;\n",
            state->data.zone_state.state_name,
            state->data.zone_state.state_name);
    }
    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_layer_%s = self->__layer_active_%s;\n",
            slot->data.zone_layer_slot.slot_name,
            slot->data.zone_layer_slot.slot_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = false;\n",
            state->data.zone_state.state_name);
    }
    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        if (slot->data.zone_layer_slot.is_pool) {
            write_indent(ctx);
            codebuf_write(ctx->out, "PGY_EFFECT_POOL_INIT(self->%s);\n",
                slot->data.zone_layer_slot.slot_name);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
            slot->data.zone_layer_slot.slot_name);
    }

    emit_domain_projection_sync_loop(ctx,
        node->data.zone_decl.slots,
        node->data.zone_decl.slot_count,
        node->data.zone_decl.refreshes,
        node->data.zone_decl.refresh_count,
        "zone_projection",
        false);

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        const char *layer_name;
        if (slot == NULL || slot->type != AST_ZONE_LAYER_SLOT
            || slot->data.zone_layer_slot.is_relation
            || slot->data.zone_layer_slot.slot_name == NULL) {
            continue;
        }
        layer_name = slot->data.zone_layer_slot.slot_name;
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__layer_cause_%s == %d) {\n",
            layer_name, PGY_PROP_CAUSE_ACTION);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n", layer_name);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state == NULL || state->type != AST_ZONE_STATE
                || state->data.zone_state.is_relation
                || state->data.zone_state.layer_slot_name == NULL
                || state->data.zone_state.state_name == NULL
                || strcmp(state->data.zone_state.layer_slot_name, layer_name) != 0) {
                continue;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                state->data.zone_state.state_name);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
        ASTNode *apply = node->data.zone_decl.applies[i];
        if (apply->data.zone_apply.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                apply->data.zone_apply.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                apply->data.zone_apply.state_name, PGY_PROP_CAUSE_APPLY);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (!state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              apply->data.zone_apply.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        state->data.zone_state.layer_slot_name,
                        PGY_PROP_CAUSE_APPLY);
                    emit_zone_bind_effect_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name, ctx);
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
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          apply->data.zone_apply.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          apply->data.zone_apply.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    state->data.zone_state.state_name, PGY_PROP_CAUSE_APPLY);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_effect_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_effects[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
            maintain->data.zone_maintain_effect.effect_slot_name);
        emit_hidden_provenance_stamp(ctx, "self", "layer",
            maintain->data.zone_maintain_effect.effect_slot_name,
            PGY_PROP_CAUSE_MAINTAIN);
        emit_zone_bind_effect_layer(ctx->out, node,
            maintain->data.zone_maintain_effect.effect_slot_name,
            maintain->data.zone_maintain_effect.target_slot_name, ctx);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_effect.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_effect.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    state->data.zone_state.state_name,
                    PGY_PROP_CAUSE_MAINTAIN);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_state_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_states[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__state_%s = true;\n",
            maintain->data.zone_maintain_state.state_name);
        emit_hidden_provenance_stamp(ctx, "self", "state",
            maintain->data.zone_maintain_state.state_name,
            PGY_PROP_CAUSE_MAINTAIN);
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (strcmp(state->data.zone_state.state_name,
                       maintain->data.zone_maintain_state.state_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                    state->data.zone_state.layer_slot_name);
                emit_hidden_provenance_stamp(ctx, "self", "layer",
                    state->data.zone_state.layer_slot_name,
                    PGY_PROP_CAUSE_MAINTAIN);
                if (!state->data.zone_state.is_relation) {
                    emit_zone_bind_effect_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name, ctx);
                } else {
                    emit_zone_bind_relation_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name,
                        state->data.zone_state.right_slot_name, ctx);
                }
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
        ASTNode *detach = node->data.zone_decl.detaches[i];
        if (detach->data.zone_detach.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                detach->data.zone_detach.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                detach->data.zone_detach.state_name, PGY_PROP_CAUSE_DETACH);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (!state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              detach->data.zone_detach.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        state->data.zone_state.layer_slot_name,
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
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (!state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          detach->data.zone_detach.effect_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          detach->data.zone_detach.target_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",

                    state->data.zone_state.state_name);
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    state->data.zone_state.state_name,
                    PGY_PROP_CAUSE_DETACH);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
        ASTNode *link = node->data.zone_decl.links[i];
        if (link->data.zone_link.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = true;\n",
                link->data.zone_link.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                link->data.zone_link.state_name, PGY_PROP_CAUSE_LINK);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              link->data.zone_link.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = true;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        state->data.zone_state.layer_slot_name,
                        PGY_PROP_CAUSE_LINK);
                    emit_zone_bind_relation_layer(ctx->out, node,
                        state->data.zone_state.layer_slot_name,
                        state->data.zone_state.left_or_target_slot_name,
                        state->data.zone_state.right_slot_name, ctx);
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
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          link->data.zone_link.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          link->data.zone_link.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          link->data.zone_link.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    state->data.zone_state.state_name, PGY_PROP_CAUSE_LINK);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.maintained_relation_count; i++) {
        ASTNode *maintain = node->data.zone_decl.maintained_relations[i];
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
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          maintain->data.zone_maintain_relation.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          maintain->data.zone_maintain_relation.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          maintain->data.zone_maintain_relation.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = true;\n",
                    state->data.zone_state.state_name);
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    state->data.zone_state.state_name,
                    PGY_PROP_CAUSE_MAINTAIN);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
        ASTNode *unlink = node->data.zone_decl.unlinks[i];
        if (unlink->data.zone_unlink.state_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__state_%s = false;\n",
                unlink->data.zone_unlink.state_name);
            emit_hidden_provenance_stamp(ctx, "self", "state",
                unlink->data.zone_unlink.state_name, PGY_PROP_CAUSE_UNLINK);
            for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
                ASTNode *state = node->data.zone_decl.states[j];
                if (state->data.zone_state.is_relation
                    && strcmp(state->data.zone_state.state_name,
                              unlink->data.zone_unlink.state_name) == 0) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "self->__layer_active_%s = false;\n",
                        state->data.zone_state.layer_slot_name);
                    emit_hidden_provenance_stamp(ctx, "self", "layer",
                        state->data.zone_state.layer_slot_name,
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
        for (size_t j = 0; j < node->data.zone_decl.state_count; j++) {
            ASTNode *state = node->data.zone_decl.states[j];
            if (state->data.zone_state.is_relation
                && strcmp(state->data.zone_state.layer_slot_name,
                          unlink->data.zone_unlink.relation_slot_name) == 0
                && strcmp(state->data.zone_state.left_or_target_slot_name,
                          unlink->data.zone_unlink.left_slot_name) == 0
                && strcmp(state->data.zone_state.right_slot_name,
                          unlink->data.zone_unlink.right_slot_name) == 0) {
                write_indent(ctx);
                codebuf_write(ctx->out, "self->__state_%s = false;\n",
                    state->data.zone_state.state_name);
                emit_hidden_provenance_stamp(ctx, "self", "state",
                    state->data.zone_state.state_name,
                    PGY_PROP_CAUSE_UNLINK);
            }
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__state_%s != _pgy_prev_state_%s) {\n",
            state->data.zone_state.state_name,
            state->data.zone_state.state_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_zone_frontier_continue = true;\n");
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__layer_active_%s != _pgy_prev_layer_%s) {\n",
            slot->data.zone_layer_slot.slot_name,
            slot->data.zone_layer_slot.slot_name);
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

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-zone)",
        "zone", &method_view, ctx);
}

#endif /* PGY_TRANSPILER_ZONE_DECL_EMIT_H */
