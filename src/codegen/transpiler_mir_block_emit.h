#ifndef PGY_TRANSPILER_MIR_BLOCK_EMIT_H
#define PGY_TRANSPILER_MIR_BLOCK_EMIT_H

#include "transpiler_mir_destructure_emit.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_block_emit_helpers.h"
#include "transpiler_mir_preserved_let_emit.h"
#include "transpiler_mir_block_schedule_emit.h"
#include "transpiler_mir_stmt_emit.h"
#include "transpiler_mir_resource_op_emit.h"
#include "transpiler_mir_assignment_emit.h"
#include "transpiler_mir_local_type_ast_lookup.h"
#include "transpiler_mir_reason.h"
#include "../parser/ast_api.h"
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
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR emission failed for block %llu in %s: missing SSA entry map",
                     (unsigned long long) block->id, func_decl->type == AST_FUNC_DECL
                     ? ast_declaration_name(func_decl)
                     : ast_intent_decl_name(func_decl));
        return false;
    }
    if (!transpiler_mir_emit_for_in_body_binding(buf, mir_routine, block, ctx,
                                                 ssa_map_out)) {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: unsupported for-in body binding",
                     (unsigned long long) block->id);
        }
        return false;
    }
    if (!transpiler_mir_seed_block_phi_names(block, ssa_map_out))
        return false;
    saved_active_ssa_map = ctx->active_ssa_map;
    ctx->active_ssa_map = ssa_map_out;
    if (!transpiler_emit_mir_pin_enter_local(buf, ctx, block, reason, reason_cap)) {
        ctx->active_ssa_map = saved_active_ssa_map;
        return false;
    }
    if (!transpiler_mir_seed_pin_view_alias(block, ssa_map_out)) {
        ctx->active_ssa_map = saved_active_ssa_map;
        return false;
    }
    source_order_mode = !transpiler_mir_routine_has_explicit_cfg(mir_routine)
        && transpiler_mir_block_has_source_order_metadata(block)
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
        ASTNode *stmt = transpiler_mir_find_stmt_for_inst(inst);

        if (!transpiler_materialize_pending_inst_uses(buf, ctx, func_decl, block,
                                                      inst, ssa_map_out, ctx->indent,
                                                      true, reason, reason_cap)) {
            ok = false;
            break;
        }

        if (inst->kind == MIR_INST_RESOURCE_OP) {
            TranspilerMIRInstEmitResult resource_result =
                transpiler_emit_mir_resource_op_inst(
                    buf, mir_routine, block, inst, inst_index, ctx,
                    ssa_map_out, reason, reason_cap);
            if (resource_result == TRANSPILE_MIR_INST_FAILED) {
                ok = false;
                break;
            }
            if (resource_result == TRANSPILE_MIR_INST_HANDLED)
                continue;
        }

        {
            TranspilerMIRLocalLetEmitResult let_result =
                transpiler_emit_mir_source_local_let_def_inst(
                    buf, block, inst, stmt, ctx, ssa_map_out,
                    reason, reason_cap);
            if (let_result == TRANSPILE_MIR_LOCAL_LET_FAILED) {
                ok = false;
                break;
            }
            if (let_result == TRANSPILE_MIR_LOCAL_LET_HANDLED)
                continue;
        }

        {
            TranspilerMIRAssignmentEmitResult assignment_result =
                transpiler_emit_mir_assignment_def_inst(
                    buf, func_decl, block, inst, inst_index, stmt, ctx,
                    ssa_map_out, reason, reason_cap);
            if (assignment_result == TRANSPILE_MIR_ASSIGNMENT_FAILED) {
                ok = false;
                break;
            }
            if (assignment_result == TRANSPILE_MIR_ASSIGNMENT_HANDLED)
                continue;
        }

        if (inst->kind == MIR_INST_DEF
            && stmt != NULL
            && !mir_instruction_uses_source_statement_emit(inst)
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
                    transpiler_mir_reasonf(reason, reason_cap,
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
                    transpiler_mir_reasonf(reason, reason_cap,
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
            transpiler_register_defer(inst->expr0, ctx);
        if (stmt == NULL || stmt->type == AST_BLOCK || stmt->type == AST_RETURN
            || stmt->type == AST_DEFER_STMT) {
            continue;
        }
        if (!mir_instruction_source_stmt_fallback_is_allowed(inst)) {
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "MIR block %llu emission failed: STMT fallback outside allowed residual statement policy",
                    (unsigned long long) block->id);
            }
            ok = false;
            break;
        }
        if (stmt->type == AST_LET_DECL
            && ast_let_name(stmt) != NULL
            && ast_let_initializer(stmt) != NULL
            && ast_let_initializer(stmt)->type != AST_CALL
            && transpiler_find_routine_exit_ssa_name(
                   mir_routine, ast_let_name(stmt)) != NULL) {
            continue;
        }
        if (transpiler_mir_stmt_is_mirrored_resource(ctx, block, stmt))
            continue;
        if (stmt->type == AST_ASSIGNMENT) {
            ASTNode *target = ast_assignment_target(stmt);
            ASTNode *value = ast_assignment_value(stmt);
            if (target != NULL
                && target->type == AST_IDENTIFIER
                && ast_identifier_name(target) != NULL
                && transpiler_is_implicit_field(ctx,
                    ast_identifier_name(target))) {
                char map_reason[256];
                if (!transpiler_expr_identifiers_mapped(
                        ctx,
                        value,
                        ssa_map_out,
                        mir_routine->name,
                        map_reason,
                        sizeof(map_reason))) {
                    continue;
                }
            }
            if (target != NULL
                && target->type == AST_IDENTIFIER
                && ast_identifier_name(target) != NULL
                && !transpiler_is_implicit_field(
                       ctx, ast_identifier_name(target))) {
                const char *target_name = ast_identifier_name(target);
                const char *mapped_target = transpiler_resolve_ssa_name(
                    (const TranspilerSSANameMap *)ssa_map_out,
                    target_name);
                if (mapped_target != NULL) {
                    char *lhs = transpiler_render_ssa_name(ctx, mapped_target);
                    char *rhs = emit_expression_with_ssa_map(
                        value, ctx, ssa_map_out);

                    if (lhs == NULL || rhs == NULL) {
                        free(lhs);
                        free(rhs);
                        if (reason != NULL && reason_cap > 0) {
                            transpiler_mir_reasonf(reason, reason_cap,
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
        }
        if (transpiler_mir_routine_has_explicit_cfg(mir_routine)
            && transpiler_mir_inst_is_cfg_container(inst, stmt)) {
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
            && ast_let_name(stmt) != NULL
            && ast_let_initializer(stmt) != NULL) {
            bool handled_let = false;
            if (!transpiler_emit_mir_preserved_let_stmt(
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
#endif /* PGY_TRANSPILER_MIR_BLOCK_EMIT_H */
