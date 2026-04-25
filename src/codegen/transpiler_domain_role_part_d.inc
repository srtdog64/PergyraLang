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
        node->data.world_decl.zone_count + node->data.world_decl.state_count + 1);
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
        node->data.world_decl.state_count + 1);
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
    for (size_t i = 0; i < node->data.world_decl.method_count; i++) {
        emit_hosted_method_forward_decl_named(name,
            node->data.world_decl.methods[i], true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-world)",
        "world", node->data.world_decl.methods,
        node->data.world_decl.method_count, ctx);
}

/* =================================================================
 * Async system emitters
 * ================================================================= */

void
emit_select_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    size_t case_count = node->data.select_stmt.case_count;

    codebuf_write(ctx->out, "\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "/* select */\n");

    if (case_count == 0) {
        if (node->data.select_stmt.default_case != NULL)
            emit_statement(node->data.select_stmt.default_case, ctx);
        return;
    }

    for (size_t i = 0; i < case_count; i++) {
        ASTNode *c = node->data.select_stmt.cases[i];
        ASTNode *channel = NULL;
        ASTNode *body = NULL;
        const char *bind_name = NULL;
        bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
        const char *inner = NULL;

        if (!valid_case || bind_name == NULL || channel == NULL
            || channel->type != AST_IDENTIFIER)
            continue;

        {
            const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
            if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                inner = slot_inner_type_name(type_name);
        }
        if (inner == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot derive receive type for select case channel '%s'",
                channel->data.identifier.name != NULL
                    ? channel->data.identifier.name
                    : "(anonymous)");
            return;
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "%s _sel_recv_%zu;\n",
                      pergyra_type_to_c(inner), i);
    }

    {
        int select_id = ctx->tmp_counter++;
        write_indent(ctx);
        codebuf_write(ctx->out, "static unsigned int _sel_rr_%d = 0;\n", select_id);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "unsigned int _sel_start_%d = _sel_rr_%d++ %% %zu;\n",
            select_id, select_id, case_count);
        write_indent(ctx);
        codebuf_write(ctx->out, "switch (_sel_start_%d) {\n", select_id);
        ctx->indent++;

        for (size_t start = 0; start < case_count; start++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "case %zu:\n", start);
            ctx->indent++;

            for (size_t offset = 0; offset < case_count; offset++) {
                size_t i = (start + offset) % case_count;
                ASTNode *c = node->data.select_stmt.cases[i];
                ASTNode *channel = NULL;
                ASTNode *body = NULL;
                const char *bind_name = NULL;
                bool valid_case = select_case_parts(c, &channel, &bind_name, &body);
                const char *inner = NULL;

                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                    const char *type_name = lookup_typed_var(ctx, channel->data.identifier.name);
                    if (type_name != NULL && strncmp(type_name, "Channel<", 8) == 0)
                        inner = slot_inner_type_name(type_name);
                }
                if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER
                    && inner == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot derive receive type for select case channel '%s'",
                        channel->data.identifier.name != NULL
                            ? channel->data.identifier.name
                            : "(anonymous)");
                    return;
                }

                write_indent(ctx);
                if (offset == 0) {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "if (1) { /* select case %zu */\n", i);
                    }
                } else {
                    if (valid_case && channel != NULL && channel->type == AST_IDENTIFIER) {
                        if (bind_name != NULL)
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_try_recv_%s(&%s, &_sel_recv_%zu)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i, i);
                        else
                            codebuf_write(ctx->out,
                                "} else if (pgy_channel_ready_%s(&%s)) { /* select case %zu */\n",
                                inner, channel->data.identifier.name, i);
                    } else {
                        codebuf_write(ctx->out, "} else if (1) { /* select case %zu */\n", i);
                    }
                }
                ctx->indent++;
                if (valid_case) {
                    if (bind_name != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "%s %s = _sel_recv_%zu;\n",
                                      pergyra_type_to_c(inner), bind_name, i);
                    } else if (channel != NULL) {
                        char *recv = emit_channel_recv(ast_create_channel_recv(channel), ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "(void)%s;\n", recv);
                        free(recv);
                    }
                    if (body != NULL)
                        emit_statement(body, ctx);
                } else if (c != NULL) {
                    emit_statement(c, ctx);
                }
                ctx->indent--;
            }

            if (node->data.select_stmt.default_case != NULL) {
                write_indent(ctx);
                codebuf_write(ctx->out, "} else { /* default */\n");
                ctx->indent++;
                emit_statement(node->data.select_stmt.default_case, ctx);
                ctx->indent--;
            }

            write_indent(ctx);
            codebuf_write(ctx->out, "}\n");
            write_indent(ctx);
            codebuf_write(ctx->out, "break;\n");
            ctx->indent--;
        }

        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

/* =================================================================
 * Event system emitters
 * ================================================================= */

void
emit_event_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.event_decl.name;
    const char *event_type = transpiler_scratch_fmt(ctx, "%s_Event", name);

    /* Generate event handler type typedef */
    codebuf_write(ctx->out, "\n/* Event: %s */\n", name);

    /* Build parameter types string */
    codebuf_write(ctx->out, "typedef void (*%s_Handler)(", name);

    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s %s", pt, param->data.let_decl.name);
    }

    codebuf_write(ctx->out, ");\n");

    /* Generate event struct with handler array */
    codebuf_write(ctx->out, "typedef struct {\n");
    codebuf_write(ctx->out, "    %s_Handler handlers[PGY_EVENT_MAX_HANDLERS];\n", name);
    codebuf_write(ctx->out, "    void* contexts[PGY_EVENT_MAX_HANDLERS];\n");
    codebuf_write(ctx->out, "    size_t count;\n");
    codebuf_write(ctx->out, "    bool is_invoking;\n");
    codebuf_write(ctx->out, "    bool pending_changes;\n");
    codebuf_write(ctx->out, "} %s;\n", event_type);
    codebuf_write(ctx->out, "static %s %s;\n", event_type, name);

    /* Generate inline init function */
    codebuf_write(ctx->out, "static inline void %s_INIT(%s* e) {\n", name, event_type);
    codebuf_write(ctx->out, "    memset(e, 0, sizeof(*e));\n");
    codebuf_write(ctx->out, "}\n");

    /* Generate subscribe function */
    codebuf_write(ctx->out, "static inline void %s_SUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    if (e->count < PGY_EVENT_MAX_HANDLERS) {\n");
    codebuf_write(ctx->out, "        e->handlers[e->count++] = h;\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");

    /* Generate unsubscribe function */
    codebuf_write(ctx->out, "static inline void %s_UNSUBSCRIBE(%s* e, %s_Handler h) {\n", name, event_type, name);
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        if (e->handlers[i] == h) {\n");
    codebuf_write(ctx->out, "            for (size_t j = i; j < e->count - 1; j++) {\n");
    codebuf_write(ctx->out, "                e->handlers[j] = e->handlers[j + 1];\n");
    codebuf_write(ctx->out, "            }\n");
    codebuf_write(ctx->out, "            e->count--;\n");
    codebuf_write(ctx->out, "            break;\n");
    codebuf_write(ctx->out, "        }\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "}\n");

    /* Generate invoke function */
    codebuf_write(ctx->out, "static inline void %s_INVOKE(%s* e", name, event_type);
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        ASTNode *param = node->data.event_decl.params[i];
        const char *pt = "void*";
        if (param->data.let_decl.type != NULL) {
            pt = pergyra_ast_type_to_c(param->data.let_decl.type);
        }
        codebuf_write(ctx->out, ", %s %s", pt, param->data.let_decl.name);
    }
    codebuf_write(ctx->out, ") {\n");

    codebuf_write(ctx->out, "    e->is_invoking = true;\n");
    codebuf_write(ctx->out, "    for (size_t i = 0; i < e->count; i++) {\n");
    codebuf_write(ctx->out, "        e->handlers[i](");
    for (size_t i = 0; i < node->data.event_decl.param_count; i++) {
        if (i > 0) codebuf_write(ctx->out, ", ");
        codebuf_write(ctx->out, "%s", node->data.event_decl.params[i]->data.let_decl.name);
    }
    codebuf_write(ctx->out, ");\n");
    codebuf_write(ctx->out, "    }\n");
    codebuf_write(ctx->out, "    e->is_invoking = false;\n");
    codebuf_write(ctx->out, "}\n");
}

void
emit_event_subscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);

    write_indent(ctx);
    codebuf_write(ctx->out, "%s_SUBSCRIBE(&%s, %s);\n",
                  event_expr, event_expr, handler_expr);

    free(event_expr);
    free(handler_expr);
}

void
emit_event_unsubscribe(ASTNode *node, TranspilerCtx *ctx)
{
    char *event_expr = emit_expression(node->data.event_op.event, ctx);
    char *handler_expr = emit_expression(node->data.event_op.handler, ctx);

    write_indent(ctx);
    codebuf_write(ctx->out, "%s_UNSUBSCRIBE(&%s, %s);\n",
                  event_expr, event_expr, handler_expr);

    free(event_expr);
    free(handler_expr);
}
