/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR assignment statement emission owner for the C backend.
 */

#include "transpiler_mir_assignment_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_mir_block_emit_helpers.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"

static bool
transpiler_mir_assignment_target_is_field(TranspilerCtx *ctx,
                                          const char *target_name)
{
    if (target_name == NULL)
        return false;
    return current_class_has_field(ctx, target_name)
        || current_party_has_field(ctx, target_name)
        || current_roster_has_field(ctx, target_name)
        || current_relation_has_field(ctx, target_name)
        || current_effect_has_field(ctx, target_name)
        || current_zone_has_field(ctx, target_name)
        || transpiler_current_world_has_field(ctx, target_name);
}

TranspilerMIRAssignmentEmitResult
transpiler_emit_mir_assignment_def_inst(CodeBuf *buf,
                                        const ASTNode *func_decl,
                                        const MIRBasicBlock *block,
                                        const MIRInstruction *inst,
                                        size_t inst_index,
                                        ASTNode *stmt,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *ssa_map_out,
                                        char *reason,
                                        size_t reason_cap)
{
    ASTNode *target = ast_assignment_target(stmt);
    ASTNode *value = ast_assignment_value(stmt);
    const char *target_name;
    char *lhs = NULL;
    char *rhs = NULL;
    bool target_is_field = false;
    bool is_local_binding = false;
    bool is_channel_receive_assignment = false;
    bool is_select_receive_assignment = false;

    if (!transpiler_mir_def_uses_source_statement_emit(
            inst, stmt, AST_ASSIGNMENT)
        || target == NULL
        || target->type != AST_IDENTIFIER
        || inst->arg0 == NULL
        || inst->result_name == NULL) {
        return TRANSPILE_MIR_ASSIGNMENT_NOT_HANDLED;
    }

    target_name = ast_identifier_name(target);
    if (target_name == NULL || strcmp(inst->arg0, target_name) != 0)
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;

    is_channel_receive_assignment =
        value != NULL && value->type == AST_CHANNEL_RECV;
    is_select_receive_assignment =
        is_channel_receive_assignment
        && transpiler_mir_def_uses_select_receive_statement_emit(
            inst, stmt, AST_ASSIGNMENT);
    if (is_channel_receive_assignment
        && !transpiler_mir_def_uses_channel_receive_statement_emit(
            inst, stmt, AST_ASSIGNMENT)) {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: channel receive assignment '%s' is missing receive emit fact",
                     (unsigned long long) block->id,
                     target_name != NULL ? target_name : "<target>");
        }
        return TRANSPILE_MIR_ASSIGNMENT_FAILED;
    }
    if (mir_instruction_uses_select_receive_statement_emit(inst)
        && !is_select_receive_assignment) {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: select receive assignment '%s' is missing select receive emit fact",
                     (unsigned long long) block->id,
                     target_name != NULL ? target_name : "<target>");
        }
        return TRANSPILE_MIR_ASSIGNMENT_FAILED;
    }

    is_local_binding = transpiler_has_explicit_local_binding(func_decl,
                                                             target_name);
    if (!is_local_binding)
        target_is_field = transpiler_mir_assignment_target_is_field(ctx,
                                                                    target_name);
    if (!target_is_field && inst->slot_anchor != NULL) {
        target_is_field = transpiler_mir_assignment_target_is_field(
            ctx, inst->slot_anchor);
    }

    if (target_is_field) {
        char *field_lhs = NULL;
        char *field_rhs = NULL;

        if (value != NULL
            && value->type == AST_IDENTIFIER
            && ast_identifier_name(value) != NULL
            && transpiler_resolve_ssa_name(
                   (const TranspilerSSANameMap *)ssa_map_out,
                   ast_identifier_name(value)) == NULL) {
            const char *prior_ssa = transpiler_find_prior_block_ssa_name(
                block, inst_index,
                ast_identifier_name(value));
            if (prior_ssa != NULL) {
                transpiler_ssa_name_map_set(
                    ssa_map_out,
                    ast_identifier_name(value),
                    prior_ssa);
            }
        }
        field_lhs = emit_expression_with_ssa_map(
            target, ctx, ssa_map_out);
        field_rhs = emit_expression_with_ssa_map(
            value, ctx, ssa_map_out);
        if (field_lhs == NULL || field_rhs == NULL) {
            free(field_lhs);
            free(field_rhs);
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                         "MIR block %llu emission failed: unable to render field assignment to '%s'",
                         (unsigned long long) block->id,
                         target_name != NULL ? target_name : "<field>");
            }
            return TRANSPILE_MIR_ASSIGNMENT_FAILED;
        }
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s = %s;\n", field_lhs, field_rhs);
        free(field_lhs);
        free(field_rhs);
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
    }

    if (!is_local_binding) {
        emit_statement(stmt, ctx);
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
    }
    if (transpiler_type_name_is_slot_like(lookup_typed_var(ctx, target_name))) {
        emit_statement(stmt, ctx);
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
    }

    lhs = transpiler_render_ssa_name(ctx, inst->result_name);
    rhs = emit_expression_with_ssa_map(value, ctx, ssa_map_out);
    if (rhs == NULL) {
        free(lhs);
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: unable to render assignment to '%s'",
                     (unsigned long long) block->id,
                     target_name != NULL ? target_name : "<target>");
        }
        return TRANSPILE_MIR_ASSIGNMENT_FAILED;
    }

    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s = %s;\n", lhs, rhs);
    free(lhs);
    free(rhs);

    if (!transpiler_ssa_name_map_set(ssa_map_out, target_name,
                                     inst->result_name)) {
        return TRANSPILE_MIR_ASSIGNMENT_FAILED;
    }
    return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
}
