/* C backend MIR residual statement helpers. */

static bool
transpiler_mir_stmt_is_mirrored_resource(TranspilerCtx *ctx,
                                         const MIRBasicBlock *block,
                                         ASTNode *stmt)
{
    if (ctx == NULL || ctx->mir == NULL || block == NULL || stmt == NULL)
        return false;

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
            resource_is_secure = lookup_slot_is_secure(ctx,
                resource_inst->slot_anchor);
        if (resource_is_secure)
            continue;
        return true;
    }
    return false;
}

static bool
transpiler_emit_mir_call_statement(CodeBuf *buf,
                                   const MIRBasicBlock *block,
                                   ASTNode *stmt,
                                   TranspilerCtx *ctx,
                                   TranspilerSSANameMap *ssa_map,
                                   char *reason,
                                   size_t reason_cap)
{
    char *expr;

    if (buf == NULL || block == NULL || stmt == NULL || ctx == NULL)
        return false;

    expr = emit_expression_with_ssa_map(stmt, ctx, ssa_map);
    if (expr == NULL || expr[0] == '\0') {
        free(expr);
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
                     "MIR block %llu emission failed: unable to render call statement with SSA mapping",
                     (unsigned long long) block->id);
        }
        return false;
    }

    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s;\n", expr);
    emit_zone_action_effect_runtime(stmt, ctx);
    free(expr);
    return true;
}
