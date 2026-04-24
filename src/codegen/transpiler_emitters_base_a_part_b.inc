            ctx->expected_type = NULL;
            codebuf_write(ctx->out, "%s = %s;\n", decl, init_expr);
            free(init_expr);
        } else {
            codebuf_write(ctx->out, "%s = 0;\n", decl);
        }
        free(decl);
    } else if (init != NULL) {
        ctx->expected_type = ann_type_name;
        char *init_expr = emit_expression(init, ctx);
        ctx->expected_type = NULL;
        codebuf_write(ctx->out, "%s %s = %s;\n", c_type, name, init_expr);
        free(init_expr);
    } else if (transpiler_c_type_uses_scalar_zero(c_type)) {
        /* Scalar / pointer default. */
        codebuf_write(ctx->out, "%s %s = 0;\n", c_type, name);
    } else {
        /* Aggregate default.  Plain `= 0` is not a valid struct initializer
         * in C99, so emit a compound literal zero.  Semantic currently
         * rejects uninitialized locals before reaching this path, but this
         * keeps the fallback well-formed as defense in depth. */
        codebuf_write(ctx->out, "%s %s = (%s){0};\n", c_type, name, c_type);
    }

    if (ann_type_name != NULL) {
        register_typed_var(ctx, name, ann_type_name);
        free(ann_type_name);
    } else if (init != NULL && init->type == AST_CALL) {
        register_typed_var(ctx, name, infer_expression_type_name(ctx, init));
    } else if (init != NULL && init->type == AST_SPAWN_EXPR) {
        char *future_type = infer_spawn_return_type_name(ctx, init);
        char *wrapped = strdup_fmt("Future<%s>", future_type);
        register_typed_var(ctx, name, wrapped);
        free(future_type);
        free(wrapped);
    } else if (init != NULL && init->type == AST_CHANNEL_RECV) {
        const char *inner = infer_expression_type_name(ctx, init);
        register_typed_var(ctx, name, inner);
    } else if (init != NULL) {
        /* Fallback: infer from any initializer (string literals, binary exprs, etc.) */
        const char *inferred = infer_expression_type_name(ctx, init);
        if (inferred != NULL) {
            register_typed_var(ctx, name, inferred);
        }
    }
}

/* -----------------------------------------------------------------
 * Function declaration emitter
 * ----------------------------------------------------------------- */

#include "transpiler_emitters_mir_inventory_ssa.inc"

static ASTNode *
transpiler_find_let_decl_by_name_in_block(ASTNode *body, const char *name)
{
    if (body == NULL || name == NULL)
        return NULL;

    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_let_decl_by_name_in_block(
                body->data.block.statements[i], name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }

    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, name) == 0) {
        return body;
    }

    if (body->type == AST_WITH_STMT)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.with_stmt.body, name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.then_branch, name);
        if (found != NULL)
            return found;
        return transpiler_find_let_decl_by_name_in_block(
            body->data.if_stmt.else_branch, name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.while_loop.body, name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_let_decl_by_name_in_block(
            body->data.for_loop.body, name);

    return NULL;
}

static ASTNode *
transpiler_find_let_decl_by_name(const ASTNode *func_decl, const char *name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL) {
        return NULL;
    }

    return transpiler_find_let_decl_by_name_in_block(
        func_decl->data.func_decl.body, name);
}

static bool
transpiler_materialize_pending_inst_uses(CodeBuf *buf,
                                         TranspilerCtx *ctx,
                                         const ASTNode *func_decl,
                                         const MIRBasicBlock *block,
                                         const MIRInstruction *inst,
                                         TranspilerSSANameMap *ssa_map_out,
                                         int indent,
                                         bool emit_assignments,
                                         char *reason,
                                         size_t reason_cap)
{
    if (ctx == NULL || block == NULL || inst == NULL || ssa_map_out == NULL)
        return true;

    for (size_t i = 0; i < inst->use_count; i++) {
        const char *versioned_use = inst->uses[i];
        const char *exit_versioned;
        ASTNode *let_decl;
        ASTNode *initializer;
        const char *existing_type;
        ASTNode *binding_type_ast;
        char *binding_type_name = NULL;
        char base[128];
        size_t version = 0;
        char *lhs = NULL;
        char *rhs = NULL;
        const char *value_type = NULL;
        char *rendered_type = NULL;

        if (versioned_use == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned_use, base, sizeof(base), &version))
            continue;
        if (transpiler_resolve_ssa_name(ssa_map_out, base) != NULL)
            continue;
        if (is_slot_var(ctx, base))
            continue;
        existing_type = lookup_typed_var(ctx, base);
        if (existing_type != NULL
            && (transpiler_type_name_is_slot_like(existing_type)
                || transpiler_type_name_is_claim_shape(existing_type)
                || strncmp(existing_type, "Channel<", 8) == 0)) {
            continue;
        }
        binding_type_ast = transpiler_find_local_type_ast(ctx, func_decl, base);
        if (binding_type_ast != NULL) {
            binding_type_name = transpiler_render_effective_local_type_name(
                ctx, binding_type_ast);
            if (binding_type_name != NULL
                && (transpiler_type_name_is_slot_like(binding_type_name)
                    || transpiler_type_name_is_claim_shape(binding_type_name)
                    || strncmp(binding_type_name, "Channel<", 8) == 0)) {
                free(binding_type_name);
                continue;
            }
            free(binding_type_name);
            binding_type_name = NULL;
        }

        exit_versioned = transpiler_find_block_exit_ssa_name(block, base);
        if (exit_versioned == NULL)
            continue;

        let_decl = NULL;
        if (block->source_statements != NULL) {
            for (size_t stmt_idx = 0; stmt_idx < block->source_statement_count; stmt_idx++) {
                ASTNode *source_stmt = block->source_statements[stmt_idx];
                if (source_stmt == NULL || source_stmt->type != AST_LET_DECL)
                    continue;
                if (source_stmt->data.let_decl.name != NULL
                    && strcmp(source_stmt->data.let_decl.name, base) == 0) {
                    let_decl = source_stmt;
                    break;
                }
            }
        }
        if (let_decl == NULL)
            let_decl = transpiler_find_let_decl_by_name(func_decl, base);
        if (let_decl == NULL || let_decl->type != AST_LET_DECL)
            continue;
        initializer = let_decl->data.let_decl.initializer;
        if (initializer == NULL)
            continue;
        if (initializer->type == AST_CALL
            && initializer->data.call.callee != NULL
            && initializer->data.call.callee->type == AST_IDENTIFIER
            && initializer->data.call.callee->data.identifier.name != NULL
            && strncmp(initializer->data.call.callee->data.identifier.name,
                       "Claim", 5) == 0) {
            continue;
        }

        if (emit_assignments) {
            lhs = transpiler_render_ssa_name(ctx, exit_versioned);
            rhs = emit_expression_with_ssa_map(initializer, ctx, ssa_map_out);
            if (lhs == NULL || rhs == NULL) {
                free(lhs);
                free(rhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to materialize pending value '%s'",
                             (unsigned long long) block->id, base);
                }
                return false;
            }
            write_indent_to(buf, indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);
        }

        if (!transpiler_ssa_name_map_set(ssa_map_out, base, exit_versioned))
            return false;

        if (let_decl->data.let_decl.type != NULL) {
            rendered_type = transpiler_render_effective_local_type_name(
                ctx, let_decl->data.let_decl.type);
            value_type = rendered_type;
        } else {
            value_type = infer_expression_type_name(ctx, initializer);
        }
        if (value_type != NULL
            && value_type[0] != '\0'
            && strcmp(value_type, "Void") != 0) {
            register_typed_var(ctx, base, value_type);
        }
        free(rendered_type);
    }

    return true;
}

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
                if (current_world_has_field(ctx, target_name))
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
                if (current_world_has_field(ctx, slot_anchor))
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
