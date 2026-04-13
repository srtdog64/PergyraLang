static void
emit_func_decl_from_mir_named(ASTNode *node, const MIRRoutine *mir_routine,
                              const char *emitted_name, CodeBuf *buf,
                              TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;
    CodeBuf *saved_out = ctx->out;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    const char *saved_zone_name = ctx->current_zone_name;
    const char *saved_class_name = ctx->current_class_name;
    const char *saved_relation_name = ctx->current_relation_name;
    const char *saved_effect_name = ctx->current_effect_name;
    const char *saved_world_name = ctx->current_world_name;
    const ASTNode *saved_func_decl = ctx->current_func_decl;
    char saved_return_type[128];
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    bool is_method = mir_routine != NULL
        && mir_routine->kind == MIR_SCOPE_METHOD;
    const char *owner_name = is_method ? mir_routine->owner_name : NULL;
    ASTNodeType owner_ast_type = is_method ? mir_routine->owner_ast_type : AST_PROGRAM;
    const char *owner_zone_name = NULL;
    const char *owner_role_subject_name = NULL;
    const char *owner_role_subject_c_type = NULL;
    bool owner_is_zone = false;
    bool owner_is_role = owner_ast_type == AST_ROLE_DECL;
    bool owner_is_relation = owner_ast_type == AST_RELATION_DECL;
    bool owner_is_effect = owner_ast_type == AST_EFFECT_DECL;
    bool owner_is_world = owner_ast_type == AST_WORLD_DECL;
    bool pointer_self = false;

    if (is_method && owner_name == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "MIR-only transpiler missing owner metadata for method '%s'",
                name != NULL ? name : "<method>");
        }
        codebuf_destroy(params_sig);
        return;
    }

    if (owner_name != NULL) {
        if (owner_ast_type == AST_ZONE_DECL) {
            owner_zone_name = owner_name;
        }
        owner_is_zone = owner_zone_name != NULL;
        if (owner_is_role) {
            ASTNode *role_decl = find_role_decl(ctx, owner_name);
            if (role_decl != NULL
                && role_decl->type == AST_ROLE_DECL
                && role_decl->data.role_decl.for_type != NULL
                && role_decl->data.role_decl.for_type->type == AST_TYPE
                && role_decl->data.role_decl.for_type->data.type.name != NULL) {
                owner_role_subject_name =
                    role_decl->data.role_decl.for_type->data.type.name;
                owner_role_subject_c_type =
                    pergyra_type_to_c(owner_role_subject_name);
            }
        }
    }

    ctx->out = buf;
    g_type_render_ctx = ctx;
    if (owner_is_zone) {
        ctx->current_zone_name = owner_zone_name;
    } else if (owner_is_role) {
        if (owner_role_subject_name != NULL)
            ctx->current_class_name = owner_role_subject_name;
    } else if (owner_is_relation) {
        ctx->current_relation_name = owner_name;
    } else if (owner_is_effect) {
        ctx->current_effect_name = owner_name;
    } else if (owner_is_world) {
        ctx->current_world_name = owner_name;
    } else if (owner_name != NULL) {
        ctx->current_class_name = owner_name;
    }
    if (!owner_is_zone
        && owner_name == NULL
        && node != NULL
        && node->type == AST_FUNC_DECL
        && node->data.func_decl.within_zone != NULL
        && find_zone_decl(ctx, node->data.func_decl.within_zone) != NULL) {
        ctx->current_zone_name = node->data.func_decl.within_zone;
    }
    if (node != NULL && node->type == AST_FUNC_DECL)
        ctx->current_func_decl = node;
    snprintf(saved_return_type, sizeof(saved_return_type), "%s",
        ctx->current_return_type);
    if (node->data.func_decl.return_type != NULL) {
        char *rendered = render_type_name(node->data.func_decl.return_type);
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
            "%s", rendered);
        free(rendered);
    } else {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Void");
    }

    if (owner_name != NULL) {
        if (owner_is_role) {
            codebuf_write(params_sig, "void *_raw_self");
        } else {
        pointer_self = owner_is_zone || owner_is_relation
            || owner_is_effect || owner_is_world
            || is_pointer_self_host_type_name(ctx, owner_name);
        codebuf_write(params_sig, "%s%s", owner_name, pointer_self ? " *self" : " self");
        }
    }

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        if (p == NULL || p->name == NULL)
            continue;
        if (is_method && p != NULL && p->name != NULL
            && strcmp(p->name, "self") == 0 && p->type == NULL) {
            continue;
        }
        if (p->type != NULL)
            pt = pergyra_ast_type_to_c(p->type);
        else if (p->name != NULL
                 && strcmp(p->name, "self") == 0
                 && mir_routine != NULL
                 && mir_routine->owner_name != NULL) {
            pt = mir_routine->owner_name;
            type_name = pergyra_strdup(mir_routine->owner_name);
        }
        if (pt == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot determine parameter type for MIR-emitted function '%s' at argument %zu",
                    name != NULL ? name : "<function>",
                    i);
            }
            codebuf_destroy(params_sig);
            free(header_decl);
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            g_type_render_ctx = saved_render_ctx;
            ctx->out = saved_out;
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", saved_return_type);
            return;
        }
        if (params_sig->len > 0)
            codebuf_write(params_sig, ", ");
        if (p->type != NULL && type_name == NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") == 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }

    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

    ctx->indent++;
    if (owner_name != NULL) {
        if (owner_is_role) {
            if (owner_role_subject_name != NULL) {
                register_typed_var(ctx, "self", owner_role_subject_name);
                write_indent(ctx);
                if (is_pointer_self_host_type_name(ctx, owner_role_subject_name)) {
                    codebuf_write(ctx->out, "%s *self = (%s *)_raw_self;\n",
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name,
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name);
                } else {
                    codebuf_write(ctx->out, "%s self = *(%s *)_raw_self;\n",
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name,
                        owner_role_subject_c_type != NULL ? owner_role_subject_c_type
                                                          : owner_role_subject_name);
                }
                write_indent(ctx);
                codebuf_write(ctx->out, "(void)self;\n");
            } else {
                write_indent(ctx);
                codebuf_write(ctx->out, "(void)_raw_self;\n");
            }
        } else {
            register_typed_var(ctx, "self", owner_name);
        }
        if (!owner_is_role
            && (owner_is_zone || owner_is_relation || owner_is_effect || owner_is_world)) {
            if (owner_is_zone) {
                ASTNode *zone_decl = find_zone_decl(ctx, owner_zone_name);
                if (zone_decl != NULL
                    && zone_decl->data.zone_decl.authority_count > 0
                    && zone_decl->data.zone_decl.authorities != NULL
                    && zone_decl->data.zone_decl.authorities[0] != NULL) {
                    const char *auth_slot =
                        zone_decl->data.zone_decl.authorities[0]
                            ->data.zone_authority.subject_slot_name;
                    if (auth_slot != NULL) {
                        char *participant_expr = strdup_fmt("&self->%s", auth_slot);
                        write_indent(ctx);
                        codebuf_write(ctx->out,
                            "PGY_ZONE_AUTHORITY_CHECK(self, %s, \"%s\", \"%s\");\n",
                            participant_expr, owner_zone_name, auth_slot);
                        free(participant_expr);
                    }
                }
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s_sync(self);\n", owner_name);
            if (owner_is_zone) {
                write_indent(ctx);
                codebuf_write(ctx->out,
                    "uint32_t __attribute__((unused)) __pgy_zone_gen = self->__sync_generation;\n");
            }
        }
    }
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        char *type_name = NULL;
        if (p == NULL || p->name == NULL)
            continue;
        if (is_method && strcmp(p->name, "self") == 0 && p->type == NULL)
            continue;
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        if (p->type == NULL
            && strcmp(p->name, "self") == 0
            && mir_routine != NULL
            && mir_routine->owner_name != NULL) {
            free(type_name);
            type_name = pergyra_strdup(mir_routine->owner_name);
        }
        if (type_name == NULL)
            continue;
        if (type_name != NULL) {
            bool boundary_slot = (strncmp(type_name, "Slot<", 5) == 0
                               || strncmp(type_name, "SecureSlot<", 11) == 0)
                && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
            register_typed_var(ctx, p->name, type_name);
            if (p->name != NULL && strcmp(p->name, "self") != 0
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_subject_ref = true;
            }
            if (strncmp(type_name, "Slot<", 5) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), false, boundary_slot);
            else if (strncmp(type_name, "SecureSlot<", 11) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), true, boundary_slot);
            free(type_name);
        }
    }
    transpiler_register_explicit_local_bindings_in_block(ctx, node,
        node->data.func_decl.body);

    {
        const char *declared_versioned_names[4096];
        size_t declared_versioned_count = 0;
        for (size_t i = 0; i < mir_routine->block_count; i++) {
            const MIRBasicBlock *block = &mir_routine->blocks[i];
            if (!block->is_reachable || block->is_cleanup)
                continue;
            for (size_t j = 0; j < block->live_in_name_count; j++) {
                if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                        &declared_versioned_count,
                                                        4096,
                                                        block->live_in_names[j])) {
                    if (ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "too many MIR SSA locals while emitting function '%s'",
                            name != NULL ? name : "<function>");
                    }
                    codebuf_destroy(params_sig);
                    free(header_decl);
                    ctx->slot_var_count = saved_slot_count;
                    ctx->typed_var_count = saved_typed_count;
                    g_type_render_ctx = saved_render_ctx;
                    ctx->out = saved_out;
                    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                        "%s", saved_return_type);
                    return;
                }
            }
            for (size_t j = 0; j < block->ssa_entry_value_count; j++) {
                if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                        &declared_versioned_count,
                                                        4096,
                                                        block->ssa_entry_values[j])) {
                    if (ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "too many MIR SSA locals while emitting function '%s'",
                            name != NULL ? name : "<function>");
                    }
                    codebuf_destroy(params_sig);
                    free(header_decl);
                    ctx->slot_var_count = saved_slot_count;
                    ctx->typed_var_count = saved_typed_count;
                    g_type_render_ctx = saved_render_ctx;
                    ctx->out = saved_out;
                    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                        "%s", saved_return_type);
                    return;
                }
            }
            for (size_t j = 0; j < block->ssa_exit_value_count; j++) {
                if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                        &declared_versioned_count,
                                                        4096,
                                                        block->ssa_exit_values[j])) {
                    if (ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "too many MIR SSA locals while emitting function '%s'",
                            name != NULL ? name : "<function>");
                    }
                    codebuf_destroy(params_sig);
                    free(header_decl);
                    ctx->slot_var_count = saved_slot_count;
                    ctx->typed_var_count = saved_typed_count;
                    g_type_render_ctx = saved_render_ctx;
                    ctx->out = saved_out;
                    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                        "%s", saved_return_type);
                    return;
                }
            }
            for (size_t j = 0; j < block->instruction_count; j++) {
                const MIRInstruction *inst = &block->instructions[j];
                for (size_t u = 0; u < inst->use_count; u++) {
                    if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                            &declared_versioned_count,
                                                            4096,
                                                            inst->uses[u])) {
                        if (ctx->backend_error == NULL) {
                            ctx->backend_error = strdup_fmt(
                                "too many MIR SSA locals while emitting function '%s'",
                                name != NULL ? name : "<function>");
                        }
                        codebuf_destroy(params_sig);
                        free(header_decl);
                        ctx->slot_var_count = saved_slot_count;
                        ctx->typed_var_count = saved_typed_count;
                        g_type_render_ctx = saved_render_ctx;
                        ctx->out = saved_out;
                        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                            "%s", saved_return_type);
                        return;
                    }
                }
                if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI)
                    && inst->result_name != NULL
                    && !transpiler_versioned_name_list_add(declared_versioned_names,
                                                           &declared_versioned_count,
                                                           4096,
                                                           inst->result_name)) {
                    if (ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "too many MIR SSA locals while emitting function '%s'",
                            name != NULL ? name : "<function>");
                    }
                    codebuf_destroy(params_sig);
                    free(header_decl);
                    ctx->slot_var_count = saved_slot_count;
                    ctx->typed_var_count = saved_typed_count;
                    g_type_render_ctx = saved_render_ctx;
                    ctx->out = saved_out;
                    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                        "%s", saved_return_type);
                    return;
                }
            }
        }

        for (size_t i = 0; i < declared_versioned_count; i++) {
            const char *versioned_name = declared_versioned_names[i];
            char base[128];
            size_t version = 0;
            const char *type_name = NULL;
            const char *c_type = NULL;
            ASTNode *type_ast = NULL;
            char *c_name = NULL;
            char *initial_expr = NULL;
            char *decl = NULL;

            if (versioned_name == NULL
                || !transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version)) {
                continue;
            }
            if (transpiler_is_implicit_field(ctx, base))
                continue;
            type_name = transpiler_find_local_type_name(ctx, node, base);
            if (transpiler_type_name_is_slot_like(type_name))
                continue;
            type_ast = transpiler_find_local_type_ast(ctx, node, base);
            if (type_ast != NULL && type_ast->type == AST_EVENT_HANDLER_TYPE) {
                c_name = transpiler_render_ssa_name(ctx, versioned_name);
                decl = pergyra_ast_typed_declarator(type_ast, c_name);
                write_indent(ctx);
                codebuf_write(ctx->out, "%s = 0;\n", decl);
                write_indent(ctx);
                codebuf_write(ctx->out, "(void)%s;\n", c_name);
                free(decl);
                free(c_name);
                continue;
            }
            if (type_name != NULL)
                c_type = pergyra_type_to_c(type_name);
            if (c_type == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot determine C type for MIR local '%s' in function '%s'",
                        versioned_name,
                        name != NULL ? name : "<function>");
                }
                if (params_sig != NULL)
                    codebuf_destroy(params_sig);
                free(header_decl);
                ctx->slot_var_count = saved_slot_count;
                ctx->typed_var_count = saved_typed_count;
                g_type_render_ctx = saved_render_ctx;
                ctx->out = saved_out;
                snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                    "%s", saved_return_type);
                return;
            }
            c_name = transpiler_render_ssa_name(ctx, versioned_name);
            write_indent(ctx);
            if (version == 0) {
                bool has_param = false;
                bool has_top_level = false;
                for (size_t p = 0; p < node->data.func_decl.param_count; p++) {
                    FuncParam *param = node->data.func_decl.params[p];
                    if (param != NULL && param->name != NULL
                        && strcmp(param->name, base) == 0) {
                        has_param = true;
                        break;
                    }
                }
                if (!has_param && node->data.func_decl.body != NULL
                    && node->data.func_decl.body->type == AST_BLOCK) {
                    ASTNode *body = node->data.func_decl.body;
                    for (size_t s = 0; s < body->data.block.count; s++) {
                        ASTNode *stmt = body->data.block.statements[s];
                        if (stmt == NULL)
                            continue;
                        if (stmt->type == AST_LET_DECL
                            && stmt->data.let_decl.name != NULL
                            && strcmp(stmt->data.let_decl.name, base) == 0) {
                            has_top_level = true;
                            break;
                        }
                        if (stmt->type == AST_WITH_STMT
                            && stmt->data.with_stmt.alias != NULL
                            && strcmp(stmt->data.with_stmt.alias, base) == 0) {
                            has_top_level = true;
                            break;
                        }
                    }
                }
                if (has_param || has_top_level)
                    initial_expr = pergyra_strdup(base);
            }
            if (initial_expr != NULL) {
                codebuf_write(ctx->out, "%s %s = %s;\n", c_type, c_name, initial_expr);
            } else if (transpiler_c_type_uses_scalar_zero(c_type))
                codebuf_write(ctx->out, "%s %s = 0;\n", c_type, c_name);
            else
                codebuf_write(ctx->out, "%s %s = (%s){0};\n", c_type, c_name, c_type);
            write_indent(ctx);
            codebuf_write(ctx->out, "(void)%s;\n", c_name);
            free(initial_expr);
            free(c_name);
        }
    }

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        if (!block->is_reachable || block->is_cleanup)
            continue;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "/* emitted-from-mir */\n");
    write_indent(ctx);
    codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, mir_routine->entry_block);

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        TranspilerSSANameMap block_ssa_map = {0};
        bool block_emitted;
        bool terminator_emitted = false;
        char block_reason[512];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        transpiler_emit_mir_block_mapping_comment(ctx->out, ctx->indent,
                                                 name,
                                                 mir_routine,
                                                 block);
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n", name, block->id);
        block_emitted = transpiler_emit_mir_block_statements(ctx->out, node, mir_routine,
                                                           block, ctx, &block_ssa_map,
                                                           block_reason,
                                                           sizeof(block_reason));
        if (!block_emitted) {
            transpiler_ssa_map_clear(&block_ssa_map);
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR block emission failed in function '%s' at block %zu: %s",
                    name != NULL ? name : "<function>",
                    block->id,
                    block_reason[0] != '\0' ? block_reason : "unknown reason");
            }
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", saved_return_type);
            g_type_render_ctx = saved_render_ctx;
            ctx->out = saved_out;
            return;
        }

        /* Ensure function parameters are in SSA map for expression resolution in branches/returns */
        for (size_t p = 0; p < node->data.func_decl.param_count; p++) {
            FuncParam *param = node->data.func_decl.params[p];
            if (param != NULL && param->name != NULL) {
                if (transpiler_resolve_ssa_name(&block_ssa_map, param->name) == NULL) {
                    transpiler_ssa_name_map_set(&block_ssa_map, param->name, param->name);
                }
            }
        }
        if (owner_name != NULL
            && transpiler_resolve_ssa_name(&block_ssa_map, "self") == NULL) {
            transpiler_ssa_name_map_set(&block_ssa_map, "self", "self");
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_RESOURCE_OP) {
                continue;
            } else if (inst->kind == MIR_INST_BRANCH) {
                char *cond = emit_expression_with_ssa_map(inst->ast, ctx,
                    &block_ssa_map);
                if (cond == NULL)
                    cond = pergyra_strdup("false");
                if (block->has_succ_true)
                    transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent, i, block,
                        &mir_routine->blocks[block->succ_true]);
                write_indent(ctx);
                codebuf_write(ctx->out, "if (%s) {\n", cond != NULL ? cond : "false");
                write_indent_to(ctx->out, ctx->indent + 1);
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, block->succ_true);
                write_indent(ctx);
                codebuf_write(ctx->out, "} else {\n");
                if (block->has_succ_false)
                    transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent + 1, i, block,
                        &mir_routine->blocks[block->succ_false]);
                write_indent_to(ctx->out, ctx->indent + 1);
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, block->succ_false);
                write_indent(ctx);
                codebuf_write(ctx->out, "}\n");
                free(cond);
                terminator_emitted = true;
            } else if (inst->kind == MIR_INST_RETURN) {
                if (inst->ast != NULL) {
                    char *ret_expr = emit_expression_with_ssa_map(inst->ast, ctx,
                        &block_ssa_map);
                    write_indent(ctx);
                    codebuf_write(ctx->out, "return %s;\n", ret_expr != NULL ? ret_expr : "0");
                    free(ret_expr);
                } else {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "return;\n");
                }
                terminator_emitted = true;
            }
        }
        if (!terminator_emitted && block->has_succ_true) {
            transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent, i, block,
                &mir_routine->blocks[block->succ_true]);
            write_indent(ctx);
            codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n", name, block->succ_true);
            terminator_emitted = true;
        }
        if (!terminator_emitted && !block->has_succ_true && !block->has_succ_false) {
            write_indent(ctx);
            if (strcmp(ctx->current_return_type, "Void") == 0) {
                codebuf_write(ctx->out, "return;\n");
            } else {
                codebuf_write(ctx->out, "__builtin_unreachable();\n");
            }
        }
    }

    /* Emit cleanup blocks if present (for intent compensation) */
    if (mir_routine->has_cleanup_block
        && node != NULL
        && node->type == AST_INTENT_DECL) {
        const char *cleanup_handle = node != NULL && node->type == AST_INTENT_DECL
            ? "__intent_handle"
            : "0";
        write_indent(ctx);
        codebuf_write(ctx->out, "/* cleanup-emitted-from-mir */\n");
        for (size_t i = 0; i < mir_routine->block_count; i++) {
            const MIRBasicBlock *block = &mir_routine->blocks[i];
            if (!block->is_cleanup || !block->is_reachable)
                continue;
            write_indent(ctx);
            codebuf_write(ctx->out, "_pgy_mir_bb_%s_%zu:\n", name, block->id);
            for (size_t j = 0; j < block->instruction_count; j++) {
                const MIRInstruction *inst = &block->instructions[j];
                if (inst->kind == MIR_INST_CLEANUP_EDGE) {
                    if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst,
                                                           cleanup_handle, true)) {
                        ctx->slot_var_count = saved_slot_count;
                        ctx->typed_var_count = saved_typed_count;
                        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                            "%s", saved_return_type);
                        g_type_render_ctx = saved_render_ctx;
                        ctx->out = saved_out;
                        return;
                    }
                } else if (inst->kind == MIR_INST_RETURN) {
                    if (inst->ast != NULL) {
                        char *ret_expr = emit_expression(inst->ast, ctx);
                        write_indent(ctx);
                        codebuf_write(ctx->out, "return %s;\n", ret_expr);
                        free(ret_expr);
                    } else {
                        write_indent(ctx);
                        codebuf_write(ctx->out, "return;\n");
                    }
                }
            }
        }
    }

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
        "%s", saved_return_type);
    ctx->current_zone_name = saved_zone_name;
    ctx->current_class_name = saved_class_name;
    ctx->current_relation_name = saved_relation_name;
    ctx->current_effect_name = saved_effect_name;
    ctx->current_world_name = saved_world_name;
    ctx->current_func_decl = saved_func_decl;
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
}

void
emit_func_decl_named(ASTNode *node, const char *emitted_name,
                     CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    bool opened_body = false;
    if (ctx != NULL && ctx->mir != NULL) {
        char reason[256];
        if (transpiler_can_emit_function_from_mir_with_reason(
                ctx, node, &mir_routine, reason, sizeof(reason))) {
            emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        } else if (ctx->backend_error == NULL) {
            const char *func_name = (node != NULL && node->type == AST_FUNC_DECL
                                     && node->data.func_decl.name != NULL)
                ? node->data.func_decl.name
                : "(anonymous)";
            ctx->backend_error = strdup_fmt(
                "MIR-only C path missing routine for function '%s': %s",
                func_name,
                reason[0] != '\0' ? reason : "unknown reason");
        }
        return;
    }
    if (transpiler_can_emit_function_from_mir(ctx, node, &mir_routine)) {
        emit_func_decl_from_mir_named(node, mir_routine, emitted_name, buf, ctx);
        return;
    }
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;
    CodeBuf *saved_out = ctx->out;
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    char saved_return_type[128];
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    ctx->out = buf;
    g_type_render_ctx = ctx;
    snprintf(saved_return_type, sizeof(saved_return_type), "%s",
        ctx->current_return_type);
    if (node->data.func_decl.return_type != NULL) {
        {
            char *rendered = render_type_name(node->data.func_decl.return_type);
            snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
                "%s", rendered);
            free(rendered);
        }
    } else {
        snprintf(ctx->current_return_type, sizeof(ctx->current_return_type), "Void");
    }

    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        const char *pt = NULL;
        char *type_name = NULL;
        char *decl = NULL;
        bool boundary_slot = false;
        bool secure_slot = false;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "function parameter '%s' of '%s'",
            p != NULL && p->name != NULL ? p->name : "(anonymous)",
            name != NULL ? name : "(anonymous)");
        pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
        if (pt == NULL)
            goto emit_func_decl_named_fail;
        if (i > 0) codebuf_write(params_sig, ", ");
        if (p->type != NULL)
            type_name = render_type_name(p->type);
        boundary_slot = type_name != NULL
            && (strncmp(type_name, "Slot<", 5) == 0
                || strncmp(type_name, "SecureSlot<", 11) == 0)
            && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
        secure_slot = type_name != NULL && strncmp(type_name, "SecureSlot<", 11) == 0;
        if (boundary_slot) {
            const char *inner = slot_inner_type_name(type_name);
            codebuf_write(params_sig, "%s *%s", pt, p->name);
            if (secure_slot)
                codebuf_write(params_sig, ", PgyToken_%s %s_token", inner, p->name);
        } else if (p->type != NULL && p->type->type == AST_EVENT_HANDLER_TYPE) {
            decl = pergyra_ast_typed_declarator(p->type, p->name);
            codebuf_write(params_sig, "%s", decl);
        } else if (p->name != NULL && strcmp(p->name, "self") != 0
                   && type_name != NULL
                   && is_pointer_self_host_type_name(ctx, type_name)) {
            /* Subject parameters are passed by pointer (reference semantics) */
            codebuf_write(params_sig, "%s *%s", pt, p->name);
        } else {
            codebuf_write(params_sig, "%s %s", pt, p->name);
        }
        free(decl);
        free(type_name);
    }
    header_decl = pergyra_func_signature_declarator(node->data.func_decl.return_type,
        name, params_sig != NULL ? params_sig->data : "void");
    codebuf_write(ctx->out, "\n%s\n{\n", header_decl);
    opened_body = true;
    free(header_decl);
    header_decl = NULL;
    codebuf_destroy(params_sig);
    params_sig = NULL;

    ctx->indent++;
    for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
        FuncParam *p = node->data.func_decl.params[i];
        if (p == NULL || p->name == NULL || p->type == NULL)
            continue;
        char *type_name = render_type_name(p->type);
        if (type_name != NULL) {
            bool boundary_slot = (strncmp(type_name, "Slot<", 5) == 0
                               || strncmp(type_name, "SecureSlot<", 11) == 0)
                && (p->mode == PARAM_MODE_OWN || p->mode == PARAM_MODE_REF);
            register_typed_var(ctx, p->name, type_name);
            /* Mark pointer-self parameters (subject/relation/effect/zone/world) for -> access */
            if (p->name != NULL && strcmp(p->name, "self") != 0
                && is_pointer_self_host_type_name(ctx, type_name)) {
                TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                if (entry != NULL)
                    entry->is_subject_ref = true;
            }
            if (strncmp(type_name, "Slot<", 5) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), false, boundary_slot);
            else if (strncmp(type_name, "SecureSlot<", 11) == 0)
                register_slot_var(ctx, p->name, slot_inner_type_name(type_name), true, boundary_slot);
            free(type_name);
        }
    }
    if (node->data.func_decl.body != NULL)
        emit_block(node->data.func_decl.body, ctx);

    /* If the function returns a non-void type and the body does not end
     * with an explicit return (e.g. ends with a match/if/while), emit a
     * fallback return to silence C compiler warnings about missing return. */
    if (strcmp(ctx->current_return_type, "Void") != 0
        && strcmp(ctx->current_return_type, "void") != 0) {
        const char *ret_type = ctx->current_return_type;
        /* Simple zero-initialised return for non-void functions that may
         * fall off the end (e.g. match without default, or if/else with
         * only one branch returning). */
        write_indent(ctx);
        if (strcmp(ret_type, "Bool") == 0) {
            codebuf_write(ctx->out, "return false;  /* fallback return */\n");
        } else if (strcmp(ret_type, "String") == 0
                   || strncmp(ret_type, "char *", 6) == 0) {
            codebuf_write(ctx->out, "return NULL;  /* fallback return */\n");
        } else if (strncmp(ret_type, "Option<", 7) == 0) {
            const char *inner = slot_inner_type_name(ret_type);
            codebuf_write(ctx->out, "return None_%s();  /* fallback return */\n", inner);
        } else if (strncmp(ret_type, "Result<", 7) == 0) {
            const char *inner = slot_inner_type_name(ret_type);
            codebuf_write(ctx->out, "return Err_%s(0);  /* fallback return */\n", inner);
        } else {
            codebuf_write(ctx->out, "return 0;  /* fallback return */\n");
        }
    }

    ctx->indent--;
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
        "%s", saved_return_type);

    codebuf_write(ctx->out, "}\n");
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
    return;

emit_func_decl_named_fail:
    if (opened_body)
        ctx->indent--;
    free(header_decl);
    if (params_sig != NULL)
        codebuf_destroy(params_sig);
    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
    snprintf(ctx->current_return_type, sizeof(ctx->current_return_type),
        "%s", saved_return_type);
    g_type_render_ctx = saved_render_ctx;
    ctx->out = saved_out;
}

void
emit_func_decl(ASTNode *node, TranspilerCtx *ctx)
{
    emit_func_decl_named(node, node->data.func_decl.name, ctx->out, ctx);
}

void
emit_extern_block(ASTNode *node, TranspilerCtx *ctx)
{
    codebuf_write(ctx->out, "\n/* extern \"%s\" */\n",
                  node->data.extern_block.abi != NULL
                    ? node->data.extern_block.abi : "");

    for (size_t i = 0; i < node->data.extern_block.count; i++) {
        ASTNode *decl = node->data.extern_block.declarations[i];
        if (decl == NULL || decl->type != AST_FUNC_DECL)
            continue;

        const char *name = decl->data.func_decl.name;
        const char *ret_type = "void";
        if (decl->data.func_decl.return_type != NULL) {
            ret_type = pergyra_ast_type_to_c(decl->data.func_decl.return_type);
        }

        codebuf_write(ctx->out, "%s %s(", ret_type, name);

        for (size_t j = 0; j < decl->data.func_decl.param_count; j++) {
            FuncParam *p = decl->data.func_decl.params[j];
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "extern parameter '%s' of '%s'",
                p != NULL && p->name != NULL ? p->name : "(anonymous)",
                name != NULL ? name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
            if (j > 0) {
                codebuf_write(ctx->out, ", ");
            }
            codebuf_write(ctx->out, "%s %s", pt, p->name);
        }

        codebuf_write(ctx->out, ");\n");
    }
}

/* -----------------------------------------------------------------
 * Class declaration emitter
 * ----------------------------------------------------------------- */

/* -----------------------------------------------------------------
 * Generic class monomorphization
 * ----------------------------------------------------------------- */

static bool
class_has_generic_params(ASTNode *node)
{
    return node != NULL
        && node->type == AST_CLASS_DECL
        && node->data.class_decl.generic_params != NULL
        && node->data.class_decl.generic_params->count > 0;
}

/* Ensure a monomorphized specialization of a generic class exists.
 * Returns the specialized name (e.g. "Node_Int") that should be used
 * as the C struct type name.  The struct + methods are emitted into
 * ctx->helpers on first invocation.
 *
 * `ann` is the AST_TYPE node for the annotation (e.g. Node<Int>).
 * We extract generic_args from it and match them to class_decl's
 * generic_params to build the bindings. */
static const char *
ensure_generic_class_specialization(TranspilerCtx *ctx,
                                     ASTNode *class_decl,
                                     ASTNode *ann)
{
    GenericParams *gp = class_decl->data.class_decl.generic_params;
    GenericParams *ga = ann->data.type.generic_args;
    bool has_effective_args = false;

    if (gp == NULL)
        return class_decl->data.class_decl.name;

    /* Build specialized name: ClassName_Arg1_Arg2 */
    CodeBuf *nbuf = codebuf_create();
    codebuf_write(nbuf, "%s", class_decl->data.class_decl.name);
    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *formal = gp->params[i];
        GenericParam *garg = (ga != NULL && i < ga->count) ? ga->params[i] : NULL;
        const char *effective_name = NULL;

        if (garg != NULL && garg->name != NULL)
            effective_name = garg->name;
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            effective_name = garg->constraint->data.type.name;
        else if (formal != NULL && formal->default_type != NULL
                 && formal->default_type->type == AST_TYPE
                 && formal->default_type->data.type.name != NULL)
            effective_name = formal->default_type->data.type.name;
        else
            return class_decl->data.class_decl.name;

        has_effective_args = true;
        codebuf_write(nbuf, "_");
        append_mangled_type_name(nbuf, effective_name);
    }

    if (!has_effective_args) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    /* Check if already emitted */
    for (int i = 0; i < ctx->generic_class_spec_count; i++) {
        if (strcmp(ctx->generic_class_specs[i].specialized_name, nbuf->data) == 0) {
            const char *result = ctx->generic_class_specs[i].specialized_name;
            codebuf_destroy(nbuf);
            return result;
        }
    }

    /* Register new specialization */
    if (ctx->generic_class_spec_count >= MAX_GENERIC_CLASS_SPECIALIZATIONS) {
        codebuf_destroy(nbuf);
        return class_decl->data.class_decl.name;
    }

    GenericClassSpecEntry *entry = &ctx->generic_class_specs[ctx->generic_class_spec_count++];
    entry->class_decl = class_decl;
    snprintf(entry->specialized_name, sizeof(entry->specialized_name), "%s", nbuf->data);
    entry->emitted = true;
    const char *spec_name = entry->specialized_name;

    /* Build bindings: T -> Int, U -> String, etc. */
    int saved_binding_count = ctx->generic_binding_count;
    for (size_t i = 0; i < gp->count; i++) {
        GenericParam *formal = gp->params[i];
        GenericParam *garg = (ga != NULL && i < ga->count) ? ga->params[i] : NULL;
        const char *effective_name = NULL;

        if (ctx->generic_binding_count >= MAX_GENERIC_BINDINGS)
            break;
        GenericBindingEntry *b = &ctx->generic_bindings[ctx->generic_binding_count++];
        snprintf(b->name, sizeof(b->name), "%s",
                 gp->params[i] != NULL ? gp->params[i]->name : "T");
        /* The concrete type name comes from the annotation's generic_args.
         * ga->params[i] is a GenericParam whose 'name' field holds the
         * actual type name (e.g. "Int") when used as a type argument. */
        if (garg != NULL && garg->name != NULL)
            effective_name = garg->name;
        else if (garg != NULL && garg->constraint != NULL
                 && garg->constraint->type == AST_TYPE
                 && garg->constraint->data.type.name != NULL)
            effective_name = garg->constraint->data.type.name;
        else if (formal != NULL && formal->default_type != NULL
                 && formal->default_type->type == AST_TYPE
                 && formal->default_type->data.type.name != NULL)
            effective_name = formal->default_type->data.type.name;

        if (effective_name != NULL)
            snprintf(b->concrete_type, sizeof(b->concrete_type), "%s",
                     effective_name);
        else {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot resolve generic class binding '%s' for specialization '%s'",
                    gp->params[i] != NULL && gp->params[i]->name != NULL
                        ? gp->params[i]->name
                        : "(anonymous)",
                    class_decl != NULL && class_decl->data.class_decl.name != NULL
                        ? class_decl->data.class_decl.name
                        : "(anonymous)");
            }
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }

        entry->bindings[i] = *b;
    }
    entry->binding_count = gp->count;

    /* Set g_type_render_ctx so pergyra_ast_type_to_c resolves T → Int */
    TranspilerCtx *saved_render_ctx = g_type_render_ctx;
    g_type_render_ctx = ctx;

    /* Emit struct typedef into helpers buffer */
    codebuf_write(ctx->helpers, "\ntypedef struct %s\n{\n", spec_name);
    for (size_t i = 0; i < class_decl->data.class_decl.field_count; i++) {
        ClassField *f = class_decl->data.class_decl.fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "generic class field '%s.%s'",
            spec_name != NULL ? spec_name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL) {
            g_type_render_ctx = saved_render_ctx;
            ctx->generic_binding_count = saved_binding_count;
            codebuf_destroy(nbuf);
            return NULL;
        }
        codebuf_write(ctx->helpers, "    %s %s;\n", ft, f->name);
    }
    codebuf_write(ctx->helpers, "} %s;\n", spec_name);

    /* Container types — suppress unused function warnings for Slot/Box helpers */
    codebuf_write(ctx->helpers,
        "\n#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        spec_name, spec_name,
        spec_name, spec_name,
        spec_name, spec_name);

    /* Methods */
    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        emit_hosted_method_forward_decl_named(spec_name, method, use_self_cell,
                                              ctx->helpers, ctx);
    }

    for (size_t i = 0; i < class_decl->data.class_decl.method_count; i++) {
        ASTNode *method = class_decl->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, spec_name);
        if (method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s *self",
                          ret_type, spec_name, method_name, spec_name);
        } else {
            codebuf_write(ctx->helpers, "\n%s\n%s_%s(%s self",
                          ret_type, spec_name, method_name, spec_name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "generic class method parameter '%s.%s(%s)'",
                spec_name != NULL ? spec_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL) {
                g_type_render_ctx = saved_render_ctx;
                ctx->generic_binding_count = saved_binding_count;
                codebuf_destroy(nbuf);
                return NULL;
            }
            codebuf_write(ctx->helpers, ", %s %s", pt, p->name);
        }
        codebuf_write(ctx->helpers, ")\n{\n");

        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_class_name = ctx->current_class_name;
            CodeBuf *saved_out = ctx->out;

            ctx->current_class_name = spec_name;
            ctx->out = ctx->helpers;
            register_typed_var(ctx, "self", spec_name);

            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *tn;
                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0 || p->type == NULL)
                    continue;
                tn = render_type_name(p->type);
                if (tn != NULL) {
                    register_typed_var(ctx, p->name, tn);
                    free(tn);
                }
            }

            ctx->indent++;
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            ctx->indent--;

            ctx->out = saved_out;
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_class_name = saved_class_name;
        }

        codebuf_write(ctx->helpers, "}\n");
    }

    /* Restore bindings and render context */
    g_type_render_ctx = saved_render_ctx;
    ctx->generic_binding_count = saved_binding_count;
    codebuf_destroy(nbuf);

    return spec_name;
}

void
emit_class_decl(ASTNode *node, TranspilerCtx *ctx)
{
    /* Generic classes are emitted lazily when first used (monomorphized). */
    if (class_has_generic_params(node))
        return;

    const char *name = node->data.class_decl.name;

    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        if (f != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out, f->type);
    }
    for (size_t i = 0; i < node->data.class_decl.method_count; i++)
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out,
            node->data.class_decl.methods[i]);

    codebuf_write(ctx->out, "\ntypedef struct %s\n{\n", name);

    /* Fields */
    for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
        ClassField *f = node->data.class_decl.fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "class field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            f != NULL && f->name != NULL ? f->name : "(anonymous)");
        ft = transpiler_require_ast_c_type(ctx, f != NULL ? f->type : NULL, surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, f->name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Auto-generate Slot and Result container types for this struct.
     * This allows Slot<MyStruct>, Result<MyStruct> in user code. */
    codebuf_write(ctx->out,
        "\n/* Auto-generated container types for %s */\n"
        "#pragma GCC diagnostic push\n"
        "#pragma GCC diagnostic ignored \"-Wunused-function\"\n"
        "PGY_SLOT_DEFINE(%s, %s)\n"
        "PGY_SECURE_SLOT_DEFINE(%s, %s)\n"
        "PGY_BOX_DEFINE(%s, %s)\n"
        "#pragma GCC diagnostic pop\n",
        name,
        name, name,
        name, name,
        name, name);

    /* Methods become free functions over a subject self-cell or class value. */
    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        emit_hosted_method_forward_decl_named(name, method, use_self_cell,
                                              ctx->out, ctx);
    }

    for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
        ASTNode *method = node->data.class_decl.methods[i];
        bool use_self_cell = is_pointer_self_host_type_name(ctx, name);
        const MIRRoutine *mir_method = transpiler_find_mir_method(ctx, name, method);
        if (method->type != AST_FUNC_DECL)
            continue;
        if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR-only C path missing routine for class method '%s.%s'",
                    name != NULL ? name : "(anonymous-class)",
                    method->data.func_decl.name != NULL
                        ? method->data.func_decl.name
                        : "(anonymous)");
            }
            return;
        }
        if (mir_method != NULL) {
            char emitted_name[256];
            snprintf(emitted_name, sizeof(emitted_name), "%s_%s", name,
                method->data.func_decl.name);
            emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
            continue;
        }

        const char *method_name = method->data.func_decl.name;
        const char *ret_type    = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        if (use_self_cell) {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s *self",
                          ret_type, name, method_name, name);
        } else {
            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, name, method_name, name);
        }

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0)
                continue;
            const char *pt = NULL;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "class method parameter '%s.%s(%s)'",
                name != NULL ? name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
            {
                char *ptn = (p->type != NULL) ? render_type_name(p->type) : NULL;
                bool subj_param = ptn != NULL && is_pointer_self_host_type_name(ctx, ptn);
                if (subj_param)
                    codebuf_write(ctx->out, ", %s *%s", pt, p->name);
                else
                    codebuf_write(ctx->out, ", %s %s", pt, p->name);
                free(ptn);
            }
        }
        codebuf_write(ctx->out, ")\n{\n");

        {
            int saved_slot_count = ctx->slot_var_count;
            int saved_typed_count = ctx->typed_var_count;
            const char *saved_class_name = ctx->current_class_name;

            ctx->current_class_name = name;
            register_typed_var(ctx, "self", name);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                char *type_name;

                if (p == NULL || p->name == NULL
                    || strcmp(p->name, "self") == 0
                    || p->type == NULL)
                    continue;

                type_name = render_type_name(p->type);
                if (type_name != NULL) {
                    register_typed_var(ctx, p->name, type_name);
                    if (is_pointer_self_host_type_name(ctx, type_name)) {
                        TypedVarEntry *entry = lookup_typed_entry(ctx, p->name);
                        if (entry != NULL)
                            entry->is_subject_ref = true;
                    }
                    free(type_name);
                }
            }
            ctx->indent++;
            if (method->data.func_decl.body != NULL)
                emit_block(method->data.func_decl.body, ctx);
            ctx->indent--;
            ctx->slot_var_count = saved_slot_count;
            ctx->typed_var_count = saved_typed_count;
            ctx->current_class_name = saved_class_name;
        }

        codebuf_write(ctx->out, "}\n");
    }
}

/* -----------------------------------------------------------------
 * with slot<T> as s { }
 * ----------------------------------------------------------------- */

void
emit_with_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias = node->data.with_stmt.alias;
    bool is_secure    = node->data.with_stmt.is_secure;
    int saved_slot_count = ctx->slot_var_count;
    int saved_typed_count = ctx->typed_var_count;

    const char *inner = NULL;
    if (node->data.with_stmt.slot_type != NULL)
        inner = node->data.with_stmt.slot_type->data.type.name;
    if (inner == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "cannot emit with-slot alias '%s': missing explicit slot type",
                alias != NULL ? alias : "(anonymous)");
        }
        return;
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    /* Register alias in slot variable table */
    register_slot_var(ctx, alias, inner, is_secure, false);

    if (is_secure) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgyToken_%s %s_token;\n", inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s_token);\n",
            inner, alias, inner, alias);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", alias);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "PgySlot_%s %s = pgy_claim_%s();\n",
            inner, alias, inner);
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", alias);
    }

    if (node->data.with_stmt.body != NULL)
        emit_block(node->data.with_stmt.body, ctx);

    /* Auto-release */
    write_indent(ctx);
    if (is_secure) {
        codebuf_write(ctx->out,
            "pgy_secure_release_%s(&%s, &%s_token);\n",
            inner, alias, alias);
    } else {
        codebuf_write(ctx->out,
            "pgy_release_%s(&%s);\n", inner, alias);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    ctx->slot_var_count = saved_slot_count;
    ctx->typed_var_count = saved_typed_count;
}

/* -----------------------------------------------------------------
 * Parallel block
 * ----------------------------------------------------------------- */

static bool
transpiler_parallel_capture_has_name(char names[MAX_SLOT_VARS][64],
                                     int count,
                                     const char *name)
{
    if (name == NULL)
        return false;
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static void
transpiler_parallel_add_capture_name(TranspilerCtx *ctx,
                                     const char *name,
                                     char slot_names[MAX_SLOT_VARS][64],
                                     int *slot_count,
                                     char typed_names[MAX_SLOT_VARS][64],
                                     int *typed_count)
{
    if (ctx == NULL || name == NULL || name[0] == '\0'
        || strcmp(name, "self") == 0) {
        return;
    }

    if (is_slot_var(ctx, name)) {
        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && slot_count != NULL && *slot_count < MAX_SLOT_VARS) {
            strncpy(slot_names[*slot_count], name, sizeof(slot_names[*slot_count]) - 1);
            slot_names[*slot_count][sizeof(slot_names[*slot_count]) - 1] = '\0';
            (*slot_count)++;
        }
        return;
    }

    if (lookup_typed_entry(ctx, name) != NULL) {
        if (!transpiler_parallel_capture_has_name(slot_names,
                slot_count != NULL ? *slot_count : 0, name)
            && !transpiler_parallel_capture_has_name(typed_names,
                typed_count != NULL ? *typed_count : 0, name)
            && typed_count != NULL && *typed_count < MAX_SLOT_VARS) {
            strncpy(typed_names[*typed_count], name, sizeof(typed_names[*typed_count]) - 1);
            typed_names[*typed_count][sizeof(typed_names[*typed_count]) - 1] = '\0';
            (*typed_count)++;
        }
    }
}

static void
transpiler_parallel_collect_stmt_captures(ASTNode *node,
                                          TranspilerCtx *ctx,
                                          char slot_names[MAX_SLOT_VARS][64],
                                          int *slot_count,
                                          char typed_names[MAX_SLOT_VARS][64],
                                          int *typed_count)
{
    if (node == NULL || ctx == NULL)
        return;

    switch (node->type) {
    case AST_IDENTIFIER:
        transpiler_parallel_add_capture_name(ctx, node->data.identifier.name,
            slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_LET_DECL:
        transpiler_parallel_collect_stmt_captures(node->data.let_decl.initializer,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ASSIGNMENT:
        transpiler_parallel_collect_stmt_captures(node->data.assignment.target,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.assignment.value,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CHANNEL_SEND:
        transpiler_parallel_collect_stmt_captures(node->data.channel_send.channel,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.channel_send.value,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CHANNEL_RECV:
        transpiler_parallel_collect_stmt_captures(node->data.channel_recv.channel,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_CALL:
        transpiler_parallel_collect_stmt_captures(node->data.call.callee,
            ctx, slot_names, slot_count, typed_names, typed_count);
        for (size_t i = 0; i < node->data.call.arg_count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.call.arguments[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_MEMBER_ACCESS:
        transpiler_parallel_collect_stmt_captures(node->data.member.object,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ARRAY_ACCESS:
        transpiler_parallel_collect_stmt_captures(node->data.array_access.array,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.array_access.index,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.array_literal.elements[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_BINARY:
        transpiler_parallel_collect_stmt_captures(node->data.binary.left,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.binary.right,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_UNARY:
        transpiler_parallel_collect_stmt_captures(node->data.unary.operand,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_AWAIT_EXPR:
        transpiler_parallel_collect_stmt_captures(node->data.await_expr.expression,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_SPAWN_EXPR:
        transpiler_parallel_collect_stmt_captures(node->data.spawn_expr.function,
            ctx, slot_names, slot_count, typed_names, typed_count);
        for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.spawn_expr.arguments[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_IF_STMT:
        transpiler_parallel_collect_stmt_captures(node->data.if_stmt.condition,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.if_stmt.then_branch,
            ctx, slot_names, slot_count, typed_names, typed_count);
        transpiler_parallel_collect_stmt_captures(node->data.if_stmt.else_branch,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_RETURN:
        transpiler_parallel_collect_stmt_captures(node->data.return_stmt.value,
            ctx, slot_names, slot_count, typed_names, typed_count);
        break;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.block.statements[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++) {
            transpiler_parallel_collect_stmt_captures(node->data.parallel.tasks[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
            transpiler_parallel_collect_stmt_captures(
                node->data.async_block.statements[i],
                ctx, slot_names, slot_count, typed_names, typed_count);
        }
        break;
    default:
        break;
    }
}

void
emit_parallel_block(ASTNode *node, TranspilerCtx *ctx)
{
    size_t count = node->data.parallel.task_count;
    if (count == 0)
        return;

    unsigned int pid = ctx->parallel_id++;

    /* ---------------------------------------------------------------
     * 1) Generate a context struct that holds pointers to all local
     *    variables currently in scope (slots + non-duplicate typed vars).
     *    Wrapper functions access outer variables through this struct.
     * --------------------------------------------------------------- */
    char capture_slot_names[MAX_SLOT_VARS][64] = {{0}};
    char capture_typed_names[MAX_SLOT_VARS][64] = {{0}};
    int capture_slot_count = 0;
    int capture_typed_count = 0;

    for (size_t i = 0; i < count; i++) {
        transpiler_parallel_collect_stmt_captures(node->data.parallel.tasks[i], ctx,
            capture_slot_names, &capture_slot_count,
            capture_typed_names, &capture_typed_count);
    }

    bool has_captures = (capture_slot_count > 0 || capture_typed_count > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers,
            "typedef struct {\n");
        for (int i = 0; i < capture_slot_count; i++) {
            const char *name = capture_slot_names[i];
            const char *inner = lookup_slot_type(ctx, name);
            bool secure = lookup_slot_is_secure(ctx, name);
            codebuf_write(ctx->helpers,
                secure ? "    PgySecureSlot_%s *%s;\n" : "    PgySlot_%s *%s;\n",
                inner != NULL ? inner : "Int",
                name);
        }
        for (int i = 0; i < capture_typed_count; i++) {
            TypedVarEntry *entry = lookup_typed_entry(ctx, capture_typed_names[i]);
            char surface_desc[256];
            const char *c_type;
            snprintf(surface_desc, sizeof(surface_desc),
                "parallel capture '%s'",
                capture_typed_names[i]);
            c_type = transpiler_require_type_name_c_type(
                ctx,
                entry != NULL ? entry->type_name : NULL,
                surface_desc);
            if (c_type == NULL)
                return;
            codebuf_write(ctx->helpers,
                "    %s *%s;\n", c_type,
                capture_typed_names[i]);
        }
        codebuf_write(ctx->helpers,
            "} _pgy_par_ctx_%u;\n\n", pid);
    }

    /* ---------------------------------------------------------------
     * 2) Generate static wrapper functions for each task.
     *    Variable references inside the wrapper go through _pctx->.
     * --------------------------------------------------------------- */
    for (size_t i = 0; i < count; i++) {
        codebuf_write(ctx->helpers,
            "static void *_pgy_par_%zu_%u(void *_arg) {\n",
            i, pid);
        if (has_captures) {
            codebuf_write(ctx->helpers,
                "    _pgy_par_ctx_%u *_pctx = "
                "(_pgy_par_ctx_%u *)_arg;\n",
                pid, pid);
        } else {
            codebuf_write(ctx->helpers, "    (void)_arg;\n");
        }

        /* Redirect output to helpers and set parallel-capture mode */
        CodeBuf *saved = ctx->out;
        int saved_indent = ctx->indent;
        bool saved_in_pw = ctx->in_parallel_wrapper;
        int saved_slot_count  = ctx->par_capture_slot_count;
        int saved_typed_count = ctx->par_capture_typed_count;
        char saved_slot_names[MAX_SLOT_VARS][64];
        char saved_typed_names[MAX_SLOT_VARS][64];

        ctx->out = ctx->helpers;
        ctx->indent = 1;
        ctx->in_parallel_wrapper  = true;
        memcpy(saved_slot_names, ctx->par_capture_slot_names, sizeof(saved_slot_names));
        memcpy(saved_typed_names, ctx->par_capture_typed_names, sizeof(saved_typed_names));
        memcpy(ctx->par_capture_slot_names, capture_slot_names, sizeof(capture_slot_names));
        memcpy(ctx->par_capture_typed_names, capture_typed_names, sizeof(capture_typed_names));
        ctx->par_capture_slot_count = capture_slot_count;
        ctx->par_capture_typed_count = capture_typed_count;

        emit_statement(node->data.parallel.tasks[i], ctx);

        ctx->out = saved;
        ctx->indent = saved_indent;
        ctx->in_parallel_wrapper  = saved_in_pw;
        memcpy(ctx->par_capture_slot_names, saved_slot_names, sizeof(saved_slot_names));
        memcpy(ctx->par_capture_typed_names, saved_typed_names, sizeof(saved_typed_names));
        ctx->par_capture_slot_count = saved_slot_count;
        ctx->par_capture_typed_count = saved_typed_count;

        codebuf_write(ctx->helpers,
            "    return NULL;\n"
            "}\n\n");
    }

    /* ---------------------------------------------------------------
     * 3) Emit context initialization + spawn + await at call site.
     * --------------------------------------------------------------- */
    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out, "_pgy_par_ctx_%u _pctx%u = { ", pid, pid);
        bool first = true;
        for (int i = 0; i < capture_slot_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", capture_slot_names[i]);
            first = false;
        }
        for (int i = 0; i < capture_typed_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", capture_typed_names[i]);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }

    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        if (has_captures) {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, &_pctx%u);\n",
                i, i, pid, pid);
        } else {
            codebuf_write(ctx->out,
                "PgyTaskHandle _ph_%zu = pgy_spawn(_pgy_par_%zu_%u, NULL);\n",
                i, i, pid);
        }
    }
    for (size_t i = 0; i < count; i++) {
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_await(_ph_%zu);\n", i);
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

static void
emit_async_block(ASTNode *node, TranspilerCtx *ctx)
{
    unsigned int pid = ctx->parallel_id++;
    char capture_slot_names[MAX_SLOT_VARS][64] = {{0}};
    char capture_typed_names[MAX_SLOT_VARS][64] = {{0}};
    int capture_slot_count = 0;
    int capture_typed_count = 0;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
        transpiler_parallel_collect_stmt_captures(node->data.async_block.statements[i], ctx,
            capture_slot_names, &capture_slot_count,
            capture_typed_names, &capture_typed_count);
    }

    bool has_captures = (capture_slot_count > 0 || capture_typed_count > 0);

    if (has_captures) {
        codebuf_write(ctx->helpers, "typedef struct {\n");
        for (int i = 0; i < capture_slot_count; i++) {
            const char *name = capture_slot_names[i];
            const char *inner = lookup_slot_type(ctx, name);
            bool secure = lookup_slot_is_secure(ctx, name);
            codebuf_write(ctx->helpers, secure
                ? "    PgySecureSlot_%s *%s;\n"
                : "    PgySlot_%s *%s;\n",
                inner != NULL ? inner : "Int", name);
        }
        for (int i = 0; i < capture_typed_count; i++) {
            TypedVarEntry *entry = lookup_typed_entry(ctx, capture_typed_names[i]);
            char surface_desc[256];
            const char *c_type;
            snprintf(surface_desc, sizeof(surface_desc),
                "async capture '%s'",
                capture_typed_names[i]);
            c_type = transpiler_require_type_name_c_type(
                ctx,
                entry != NULL ? entry->type_name : NULL,
                surface_desc);
            if (c_type == NULL)
                return;
            codebuf_write(ctx->helpers, "    %s *%s;\n",
                c_type,
                capture_typed_names[i]);
        }
        codebuf_write(ctx->helpers, "} _pgy_async_ctx_%u;\n\n", pid);
    }

    codebuf_write(ctx->helpers, "static void *_pgy_async_%u(void *_arg) {\n", pid);
    if (has_captures) {
        codebuf_write(ctx->helpers,
            "    _pgy_async_ctx_%u *_pctx = (_pgy_async_ctx_%u *)_arg;\n",
            pid, pid);
        codebuf_write(ctx->helpers,
            "    if (_pctx == NULL) return NULL;\n");
    } else {
        codebuf_write(ctx->helpers, "    (void)_arg;\n");
    }

    CodeBuf *saved = ctx->out;
    int saved_indent = ctx->indent;
    bool saved_in_pw = ctx->in_parallel_wrapper;
    int saved_slot_count  = ctx->par_capture_slot_count;
    int saved_typed_count = ctx->par_capture_typed_count;
    char saved_slot_names[MAX_SLOT_VARS][64];
    char saved_typed_names[MAX_SLOT_VARS][64];

    ctx->out = ctx->helpers;
    ctx->indent = 1;
    ctx->in_parallel_wrapper  = true;
    memcpy(saved_slot_names, ctx->par_capture_slot_names, sizeof(saved_slot_names));
    memcpy(saved_typed_names, ctx->par_capture_typed_names, sizeof(saved_typed_names));
    memcpy(ctx->par_capture_slot_names, capture_slot_names, sizeof(capture_slot_names));
    memcpy(ctx->par_capture_typed_names, capture_typed_names, sizeof(capture_typed_names));
    ctx->par_capture_slot_count = capture_slot_count;
    ctx->par_capture_typed_count = capture_typed_count;

    for (size_t i = 0; i < node->data.async_block.statement_count; i++)
        emit_statement(node->data.async_block.statements[i], ctx);

    ctx->out = saved;
    ctx->indent = saved_indent;
    ctx->in_parallel_wrapper  = saved_in_pw;
    memcpy(ctx->par_capture_slot_names, saved_slot_names, sizeof(saved_slot_names));
    memcpy(ctx->par_capture_typed_names, saved_typed_names, sizeof(saved_typed_names));
    ctx->par_capture_slot_count = saved_slot_count;
    ctx->par_capture_typed_count = saved_typed_count;

    if (has_captures)
        codebuf_write(ctx->helpers, "    free(_pctx);\n");
    codebuf_write(ctx->helpers, "    return NULL;\n}\n\n");

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (has_captures) {
        write_indent(ctx);
        codebuf_write(ctx->out,
            "_pgy_async_ctx_%u *_pctx%u = (_pgy_async_ctx_%u *)malloc(sizeof(_pgy_async_ctx_%u));\n",
            pid, pid, pid, pid);
        write_indent(ctx);
        codebuf_write(ctx->out,
            "if (_pctx%u == NULL) { PGY_PANIC(\"async capture allocation failed\"); }\n",
            pid);
        write_indent(ctx);
        codebuf_write(ctx->out, "*_pctx%u = (_pgy_async_ctx_%u){ ", pid, pid);
        bool first = true;
        for (int i = 0; i < capture_slot_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", capture_slot_names[i]);
            first = false;
        }
        for (int i = 0; i < capture_typed_count; i++) {
            if (!first) codebuf_write(ctx->out, ", ");
            codebuf_write(ctx->out, "&%s", capture_typed_names[i]);
            first = false;
        }
        codebuf_write(ctx->out, " };\n");
    }
    write_indent(ctx);
    if (has_captures) {
        codebuf_write(ctx->out,
            "PgyTaskHandle _ah_%u = pgy_async_spawn(_pgy_async_%u, _pctx%u);\n",
            pid, pid, pid);
    } else {
        codebuf_write(ctx->out,
            "PgyTaskHandle _ah_%u = pgy_async_spawn(_pgy_async_%u, NULL);\n",
            pid, pid);
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "pgy_async_detach(_ah_%u);\n", pid);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

/* -----------------------------------------------------------------
 * Control flow
 * ----------------------------------------------------------------- */

void
emit_if_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.if_stmt.condition, ctx);
    write_indent(ctx);
    codebuf_write(ctx->out, "if (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.if_stmt.then_branch != NULL)
        emit_block(node->data.if_stmt.then_branch, ctx);
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");

    if (node->data.if_stmt.else_branch != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_statement(node->data.if_stmt.else_branch, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }
}

static int
transpiler_find_loop_label_depth(const TranspilerCtx *ctx, const char *label)
{
    if (ctx == NULL || label == NULL)
        return -1;

    for (int i = ctx->loop_depth - 1; i >= 0; i--) {
        if (ctx->loop_labels[i] != NULL
            && strcmp(ctx->loop_labels[i], label) == 0) {
            return i;
        }
    }

    return -1;
}

void
emit_for_loop(ASTNode *node, TranspilerCtx *ctx)
{
    const char *var = node->data.for_loop.variable;
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;

    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = node->data.for_loop.label;
        snprintf(ctx->loop_break_labels[loop_slot],
                 sizeof(ctx->loop_break_labels[loop_slot]),
                 "_pgy_loop_break_%d", loop_id);
        snprintf(ctx->loop_continue_labels[loop_slot],
                 sizeof(ctx->loop_continue_labels[loop_slot]),
                 "_pgy_loop_continue_%d", loop_id);
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_depth++;
    }

    /* for-in collection loop: for item in array { } */
    if (node->data.for_loop.iterable != NULL) {
        char *coll = emit_expression(node->data.for_loop.iterable, ctx);
        const char *coll_type = infer_expression_type_name(ctx,
            node->data.for_loop.iterable);
        const char *elem_type = NULL;
        const char *length_field = "count";  /* default for List */
        if (coll_type != NULL
            && (strncmp(coll_type, "Array<", 6) == 0
                || strncmp(coll_type, "Slice<", 6) == 0)) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
            length_field = "length";  /* Array uses .length, not .count */
        } else if (coll_type != NULL && strncmp(coll_type, "List<", 5) == 0) {
            elem_type = pergyra_type_to_c(slot_inner_type_name(coll_type));
            length_field = "count";  /* List uses .count */
        }
        if (elem_type == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot derive concrete element type for for-in iterable '%s'",
                    coll_type != NULL ? coll_type : "(unknown)");
            }
            free(coll);
            return;
        }

        int idx_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out,
            "for (size_t _pgy_idx_%d = 0; "
            "_pgy_idx_%d < %s.%s; "
            "_pgy_idx_%d++)\n",
            idx_id, idx_id, coll, length_field, idx_id);
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s %s = %s.data[_pgy_idx_%d];\n",
            elem_type, var, coll, idx_id);
        register_typed_var(ctx, var, slot_inner_type_name(coll_type));
        if (node->data.for_loop.body != NULL)
            emit_block(node->data.for_loop.body, ctx);
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_continue_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_continue_labels[loop_slot]);
        }
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
            && ctx->loop_break_label_used[loop_slot]) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s: ;\n",
                ctx->loop_break_labels[loop_slot]);
        }
        if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
            ctx->loop_depth--;
            ctx->loop_labels[loop_slot] = NULL;
        }
        free(coll);
        return;
    }

    /* Range loop: for x in start..end { } */
    char *start = emit_expression(node->data.for_loop.range_start, ctx);
    char *end   = emit_expression(node->data.for_loop.range_end,   ctx);

    write_indent(ctx);
    codebuf_write(ctx->out,
        "for (int32_t %s = %s; %s < %s; %s++)\n",
        var, start, var, end, var);
    free(start);
    free(end);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.for_loop.body != NULL)
        emit_block(node->data.for_loop.body, ctx);
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
}

void
emit_while_loop(ASTNode *node, TranspilerCtx *ctx)
{
    char *cond = emit_expression(node->data.while_loop.condition, ctx);
    int loop_slot = ctx->loop_depth;
    int loop_id = ++ctx->tmp_counter;
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_labels[loop_slot] = node->data.while_loop.label;
        snprintf(ctx->loop_break_labels[loop_slot],
                 sizeof(ctx->loop_break_labels[loop_slot]),
                 "_pgy_loop_break_%d", loop_id);
        snprintf(ctx->loop_continue_labels[loop_slot],
                 sizeof(ctx->loop_continue_labels[loop_slot]),
                 "_pgy_loop_continue_%d", loop_id);
        ctx->loop_break_label_used[loop_slot] = false;
        ctx->loop_continue_label_used[loop_slot] = false;
        ctx->loop_depth++;
    }
    write_indent(ctx);
    codebuf_write(ctx->out, "while (%s)\n", cond);
    free(cond);

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;
    if (node->data.while_loop.body != NULL)
        emit_block(node->data.while_loop.body, ctx);
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_continue_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_continue_labels[loop_slot]);
    }
    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH
        && ctx->loop_break_label_used[loop_slot]) {
        write_indent(ctx);
        codebuf_write(ctx->out, "%s: ;\n",
            ctx->loop_break_labels[loop_slot]);
    }
    if (loop_slot < TRANSPILE_MAX_LOOP_DEPTH) {
        ctx->loop_depth--;
        ctx->loop_labels[loop_slot] = NULL;
    }
}

/* Check if a match-case pattern is a destructor like Ok(x), Err(x),
 * or a tagged union variant like Circle(r), Rect(w, h) */
static bool
is_result_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    if (pat == NULL || pat->type != AST_CALL)
        return false;
    if (pat->data.call.callee == NULL || pat->data.call.callee->type != AST_IDENTIFIER)
        return false;
    const char *name = pat->data.call.callee->data.identifier.name;
    if (strcmp(name, "Ok") != 0 && strcmp(name, "Err") != 0)
        return false;
    *kind = name;
    if (pat->data.call.arg_count > 0
        && pat->data.call.arguments[0] != NULL
        && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
        *binding = pat->data.call.arguments[0]->data.identifier.name;
    } else {
        *binding = NULL;
    }
    return true;
}

static bool
is_option_destructor(ASTNode *pat, const char **kind, const char **binding)
{
    *kind = NULL;
    *binding = NULL;

    if (pat == NULL)
        return false;

    if (pat->type == AST_IDENTIFIER) {
        const char *name = pat->data.identifier.name;
        if (name != NULL && strcmp(name, "None") == 0) {
            *kind = "None";
            return true;
        }
        return false;
    }

    if (pat->type != AST_CALL
        || pat->data.call.callee == NULL
        || pat->data.call.callee->type != AST_IDENTIFIER) {
        return false;
    }

    const char *name = pat->data.call.callee->data.identifier.name;
    if (name == NULL)
        return false;

    if (strcmp(name, "None") == 0 && pat->data.call.arg_count == 0) {
        *kind = "None";
        return true;
    }
    if (strcmp(name, "Some") == 0 && pat->data.call.arg_count == 1) {
        *kind = "Some";
        if (pat->data.call.arguments[0] != NULL
            && pat->data.call.arguments[0]->type == AST_IDENTIFIER) {
            *binding = pat->data.call.arguments[0]->data.identifier.name;
        }
        return true;
    }

    return false;
}

/* Check if pattern is a tagged union variant destructor: Circle(r), Rect(w, h), None */
static bool
is_enum_variant_destructor(ASTNode *pat, TranspilerCtx *ctx,
                           const char **variant_name_out,
                           const char **enum_name_out,
                           const char ***bindings_out,
                           size_t *binding_count_out)
{
    const char *name = NULL;
    size_t argc = 0;

    if (pat == NULL) return false;

    if (pat->type == AST_CALL
        && pat->data.call.callee != NULL
        && pat->data.call.callee->type == AST_IDENTIFIER) {
        name = pat->data.call.callee->data.identifier.name;
        argc = pat->data.call.arg_count;
    } else if (pat->type == AST_IDENTIFIER) {
        name = pat->data.identifier.name;
        argc = 0;
    } else {
        return false;
    }

    if (name == NULL) return false;

    /* Look up in the active program inventory. */
    size_t type_count = 0;
    ASTNode **types = NULL;
    transpiler_active_inventory(ctx, AST_ENUM_DECL, &types, &type_count);
    if (types == NULL) {
        return false;
    }
    for (size_t i = 0; i < type_count; i++) {
        ASTNode *stmt = types[i];
        if (stmt == NULL || stmt->type != AST_ENUM_DECL)
            continue;
        /* Only tagged unions (at least one variant has data) */
        bool has_data = false;
        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (stmt->data.enum_decl.variant_param_counts != NULL
                && stmt->data.enum_decl.variant_param_counts[j] > 0) {
                has_data = true;
                break;
            }
        }
        if (!has_data) continue;

        for (size_t j = 0; j < stmt->data.enum_decl.variant_count; j++) {
            if (strcmp(stmt->data.enum_decl.variants[j], name) == 0) {
                *variant_name_out = name;
                *enum_name_out = stmt->data.enum_decl.name;
                /* Collect bindings */
                static const char *bindings_buf[8];
                *binding_count_out = 0;
                for (size_t k = 0; k < argc && k < 8; k++) {
                    ASTNode *arg = pat->data.call.arguments[k];
                    if (arg != NULL && arg->type == AST_IDENTIFIER)
                        bindings_buf[k] = arg->data.identifier.name;
                    else
                        bindings_buf[k] = NULL;
                    (*binding_count_out)++;
                }
                *bindings_out = bindings_buf;
                return true;
            }
        }
    }
    return false;
}

void
emit_match_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    char *subj = emit_expression(node->data.match_stmt.subject, ctx);
    int tmp_id = ctx->tmp_counter++;
    const char *subject_type = infer_expression_type_name(ctx, node->data.match_stmt.subject);
    bool subject_is_option = subject_type != NULL && strncmp(subject_type, "Option<", 7) == 0;

    /* Detect if any case uses Ok()/Err() or tagged union destructuring */
    bool is_result_match = false;
    bool is_option_match = false;
    bool is_enum_match = false;
    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *pat = node->data.match_stmt.cases[i]->data.match_case.pattern;
        const char *k, *b;
        if (subject_is_option && is_option_destructor(pat, &k, &b)) {
            is_option_match = true;
            break;
        }
        if (is_result_destructor(pat, &k, &b)) {
            is_result_match = true;
            break;
        }
        const char *vn, *en; const char **bs; size_t bc;
        if (is_enum_variant_destructor(pat, ctx, &vn, &en, &bs, &bc)) {
            is_enum_match = true;
            break;
        }
    }

    write_indent(ctx);
    codebuf_write(ctx->out, "{\n");
    ctx->indent++;

    if (is_result_match || is_option_match || is_enum_match) {
        /* Struct match: store subject as-is (tagged union/result) */
        write_indent(ctx);
        codebuf_write(ctx->out, "__typeof__(%s) __match_%d = %s;\n", subj, tmp_id, subj);
    } else {
        write_indent(ctx);
        codebuf_write(ctx->out, "int32_t __match_%d = %s;\n", tmp_id, subj);
    }
    free(subj);

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        const char *kind = NULL, *binding = NULL;
        bool option_case = subject_is_option
            && is_option_destructor(mc->data.match_case.pattern, &kind, &binding);

        write_indent(ctx);

        if (mc->data.match_case.patterns != NULL
            && mc->data.match_case.pattern_count > 1) {
            if (i == 0)
                codebuf_write(ctx->out, "if (");
            else
                codebuf_write(ctx->out, "else if (");
            for (size_t p = 0; p < mc->data.match_case.pattern_count; p++) {
                char *pat = emit_expression(mc->data.match_case.patterns[p], ctx);
                if (p > 0)
                    codebuf_write(ctx->out, " || ");
                codebuf_write(ctx->out, "__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        } else if (option_case) {
            const char *tag_val = (strcmp(kind, "Some") == 0)
                ? "PgyOptionSome" : "PgyOptionNone";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else if (is_result_destructor(mc->data.match_case.pattern, &kind, &binding)) {
            /* case Ok(x): → if (__match_N.tag == PGY_RESULT_OK) { ... } */
            const char *tag_val = (strcmp(kind, "Ok") == 0)
                ? "PgyResultOk" : "PgyResultErr";
            if (i == 0)
                codebuf_write(ctx->out, "if (__match_%d.tag == %s",
                    tmp_id, tag_val);
            else
                codebuf_write(ctx->out, "else if (__match_%d.tag == %s",
                    tmp_id, tag_val);
        } else {
            /* Check for tagged union variant pattern */
            const char *vname = NULL, *ename = NULL;
            const char **bindings = NULL;
            size_t bind_count = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vname, &ename, &bindings, &bind_count)) {
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                else
                    codebuf_write(ctx->out, "else if (__match_%d.tag == %s_TAG_%s",
                        tmp_id, ename, vname);
                /* Mark for binding emission below */
                kind = vname;
            } else {
                char *pat = emit_expression(mc->data.match_case.pattern, ctx);
                if (i == 0)
                    codebuf_write(ctx->out, "if (__match_%d == %s", tmp_id, pat);
                else
                    codebuf_write(ctx->out, "else if (__match_%d == %s", tmp_id, pat);
                free(pat);
            }
        }

        if (mc->data.match_case.guard != NULL) {
            char *guard = emit_expression(mc->data.match_case.guard, ctx);
            codebuf_write(ctx->out, " && %s", guard);
            free(guard);
        }
        codebuf_write(ctx->out, ")\n");

        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;

        /* Emit binding variable if destructuring */
        if (binding != NULL && kind != NULL) {
            write_indent(ctx);
            if (strcmp(kind, "Some") == 0)
                codebuf_write(ctx->out, "__typeof__(__match_%d.value) %s = __match_%d.value;\n",
                    tmp_id, binding, tmp_id);
            else if (strcmp(kind, "Ok") == 0)
                codebuf_write(ctx->out, "int32_t %s = __match_%d.ok;\n",
                    binding, tmp_id);
            else if (strcmp(kind, "Err") == 0)
                codebuf_write(ctx->out, "PgyError %s = __match_%d.err;\n",
                    binding, tmp_id);
        }
        /* Emit enum variant bindings: case Circle(r) → int32_t r = __match_N.Circle._0 */
        if (kind != NULL
            && strcmp(kind, "Some") != 0
            && strcmp(kind, "None") != 0
            && strcmp(kind, "Ok") != 0
            && strcmp(kind, "Err") != 0) {
            const char *vn2 = NULL, *en2 = NULL;
            const char **bs2 = NULL;
            size_t bc2 = 0;
            if (is_enum_variant_destructor(mc->data.match_case.pattern, ctx,
                                            &vn2, &en2, &bs2, &bc2)) {
                for (size_t b = 0; b < bc2; b++) {
                    if (bs2[b] != NULL) {
                        write_indent(ctx);
                        codebuf_write(ctx->out,
                            "int32_t %s = __match_%d.%s._%zu;\n",
                            bs2[b], tmp_id, vn2, b);
                    }
                }
            }
        }

        emit_block(mc->data.match_case.body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    if (node->data.match_stmt.default_body != NULL) {
        write_indent(ctx);
        codebuf_write(ctx->out, "else\n");
        write_indent(ctx);
        codebuf_write(ctx->out, "{\n");
        ctx->indent++;
        emit_block(node->data.match_stmt.default_body, ctx);
        ctx->indent--;
        write_indent(ctx);
        codebuf_write(ctx->out, "}\n");
    }

    ctx->indent--;
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
}

void
emit_return_stmt(ASTNode *node, TranspilerCtx *ctx)
{
    write_indent(ctx);
    if (node->data.return_stmt.value != NULL) {
        if (ctx->current_return_type[0] != '\0'
            && strncmp(ctx->current_return_type, "Option<", 7) == 0) {
            const char *inner = slot_inner_type_name(ctx->current_return_type);
            ASTNode *value = node->data.return_stmt.value;
            if (value->type == AST_CALL
                && value->data.call.callee != NULL
                && value->data.call.callee->type == AST_IDENTIFIER) {
                const char *callee_name = value->data.call.callee->data.identifier.name;
                if (strcmp(callee_name, "Some") == 0 && value->data.call.arg_count == 1) {
                    char *arg = emit_expression(value->data.call.arguments[0], ctx);
                    codebuf_write(ctx->out, "return Some_%s(%s);\n", inner, arg);
                    free(arg);
                    return;
                }
                if (strcmp(callee_name, "None") == 0 && value->data.call.arg_count == 0) {
                    codebuf_write(ctx->out, "return None_%s();\n", inner);
                    return;
                }
            }
        }
        char *val = emit_expression(node->data.return_stmt.value, ctx);
        codebuf_write(ctx->out, "return %s;\n", val);
        free(val);
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
}

/* -----------------------------------------------------------------
 * Statement dispatcher
 * ----------------------------------------------------------------- */

static void
emit_type_alias_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *alias_name;
    const char *target_c_type;

    if (node == NULL || node->type != AST_TYPE_ALIAS
        || node->data.type_alias.name == NULL
        || node->data.type_alias.target_type == NULL) {
        return;
    }

    alias_name = node->data.type_alias.name;
    ensure_type_specializations_from_ast(ctx, node->data.type_alias.target_type);
    target_c_type = pergyra_ast_type_to_c(node->data.type_alias.target_type);
    codebuf_write(ctx->out, "typedef %s %s;\n", target_c_type, alias_name);
}

void
emit_statement(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    switch (node->type) {
    case AST_LET_DECL:
        emit_let_decl(node, ctx);
        break;
    case AST_TYPE_ALIAS:
        emit_type_alias_decl(node, ctx);
        break;
    case AST_LET_DESTRUCTURE:
    {
        /* let (a, b, c) = expr;
         * → auto _tmp = expr;
         *   Type a = _tmp.data[0]; // Array
         *   Type b = _tmp.data[1]; */
        ASTNode *init = node->data.let_destructure.initializer;
        char *init_expr = emit_expression(init, ctx);
        const char *init_type = infer_expression_type_name(ctx, init);
        const char *c_init_type = pergyra_type_to_c(init_type);
        const char *elem_c_type = NULL;
        const char *inner = NULL;
        if (init_type != NULL
            && (strncmp(init_type, "Array<", 6) == 0
                || strncmp(init_type, "Slice<", 6) == 0)) {
            inner = slot_inner_type_name(init_type);
            elem_c_type = pergyra_type_to_c(inner);
        }
        if (c_init_type == NULL || elem_c_type == NULL || inner == NULL) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "cannot lower destructuring initializer of type '%s' to a concrete array element type",
                    init_type != NULL ? init_type : "(unknown)");
            }
            return;
        }
        int tmp_id = ++ctx->tmp_counter;
        write_indent(ctx);
        codebuf_write(ctx->out, "%s _pgy_destr_%d = %s;\n",
            c_init_type, tmp_id, init_expr);
        for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s %s = _pgy_destr_%d.data[%zu];\n",
                elem_c_type, node->data.let_destructure.names[i], tmp_id, i);
            register_typed_var(ctx, node->data.let_destructure.names[i], inner);
        }
        free(init_expr);
        break;
    }
    case AST_FUNC_DECL:
        emit_func_decl(node, ctx);
        break;
    case AST_CLASS_DECL:
        emit_class_decl(node, ctx);
        break;
    case AST_EXTERN_BLOCK:
        emit_extern_block(node, ctx);
        break;
    case AST_IMPORT_DECL:
        /* Import is resolved at driver level (AST merging).
         * Nothing to emit — the imported declarations are
         * already present in the merged AST. */
        break;
    case AST_USE_DECL:
        /* use module; — standard library modules.
         * Runtime functions are always available via pgy_runtime.h.
         * Future: emit module-specific includes/initializers here. */
        codebuf_write(ctx->out, "/* use %s */\n",
            node->data.use_decl.module_name != NULL
                ? node->data.use_decl.module_name : "unknown");
        break;
    case AST_UNSAFE_BLOCK:
        /* unsafe { ... } → emit body directly (no safety wrappers) */
        write_indent(ctx);
        codebuf_write(ctx->out, "/* unsafe */\n");
        if (node->data.unsafe_block.body != NULL)
            emit_block(node->data.unsafe_block.body, ctx);
        break;
    case AST_DEFER_STMT: {
        /* defer { body } → GCC __attribute__((cleanup)) pattern.
         * 1. Emit a static cleanup function into the helpers buffer.
         * 2. Declare a sentinel variable with cleanup attribute.
         * When the sentinel goes out of scope, GCC calls the cleanup. */
        int defer_id = ctx->defer_counter++;

        /* Generate cleanup function in helpers buffer */
        codebuf_write(ctx->helpers,
            "\nstatic void _pgy_defer_%d(int *_pgy_unused) {\n"
            "    (void)_pgy_unused;\n", defer_id);

        /* Save current output, redirect to helpers for the body */
        CodeBuf *saved_out = ctx->out;
        int saved_indent = ctx->indent;
        ctx->out = ctx->helpers;
        ctx->indent = 1;
        if (node->data.defer_stmt.body != NULL)
            emit_block(node->data.defer_stmt.body, ctx);
        ctx->out = saved_out;
        ctx->indent = saved_indent;

        codebuf_write(ctx->helpers, "}\n");

        /* Emit sentinel with cleanup attribute */
        write_indent(ctx);
        codebuf_write(ctx->out,
            "int _pgy_defer_sentinel_%d "
            "__attribute__((cleanup(_pgy_defer_%d))) = 0;\n",
            defer_id, defer_id);
        break;
    }
    case AST_BIND_STMT: {
        /* bind party.slot = Role;
         * → lookup party's typed_var to get PartyType,
         *   then emit PartyType_bind_slot(&party, NULL, &Role_Ability_vtable_instance)
         * For now: use the typed_var mapping to find the party type. */
        const char *pvar = node->data.bind_stmt.party_var;
        const char *slot = node->data.bind_stmt.slot_name;
        const char *role = node->data.bind_stmt.role_name;
        const char *party_type = NULL;
        for (int ti = 0; ti < ctx->typed_var_count; ti++) {
            if (strcmp(ctx->typed_vars[ti].name, pvar) == 0) {
                party_type = ctx->typed_vars[ti].type_name;
                break;
            }
        }
        if (party_type == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot resolve party type for bind statement '%s.%s = %s'",
                pvar != NULL ? pvar : "<party>",
                slot != NULL ? slot : "<slot>",
                role != NULL ? role : "<role>");
            break;
        }

        /* Find the ability name by scanning the current program view for the
         * party declaration. In MIR-backed emission this view is synthesized
         * from MIRProgram inventory, not from the original HIR. The dyn role
         * slot records the required ability. */
        const char *ability_name = NULL;
        char *ability_tag = NULL;
        {
            ASTNode *it = find_party_decl(ctx, party_type);
            if (it == NULL) {
                transpiler_set_backend_error(
                    ctx,
                    "cannot resolve party declaration '%s' while emitting bind statement '%s.%s = %s'",
                    party_type,
                    pvar != NULL ? pvar : "<party>",
                    slot != NULL ? slot : "<slot>",
                    role != NULL ? role : "<role>");
                break;
            }
            for (size_t ri = 0; ri < it->data.party_decl.role_count; ri++) {
                ASTNode *rs = it->data.party_decl.role_slots[ri];
                if (strcmp(rs->data.role_slot.slot_name, slot) == 0
                    && rs->data.role_slot.ability_count > 0
                    && rs->data.role_slot.required_abilities[0] != NULL) {
                    ability_tag = render_ability_ref_vtable_tag(
                        rs->data.role_slot.required_abilities[0]);
                    ability_name = ability_tag;
                    break;
                }
            }
        }
        if (ability_name == NULL) {
            transpiler_set_backend_error(
                ctx,
                "cannot resolve required ability tag for party slot '%s.%s' while emitting bind statement",
                party_type,
                slot != NULL ? slot : "<slot>");
            free(ability_tag);
            break;
        }
        write_indent(ctx);
        codebuf_write(ctx->out,
            "%s_bind_%s(&%s, NULL, &%s_%s_vtable_instance);\n",
            party_type, slot, pvar,
            role, ability_name);
        free(ability_tag);
        break;
    }
    case AST_ABILITY_DECL:
        emit_ability_decl(node, ctx);
        break;
    case AST_ROLE_DECL:
        emit_role_decl(node, ctx);
        break;
    case AST_PARTY_DECL:
        emit_party_decl(node, ctx);
        break;
    case AST_ROSTER_DECL:
        emit_roster_decl(node, ctx);
        break;
    case AST_WORLD_DECL:
        emit_world_decl(node, ctx);
        break;
    case AST_RELATION_DECL:
        emit_relation_decl(node, ctx);
        break;
    case AST_EFFECT_DECL:
        emit_effect_decl(node, ctx);
        break;
    case AST_ZONE_DECL:
        emit_zone_decl(node, ctx);
        break;
    case AST_EVENT_DECL:
        emit_event_decl(node, ctx);
        break;
    case AST_EVENT_SUBSCRIBE:
        emit_event_subscribe(node, ctx);
        break;
    case AST_EVENT_UNSUBSCRIBE:
        emit_event_unsubscribe(node, ctx);
        break;
    case AST_IF_STMT:
        emit_if_stmt(node, ctx);
        break;
    case AST_FOR_LOOP:
        emit_for_loop(node, ctx);
        break;
    case AST_WHILE_LOOP:
        emit_while_loop(node, ctx);
        break;
    case AST_MATCH_STMT:
        emit_match_stmt(node, ctx);
        break;
    case AST_RETURN:
        emit_return_stmt(node, ctx);
        break;
    case AST_BREAK:
        write_indent(ctx);
        if (node->data.break_stmt.label != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, node->data.break_stmt.label);
            if (target >= 0) {
                ctx->loop_break_label_used[target] = true;
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_break_labels[target]);
            } else
                codebuf_write(ctx->out, "break;\n");
        } else {
            codebuf_write(ctx->out, "break;\n");
        }
        break;
    case AST_ENUM_DECL: {
        const char *ename = node->data.enum_decl.name;

        /* Check if any variant has data → tagged union */
        bool has_data = false;
        for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
            if (node->data.enum_decl.variant_param_counts != NULL
                && node->data.enum_decl.variant_param_counts[i] > 0) {
                has_data = true;
                break;
            }
        }

        if (!has_data) {
            /* Simple enum: typedef enum { Color_Red=0, ... } Color; */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s;\n\n", ename);
        } else {
            /* Tagged union:
             * typedef enum { Shape_TAG_Circle, Shape_TAG_Rect, Shape_TAG_None } Shape_Tag;
             * typedef struct {
             *     Shape_Tag tag;
             *     union {
             *         struct { int32_t _0; } Circle;
             *         struct { int32_t _0; int32_t _1; } Rect;
             *     };
             * } Shape; */

            /* Tag enum */
            codebuf_write(ctx->out, "typedef enum {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                codebuf_write(ctx->out, "    %s_TAG_%s = %zu",
                    ename, node->data.enum_decl.variants[i], i);
                if (i + 1 < node->data.enum_decl.variant_count)
                    codebuf_write(ctx->out, ",");
                codebuf_write(ctx->out, "\n");
            }
            codebuf_write(ctx->out, "} %s_Tag;\n\n", ename);

            /* Tagged union struct */
            codebuf_write(ctx->out, "typedef struct {\n");
            codebuf_write(ctx->out, "    %s_Tag tag;\n", ename);
            codebuf_write(ctx->out, "    union {\n");
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                if (pc == 0) continue;
                codebuf_write(ctx->out, "        struct { ");
                for (size_t p = 0; p < pc; p++) {
                    ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                    const char *ctype = transpiler_require_ast_c_type(
                        ctx,
                        pt,
                        "enum variant payload field");
                    if (ctype == NULL)
                        return;
                    codebuf_write(ctx->out, "%s _%zu; ", ctype, p);
                }
                codebuf_write(ctx->out, "} %s;\n",
                    node->data.enum_decl.variants[i]);
            }
            codebuf_write(ctx->out, "    };\n");
            codebuf_write(ctx->out, "} %s;\n\n", ename);

            /* Constructor functions:
             * static inline Shape Shape_Circle(int32_t _0) {
             *     Shape v; v.tag = Shape_TAG_Circle; v.Circle._0 = _0; return v;
             * } */
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                size_t pc = (node->data.enum_decl.variant_param_counts != NULL)
                    ? node->data.enum_decl.variant_param_counts[i] : 0;
                const char *vname = node->data.enum_decl.variants[i];
                if (pc == 0) {
                    /* No-data variant: macro constant */
                    codebuf_write(ctx->out,
                        "#define %s_%s() ((%s){ .tag = %s_TAG_%s })\n",
                        ename, vname, ename, ename, vname);
                } else {
                    /* Data variant: constructor function */
                    codebuf_write(ctx->out,
                        "static inline %s %s_%s(", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++) {
                        ASTNode *pt = node->data.enum_decl.variant_params[i][p];
                        const char *ctype = transpiler_require_ast_c_type(
                            ctx,
                            pt,
                            "enum variant constructor parameter");
                        if (ctype == NULL)
                            return;
                        if (p > 0) codebuf_write(ctx->out, ", ");
                        codebuf_write(ctx->out, "%s _%zu", ctype, p);
                    }
                    codebuf_write(ctx->out, ") {\n");
                    codebuf_write(ctx->out,
                        "    %s _v; _v.tag = %s_TAG_%s;\n", ename, ename, vname);
                    for (size_t p = 0; p < pc; p++)
                        codebuf_write(ctx->out,
                            "    _v.%s._%zu = _%zu;\n", vname, p, p);
                    codebuf_write(ctx->out, "    return _v;\n}\n");
                }
            }
            codebuf_write(ctx->out, "\n");
        }

        for (size_t i = 0; i < node->data.enum_decl.method_count; i++) {
            ASTNode *method = node->data.enum_decl.methods[i];
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            emit_hosted_method_forward_decl_named(ename, method, false,
                                                  ctx->out, ctx);
        }

        for (size_t i = 0; i < node->data.enum_decl.method_count; i++) {
            ASTNode *method = node->data.enum_decl.methods[i];
            const MIRRoutine *mir_method = transpiler_find_mir_method(ctx, ename, method);
            if (method == NULL || method->type != AST_FUNC_DECL)
                continue;
            if (ctx != NULL && ctx->mir != NULL && mir_method == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "MIR-only C path missing routine for enum method '%s.%s'",
                        ename != NULL ? ename : "(anonymous-enum)",
                        method->data.func_decl.name != NULL
                            ? method->data.func_decl.name
                            : "(anonymous)");
                }
                return;
            }
            if (mir_method != NULL) {
                char emitted_name[256];
                snprintf(emitted_name, sizeof(emitted_name), "%s_%s", ename,
                    method->data.func_decl.name);
                emit_func_decl_from_mir_named(method, mir_method, emitted_name, ctx->out, ctx);
                continue;
            }

            const char *method_name = method->data.func_decl.name;
            const char *ret_type = "void";
            if (method->data.func_decl.return_type != NULL)
                ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

            codebuf_write(ctx->out, "\n%s\n%s_%s(%s self",
                          ret_type, ename, method_name, ename);
            for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                FuncParam *p = method->data.func_decl.params[j];
                if (p == NULL || p->name == NULL || strcmp(p->name, "self") == 0)
                    continue;
                const char *pt = NULL;
                char surface_desc[256];
                snprintf(surface_desc, sizeof(surface_desc),
                    "enum method parameter '%s.%s(%s)'",
                    ename != NULL ? ename : "(anonymous)",
                    method_name != NULL ? method_name : "(anonymous)",
                    p != NULL && p->name != NULL ? p->name : "(anonymous)");
                pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
                if (pt == NULL)
                    return;
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");
            {
                int saved_slot_count = ctx->slot_var_count;
                int saved_typed_count = ctx->typed_var_count;
                const char *saved_class_name = ctx->current_class_name;

                ctx->current_class_name = ename;
                register_typed_var(ctx, "self", ename);
                for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
                    FuncParam *p = method->data.func_decl.params[j];
                    char *tn;
                    if (p == NULL || p->name == NULL
                        || strcmp(p->name, "self") == 0 || p->type == NULL)
                        continue;
                    tn = render_type_name(p->type);
                    if (tn != NULL) {
                        register_typed_var(ctx, p->name, tn);
                        free(tn);
                    }
                }
                ctx->indent++;
                if (method->data.func_decl.body != NULL)
                    emit_block(method->data.func_decl.body, ctx);
                ctx->indent--;
                ctx->slot_var_count = saved_slot_count;
                ctx->typed_var_count = saved_typed_count;
                ctx->current_class_name = saved_class_name;
            }
            codebuf_write(ctx->out, "}\n");
        }
        break;
    }
    case AST_CONTINUE:
        write_indent(ctx);
        if (node->data.continue_stmt.label != NULL) {
            int target = transpiler_find_loop_label_depth(
                ctx, node->data.continue_stmt.label);
            if (target >= 0) {
                ctx->loop_continue_label_used[target] = true;
                codebuf_write(ctx->out, "goto %s;\n",
                    ctx->loop_continue_labels[target]);
            } else
                codebuf_write(ctx->out, "continue;\n");
        } else {
            codebuf_write(ctx->out, "continue;\n");
        }
        break;
    case AST_WITH_STMT:
        emit_with_stmt(node, ctx);
        break;
    case AST_PARALLEL_BLOCK:
        emit_parallel_block(node, ctx);
        break;
    case AST_BLOCK:
        emit_block(node, ctx);
        break;
    case AST_LAMBDA_EXPR:
        {
            char *expr = emit_expression(node, ctx);
            if (expr != NULL && expr[0] != '\0') {
                write_indent(ctx);
                codebuf_write(ctx->out, "%s;\n", expr);
            }
            free(expr);
            break;
        }
    case AST_SELECT_STMT:
        emit_select_stmt(node, ctx);
        break;
    case AST_ASYNC_BLOCK:
        emit_async_block(node, ctx);
        break;
    default: {
        /* Expression statement (including event invoke) */
        char *expr = emit_expression(node, ctx);
        if (expr != NULL && expr[0] != '\0') {
            write_indent(ctx);
            codebuf_write(ctx->out, "%s;\n", expr);
            if (node->type == AST_CALL)
                emit_zone_action_effect_runtime(node, ctx);
        }
        free(expr);
        break;
    }
    }
}

void
emit_block(ASTNode *node, TranspilerCtx *ctx)
{
    if (node == NULL)
        return;

    if (node->type == AST_BLOCK) {
        int saved_slot_count = ctx->slot_var_count;
        int saved_typed_count = ctx->typed_var_count;
        int saved_alias_count = ctx->alias_var_count;
        for (size_t i = 0; i < node->data.block.count; i++)
            emit_statement(node->data.block.statements[i], ctx);

        /* Slot sugar: auto-release slot vars declared in this scope (LIFO).
         * Skip slots already explicitly released by the user. */
        for (int i = ctx->slot_var_count - 1; i >= saved_slot_count; i--) {
            SlotVarEntry *e = &ctx->slot_vars[i];
            if (e->released) continue;
            write_indent(ctx);
            if (e->is_secure) {
                codebuf_write(ctx->out,
                    "pgy_secure_release_%s(&%s, &%s_token);\n",
                    e->inner_type, e->name, e->name);
            } else {
                codebuf_write(ctx->out,
                    "pgy_release_%s(&%s);\n",
                    e->inner_type, e->name);
            }
        }

        ctx->slot_var_count = saved_slot_count;
        ctx->typed_var_count = saved_typed_count;
        ctx->alias_var_count = saved_alias_count;
    } else {
        emit_statement(node, ctx);
    }
}

static ASTNode *
find_intent_participant_local(ASTNode *intent, const char *alias)
{
    if (intent == NULL || intent->type != AST_INTENT_DECL || alias == NULL)
        return NULL;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves != NULL && involves->type == AST_INTENT_INVOLVES
            && involves->data.intent_involves.alias != NULL
            && strcmp(involves->data.intent_involves.alias, alias) == 0) {
            return involves;
        }
    }
    return NULL;
}

static ASTNode *
find_subject_action_decl(TranspilerCtx *ctx, const char *subject_name, const char *action_name)
{
    ASTNode *decl;
    if (ctx == NULL || subject_name == NULL || action_name == NULL)
        return NULL;
    decl = find_subject_host_decl(ctx, subject_name);
    if (decl == NULL || decl->type != AST_CLASS_DECL
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_SUBJECT) {
        return NULL;
    }
    for (size_t i = 0; i < decl->data.class_decl.method_count; i++) {
        ASTNode *method = decl->data.class_decl.methods[i];
        if (method != NULL && method->type == AST_FUNC_DECL
            && method->data.func_decl.is_action
            && method->data.func_decl.name != NULL
            && strcmp(method->data.func_decl.name, action_name) == 0) {
            return method;
        }
    }
    return NULL;
}

static ASTNode *
find_zone_decl_in_program_view(TranspilerCtx *ctx, const char *zone_name)
{
    return find_zone_decl(ctx, zone_name);
}

static const char *
intent_participant_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias);

static const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias);

static const char *
intent_step_effective_zone_alias(ASTNode *step)
{
    if (step == NULL || step->type != AST_INTENT_STEP)
        return NULL;
    if (step->data.intent_step.using_expr != NULL
        && step->data.intent_step.using_expr->type == AST_IDENTIFIER) {
        return step->data.intent_step.using_expr->data.identifier.name;
    }
    return step->data.intent_step.transfer_to_alias;
}

static const char *
intent_zone_binding_type_name(ASTNode *intent, const char *alias)
{
    ASTNode *involves = find_intent_participant_local(intent, alias);
    if (involves != NULL
        && involves->data.intent_involves.subject_type != NULL
        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
        return involves->data.intent_involves.subject_type->data.type.name;
    }
    return NULL;
}

static const char *
intent_involves_type_name_local(ASTNode *involves)
{
    if (involves == NULL || involves->type != AST_INTENT_INVOLVES
        || involves->data.intent_involves.subject_type == NULL
        || involves->data.intent_involves.subject_type->type != AST_TYPE) {
        return NULL;
    }
    return involves->data.intent_involves.subject_type->data.type.name;
}

static bool
intent_involves_is_subject_participant(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_subject_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}

static bool
intent_involves_uses_pointer_self(TranspilerCtx *ctx, ASTNode *involves)
{
    const char *type_name = intent_involves_type_name_local(involves);
    if (type_name == NULL)
        return false;
    return is_pointer_self_host_type_name(ctx, type_name)
        || find_subject_host_decl(ctx, type_name) != NULL;
}

static void
emit_intent_step_bind_bound_zone(CodeBuf *out, TranspilerCtx *ctx,
                                 ASTNode *intent, ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;
    const char *from_alias;
    const char *from_zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    from_alias = step->data.intent_step.transfer_from_alias;
    from_zone_type = intent_zone_binding_type_name(intent, from_alias);

    if (from_alias != NULL && from_zone_type != NULL) {
        for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
            const char *alias = step->data.intent_step.who_names[i];
            const char *from_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                from_zone_type, alias);
            const char *to_slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent,
                zone_type, alias);
            if (alias == NULL)
                continue;
            if (from_slot_name != NULL && strcmp(from_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", from_alias, from_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
                    alias, from_slot_name, from_zone_type);
            }
            if (to_slot_name != NULL && strcmp(to_slot_name, "<unbound>") != 0) {
                write_indent(ctx);
                codebuf_write(out, "%s->%s = *%s;\n", zone_alias, to_slot_name, alias);
                write_indent(ctx);
                codebuf_write(out,
                    "pgy_intent_trace_transfer_export(__intent_handle, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");\n",
                    alias,
                    from_zone_type != NULL ? from_zone_type : "<zone>",
                    from_slot_name != NULL ? from_slot_name : "<unbound>",
                    zone_type,
                    to_slot_name);
            }
        }
        write_indent(ctx);
        codebuf_write(out, "%s_sync(%s);\n", from_zone_type, from_alias);
        if (strcmp(from_alias, zone_alias) != 0 || strcmp(from_zone_type, zone_type) != 0) {
            write_indent(ctx);
            codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
        }
        return;
    }

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name(ctx, intent, step, alias);
        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0)
            continue;
        write_indent(ctx);
        codebuf_write(out, "%s->%s = *%s;\n", zone_alias, slot_name, alias);
        write_indent(ctx);
        codebuf_write(out,
            "pgy_intent_trace_materialize_export(__intent_handle, \"%s\", \"%s\", \"%s\");\n",
            alias, slot_name, zone_type);
    }
    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static bool
emit_intent_step_rebind_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                           ASTNode *intent, ASTNode *step,
                                           size_t step_index)
{
    const char *zone_alias;
    const char *zone_type;
    bool rebound = false;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return false;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return false;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);
        const char *participant_c_type;

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        participant_c_type = pergyra_ast_type_to_c(involves->data.intent_involves.subject_type);
        write_indent(ctx);
        codebuf_write(out, "%s *__intent_saved_%zu_%s = %s;\n",
            participant_c_type, step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = &%s->%s;\n", alias, zone_alias, slot_name);
        rebound = true;
    }

    return rebound;
}

static void
emit_intent_step_sync_effective_zone(CodeBuf *out, TranspilerCtx *ctx,
                                     ASTNode *step)
{
    const char *zone_alias;
    const char *zone_type;

    if (out == NULL || ctx == NULL || step == NULL || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_alias = intent_step_effective_zone_alias(step);
    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_alias == NULL || zone_type == NULL)
        return;

    write_indent(ctx);
    codebuf_write(out, "%s_sync(%s);\n", zone_type, zone_alias);
}

static void
emit_intent_step_restore_bound_zone_aliases(CodeBuf *out, TranspilerCtx *ctx,
                                            ASTNode *intent, ASTNode *step,
                                            size_t step_index)
{
    const char *zone_type;

    if (out == NULL || ctx == NULL || intent == NULL || step == NULL
        || step->type != AST_INTENT_STEP
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE) {
        return;
    }

    zone_type = step->data.intent_step.where_type->data.type.name;
    if (zone_type == NULL)
        return;

    for (size_t i = 0; i < step->data.intent_step.who_count; i++) {
        const char *alias = step->data.intent_step.who_names[i];
        const char *slot_name = resolve_intent_zone_slot_name_for_zone(ctx, intent, zone_type, alias);
        ASTNode *involves = find_intent_participant_local(intent, alias);

        if (alias == NULL || slot_name == NULL || strcmp(slot_name, "<unbound>") == 0
            || involves == NULL || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }

        write_indent(ctx);
        codebuf_write(out, "*__intent_saved_%zu_%s = *%s;\n", step_index, alias, alias);
        write_indent(ctx);
        codebuf_write(out, "%s = __intent_saved_%zu_%s;\n", alias, step_index, alias);
    }
}

static const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias)
{
    if (ctx == NULL || intent == NULL || step == NULL || alias == NULL
        || step->data.intent_step.where_type == NULL
        || step->data.intent_step.where_type->type != AST_TYPE
        || step->data.intent_step.where_type->data.type.name == NULL) {
        return "<unbound>";
    }
    return resolve_intent_zone_slot_name_for_zone(ctx, intent,
        step->data.intent_step.where_type->data.type.name, alias);
}

static const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias)
{
    ASTNode *zone_decl = NULL;
    const char *participant_type = NULL;
    ASTNode *named_match = NULL;
    ASTNode *typed_match = NULL;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = find_zone_decl_in_program_view(ctx, zone_type_name);
    participant_type = intent_participant_type_name(intent, alias);
    if (zone_decl == NULL)
        return "<unbound>";

    for (size_t i = 0; i < zone_decl->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone_decl->data.zone_decl.slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL) {
            continue;
        }
        if (strcmp(slot->data.domain_slot.slot_name, alias) == 0) {
            named_match = slot;
            break;
        }
        if (participant_type != NULL
            && slot->data.domain_slot.type != NULL
            && slot->data.domain_slot.type->type == AST_TYPE
            && slot->data.domain_slot.type->data.type.name != NULL
            && strcmp(slot->data.domain_slot.type->data.type.name, participant_type) == 0) {
            if (typed_match != NULL)
                typed_match = (ASTNode *)(uintptr_t)1;
            else
                typed_match = slot;
        }
    }

    if (named_match != NULL)
        return named_match->data.domain_slot.slot_name;
    if (typed_match != NULL && typed_match != (ASTNode *)(uintptr_t)1)
        return typed_match->data.domain_slot.slot_name;
    return "<unbound>";
}

static bool
intent_action_has_only_self(ASTNode *action_decl)
{
    size_t real_pc = 0;
    if (action_decl == NULL || action_decl->type != AST_FUNC_DECL)
        return false;
    for (size_t i = 0; i < action_decl->data.func_decl.param_count; i++) {
        FuncParam *p = action_decl->data.func_decl.params[i];
        if (p == NULL || p->name == NULL)
            continue;
        if (p->type == NULL && strcmp(p->name, "self") == 0)
            continue;
        real_pc++;
    }
    return real_pc == 0;
}

static void
emit_intent_forward_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    const MIRRoutine *mir_routine = NULL;
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;
    mir_routine = transpiler_find_mir_intent(ctx, node);
    if (mir_routine != NULL) {
        participant_count = transpiler_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
    }
    codebuf_write(buf, "\nbool\n%s(", node->data.intent_decl.name);
    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        const char *pt = NULL;
        const char *alias = involves != NULL && involves->data.intent_involves.alias != NULL
            ? involves->data.intent_involves.alias : "participant";
        const char *participant_type = participant_types != NULL && i < participant_count
            ? participant_types[i]
            : NULL;
        bool pointer_param = false;
        char surface_desc[256];
        if (i > 0)
            codebuf_write(buf, ", ");
        snprintf(surface_desc, sizeof(surface_desc),
            "intent participant '%s' of '%s'",
            alias != NULL ? alias : "(anonymous)",
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        if (participant_type != NULL) {
            pt = transpiler_require_type_name_c_type(ctx, participant_type, surface_desc);
            pointer_param = is_pointer_self_host_type_name(ctx, participant_type);
        } else if (involves != NULL && involves->data.intent_involves.subject_type != NULL) {
            pt = transpiler_require_ast_c_type(
                ctx, involves->data.intent_involves.subject_type, surface_desc);
            pointer_param = intent_involves_uses_pointer_self(ctx, involves);
        }
        if (pt == NULL) {
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        if (participant_aliases != NULL && i < participant_count
            && participant_aliases[i] != NULL) {
            alias = participant_aliases[i];
        }
        codebuf_write(buf, "%s%s%s", pt, pointer_param ? " *" : " ", alias);
    }
    for (size_t i = 0; i < node->data.intent_decl.value_count; i++) {
        ASTNode *value = node->data.intent_decl.values[i];
        const char *pt = NULL;
        char surface_desc[256];
        if (node->data.intent_decl.involve_count > 0 || i > 0)
            codebuf_write(buf, ", ");
        snprintf(surface_desc, sizeof(surface_desc),
            "intent value '%s' of '%s'",
            value != NULL && value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias
                : "value",
            node->data.intent_decl.name != NULL ? node->data.intent_decl.name : "(anonymous)");
        pt = transpiler_require_ast_c_type(
            ctx,
            value != NULL ? value->data.intent_value.value_type : NULL,
            surface_desc);
        if (pt == NULL) {
            free((void *)participant_aliases);
            free((void *)participant_types);
            return;
        }
        codebuf_write(buf, "%s %s", pt,
            value != NULL && value->data.intent_value.alias != NULL
                ? value->data.intent_value.alias : "value");
    }
    codebuf_write(buf, ");\n");
    free((void *)participant_aliases);
    free((void *)participant_types);
}

static bool
transpiler_can_forward_declare_intent_early(TranspilerCtx *ctx, ASTNode *intent)
{
    if (ctx == NULL || intent == NULL || intent->type != AST_INTENT_DECL)
        return false;
    for (size_t i = 0; i < intent->data.intent_decl.involve_count; i++) {
        ASTNode *involves = intent->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES
            || involves->data.intent_involves.subject_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, involves->data.intent_involves.subject_type)) {
            return false;
        }
    }
    for (size_t i = 0; i < intent->data.intent_decl.value_count; i++) {
        ASTNode *value = intent->data.intent_decl.values[i];
        if (value == NULL || value->type != AST_INTENT_VALUE
            || value->data.intent_value.value_type == NULL) {
            continue;
        }
        if (!transpiler_can_forward_declare_type_early(
                ctx, value->data.intent_value.value_type)) {
            return false;
        }
    }
    return true;
}

static void
