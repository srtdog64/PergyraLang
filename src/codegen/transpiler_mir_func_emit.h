static void
emit_func_decl_from_mir_named(ASTNode *node, const MIRRoutine *mir_routine,
                              const char *emitted_name, CodeBuf *buf,
                              TranspilerCtx *ctx)
{
    const char *name = emitted_name != NULL ? emitted_name : node->data.func_decl.name;
    TranspilerMirEmitState saved_emit_state;
    CodeBuf *params_sig = codebuf_create();
    char *header_decl = NULL;
    bool is_method = mir_routine != NULL
        && mir_routine->kind == MIR_SCOPE_METHOD;
    const char *owner_name = is_method ? mir_routine->owner_name : NULL;
    ASTNodeType owner_ast_type = is_method ? mir_routine->owner_ast_type : AST_PROGRAM;
    const char *owner_role_subject_name = NULL;
    const char *owner_role_subject_c_type = NULL;
    bool owner_is_zone = false;
    bool owner_is_role = owner_ast_type == AST_ROLE_DECL;
    bool owner_is_relation = owner_ast_type == AST_RELATION_DECL;
    bool owner_is_effect = owner_ast_type == AST_EFFECT_DECL;
    bool owner_is_world = owner_ast_type == AST_WORLD_DECL;
    bool pointer_self = false;
    ASTNode *resolved_host_decl = NULL;

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
        owner_is_zone = owner_ast_type == AST_ZONE_DECL;
        if (owner_is_role) {
            owner_role_subject_name =
                transpiler_role_subject_name_local(ctx, owner_name);
            if (owner_role_subject_name != NULL)
                owner_role_subject_c_type =
                    pergyra_type_to_c(owner_role_subject_name);
        }
    }

    if (owner_is_role) {
        if (owner_role_subject_name != NULL) {
            resolved_host_decl = transpiler_find_host_decl_from_owner_local(
                ctx, owner_name, owner_ast_type);
        }
    } else if (owner_name != NULL) {
        resolved_host_decl = transpiler_find_host_decl_from_owner_local(
            ctx, owner_name, owner_ast_type);
    }

    if (owner_name != NULL && !owner_is_role && resolved_host_decl == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "MIR-only transpiler missing declaration inventory for host '%s' of method '%s'",
                owner_name,
                name != NULL ? name : "<method>");
        }
        codebuf_destroy(params_sig);
        return;
    }

    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);
    ctx->out = buf;
    g_type_render_ctx = ctx;
    transpiler_bind_function_emit_host_local(ctx, resolved_host_decl, node);

    ensure_collection_specializations_from_stmt_to(ctx, ctx->decls, node);

    if (node->data.func_decl.return_type != NULL) {
        char *rendered = render_type_name(node->data.func_decl.return_type);
        transpiler_set_current_return_type_local(ctx, rendered);
        free(rendered);
    } else {
        transpiler_set_current_return_type_local(ctx, "Void");
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
                    "cannot determine parameter type for MIR-emitted function '%s' at argument %llu",
                    name != NULL ? name : "<function>",
                    (unsigned long long) i);
            }
            codebuf_destroy(params_sig);
            free(header_decl);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
                ASTNode *zone_decl = (resolved_host_decl != NULL
                    && resolved_host_decl->type == AST_ZONE_DECL)
                    ? resolved_host_decl
                    : NULL;
                if (zone_decl != NULL
                    && zone_decl->data.zone_decl.authority_count > 0
                    && zone_decl->data.zone_decl.authorities != NULL
                    && zone_decl->data.zone_decl.authorities[0] != NULL) {
                    const char *auth_slot =
                        zone_decl->data.zone_decl.authorities[0]
                            ->data.zone_authority.subject_slot_name;
                    if (auth_slot != NULL) {
                        char *participant_expr =
                            transpiler_scratch_fmt(ctx, "&self->%s", auth_slot);
                        write_indent(ctx);
                        codebuf_write(ctx->out,
                            "PGY_ZONE_AUTHORITY_CHECK(self, %s, \"%s\", \"%s\");\n",
                            participant_expr, owner_name, auth_slot);
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
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
            if (transpiler_type_name_is_claim_shape(type_name))
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
            if (c_type == NULL
                || c_type[0] == '\0'
                || (type_name != NULL && strcmp(type_name, "Unknown") == 0)
                || strcmp(c_type, "Unknown") == 0) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot determine C type for MIR local '%s' in function '%s'",
                        versioned_name,
                        name != NULL ? name : "<function>");
                }
                if (params_sig != NULL)
                    codebuf_destroy(params_sig);
                free(header_decl);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
        write_indent(ctx);
        codebuf_write(ctx->out, ";\n");
        block_emitted = transpiler_emit_mir_block_statements(ctx->out, node, mir_routine,
                                                           block, ctx, &block_ssa_map,
                                                           block_reason,
                                                           sizeof(block_reason));
        if (!block_emitted) {
            transpiler_ssa_map_clear(&block_ssa_map);
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR block emission failed in function '%s' at block %llu: %s",
                    name != NULL ? name : "<function>",
                    (unsigned long long) block->id,
                    block_reason[0] != '\0' ? block_reason : "unknown reason");
            }
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
                const char *cond_text = NULL;
                bool owns_cond = false;
                if (cond != NULL) {
                    cond_text = cond;
                    owns_cond = true;
                } else {
                    cond_text = transpiler_scratch_strdup(ctx, "false");
                }
                if (block->has_succ_true)
                    transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent, i, block,
                        &mir_routine->blocks[block->succ_true]);
                write_indent(ctx);
                transpiler_write_condition_head(ctx, "if",
                    cond_text != NULL ? cond_text : "false", " {\n");
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
                if (owns_cond)
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
            write_indent(ctx);
            codebuf_write(ctx->out, ";\n");
            for (size_t j = 0; j < block->instruction_count; j++) {
                const MIRInstruction *inst = &block->instructions[j];
                if (inst->kind == MIR_INST_CLEANUP_EDGE) {
                    if (!transpiler_emit_mir_resource_hook(ctx, ctx->out, ctx->indent, inst,
                                                           cleanup_handle, true)) {
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}
