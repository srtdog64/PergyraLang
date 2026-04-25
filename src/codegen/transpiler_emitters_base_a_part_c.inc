static bool
transpiler_emit_mir_block_statements(CodeBuf *buf, const ASTNode *func_decl,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *out_ssa_map,
                                     char *reason,
                                     size_t reason_cap)
{
    TranspilerSSANameMap ssa_map = {0};
    TranspilerSSANameMap *ssa_map_out = &ssa_map;
    const void *saved_active_ssa_map;
    bool source_order_mode = false;
    size_t *inst_order = NULL;
    bool ok = true;

    if (buf == NULL || func_decl == NULL || mir_routine == NULL || block == NULL || ctx == NULL)
        return false;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';

    if (out_ssa_map != NULL)
        ssa_map_out = out_ssa_map;
    if (!transpiler_emit_mir_block_with_ssa_map(ssa_map_out, block)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR emission failed for block %llu in %s: missing SSA entry map",
                     (unsigned long long) block->id, func_decl->type == AST_FUNC_DECL
                     ? func_decl->data.func_decl.name
                     : func_decl->data.intent_decl.name);
        return false;
    }
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        char base[128];
        size_t version = 0;
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        if (!transpiler_parse_versioned_name(inst->result_name, base, sizeof(base), &version))
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map_out, base, inst->result_name))
            return false;
    }

    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map_out;

    source_order_mode = !transpiler_mir_routine_has_explicit_cfg(mir_routine)
        && block->source_statement_count > 0
        && block->source_statements != NULL
        && block->instruction_count > 0;
    if (source_order_mode) {
        bool *scheduled = calloc(block->instruction_count, sizeof(bool));
        inst_order = calloc(block->instruction_count, sizeof(size_t));
        size_t order_count = 0;

        if (scheduled == NULL || inst_order == NULL) {
            free(scheduled);
            free(inst_order);
            ctx->active_ssa_map = saved_active_ssa_map;
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                         "MIR block %llu emission failed: unable to allocate source-order schedule",
                         (unsigned long long) block->id);
            }
            return false;
        }

        for (size_t stmt_idx = 0; stmt_idx < block->source_statement_count; stmt_idx++) {
            ASTNode *source_stmt = block->source_statements[stmt_idx];
            for (size_t inst_idx = 0; inst_idx < block->instruction_count; inst_idx++) {
                const MIRInstruction *inst = &block->instructions[inst_idx];
                bool attach_to_source_stmt = false;
                if (scheduled[inst_idx])
                    continue;
                if (inst->ast == source_stmt) {
                    attach_to_source_stmt = true;
                } else if (source_stmt != NULL
                           && inst->kind == MIR_INST_DEF
                           && inst->arg0 != NULL) {
                    if (source_stmt->type == AST_LET_DECL
                        && source_stmt->data.let_decl.name != NULL
                        && strcmp(inst->arg0, source_stmt->data.let_decl.name) == 0) {
                        attach_to_source_stmt = true;
                    } else if (source_stmt->type == AST_ASSIGNMENT
                               && source_stmt->data.assignment.target != NULL
                               && source_stmt->data.assignment.target->type == AST_IDENTIFIER
                               && source_stmt->data.assignment.target->data.identifier.name != NULL
                               && strcmp(inst->arg0,
                                         source_stmt->data.assignment.target->data.identifier.name) == 0) {
                        attach_to_source_stmt = true;
                    }
                }
                if (!attach_to_source_stmt)
                    continue;
                inst_order[order_count++] = inst_idx;
                scheduled[inst_idx] = true;
            }
        }
        for (size_t inst_idx = 0; inst_idx < block->instruction_count; inst_idx++) {
            if (scheduled[inst_idx])
                continue;
            inst_order[order_count++] = inst_idx;
        }
        free(scheduled);
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_RESOURCE_OP)
            continue;
        if (inst->name == NULL || strcmp(inst->name, "Claim") != 0)
            continue;
        if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent, inst, "0", false)) {
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                         "MIR block %zu emission failed: unable to emit claim op for '%s'",
                         block->id,
                         inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
            }
            ok = false;
            break;
        }
    }

    if (!ok) {
        free(inst_order);
        ctx->active_ssa_map = saved_active_ssa_map;
        return false;
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        size_t inst_index = source_order_mode ? inst_order[i] : i;
        const MIRInstruction *inst = &block->instructions[inst_index];
        ASTNode *stmt = inst->ast;

        if (!transpiler_materialize_pending_inst_uses(buf, ctx, func_decl, block,
                                                      inst, ssa_map_out, ctx->indent,
                                                      true, reason, reason_cap)) {
            ok = false;
            break;
        }

        if (inst->kind == MIR_INST_RESOURCE_OP) {
            if (inst->name != NULL
                && strcmp(inst->name, "Write") == 0
                && inst->ast != NULL
                && inst->ast->type == AST_CALL) {
                ASTNode *callee = inst->ast->data.call.callee;
                ASTNode *value_expr = NULL;
                char map_reason[256];
                if (callee != NULL
                    && callee->type == AST_IDENTIFIER
                    && callee->data.identifier.name != NULL
                    && strcmp(callee->data.identifier.name, "Write") == 0
                    && inst->ast->data.call.arg_count >= 2) {
                    value_expr = inst->ast->data.call.arguments[1];
                } else if (callee != NULL
                           && callee->type == AST_MEMBER_ACCESS
                           && callee->data.member.name != NULL
                           && strcmp(callee->data.member.name, "Write") == 0
                           && inst->ast->data.call.arg_count >= 1) {
                    value_expr = inst->ast->data.call.arguments[0];
                }
                if (value_expr != NULL) {
                    if (!transpiler_expr_identifiers_mapped(
                            ctx,
                            value_expr,
                            (const TranspilerSSANameMap *)ssa_map_out,
                            mir_routine->name,
                            map_reason,
                            sizeof(map_reason))) {
                        continue;
                    }
                    if (!transpiler_seed_expr_identifier_mappings(
                            block, inst_index, value_expr, ssa_map_out)) {
                        ok = false;
                        break;
                    }
                }
            }
            if (inst->arg1 != NULL
                && transpiler_resolve_ssa_name(
                       (const TranspilerSSANameMap *)ssa_map_out,
                       inst->arg1) == NULL) {
                const char *mapped_value = transpiler_find_prior_block_ssa_name(
                    block, inst_index, inst->arg1);
                if (mapped_value == NULL)
                    mapped_value = transpiler_find_block_exit_ssa_name(block, inst->arg1);
                if (mapped_value != NULL) {
                    if (!transpiler_ssa_name_map_set(ssa_map_out, inst->arg1, mapped_value)) {
                        ok = false;
                        break;
                    }
                }
            }
            if (inst->name != NULL && strcmp(inst->name, "Claim") == 0)
                continue;
            if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent, inst, "0", false)) {
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to emit resource op '%s'",
                             (unsigned long long) block->id,
                             inst->name != NULL ? inst->name : "<op>");
                }
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && inst->arg0 != NULL
            && inst->result_name != NULL
            && strcmp(inst->arg0, stmt->data.let_decl.name) == 0) {
            const char *saved_type_hint = ctx->active_type_hint;
            char *rendered_type_hint = NULL;
            char *local_type_name_owned = NULL;
            char *lhs = NULL;
            char *rhs = NULL;

            if (stmt->data.let_decl.type != NULL) {
                rendered_type_hint = render_type_name(stmt->data.let_decl.type);
                ctx->active_type_hint = rendered_type_hint;
            }
            if (rendered_type_hint != NULL)
                local_type_name_owned = pergyra_strdup(rendered_type_hint);
            else if (stmt->data.let_decl.initializer != NULL) {
                const char *inferred =
                    infer_expression_type_name(ctx, stmt->data.let_decl.initializer);
                if (inferred != NULL)
                    local_type_name_owned = pergyra_strdup(inferred);
            }
            if (transpiler_type_name_is_slot_like(local_type_name_owned)) {
                bool claim_backed_slot = transpiler_block_has_claim_for_slot_local(
                    block, stmt->data.let_decl.name);
                if (claim_backed_slot) {
                    ctx->active_type_hint = saved_type_hint;
                    free(rendered_type_hint);
                    free(local_type_name_owned);
                    continue;
                }
                emit_statement(stmt, ctx);
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(local_type_name_owned);
                continue;
            }
            if (local_type_name_owned != NULL
                && strncmp(local_type_name_owned, "Channel<", 8) == 0) {
                emit_statement(stmt, ctx);
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(local_type_name_owned);
                continue;
            }

            if (stmt->data.let_decl.type != NULL) {
                ensure_type_specializations_from_ast_to(ctx, ctx->out,
                                                        stmt->data.let_decl.type);
            } else if (local_type_name_owned != NULL) {
                ensure_result_specialization_from_type_name_to(ctx, ctx->out,
                                                               local_type_name_owned);
            }

            lhs = transpiler_render_ssa_name(ctx, inst->result_name);
            {
                const char *saved_expected_type = ctx->expected_type;
                ctx->expected_type = local_type_name_owned;
                rhs = emit_expression_with_ssa_map(stmt->data.let_decl.initializer,
                                                   ctx, ssa_map_out);
                ctx->expected_type = saved_expected_type;
            }
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);

            if (rhs == NULL) {
                free(lhs);
                free(local_type_name_owned);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render initializer for '%s'",
                             (unsigned long long) block->id,
                             stmt->data.let_decl.name != NULL
                                 ? stmt->data.let_decl.name
                                 : "<binding>");
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (!transpiler_ssa_name_map_set(ssa_map_out, stmt->data.let_decl.name,
                                             inst->result_name)) {
                free(local_type_name_owned);
                ok = false;
                break;
            }
            if (local_type_name_owned != NULL)
                register_typed_var(ctx, stmt->data.let_decl.name, local_type_name_owned);
            free(local_type_name_owned);
            continue;
        }

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type == AST_ASSIGNMENT
            && stmt->data.assignment.target != NULL
            && stmt->data.assignment.target->type == AST_IDENTIFIER
            && inst->arg0 != NULL
            && inst->result_name != NULL) {
            const char *target_name = stmt->data.assignment.target->data.identifier.name;
            char *lhs = NULL;
            char *rhs = NULL;
            bool target_is_field = false;
            bool is_local_binding = false;

            if (target_name == NULL || strcmp(inst->arg0, target_name) != 0)
                continue;
            is_local_binding = transpiler_has_explicit_local_binding(func_decl, target_name);
            if (!is_local_binding) {
                if (current_class_has_field(ctx, target_name))
                    target_is_field = true;
                if (current_relation_has_field(ctx, target_name))
                    target_is_field = true;
                if (current_effect_has_field(ctx, target_name))
                    target_is_field = true;
                if (current_zone_has_field(ctx, target_name))
                    target_is_field = true;
                if (transpiler_current_world_has_field(ctx, target_name))
                    target_is_field = true;
            }
            if (!target_is_field && inst->slot_anchor != NULL) {
                const char *slot_anchor = inst->slot_anchor;
                if (current_class_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (current_relation_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (current_effect_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (current_zone_has_field(ctx, slot_anchor))
                    target_is_field = true;
                if (transpiler_current_world_has_field(ctx, slot_anchor))
                    target_is_field = true;
            }
            if (target_is_field) {
                char *field_lhs = NULL;
                char *field_rhs = NULL;

                if (stmt->data.assignment.value != NULL
                    && stmt->data.assignment.value->type == AST_IDENTIFIER
                    && stmt->data.assignment.value->data.identifier.name != NULL
                    && transpiler_resolve_ssa_name(
                           (const TranspilerSSANameMap *)ssa_map_out,
                           stmt->data.assignment.value->data.identifier.name) == NULL) {
                    const char *prior_ssa = transpiler_find_prior_block_ssa_name(
                        block, inst_index, stmt->data.assignment.value->data.identifier.name);
                    if (prior_ssa != NULL) {
                        transpiler_ssa_name_map_set(ssa_map_out,
                            stmt->data.assignment.value->data.identifier.name,
                            prior_ssa);
                    }
                }
                field_lhs = emit_expression_with_ssa_map(
                    stmt->data.assignment.target, ctx, ssa_map_out);
                field_rhs = emit_expression_with_ssa_map(
                    stmt->data.assignment.value, ctx, ssa_map_out);
                if (field_lhs == NULL || field_rhs == NULL) {
                    free(field_lhs);
                    free(field_rhs);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %llu emission failed: unable to render field assignment to '%s'",
                                 (unsigned long long) block->id,
                                 target_name != NULL ? target_name : "<field>");
                    }
                    ok = false;
                    break;
                }
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", field_lhs, field_rhs);
                free(field_lhs);
                free(field_rhs);
                continue;
            }
            if (!is_local_binding) {
                emit_statement(stmt, ctx);
                continue;
            }
            if (transpiler_type_name_is_slot_like(lookup_typed_var(ctx, target_name))) {
                emit_statement(stmt, ctx);
                continue;
            }

            lhs = transpiler_render_ssa_name(ctx, inst->result_name);
            rhs = emit_expression_with_ssa_map(stmt->data.assignment.value, ctx, ssa_map_out);
            if (rhs == NULL) {
                free(lhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render assignment to '%s'",
                             (unsigned long long) block->id, target_name != NULL ? target_name : "<target>");
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (!transpiler_ssa_name_map_set(ssa_map_out, target_name, inst->result_name)) {
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && stmt->type != AST_LET_DECL
            && stmt->type != AST_ASSIGNMENT
            && inst->result_name != NULL) {
            ASTNode *binding_type_ast = NULL;
            char *lhs = NULL;
            char *rhs = NULL;
            const char *binding_name = inst->arg0;
            const char *value_type = NULL;

            if (binding_name != NULL)
                binding_type_ast =
                    transpiler_find_local_type_ast(ctx, func_decl, binding_name);
            if (binding_type_ast != NULL) {
                ensure_type_specializations_from_ast_to(ctx, ctx->decls,
                                                        binding_type_ast);
                value_type = transpiler_find_local_type_name(ctx, func_decl,
                                                             binding_name);
            }
            if (value_type == NULL || strcmp(value_type, "Unknown") == 0)
                value_type = infer_expression_type_name(ctx, stmt);

            lhs = transpiler_render_ssa_name(ctx, inst->result_name);
            {
                const char *saved_expected_type = ctx->expected_type;
                ctx->expected_type = value_type;
                rhs = emit_expression_with_ssa_map(stmt, ctx, ssa_map_out);
                ctx->expected_type = saved_expected_type;
            }
            if (lhs == NULL || rhs == NULL) {
                free(lhs);
                free(rhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render DEF for '%s'",
                             (unsigned long long) block->id,
                             binding_name != NULL ? binding_name : inst->result_name);
                }
                ok = false;
                break;
            }

            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);

            if (binding_name != NULL) {
                if (!transpiler_ssa_name_map_set(ssa_map_out,
                                                 binding_name,
                                                 inst->result_name)) {
                    ok = false;
                    break;
                }
                if (value_type != NULL
                    && value_type[0] != '\0'
                    && strcmp(value_type, "Unknown") != 0) {
                    register_typed_var(ctx, binding_name, value_type);
                }
            }
            continue;
        }

        if (inst->kind != MIR_INST_STMT)
            continue;
        if (stmt == NULL || stmt->type == AST_BLOCK || stmt->type == AST_RETURN) {
            continue;
        }
        if (stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && stmt->data.let_decl.initializer != NULL
            && stmt->data.let_decl.initializer->type != AST_CALL
            && transpiler_find_routine_exit_ssa_name(
                   mir_routine, stmt->data.let_decl.name) != NULL) {
            continue;
        }
        if (ctx->mir != NULL) {
            bool mirrored_resource_stmt = false;
            for (size_t ri = 0; ri < block->instruction_count; ri++) {
                const MIRInstruction *resource_inst = &block->instructions[ri];
                bool resource_is_secure = false;
                if (resource_inst->kind != MIR_INST_RESOURCE_OP)
                    continue;
                if (resource_inst->ast != stmt)
                    continue;
                if (resource_inst->name != NULL
                    && strcmp(resource_inst->name, "Read") == 0) {
                    continue;
                }
                if (resource_inst->slot_anchor != NULL)
                    resource_is_secure = lookup_slot_is_secure(ctx, resource_inst->slot_anchor);
                if (resource_is_secure)
                    continue;
                mirrored_resource_stmt = true;
                break;
            }
            if (mirrored_resource_stmt)
                continue;
        }
        if (stmt->type == AST_ASSIGNMENT
            && stmt->data.assignment.target != NULL
            && stmt->data.assignment.target->type == AST_IDENTIFIER
            && stmt->data.assignment.target->data.identifier.name != NULL
            && transpiler_is_implicit_field(ctx,
                stmt->data.assignment.target->data.identifier.name)) {
            char map_reason[256];
            if (!transpiler_expr_identifiers_mapped(
                    ctx,
                    stmt->data.assignment.value,
                    ssa_map_out,
                    mir_routine->name,
                    map_reason,
                    sizeof(map_reason))) {
                continue;
            }
        }
        if (stmt->type == AST_ASSIGNMENT
            && stmt->data.assignment.target != NULL
            && stmt->data.assignment.target->type == AST_IDENTIFIER
            && stmt->data.assignment.target->data.identifier.name != NULL
            && !transpiler_is_implicit_field(
                   ctx, stmt->data.assignment.target->data.identifier.name)) {
            const char *target_name =
                stmt->data.assignment.target->data.identifier.name;
            const char *mapped_target = transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ssa_map_out,
                target_name);
            if (mapped_target != NULL) {
                char *lhs = transpiler_render_ssa_name(ctx, mapped_target);
                char *rhs = emit_expression_with_ssa_map(
                    stmt->data.assignment.value, ctx, ssa_map_out);

                if (lhs == NULL || rhs == NULL) {
                    free(lhs);
                    free(rhs);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %llu emission failed: unable to render local assignment to '%s'",
                                 (unsigned long long) block->id,
                                 target_name);
                    }
                    ok = false;
                    break;
                }
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);
                continue;
            }
        }
        if (transpiler_mir_routine_has_explicit_cfg(mir_routine)
            && (stmt->type == AST_WITH_STMT
                || stmt->type == AST_IF_STMT
                || stmt->type == AST_WHILE_LOOP)) {
            continue;
        }
        if (stmt->type == AST_CALL) {
            char *expr = emit_expression_with_ssa_map(stmt, ctx, ssa_map_out);
            if (expr == NULL || expr[0] == '\0') {
                free(expr);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render call statement with SSA mapping",
                             (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s;\n", expr);
            emit_zone_action_effect_runtime(stmt, ctx);
            free(expr);
            continue;
        }
        if (stmt->type == AST_LET_DESTRUCTURE) {
            /* MIR lowering for `let (a, b, c) = rhs`.
             * The routine's SSA header pre-declared `_pgy_ssa_<name>_1 = 0`
             * for each binding; here we emit the initializer into a scratch
             * and assign element-wise into the SSA-renamed targets. */
            ASTNode *init = stmt->data.let_destructure.initializer;
            /* Special case: ClaimSecureSlot destructuring must emit a
             * PgyToken + PgySecureSlot pair (MIR header already skipped
             * SSA pre-decl for the slot/token because their resolved
             * type is Slot-like). */
            if (init != NULL && init->type == AST_CALL
                && init->data.call.callee != NULL
                && init->data.call.callee->type == AST_IDENTIFIER
                && init->data.call.callee->data.identifier.name != NULL
                && stmt->data.let_destructure.name_count == 2) {
                const char *cname =
                    init->data.call.callee->data.identifier.name;
                if (strcmp(cname, "ClaimSecureSlot") == 0) {
                    const char *inner = NULL;
                    if (init->data.call.generic_args != NULL
                        && init->data.call.generic_args->count > 0
                        && init->data.call.generic_args->params[0] != NULL)
                        inner = init->data.call.generic_args->params[0]->name;
                    if (inner == NULL) inner = "Int";
                    const char *slot_name =
                        stmt->data.let_destructure.names[0];
                    const char *token_name =
                        stmt->data.let_destructure.names[1];
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf,
                        "PgyToken_%s %s;\n", inner, token_name);
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf,
                        "PgySecureSlot_%s %s = pgy_claim_secure_%s(&%s);\n",
                        inner, slot_name, inner, token_name);
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf, "(void)%s;\n", slot_name);
                    register_slot_var(ctx, slot_name, inner, true, false);
                    set_slot_token_name(ctx, slot_name, token_name);
                    char typed_tok[64];
                    snprintf(typed_tok, sizeof(typed_tok), "Token<%s>", inner);
                    register_typed_var(ctx, token_name, typed_tok);
                    char typed_slot[64];
                    snprintf(typed_slot, sizeof(typed_slot), "SecureSlot<%s>", inner);
                    register_typed_var(ctx, slot_name, typed_slot);
                    transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
                    transpiler_ssa_name_map_set(ssa_map_out, token_name, token_name);
                    continue;
                }
            }
            if (init != NULL && init->type == AST_CALL
                && init->data.call.callee != NULL
                && init->data.call.callee->type == AST_IDENTIFIER
                && init->data.call.callee->data.identifier.name != NULL
                && stmt->data.let_destructure.name_count == 1
                && strcmp(init->data.call.callee->data.identifier.name,
                          "ClaimSlot") == 0) {
                const char *inner = NULL;
                if (init->data.call.generic_args != NULL
                    && init->data.call.generic_args->count > 0
                    && init->data.call.generic_args->params[0] != NULL)
                    inner = init->data.call.generic_args->params[0]->name;
                if (inner == NULL) inner = "Int";
                const char *slot_name =
                    stmt->data.let_destructure.names[0];
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf,
                    "PgySlot_%s %s = pgy_claim_%s();\n",
                    inner, slot_name, inner);
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "(void)%s;\n", slot_name);
                register_slot_var(ctx, slot_name, inner, false, false);
                char typed_slot[64];
                snprintf(typed_slot, sizeof(typed_slot), "Slot<%s>", inner);
                register_typed_var(ctx, slot_name, typed_slot);
                transpiler_ssa_name_map_set(ssa_map_out, slot_name, slot_name);
                continue;
            }
            const char *init_type_name =
                infer_expression_type_name(ctx, init);
            if ((init_type_name == NULL || strcmp(init_type_name, "Unknown") == 0)
                && init != NULL
                && init->type == AST_IDENTIFIER
                && init->data.identifier.name != NULL
                && ctx->current_func_decl != NULL) {
                const char *resolved =
                    transpiler_find_local_type_name(ctx, ctx->current_func_decl,
                        init->data.identifier.name);
                if (resolved != NULL)
                    init_type_name = resolved;
            }
            const char *c_init_type =
                pergyra_type_to_c(init_type_name);

            /* Tuple destructuring: let (a, b, ...) = tuple_expr (MIR path) */
            if (init_type_name != NULL && init_type_name[0] == '(') {
                char elem_names[8][64];
                size_t arity = 0;
                {
                    size_t ti = 1;
                    size_t tlen = strlen(init_type_name);
                    while (ti < tlen && init_type_name[ti] != ')' && arity < 8) {
                        while (ti < tlen
                               && (init_type_name[ti] == ' '
                                   || init_type_name[ti] == '\t'))
                            ti++;
                        size_t eo = 0;
                        int depth = 0;
                        while (ti < tlen && eo + 1 < sizeof(elem_names[0])) {
                            char c = init_type_name[ti];
                            if (depth == 0 && (c == ',' || c == ')'))
                                break;
                            if (c == '<' || c == '(') depth++;
                            if (c == '>' || c == ')') depth--;
                            elem_names[arity][eo++] = c;
                            ti++;
                        }
                        elem_names[arity][eo] = '\0';
                        while (eo > 0
                               && (elem_names[arity][eo-1] == ' '
                                   || elem_names[arity][eo-1] == '\t'))
                            elem_names[arity][--eo] = '\0';
                        arity++;
                        if (ti < tlen && init_type_name[ti] == ',') ti++;
                    }
                }
                if (arity != stmt->data.let_destructure.name_count) {
                    if (ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                            (unsigned long long) stmt->data.let_destructure.name_count,
                            (unsigned long long) arity);
                    }
                    ok = false;
                    break;
                }
                char *rhs_t = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
                if (rhs_t == NULL) {
                    ok = false;
                    break;
                }
                int tmp_id = ++ctx->tmp_counter;
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                    c_init_type, tmp_id, rhs_t);
                free(rhs_t);
                for (size_t dn = 0;
                     dn < stmt->data.let_destructure.name_count; dn++) {
                    const char *bname = stmt->data.let_destructure.names[dn];
                    if (bname == NULL)
                        continue;
                    char ssa_versioned_tmp[192];
                    snprintf(ssa_versioned_tmp, sizeof(ssa_versioned_tmp),
                             "%s.1", bname);
                    char *ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
                    if (ssa_versioned == NULL) {
                        ok = false;
                        break;
                    }
                    char *ssa_lhs =
                        transpiler_render_ssa_name(ctx, ssa_versioned);
                    if (ssa_lhs == NULL) {
                        free(ssa_versioned);
                        continue;
                    }
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf,
                        "%s = _pgy_destr_%d.f%zu;\n",
                        ssa_lhs, tmp_id, dn);
                    free(ssa_lhs);
                    register_typed_var(ctx, bname, elem_names[dn]);
                    transpiler_ssa_name_map_set(ssa_map_out, bname,
                        ssa_versioned);
                }
                if (!ok) break;
                continue;
            }

            const char *elem_inner = NULL;
            const char *elem_c_type = NULL;
            if (init_type_name != NULL
                && (strncmp(init_type_name, "Array<", 6) == 0
                    || strncmp(init_type_name, "Slice<", 6) == 0)) {
                elem_inner = slot_inner_type_name(init_type_name);
                elem_c_type = pergyra_type_to_c(elem_inner);
            }
            if (c_init_type == NULL || elem_c_type == NULL
                || elem_inner == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "cannot lower destructuring initializer of type '%s' to a concrete array element type",
                        init_type_name != NULL
                            ? init_type_name
                            : "(unknown)");
                }
                ok = false;
                break;
            }
            char *rhs = emit_expression_with_ssa_map(init, ctx, ssa_map_out);
            if (rhs == NULL) {
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to render destructuring initializer",
                             (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            int tmp_id = ++ctx->tmp_counter;
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s _pgy_destr_%d = %s;\n",
                c_init_type, tmp_id, rhs);
            free(rhs);
            for (size_t dn = 0;
                 dn < stmt->data.let_destructure.name_count; dn++) {
                const char *bname = stmt->data.let_destructure.names[dn];
                if (bname == NULL)
                    continue;
                /* The SSA map stores pointers without duplicating, so
                 * allocate a persistent "NAME.1" string per binding. */
                char ssa_versioned_tmp[192];
                snprintf(ssa_versioned_tmp, sizeof(ssa_versioned_tmp),
                         "%s.1", bname);
                char *ssa_versioned = pergyra_strdup(ssa_versioned_tmp);
                if (ssa_versioned == NULL) {
                    ok = false;
                    break;
                }
                char *ssa_lhs =
                    transpiler_render_ssa_name(ctx, ssa_versioned);
                if (ssa_lhs == NULL) {
                    free(ssa_versioned);
                    continue;
                }
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf,
                    "%s = _pgy_destr_%d.data[%zu];\n",
                    ssa_lhs, tmp_id, dn);
                free(ssa_lhs);
                register_typed_var(ctx, bname, elem_inner);
                transpiler_ssa_name_map_set(ssa_map_out, bname,
                    ssa_versioned);
            }
            if (!ok)
                break;
            continue;
        }
        if (stmt != NULL
            && stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && stmt->data.let_decl.initializer != NULL) {
            const char *versioned_local =
                transpiler_find_block_exit_ssa_name(block,
                                                    stmt->data.let_decl.name);
            if (versioned_local == NULL)
                versioned_local = transpiler_resolve_ssa_name(
                    (const TranspilerSSANameMap *)ssa_map_out,
                    stmt->data.let_decl.name);
            if (versioned_local != NULL) {
                char *lhs = transpiler_render_ssa_name(ctx, versioned_local);
                char *rhs = emit_expression_with_ssa_map(
                    stmt->data.let_decl.initializer, ctx, ssa_map_out);
                char *rendered_type = NULL;
                const char *value_type = NULL;

                if (stmt->data.let_decl.type != NULL) {
                    rendered_type = transpiler_render_effective_local_type_name(
                        ctx, stmt->data.let_decl.type);
                    value_type = rendered_type;
                } else {
                    value_type = infer_expression_type_name(
                        ctx, stmt->data.let_decl.initializer);
                }

                if (value_type != NULL
                    && strcmp(value_type, "Unknown") != 0) {
                    ASTNode *binding_type_ast =
                        transpiler_find_local_type_ast(ctx, func_decl,
                                                       stmt->data.let_decl.name);
                    char *binding_type_name = NULL;
                    if (binding_type_ast != NULL)
                        binding_type_name =
                            transpiler_render_effective_local_type_name(ctx,
                                                                        binding_type_ast);
                    if (is_slot_var(ctx, stmt->data.let_decl.name)
                        || (binding_type_name != NULL
                            && (transpiler_type_name_is_slot_like(binding_type_name)
                                || transpiler_type_name_is_claim_shape(binding_type_name)
                                || strncmp(binding_type_name, "Channel<", 8) == 0))
                        || (value_type != NULL
                            && (transpiler_type_name_is_slot_like(value_type)
                                || transpiler_type_name_is_claim_shape(value_type)
                                || strncmp(value_type, "Channel<", 8) == 0))) {
                        free(binding_type_name);
                        free(lhs);
                        free(rhs);
                        free(rendered_type);
                        continue;
                    }
                    free(binding_type_name);
                }

                if (lhs == NULL || rhs == NULL) {
                    free(lhs);
                    free(rhs);
                    free(rendered_type);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %zu emission failed: unable to materialize preserved let-binding '%s'",
                                 block->id,
                                 stmt->data.let_decl.name);
                    }
                    ok = false;
                    break;
                }

                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = %s;\n", lhs, rhs);
                free(lhs);
                free(rhs);

                if (!transpiler_ssa_name_map_set(ssa_map_out,
                                                 stmt->data.let_decl.name,
                                                 versioned_local)) {
                    free(rendered_type);
                    ok = false;
                    break;
                }
                if (value_type != NULL && value_type[0] != '\0')
                    register_typed_var(ctx, stmt->data.let_decl.name, value_type);
                free(rendered_type);
                continue;
            }
            if (transpiler_find_routine_exit_ssa_name(
                    mir_routine, stmt->data.let_decl.name) != NULL) {
                continue;
            }
        }
        emit_statement(stmt, ctx);
    }

    ctx->active_ssa_map = saved_active_ssa_map;
    return ok;
}

static bool
transpiler_can_emit_function_from_mir(const TranspilerCtx *ctx,
                                      const ASTNode *func_decl,
                                      const MIRRoutine **mir_routine_out)
{
    return transpiler_can_emit_function_from_mir_with_reason(
        ctx, func_decl, mir_routine_out, NULL, 0);
}

static bool
transpiler_can_emit_intent_cleanup_from_mir(const TranspilerCtx *ctx,
                                            const ASTNode *intent_decl,
                                            const MIRRoutine **mir_routine_out)
{
    return transpiler_can_emit_intent_cleanup_from_mir_with_reason(
        ctx, intent_decl, mir_routine_out, NULL, 0);
}
