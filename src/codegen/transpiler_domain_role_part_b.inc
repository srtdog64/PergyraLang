
    codebuf_write(ctx->out, "\n/* Role: %s */\n", name);
    emit_included_role_impls(node, ctx);

    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl->type == AST_IMPL_ABILITY) {
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method->type != AST_FUNC_DECL)
                    continue;
                emit_role_method_impl(name, method, ctx);
            }

            emit_role_vtable_instance(name, impl, ctx);

        } else if (impl->type == AST_OVERRIDE_FUNC) {
            ASTNode *func = impl->data.override_func.func_decl;
            if (func == NULL || func->type != AST_FUNC_DECL)
                continue;

            const char *method_name = func->data.func_decl.name;
            const char *ret_type = "void";
            if (func->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(func->data.func_decl.return_type);

            codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                          ret_type, name, method_name);

            for (size_t k = 0; k < func->data.func_decl.param_count; k++) {
                FuncParam *p = func->data.func_decl.params[k];
                if (strcmp(p->name, "self") == 0 && p->type == NULL)
                    continue;
                const char *pt = NULL;
                char surface_desc[256];
                snprintf(surface_desc, sizeof(surface_desc),
                    "role override parameter '%s.%s(%s)'",
                    name != NULL ? name : "(anonymous)",
                    method_name != NULL ? method_name : "(anonymous)",
                    p != NULL && p->name != NULL ? p->name : "(anonymous)");
                pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
                if (pt == NULL)
                    return;
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");

            ctx->indent++;
            if (func->data.func_decl.body != NULL)
                emit_block(func->data.func_decl.body, ctx);
            ctx->indent--;

            codebuf_write(ctx->out, "}\n");
        }
    }

    emit_role_operator_aliases(node, ctx);
}

/* =================================================================
 * Party system emitters
 * ================================================================= */

/*
 * Party → C struct with role slot pointers + shared fields
 * Party methods → free functions
 */
void
emit_party_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.party_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_PARTY_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Party: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Role slots as void* + vtable pointer */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        const char *slot_name = rs->data.role_slot.slot_name;
        bool is_dyn = rs->data.role_slot.is_dynamic;
        codebuf_write(ctx->out, "    void *%s;\n", slot_name);
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab != NULL && ab->data.type.name != NULL) {
                char typedef_name[128];
                char *vtable_tag = render_ability_ref_vtable_tag(ab);
                ensure_ability_ref_vtable_decl(ab, ctx);
                if (!ability_ref_vtable_typedef_name(ab, typedef_name, sizeof(typedef_name), ctx)) {
                    free(vtable_tag);
                    return;
                }
                if (vtable_tag == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for party '%s' slot '%s'",
                        name != NULL ? name : "<party>",
                        slot_name != NULL ? slot_name : "<slot>");
                    return;
                }
                if (is_dyn) {
                    /* dyn: mutable vtable pointer — swappable at runtime */
                    codebuf_write(ctx->out,
                        "    const %s *%s_%s_vt; /* dyn */\n",
                        typedef_name, slot_name, vtable_tag);
                } else {
                    codebuf_write(ctx->out,
                        "    const %s *%s_%s_vt;\n",
                        typedef_name, slot_name, vtable_tag);
                }
                free(vtable_tag);
            }
        }
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "party shared field '%s.%s'",
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

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods as free functions */
    for (size_t i = 0; i < node->data.party_decl.method_count; i++) {
        emit_hosted_method_forward_decl_named(name,
            node->data.party_decl.methods[i], true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-party)",
        "party", node->data.party_decl.methods,
        node->data.party_decl.method_count, ctx);

    /* Emit bind helpers for dyn role slots */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        if (!rs->data.role_slot.is_dynamic)
            continue;
        const char *slot_name = rs->data.role_slot.slot_name;
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab == NULL || ab->data.type.name == NULL)
                continue;
            char typedef_name[128];
            char *vtable_tag = render_ability_ref_vtable_tag(ab);
            ensure_ability_ref_vtable_decl(ab, ctx);
            if (!ability_ref_vtable_typedef_name(ab, typedef_name, sizeof(typedef_name), ctx)) {
                free(vtable_tag);
                return;
            }
            if (vtable_tag == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for party '%s' bind helper slot '%s'",
                    name != NULL ? name : "<party>",
                    slot_name != NULL ? slot_name : "<slot>");
                return;
            }
            codebuf_write(ctx->out,
                "\nstatic inline void\n"
                "%s_bind_%s(%s *self, void *impl, const %s *vt)\n"
                "{\n"
                "    self->%s = impl;\n"
                "    self->%s_%s_vt = vt;\n"
                "}\n",
                name, slot_name, name, typedef_name,
                slot_name,
                slot_name, vtable_tag);
            free(vtable_tag);
        }
    }
}

/* =================================================================
 * Roster/World system emitters
 * ================================================================= */

void
emit_roster_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.roster_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_ROSTER_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Roster: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Party slots */
    for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
        ASTNode *ps = node->data.roster_decl.party_slots[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ps->data.roster_slot.party_type,
            ps->data.roster_slot.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.roster_decl.shared_count; i++) {
        ASTNode *shared = node->data.roster_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "roster shared field '%s.%s'",
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

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods */
    for (size_t i = 0; i < node->data.roster_decl.method_count; i++) {
        emit_hosted_method_forward_decl_named(name,
            node->data.roster_decl.methods[i], true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-roster)",
        "roster", node->data.roster_decl.methods,
        node->data.roster_decl.method_count, ctx);
}

void
emit_relation_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.relation_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_RELATION_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Relation: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
        ASTNode *slot = node->data.relation_decl.slots[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "relation slot '%s.%s'",
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

    for (size_t i = 0; i < node->data.relation_decl.shared_count; i++) {
        ASTNode *shared = node->data.relation_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "relation shared field '%s.%s'",
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

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    emit_domain_projection_sync_loop(ctx,
        node->data.relation_decl.slots,
        node->data.relation_decl.slot_count,
        node->data.relation_decl.refreshes,
        node->data.relation_decl.refresh_count,
        "relation_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.relation_decl.method_count; i++) {
        emit_hosted_method_forward_decl_named(name,
            node->data.relation_decl.methods[i], true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-relation)",
        "relation", node->data.relation_decl.methods,
        node->data.relation_decl.method_count, ctx);
}

void
emit_effect_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.effect_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_EFFECT_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Effect: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
        ASTNode *slot = node->data.effect_decl.slots[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "effect slot '%s.%s'",
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

    for (size_t i = 0; i < node->data.effect_decl.shared_count; i++) {
        ASTNode *shared = node->data.effect_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "effect shared field '%s.%s'",
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

    codebuf_write(ctx->out, "} %s;\n", name);

    codebuf_write(ctx->out, "\nstatic inline void\n%s_sync(%s *self)\n{\n",
                  name, name);
    ctx->indent++;
    emit_domain_projection_sync_loop(ctx,
        node->data.effect_decl.slots,
        node->data.effect_decl.slot_count,
        node->data.effect_decl.refreshes,
        node->data.effect_decl.refresh_count,
        "effect_projection",
        true);
    ctx->indent--;
    codebuf_write(ctx->out, "}\n");

    for (size_t i = 0; i < node->data.effect_decl.method_count; i++) {
        emit_hosted_method_forward_decl_named(name,
            node->data.effect_decl.methods[i], true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-effect)",
        "effect", node->data.effect_decl.methods,
        node->data.effect_decl.method_count, ctx);
}

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
        node->data.zone_decl.state_count + node->data.zone_decl.layer_slot_count + 1);
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
