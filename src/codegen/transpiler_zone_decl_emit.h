void
emit_zone_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.zone_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_ZONE_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

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
    for (size_t i = 0; i < node->data.zone_decl.method_count; i++)
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out,
            node->data.zone_decl.methods[i]);

    codebuf_write(ctx->out, "\n/* Zone: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.slots[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "zone slot '%s.%s'",
            name != NULL ? name : "(anonymous)",
            slot != NULL && slot->data.domain_slot.slot_name != NULL
                ? slot->data.domain_slot.slot_name
                : "(anonymous)");
        ft = transpiler_require_ast_c_type(
            ctx,
            slot != NULL ? slot->data.domain_slot.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, slot->data.domain_slot.slot_name);
        if (!slot->data.domain_slot.is_subject) {
            codebuf_write(ctx->out, "    bool __projection_ready_%s;\n",
                slot->data.domain_slot.slot_name);
            codebuf_write(ctx->out, "    bool __projection_dirty_%s;\n",
                slot->data.domain_slot.slot_name);
            emit_hidden_provenance_fields(ctx, "projection",
                slot->data.domain_slot.slot_name);
        }
    }

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        if (slot->data.zone_layer_slot.is_pool) {
            int cap = slot->data.zone_layer_slot.pool_capacity;
            if (cap <= 0)
                cap = 1;
            codebuf_write(ctx->out,
                "    struct { %s items[%d]; bool active[%d]; uint8_t count; uint8_t cap; } %s;\n",
                slot->data.zone_layer_slot.layer_type,
                cap,
                cap,
                slot->data.zone_layer_slot.slot_name);
        } else {
            codebuf_write(ctx->out, "    %s %s;\n",
                slot->data.zone_layer_slot.layer_type,
                slot->data.zone_layer_slot.slot_name);
        }
        codebuf_write(ctx->out, "    bool __layer_active_%s;\n",
            slot->data.zone_layer_slot.slot_name);
        emit_hidden_provenance_fields(ctx, "layer",
            slot->data.zone_layer_slot.slot_name);
    }

    for (size_t i = 0; i < node->data.zone_decl.shared_count; i++) {
        ASTNode *shared = node->data.zone_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "zone shared field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            shared != NULL && shared->data.party_shared.name != NULL
                ? shared->data.party_shared.name
                : "(anonymous)");
        ft = transpiler_require_ast_c_type(
            ctx,
            shared != NULL ? shared->data.party_shared.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    for (size_t i = 0; i < node->data.zone_decl.state_count; i++) {
        ASTNode *state = node->data.zone_decl.states[i];
        codebuf_write(ctx->out, "    bool __state_%s;\n",
            state->data.zone_state.state_name);
        emit_hidden_provenance_fields(ctx, "state",
            state->data.zone_state.state_name);
    }

    /* Concurrency protection + stale-state detection */
    codebuf_write(ctx->out, "    PGY_ZONE_LOCK_FIELD\n");
    codebuf_write(ctx->out, "    PGY_ZONE_GENERATION_FIELD\n");
    codebuf_write(ctx->out, "} %s;\n", name);

    for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = node->data.zone_decl.layer_slots[i];
        const char *slot_name = slot->data.zone_layer_slot.slot_name;

        if (slot_name == NULL)
            continue;

        codebuf_write(ctx->out,
            "\nstatic inline bool\n%s_has_layer_%s(%s *self, uint32_t expected_gen)\n{\n",
            name, slot_name, name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "bool result;\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "PGY_ZONE_RDLOCK(self);\n");
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PGY_ZONE_GENERATION_WARN_IF_STALE(self, expected_gen, \"%s.%s\");\n",
            name, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "result = self->__layer_active_%s;\n", slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "PGY_ZONE_UNLOCK(self);\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "return result;\n");
        ctx->indent--;
        codebuf_write(ctx->out, "}\n");
    }

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
        pgy_frontier_zone_pass_limit(node->data.zone_decl.state_count,
            node->data.zone_decl.layer_slot_count));
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
        "PGY_PANIC(\"zone frontier recompute exceeded bounded pass limit\");\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    /* Release write lock */
    write_indent(ctx);
    codebuf_write(ctx->out, "PGY_ZONE_UNLOCK(self);\n");

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.zone_decl.method_count; i++) {
        emit_hosted_method_forward_decl_named(name,
            node->data.zone_decl.methods[i], true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-zone)",
        "zone", node->data.zone_decl.methods,
        node->data.zone_decl.method_count, ctx);
}
