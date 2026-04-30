/* C backend MIR resource-op statement emission owner. */
typedef enum
{
    TRANSPILE_MIR_INST_NOT_HANDLED,
    TRANSPILE_MIR_INST_HANDLED,
    TRANSPILE_MIR_INST_FAILED
} TranspilerMIRInstEmitResult;

static TranspilerMIRInstEmitResult
transpiler_emit_mir_resource_op_inst(CodeBuf *buf,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     const MIRInstruction *inst,
                                     size_t inst_index,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *ssa_map_out,
                                     char *reason,
                                     size_t reason_cap)
{
    if (inst == NULL || inst->kind != MIR_INST_RESOURCE_OP)
        return TRANSPILE_MIR_INST_NOT_HANDLED;
    if (!transpiler_mir_seed_resource_alias_local(ssa_map_out, inst))
        return TRANSPILE_MIR_INST_FAILED;
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
                    ctx, value_expr, (const TranspilerSSANameMap *)ssa_map_out,
                    mir_routine->name, map_reason, sizeof(map_reason))) {
                return TRANSPILE_MIR_INST_HANDLED;
            }
            if (!transpiler_seed_expr_identifier_mappings(
                    block, inst_index, value_expr, ssa_map_out)) {
                return TRANSPILE_MIR_INST_FAILED;
            }
        }
    }
    if (inst->arg1 != NULL
        && transpiler_resolve_ssa_name(
               (const TranspilerSSANameMap *)ssa_map_out, inst->arg1) == NULL) {
        const char *mapped_value = transpiler_find_prior_block_ssa_name(
            block, inst_index, inst->arg1);
        if (mapped_value == NULL)
            mapped_value = transpiler_find_block_exit_ssa_name(block, inst->arg1);
        if (mapped_value != NULL
            && !transpiler_ssa_name_map_set(ssa_map_out, inst->arg1, mapped_value)) {
            return TRANSPILE_MIR_INST_FAILED;
        }
    }
    if (inst->name != NULL && strcmp(inst->name, "Claim") == 0
        && (inst->ast == NULL || inst->ast->type != AST_WITH_STMT)) {
        return TRANSPILE_MIR_INST_HANDLED;
    }
    if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent, inst, "0", false)) {
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
                     "MIR block %llu emission failed: unable to emit resource op '%s'",
                     (unsigned long long) block->id,
                     inst->name != NULL ? inst->name : "<op>");
        }
        return TRANSPILE_MIR_INST_FAILED;
    }
    return TRANSPILE_MIR_INST_HANDLED;
}
