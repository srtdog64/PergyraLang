void
emit_world_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.world_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_WORLD_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* World: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Roster instances */
    for (size_t i = 0; i < node->data.world_decl.roster_count; i++) {
        ASTNode *ws = node->data.world_decl.rosters[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ws->data.world_roster.roster_type,
            ws->data.world_roster.slot_name);
    }

    /* Zone instances */
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            wz->data.world_zone.zone_type,
            wz->data.world_zone.slot_name);
        codebuf_write(ctx->out, "    bool __zone_active_%s;\n",
            wz->data.world_zone.slot_name);
        codebuf_write(ctx->out, "    bool __zone_dirty_%s;\n",
            wz->data.world_zone.slot_name);
        codebuf_write(ctx->out, "    uint32_t __zone_seen_generation_%s;\n",
            wz->data.world_zone.slot_name);
        emit_hidden_provenance_fields(ctx, "zone",
            wz->data.world_zone.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.world_decl.shared_count; i++) {
        ASTNode *shared = node->data.world_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "world shared field '%s.%s'",
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

    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        codebuf_write(ctx->out, "    bool __zone_state_%s;\n",
            state->data.world_state.state_name);
        emit_hidden_provenance_fields(ctx, "zone_state",
            state->data.world_state.state_name);
    }
    codebuf_write(ctx->out, "    bool __world_derived_dirty;\n");

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: reset */\n");
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_active_%s = self->__zone_active_%s;\n",
            wz->data.world_zone.slot_name, wz->data.world_zone.slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out, "self->__zone_active_%s = false;\n",
            wz->data.world_zone.slot_name);
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world command pass: directives */\n");
    for (size_t i = 0; i < node->data.world_decl.activate_count; i++) {
        ASTNode *act = node->data.world_decl.activations[i];
        const char *slot_name = act->data.world_activate.zone_slot_name;
        if (slot_name == NULL && act->data.world_activate.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, act->data.world_activate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (transpiler_world_has_zone_slot(node, act->data.world_activate.state_name))
                slot_name = act->data.world_activate.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n", slot_name);
            emit_hidden_provenance_stamp(ctx, "self", "zone", slot_name,
                PGY_PROP_CAUSE_WORLD_ACTIVATE);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.maintained_zone_count; i++) {
        ASTNode *mnt = node->data.world_decl.maintained_zones[i];
        const char *slot_name = mnt->data.world_maintain.zone_slot_name;
        if (slot_name == NULL && mnt->data.world_maintain.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, mnt->data.world_maintain.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (transpiler_world_has_zone_slot(node, mnt->data.world_maintain.state_name))
                slot_name = mnt->data.world_maintain.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = true;\n", slot_name);
            emit_hidden_provenance_stamp(ctx, "self", "zone", slot_name,
                PGY_PROP_CAUSE_WORLD_MAINTAIN);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.deactivate_count; i++) {
        ASTNode *act = node->data.world_decl.deactivations[i];
        const char *slot_name = act->data.world_deactivate.zone_slot_name;
        if (slot_name == NULL && act->data.world_deactivate.state_name != NULL) {
            ASTNode *state = find_world_state_decl(node, act->data.world_deactivate.state_name);
            if (state != NULL)
                slot_name = state->data.world_state.zone_slot_name;
            else if (transpiler_world_has_zone_slot(node, act->data.world_deactivate.state_name))
                slot_name = act->data.world_deactivate.state_name;
        }
        if (slot_name != NULL) {
            write_indent(ctx);
            codebuf_write(ctx->out, "self->__zone_active_%s = false;\n", slot_name);
            emit_hidden_provenance_stamp(ctx, "self", "zone", slot_name,
                PGY_PROP_CAUSE_WORLD_DEACTIVATE);
        }
    }
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        const char *slot_name = wz->data.world_zone.slot_name;
        write_indent(ctx);
        codebuf_write(ctx->out, "if (self->__zone_active_%s != _pgy_prev_active_%s) {\n",
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
        pgy_frontier_world_transitive_pass_limit(node->data.world_decl.zone_count,
            node->data.world_decl.state_count));
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_frontier_continue = true;\n");
    write_indent(ctx);
    codebuf_write(ctx->out,
        "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit) {\n");
    ctx->indent++;
    write_indent(ctx);
    codebuf_write(ctx->out, "bool _pgy_world_needs_derived = self->__world_derived_dirty;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_frontier_continue = false;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "_pgy_world_frontier_pass++;\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* world zone sync pass */\n");
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        const char *slot_name = wz->data.world_zone.slot_name;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->%s.__sync_generation != self->__zone_seen_generation_%s) {\n",
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
        codebuf_write(ctx->out, "%s_sync(&self->%s);\n",
            wz->data.world_zone.zone_type, slot_name);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "self->__zone_seen_generation_%s = self->%s.__sync_generation;\n",
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
        pgy_frontier_world_derived_pass_limit(node->data.world_decl.state_count));
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
    for (size_t i = 0; i < node->data.world_decl.state_count; i++) {
        ASTNode *state = node->data.world_decl.states[i];
        const char *expr_fmt = "self->__zone_active_%s";
        const char *detail_name = state->data.world_state.detail_name;
        write_indent(ctx);
        codebuf_write(ctx->out, "bool _pgy_prev_zone_state_%s = self->__zone_state_%s;\n",
            state->data.world_state.state_name,
            state->data.world_state.state_name);
        write_indent(ctx);
        switch (state->data.world_state.source_kind) {
        case WORLD_STATE_SOURCE_ALL:
        case WORLD_STATE_SOURCE_ANY: {
            bool first = true;
            codebuf_write(ctx->out, "self->__zone_state_%s = ",
                state->data.world_state.state_name);
            codebuf_write(ctx->out, "(");
            for (size_t input_i = 0; input_i < state->data.world_state.input_count; input_i++) {
                const char *input_name = state->data.world_state.input_names[input_i];
                if (input_name == NULL)
                    continue;
                if (!first) {
                    codebuf_write(ctx->out,
                        state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                            ? " && " : " || ");
                }
                if (transpiler_world_has_zone_slot(node, input_name))
                    codebuf_write(ctx->out, "self->__zone_active_%s", input_name);
                else
                    codebuf_write(ctx->out, "self->__zone_state_%s", input_name);
                first = false;
            }
            if (first)
                codebuf_write(ctx->out,
                    state->data.world_state.source_kind == WORLD_STATE_SOURCE_ALL
                        ? "true" : "false");
            codebuf_write(ctx->out, ");\n");
            break;
        }
        case WORLD_STATE_SOURCE_PROJECTION:
            expr_fmt = "(self->__zone_active_%s && self->%s.__projection_ready_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_LAYER:
            expr_fmt = "(self->__zone_active_%s && self->%s.__layer_active_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_STATE:
            expr_fmt = "(self->__zone_active_%s && self->%s.__state_%s)";
            codebuf_write(ctx->out, "self->__zone_state_%s = ", state->data.world_state.state_name);
            codebuf_write(ctx->out, expr_fmt,
                state->data.world_state.zone_slot_name,
                state->data.world_state.zone_slot_name,
                detail_name != NULL ? detail_name : "");
            codebuf_write(ctx->out, ";\n");
            break;
        case WORLD_STATE_SOURCE_ZONE:
        default:
            codebuf_write(ctx->out, "self->__zone_state_%s = self->__zone_active_%s;\n",
                state->data.world_state.state_name,
                state->data.world_state.zone_slot_name);
            break;
        }
        emit_hidden_provenance_stamp(ctx, "self", "zone_state",
            state->data.world_state.state_name, PGY_PROP_CAUSE_WORLD_DERIVED);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (self->__zone_state_%s != _pgy_prev_zone_state_%s) {\n",
            state->data.world_state.state_name,
            state->data.world_state.state_name);
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_world_continue = true;\n");
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
    codebuf_write(ctx->out,
        "PGY_PANIC(\"world derived recompute exceeded bounded pass limit\");\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "self->__world_derived_dirty = false;\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "if (self->__world_derived_dirty");
    for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
        ASTNode *wz = node->data.world_decl.zones[i];
        if (wz == NULL || wz->type != AST_WORLD_ZONE
            || wz->data.world_zone.slot_name == NULL) {
            continue;
        }
        codebuf_write(ctx->out, " || self->__zone_dirty_%s",
            wz->data.world_zone.slot_name);
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
    codebuf_write(ctx->out,
        "PGY_PANIC(\"world frontier recompute exceeded bounded pass limit\");\n");
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    /* Methods */
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);

    for (size_t i = 0; i < method_view.count; i++) {
        ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_named(name,
            method, true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-world)",
        "world", &method_view, ctx);
}
