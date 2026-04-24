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

static bool
transpiler_seed_expr_identifier_mappings(const MIRBasicBlock *block,
                                         size_t inst_index,
                                         const ASTNode *expr,
                                         TranspilerSSANameMap *ssa_map_out)
{
    if (block == NULL || expr == NULL || ssa_map_out == NULL)
        return true;

    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        const char *name = expr->data.identifier.name;
        const char *mapped_value;
        if (transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ssa_map_out, name) != NULL) {
            return true;
        }
        mapped_value = transpiler_find_prior_block_ssa_name(block, inst_index, name);
        if (mapped_value == NULL)
            mapped_value = transpiler_find_block_exit_ssa_name(block, name);
        if (mapped_value != NULL)
            return transpiler_ssa_name_map_set(ssa_map_out, name, mapped_value);
        return true;
    }

    switch (expr->type) {
        case AST_BINARY:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.binary.left, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.binary.right, ssa_map_out);
        case AST_UNARY:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.unary.operand, ssa_map_out);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type != AST_IDENTIFIER
                && !transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.call.callee, ssa_map_out)) {
                return false;
            }
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, expr->data.call.arguments[i], ssa_map_out)) {
                    return false;
                }
            }
            return true;
        case AST_MEMBER_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.member.object, ssa_map_out);
        case AST_ARRAY_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.array_access.array, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.array_access.index, ssa_map_out);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, expr->data.array_literal.elements[i], ssa_map_out)) {
                    return false;
                }
            }
            return true;
        case AST_ASSIGNMENT:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.assignment.target, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.assignment.value, ssa_map_out);
        case AST_AWAIT_EXPR:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.await_expr.expression, ssa_map_out);
        case AST_CHANNEL_SEND:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.channel_send.channel, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.channel_send.value, ssa_map_out);
        case AST_CHANNEL_RECV:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.channel_recv.channel, ssa_map_out);
        default:
            return true;
    }
}

static bool
transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                  const ASTNode *expr,
                                  const TranspilerSSANameMap *ssa_map,
                                  const char *routine_name,
                                  char *reason,
                                  size_t reason_cap)
{
    if (expr == NULL)
        return true;
    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        const char *name = expr->data.identifier.name;
        const char *existing_type = NULL;
        if (transpiler_resolve_ssa_name(ssa_map, expr->data.identifier.name) != NULL)
            return true;
        if (transpiler_name_is_token_local(name))
            return true;
        if (ctx != NULL) {
            existing_type = lookup_typed_var((TranspilerCtx *)ctx, name);
            if (is_slot_var((TranspilerCtx *)ctx, name))
                return true;
            if (existing_type != NULL
                && (transpiler_type_name_is_slot_like(existing_type)
                    || transpiler_type_name_is_claim_shape(existing_type)
                    || strncmp(existing_type, "Channel<", 8) == 0)) {
                return true;
            }
        }
        if (ctx != NULL && find_enum_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (ctx != NULL && lookup_enum_variant_qualified_name((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (ctx != NULL && find_function_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
                     "MIR contract breach in %s at line %u: unresolved identifier `%s` (expected SSA-mapped local)",
                     routine_name != NULL ? routine_name : "<routine>",
                     expr->line,
                     expr->data.identifier.name);
        }
        return false;
    }

    switch (expr->type) {
        case AST_BINARY:
            if (!transpiler_expr_identifiers_mapped(ctx, expr->data.binary.left, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, expr->data.binary.right, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_UNARY:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.unary.operand, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type == AST_IDENTIFIER
                && expr->data.call.callee->data.identifier.name != NULL
                && (strcmp(expr->data.call.callee->data.identifier.name, "ToObject") == 0
                    || strcmp(expr->data.call.callee->data.identifier.name, "ToTObject") == 0)) {
                for (size_t i = 1; i < expr->data.call.arg_count; i++) {
                    if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.arguments[i], ssa_map,
                                                           routine_name, reason, reason_cap))
                        return false;
                }
                return true;
            }
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type != AST_IDENTIFIER) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.callee, ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            } else if (expr->data.call.callee != NULL
                       && expr->data.call.callee->type == AST_IDENTIFIER
                       && expr->data.call.callee->data.identifier.name != NULL) {
                const char *call_target = expr->data.call.callee->data.identifier.name;
                if (transpiler_resolve_ssa_name(ssa_map, call_target) != NULL
                    && !transpiler_expr_identifiers_mapped(ctx, expr->data.call.callee, ssa_map,
                                                          routine_name, reason, reason_cap))
                    return false;
            }
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.arguments[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        case AST_MEMBER_ACCESS:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.member.object, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_ACCESS:
            if (!transpiler_expr_identifiers_mapped(ctx, expr->data.array_access.array, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, expr->data.array_access.index, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_LITERAL: {
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.array_literal.elements[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        }
        case AST_ASSIGNMENT:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.assignment.target, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, expr->data.assignment.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_AWAIT_EXPR:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.await_expr.expression, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_SEND:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.channel_send.channel, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, expr->data.channel_send.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_RECV:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.channel_recv.channel, ssa_map,
                                                     routine_name, reason, reason_cap);
        default:
            return true;
    }
}

static bool
transpiler_has_mapping_for_all_emitted_blocks(const TranspilerCtx *ctx,
                                             const MIRRoutine *routine,
                                             const ASTNode *func_decl,
                                             bool require_non_cleanup,
                                             char *reason,
                                             size_t reason_cap)
{
    const char *routine_name = routine != NULL && routine->name != NULL ? routine->name : "<routine>";
    if (routine == NULL || routine->blocks == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        TranspilerSSANameMap ssa_map = {0};

        if (block == NULL || (require_non_cleanup && block->is_cleanup)
            || !block->is_reachable)
            continue;
        if (!transpiler_emit_mir_block_with_ssa_map(&ssa_map, block))
            return false;
        /* Add function/intent parameters to SSA map - they're valid C identifiers, not SSA vars */
        if (func_decl != NULL) {
            if (func_decl->type == AST_FUNC_DECL) {
                for (size_t p = 0; p < func_decl->data.func_decl.param_count; p++) {
                    FuncParam *param = func_decl->data.func_decl.params[p];
                    if (param != NULL && param->name != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, param->name, param->name);
                    }
                }
                transpiler_register_with_alias_bindings_in_block(&ssa_map,
                    func_decl->data.func_decl.body);
            } else if (func_decl->type == AST_INTENT_DECL) {
                /* For intent, extract alias from involves/value bindings */
                for (size_t p = 0; p < func_decl->data.intent_decl.involve_count; p++) {
                    ASTNode *involves = func_decl->data.intent_decl.involves[p];
                    if (involves != NULL && involves->type == AST_INTENT_INVOLVES
                        && involves->data.intent_involves.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, involves->data.intent_involves.alias, involves->data.intent_involves.alias);
                    }
                }
                for (size_t p = 0; p < func_decl->data.intent_decl.value_count; p++) {
                    ASTNode *value = func_decl->data.intent_decl.values[p];
                    if (value != NULL && value->type == AST_INTENT_VALUE
                        && value->data.intent_value.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, value->data.intent_value.alias,
                            value->data.intent_value.alias);
                    }
                }
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_STMT && inst->ast != NULL) {
                if (inst->ast->type == AST_LET_DECL
                    && inst->ast->data.let_decl.name != NULL) {
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(
                            block, inst->ast->data.let_decl.name);
                    if (versioned_name != NULL) {
                        if (!transpiler_ssa_name_map_set(
                                &ssa_map,
                                inst->ast->data.let_decl.name,
                                versioned_name)) {
                            return false;
                        }
                    }
                } else if (inst->ast->type == AST_LET_DESTRUCTURE
                           && inst->ast->data.let_destructure.names != NULL) {
                    for (size_t dn = 0;
                         dn < inst->ast->data.let_destructure.name_count;
                         dn++) {
                        const char *binding =
                            inst->ast->data.let_destructure.names[dn];
                        const char *versioned_name;
                        if (binding == NULL)
                            continue;
                        versioned_name =
                            transpiler_find_block_exit_ssa_name(block, binding);
                        if (versioned_name != NULL) {
                            if (!transpiler_ssa_name_map_set(&ssa_map,
                                                             binding,
                                                             versioned_name)) {
                                return false;
                            }
                        }
                    }
                } else if (inst->ast->type == AST_ASSIGNMENT
                           && inst->ast->data.assignment.target != NULL
                           && inst->ast->data.assignment.target->type == AST_IDENTIFIER
                           && inst->ast->data.assignment.target->data.identifier.name != NULL) {
                    const char *target_name =
                        inst->ast->data.assignment.target->data.identifier.name;
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(block, target_name);
                    if (versioned_name != NULL) {
                        if (!transpiler_ssa_name_map_set(&ssa_map,
                                                         target_name,
                                                         versioned_name)) {
                            return false;
                        }
                    }
                }
            }
            if (!transpiler_materialize_pending_inst_uses(NULL,
                                                          (TranspilerCtx *)ctx,
                                                          func_decl,
                                                          block,
                                                          inst,
                                                          &ssa_map,
                                                          0,
                                                          false,
                                                          reason,
                                                          reason_cap)) {
                return false;
            }
            if ((inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
                 || inst->kind == MIR_INST_RESOURCE_OP || inst->kind == MIR_INST_CLEANUP_EDGE)
                && inst->ast != NULL) {
                if (!transpiler_expr_identifiers_mapped(ctx, inst->ast, &ssa_map, routine_name,
                                                       reason, reason_cap))
                    return false;
            }
            if (inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI) {
                char base[128];
                size_t version = 0;
                const char *base_name = (inst->kind == MIR_INST_PHI
                                         ? inst->name : inst->arg0);
                if (base_name != NULL && base_name[0] != '\0'
                    && inst->result_name != NULL
                    && transpiler_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                    if (!transpiler_ssa_name_map_set(&ssa_map, base_name, inst->result_name))
                        return false;
                }
            }
        }
    }
    return true;
}

static bool
transpiler_validate_mir_emission_contract(const TranspilerCtx *ctx,
                                         const MIRRoutine *routine,
                                         const ASTNode *decl,
                                         bool require_cleanup,
                                         bool require_cleanup_blocks,
                                         char *reason,
                                         size_t reason_cap)
{
    const char *routine_name = "<routine>";
    const char *decl_name = NULL;

    if (decl != NULL) {
        if (decl->type == AST_FUNC_DECL && decl->data.func_decl.name != NULL)
            decl_name = decl->data.func_decl.name;
        if (decl->type == AST_INTENT_DECL && decl->data.intent_decl.name != NULL)
            decl_name = decl->data.intent_decl.name;
    }
    routine_name = decl_name != NULL ? decl_name
        : (routine != NULL && routine->name != NULL ? routine->name : "<routine>");

    if (routine == NULL || routine->blocks == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "MIR contract invalid for %s: no routine", routine_name);
        return false;
    }

    {
        char *topology_error = NULL;
        if (!mir_validate_emission_topology(routine,
                                           require_cleanup,
                                           !require_cleanup_blocks,
                                           &topology_error)) {
            if (reason != NULL && reason_cap > 0) {
                if (topology_error != NULL)
                    snprintf(reason, reason_cap, "%s", topology_error);
                else
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: topology validation failed",
                             routine_name);
            }
            free(topology_error);
            return false;
        }
        free(topology_error);
    }
