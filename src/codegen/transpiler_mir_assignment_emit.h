typedef enum TranspilerMIRAssignmentEmitResult {
    TRANSPILE_MIR_ASSIGNMENT_NOT_HANDLED = 0,
    TRANSPILE_MIR_ASSIGNMENT_HANDLED,
    TRANSPILE_MIR_ASSIGNMENT_FAILED
} TranspilerMIRAssignmentEmitResult;

static bool
transpiler_mir_assignment_target_is_field(TranspilerCtx *ctx,
                                          const char *target_name)
{
    if (target_name == NULL)
        return false;
    return current_class_has_field(ctx, target_name)
        || current_relation_has_field(ctx, target_name)
        || current_effect_has_field(ctx, target_name)
        || current_zone_has_field(ctx, target_name)
        || transpiler_current_world_has_field(ctx, target_name);
}

static TranspilerMIRAssignmentEmitResult
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
    const char *target_name;
    char *lhs = NULL;
    char *rhs = NULL;
    bool target_is_field = false;
    bool is_local_binding = false;

    if (inst->kind != MIR_INST_DEF
        || stmt == NULL
        || stmt->type != AST_ASSIGNMENT
        || stmt->data.assignment.target == NULL
        || stmt->data.assignment.target->type != AST_IDENTIFIER
        || inst->arg0 == NULL
        || inst->result_name == NULL) {
        return TRANSPILE_MIR_ASSIGNMENT_NOT_HANDLED;
    }

    target_name = stmt->data.assignment.target->data.identifier.name;
    if (target_name == NULL || strcmp(inst->arg0, target_name) != 0)
        return TRANSPILE_MIR_ASSIGNMENT_HANDLED;

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

        if (stmt->data.assignment.value != NULL
            && stmt->data.assignment.value->type == AST_IDENTIFIER
            && stmt->data.assignment.value->data.identifier.name != NULL
            && transpiler_resolve_ssa_name(
                   (const TranspilerSSANameMap *)ssa_map_out,
                   stmt->data.assignment.value->data.identifier.name) == NULL) {
            const char *prior_ssa = transpiler_find_prior_block_ssa_name(
                block, inst_index,
                stmt->data.assignment.value->data.identifier.name);
            if (prior_ssa != NULL) {
                transpiler_ssa_name_map_set(
                    ssa_map_out,
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
    rhs = emit_expression_with_ssa_map(stmt->data.assignment.value, ctx,
                                       ssa_map_out);
    if (rhs == NULL) {
        free(lhs);
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
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
