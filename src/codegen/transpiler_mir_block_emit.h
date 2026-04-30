/* C backend MIR block statement emission owner. */
#include "transpiler_mir_destructure_emit.h"
#include "transpiler_mir_fallback_let_emit.h"
#include "transpiler_mir_block_schedule_emit.h"
#include "transpiler_mir_stmt_emit.h"
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
    if (!transpiler_mir_emit_for_in_body_binding(buf, mir_routine, block, ctx,
                                                 ssa_map_out)) {
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
                     "MIR block %llu emission failed: unsupported for-in body binding",
                     (unsigned long long) block->id);
        }
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
    if (!transpiler_emit_mir_pin_enter_local(buf, ctx, block, reason, reason_cap)) {
        ctx->active_ssa_map = saved_active_ssa_map;
        return false;
    }
    if (block->is_pin_region
        && block->pin_view_name != NULL
        && block->pin_source_name != NULL) {
        if (!transpiler_ssa_name_map_set(ssa_map_out,
                                         block->pin_view_name,
                                         block->pin_source_name)) {
            ctx->active_ssa_map = saved_active_ssa_map;
            return false;
        }
    }
    source_order_mode = !transpiler_mir_routine_has_explicit_cfg(mir_routine)
        && block->source_statement_count > 0
        && block->source_statements != NULL
        && block->instruction_count > 0;
    if (source_order_mode) {
        if (!transpiler_mir_block_build_source_order(block, &inst_order,
                                                     reason, reason_cap)) {
            ctx->active_ssa_map = saved_active_ssa_map;
            return false;
        }
    }

    if (!transpiler_emit_mir_claim_prepass(buf, block, ctx,
                                           reason, reason_cap)) {
        free(inst_order);
        ctx->active_ssa_map = saved_active_ssa_map;
        return false;
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        size_t inst_index = source_order_mode ? inst_order[i] : i;
        const MIRInstruction *inst = &block->instructions[inst_index];
        ASTNode *stmt = inst->ast;

        if (stmt == NULL
            && inst->kind == MIR_INST_DEF
            && inst->arg0 != NULL) {
            if (block->source_ast != NULL)
                stmt = transpiler_find_let_decl_by_name_in_block(block->source_ast,
                                                                 inst->arg0);
            if (stmt == NULL)
                stmt = transpiler_find_let_decl_by_name(func_decl, inst->arg0);
        }

        if (!transpiler_materialize_pending_inst_uses(buf, ctx, func_decl, block,
                                                      inst, ssa_map_out, ctx->indent,
                                                      true, reason, reason_cap)) {
            ok = false;
            break;
        }

        if (inst->kind == MIR_INST_RESOURCE_OP) {
            if (!transpiler_mir_seed_resource_alias_local(ssa_map_out, inst)) {
                ok = false;
                break;
            }
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
            if (inst->name != NULL && strcmp(inst->name, "Claim") == 0) {
                if (inst->ast == NULL || inst->ast->type != AST_WITH_STMT)
                    continue;
            }
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
                if (transpiler_type_name_is_view_like(local_type_name_owned)) {
                    emit_statement(stmt, ctx);
                    ctx->active_type_hint = saved_type_hint;
                    free(rendered_type_hint);
                    free(local_type_name_owned);
                    continue;
                }
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
            if (stmt->data.let_decl.initializer != NULL
                && stmt->data.let_decl.initializer->type == AST_UNARY
                && stmt->data.let_decl.initializer->data.unary.op.type == TOKEN_QUESTION) {
                ASTNode *operand = stmt->data.let_decl.initializer->data.unary.operand;
                const char *result_type = infer_expression_type_name(ctx, operand);
                const char *result_c_type = NULL;
                char *operand_expr = NULL;
                int try_id;

                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                if (lhs == NULL
                    || result_type == NULL
                    || strncmp(result_type, "Result<", 7) != 0) {
                    free(lhs);
                    free(local_type_name_owned);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %llu emission failed: '?' let binding '%s' requires Result<T,E> operand",
                                 (unsigned long long) block->id,
                                 stmt->data.let_decl.name != NULL
                                     ? stmt->data.let_decl.name
                                     : "<binding>");
                    }
                    ok = false;
                    break;
                }
                if (ctx->current_return_type[0] == '\0'
                    || strncmp(ctx->current_return_type, "Result<", 7) != 0) {
                    free(lhs);
                    free(local_type_name_owned);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %llu emission failed: '?' let binding '%s' requires a Result-returning function",
                                 (unsigned long long) block->id,
                                 stmt->data.let_decl.name != NULL
                                     ? stmt->data.let_decl.name
                                     : "<binding>");
                    }
                    ok = false;
                    break;
                }

                result_c_type = pergyra_type_to_c(result_type);
                operand_expr = emit_expression_with_ssa_map(operand, ctx, ssa_map_out);
                if (operand_expr == NULL) {
                    free(lhs);
                    free(local_type_name_owned);
                    if (reason != NULL && reason_cap > 0) {
                        snprintf(reason, reason_cap,
                                 "MIR block %llu emission failed: unable to render '?' operand for '%s'",
                                 (unsigned long long) block->id,
                                 stmt->data.let_decl.name != NULL
                                     ? stmt->data.let_decl.name
                                     : "<binding>");
                    }
                    ok = false;
                    break;
                }
                try_id = ctx->tmp_counter++;
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s __try_%d = %s;\n",
                              result_c_type, try_id, operand_expr);
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf,
                              "if (__try_%d.tag != PgyResultOk) return __try_%d;\n",
                              try_id, try_id);
                write_indent_to(buf, ctx->indent);
                codebuf_write(buf, "%s = __try_%d.ok;\n", lhs, try_id);
                free(operand_expr);
                free(lhs);

                if (!transpiler_ssa_name_map_set(ssa_map_out,
                                                 stmt->data.let_decl.name,
                                                 inst->result_name)) {
                    free(local_type_name_owned);
                    ok = false;
                    break;
                }
                if (local_type_name_owned != NULL)
                    register_typed_var(ctx, stmt->data.let_decl.name,
                                       local_type_name_owned);
                free(local_type_name_owned);
                continue;
            }
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

        if (inst->kind == MIR_INST_LOOP_INIT) {
            if (!transpiler_mir_emit_for_loop_init_inst(buf, inst, ctx,
                                                        ssa_map_out)) {
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unsupported for-in CFG lowering",
                             (unsigned long long) block->id);
                }
                ok = false;
                break;
            }
            continue;
        }

        if (inst->kind != MIR_INST_STMT)
            continue;
        if (stmt != NULL && stmt->type == AST_DEFER_STMT)
            transpiler_register_defer(stmt->data.defer_stmt.body, ctx);
        if (stmt == NULL || stmt->type == AST_BLOCK || stmt->type == AST_RETURN
            || stmt->type == AST_DEFER_STMT) {
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
        if (transpiler_mir_stmt_is_mirrored_resource(ctx, block, stmt))
            continue;
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
            && transpiler_mir_stmt_is_cfg_container(stmt)) {
            continue;
        }
        if (stmt->type == AST_CALL) {
            if (!transpiler_emit_mir_call_statement(buf, block, stmt, ctx,
                                                    ssa_map_out,
                                                    reason, reason_cap)) {
                ok = false;
                break;
            }
            continue;
        }
        if (stmt->type == AST_LET_DESTRUCTURE) {
            if (!transpiler_emit_mir_let_destructure_stmt(
                    buf, block, stmt, ctx, ssa_map_out,
                    reason, reason_cap)) {
                ok = false;
                break;
            }
            continue;
        }
        if (stmt != NULL
            && stmt->type == AST_LET_DECL
            && stmt->data.let_decl.name != NULL
            && stmt->data.let_decl.initializer != NULL) {
            bool handled_let = false;
            if (!transpiler_emit_mir_fallback_let_stmt(
                    buf, func_decl, mir_routine, block, stmt, ctx,
                    ssa_map_out, &handled_let, reason, reason_cap)) {
                ok = false;
                break;
            }
            if (handled_let)
                continue;
        }
        emit_statement(stmt, ctx);
    }
    ctx->active_ssa_map = saved_active_ssa_map;
    return ok;
}
#include "transpiler_mir_emit_predicates.h"
