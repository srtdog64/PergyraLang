#ifndef PERGYRA_TRANSPILER_MIR_BLOCK_SCHEDULE_EMIT_H
#define PERGYRA_TRANSPILER_MIR_BLOCK_SCHEDULE_EMIT_H

/* MIR block scheduling/prepass helpers for C emission. */
static bool
transpiler_mir_block_build_source_order(const MIRBasicBlock *block,
                                        size_t **inst_order_out,
                                        char *reason,
                                        size_t reason_cap)
{
    bool *scheduled;
    size_t *inst_order;
    size_t order_count = 0;

    if (inst_order_out != NULL)
        *inst_order_out = NULL;
    if (block == NULL || inst_order_out == NULL)
        return false;

    scheduled = calloc(block->instruction_count, sizeof(bool));
    inst_order = calloc(block->instruction_count, sizeof(size_t));
    if (scheduled == NULL || inst_order == NULL) {
        free(scheduled);
        free(inst_order);
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
                    && strcmp(inst->arg0,
                              source_stmt->data.let_decl.name) == 0) {
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
        if (!scheduled[inst_idx])
            inst_order[order_count++] = inst_idx;
    }

    free(scheduled);
    *inst_order_out = inst_order;
    return true;
}

static bool
transpiler_emit_mir_claim_prepass(CodeBuf *buf,
                                  const MIRBasicBlock *block,
                                  TranspilerCtx *ctx,
                                  char *reason,
                                  size_t reason_cap)
{
    if (buf == NULL || block == NULL || ctx == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_RESOURCE_OP)
            continue;
        if (inst->name == NULL || strcmp(inst->name, "Claim") != 0)
            continue;
        if (!transpiler_mir_block_has_local_def_for_anchor(
                block, inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0)) {
            continue;
        }
        if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent,
                                               inst, "0", false)) {
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                         "MIR block %zu emission failed: unable to emit claim op for '%s'",
                         block->id,
                         inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
            }
            return false;
        }
    }
    return true;
}

#endif /* PERGYRA_TRANSPILER_MIR_BLOCK_SCHEDULE_EMIT_H */
