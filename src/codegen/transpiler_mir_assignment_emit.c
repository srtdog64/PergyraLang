/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR assignment statement emission owner for the C backend.
 */

#include "transpiler_mir_assignment_emit.h"

#include <stdlib.h>

#include "transpiler_format.h"
#include <string.h>

#include "../compiler/mir_source_local_expr_types.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_assignment_emit.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_block_emit_helpers.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_overlay_host_fields.h"
#include "transpiler_projection.h"
#include "transpiler_symbols.h"

static bool
transpiler_mir_def_is_source_assignment_emit(const MIRInstruction *inst)
{
    return inst != NULL
        && inst->kind == MIR_INST_DEF
        && mir_instruction_uses_source_statement_emit(inst)
        && mir_instruction_source_is_assignment(inst);
}

static char *
transpiler_mir_emit_assignment_parts_with_ssa_map(
    ASTNode *target,
    ASTNode *value,
    const char *expected_type,
    TranspilerCtx *ctx,
    const TranspilerSSANameMap *ssa_map)
{
    const void *saved_active_ssa_map;
    const char *saved_expected_type;
    char *result;

    if (ctx == NULL)
        return NULL;
    saved_active_ssa_map = ctx->active_ssa_map;
    saved_expected_type = ctx->expected_type;
    ctx->active_ssa_map = ssa_map;
    if (expected_type != NULL && expected_type[0] != '\0'
        && strcmp(expected_type, "Unknown") != 0) {
        ctx->expected_type = expected_type;
    }
    result = transpiler_emit_assignment_expression_parts(ctx, target, value);
    ctx->expected_type = saved_expected_type;
    ctx->active_ssa_map = saved_active_ssa_map;
    return result;
}

static const char *
transpiler_mir_assignment_expected_type_name(
    TranspilerCtx *ctx,
    const MIRRoutine *routine,
    ASTNode *target,
    MIRSourceLocalTypeScratch *scratch)
{
    const MIRProgram *mir = transpiler_active_mir_identity(ctx);

    if (mir == NULL || routine == NULL
        || target == NULL || scratch == NULL) {
        return NULL;
    }
    return mir_source_local_expr_type_name(
        mir, routine, scratch, target);
}

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

static bool
transpiler_mir_routine_has_param_name(const MIRRoutine *routine,
                                      const char *base_name)
{
    if (routine == NULL || base_name == NULL
        || !mir_routine_has_signature(routine)) {
        return false;
    }
    for (size_t i = 0; i < mir_routine_param_count(routine); i++) {
        FuncParam *param = mir_routine_param(routine, i);
        if (param != NULL
            && param->name != NULL
            && strcmp(param->name, base_name) == 0) {
            return true;
        }
    }
    return false;
}

static bool
transpiler_mir_assignment_target_is_local(const ASTNode *func_decl,
                                          const MIRRoutine *routine,
                                          const char *target_name,
                                          TranspilerCtx *ctx)
{
    if (target_name == NULL)
        return false;
    if (transpiler_mir_routine_has_param_name(routine, target_name)
        || transpiler_mir_routine_has_source_local_binding(
            routine, target_name)) {
        return true;
    }
    if (transpiler_mir_assignment_target_is_field(ctx, target_name)) {
        return false;
    }
    if (routine != NULL)
        return false;
    return transpiler_has_explicit_local_binding(func_decl, target_name);
}

bool
transpiler_emit_mir_assignment_expr_stmt(CodeBuf *buf,
                                         const MIRRoutine *mir_routine,
                                         const MIRBasicBlock *block,
                                         const MIRInstruction *inst,
                                         TranspilerCtx *ctx,
                                         TranspilerSSANameMap *ssa_map_out,
                                         char *reason,
                                         size_t reason_cap)
{
    MIRSourceLocalTypeScratch type_scratch = {0};
    const char *expected_type = NULL;
    char *expr = NULL;

    if (buf == NULL || block == NULL || inst == NULL
        || ctx == NULL || ssa_map_out == NULL
        || inst->kind != MIR_INST_ASSIGN
        || inst->expr0 == NULL
        || inst->expr1 == NULL) {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                "MIR block %llu emission failed: ASSIGN instruction missing MIR assignment facts",
                block != NULL ? (unsigned long long) block->id : 0ULL);
        }
        return false;
    }

    expected_type = transpiler_mir_assignment_expected_type_name(
        ctx, mir_routine, inst->expr0, &type_scratch);
    expr = transpiler_mir_emit_assignment_parts_with_ssa_map(
        inst->expr0, inst->expr1, expected_type, ctx, ssa_map_out);
    if (expr == NULL) {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                "MIR block %llu emission failed: unable to render MIR assignment expression",
                (unsigned long long) block->id);
        }
        return false;
    }

    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s;\n", expr);
    free(expr);
    return true;
}

TranspilerMIRAssignmentEmitResult
transpiler_emit_mir_assignment_def_inst(CodeBuf *buf,
                                        const ASTNode *func_decl,
                                        const MIRRoutine *mir_routine,
                                        const MIRBasicBlock *block,
                                        const MIRInstruction *inst,
                                        size_t inst_index,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *ssa_map_out,
                                        char *reason,
                                        size_t reason_cap)
{
    ASTNode *target = inst != NULL ? inst->expr1 : NULL;
    ASTNode *value = inst != NULL ? inst->expr0 : NULL;
    const char *target_name;
    char *lhs = NULL;
    char *rhs = NULL;
    bool target_is_field = false;
    bool is_local_binding = false;
    bool is_channel_receive_assignment = false;
    bool is_select_receive_assignment = false;

    if (!transpiler_mir_def_is_source_assignment_emit(inst))
        return TRANSPILE_MIR_ASSIGNMENT_NOT_HANDLED;
    if (target == NULL
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
        && mir_instruction_uses_select_receive_statement_emit(inst);
    if (is_channel_receive_assignment
        && !mir_instruction_uses_channel_receive_statement_emit(inst)) {
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

    is_local_binding = transpiler_mir_assignment_target_is_local(
        func_decl, mir_routine, target_name, ctx);
    if (!is_local_binding)
        target_is_field = transpiler_mir_assignment_target_is_field(ctx,
                                                                    target_name);
    if (!target_is_field && inst->slot_anchor != NULL) {
        target_is_field = transpiler_mir_assignment_target_is_field(
            ctx, inst->slot_anchor);
    }

    if (target_is_field) {
        MIRSourceLocalTypeScratch type_scratch = {0};
        const char *expected_type =
            transpiler_mir_assignment_expected_type_name(
                ctx, mir_routine, target, &type_scratch);
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
        /* Build the LHS directly as `self->base` (or `self.base` for
         * self-cell hosts). Routing through emit_expression_with_ssa_map
         * lets the identifier emit path consult the SSA map and rewrite
         * `hp` to `_pgy_ssa_hp_N` -- which would land the field
         * assignment on a parallel local instead of the actual field.
         *
         * RHS still goes through emit_expression_with_ssa_map so prior
         * SSA values feed in correctly. */
        {
            const char *base = target_name;
            bool host_field =
                base != NULL
                && (current_class_has_field(ctx, base)
                    || current_party_has_field(ctx, base)
                    || current_roster_has_field(ctx, base)
                    || current_relation_has_field(ctx, base)
                    || current_effect_has_field(ctx, base)
                    || current_zone_has_field(ctx, base)
                    || transpiler_current_world_has_field(ctx, base));
            if (host_field && current_class_has_field(ctx, base)) {
                field_lhs = strdup_fmt(
                    current_class_uses_self_cell(ctx)
                        ? "self->%s" : "self.%s", base);
            } else if (host_field) {
                field_lhs = strdup_fmt("self->%s", base);
            } else {
                field_lhs = emit_expression_with_ssa_map(
                    target, ctx, ssa_map_out);
            }
        }
        {
            const char *saved_expected_type = ctx->expected_type;
            if (expected_type != NULL && expected_type[0] != '\0'
                && strcmp(expected_type, "Unknown") != 0) {
                ctx->expected_type = expected_type;
            }
            field_rhs = emit_expression_with_ssa_map(
                value, ctx, ssa_map_out);
            ctx->expected_type = saved_expected_type;
        }
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
        /* Snapshot the post-write value into `_pgy_ssa_base_N`. MIR
         * versioned the assignment for SSA tracking, and downstream
         * phi merges / cross-block uses reference the versioned name.
         * Without this sync, those reads see the 0-init local
         * instead of the value the field now holds. */
        if (inst->result_name != NULL) {
            char ssa_base[128];
            size_t ssa_version = 0;
            if (transpiler_parse_versioned_name(inst->result_name,
                    ssa_base, sizeof(ssa_base), &ssa_version)
                && ssa_version > 0) {
                char *ssa_lhs = transpiler_render_ssa_name(ctx,
                    inst->result_name);
                if (ssa_lhs != NULL) {
                    write_indent_to(buf, ctx->indent);
                    codebuf_write(buf, "%s = %s;\n", ssa_lhs, field_lhs);
                    free(ssa_lhs);
                }
            }
        }
        free(field_lhs);
        free(field_rhs);
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
    }

    if (!is_local_binding) {
        MIRInstruction assign_inst = *inst;
        assign_inst.kind = MIR_INST_ASSIGN;
        assign_inst.expr0 = target;
        assign_inst.expr1 = value;
        if (!transpiler_emit_mir_assignment_expr_stmt(
                buf, mir_routine, block, &assign_inst, ctx, ssa_map_out,
                reason, reason_cap)) {
            return TRANSPILE_MIR_ASSIGNMENT_FAILED;
        }
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
    }
    if (transpiler_type_name_is_slot_like(lookup_typed_var(ctx, target_name))) {
        MIRInstruction assign_inst = *inst;
        assign_inst.kind = MIR_INST_ASSIGN;
        assign_inst.expr0 = target;
        assign_inst.expr1 = value;
        if (!transpiler_emit_mir_assignment_expr_stmt(
                buf, mir_routine, block, &assign_inst, ctx, ssa_map_out,
                reason, reason_cap)) {
            return TRANSPILE_MIR_ASSIGNMENT_FAILED;
        }
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
    }

    lhs = transpiler_render_ssa_name(ctx, inst->result_name);
    {
        MIRSourceLocalTypeScratch type_scratch = {0};
        const char *expected_type =
            transpiler_mir_assignment_expected_type_name(
                ctx, mir_routine, target, &type_scratch);
        const char *saved_expected_type = ctx->expected_type;
        if (expected_type != NULL && expected_type[0] != '\0'
            && strcmp(expected_type, "Unknown") != 0) {
            ctx->expected_type = expected_type;
        }
        rhs = emit_expression_with_ssa_map(value, ctx, ssa_map_out);
        ctx->expected_type = saved_expected_type;
    }
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
    {
        const char *value_type =
            transpiler_mir_routine_source_local_type_name(
                mir_routine, target_name);
        if (!transpiler_emit_mir_embedded_zone_local_init(
                ctx, buf, ctx->indent, inst->result_name, value_type,
                lhs)) {
            free(lhs);
            free(rhs);
            return TRANSPILE_MIR_ASSIGNMENT_FAILED;
        }
    }
    free(lhs);
    free(rhs);

    if (!transpiler_ssa_name_map_set(ssa_map_out, target_name,
                                     inst->result_name)) {
        return TRANSPILE_MIR_ASSIGNMENT_FAILED;
    }
    return TRANSPILE_MIR_ASSIGNMENT_HANDLED;
}
