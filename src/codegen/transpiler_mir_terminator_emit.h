#ifndef PGY_TRANSPILER_MIR_TERMINATOR_EMIT_H
#define PGY_TRANSPILER_MIR_TERMINATOR_EMIT_H

#include "transpiler_mir_expr_ssa.h"

/* C backend MIR terminator emission owner. */

static void
transpiler_mir_set_pin_cleanup_error(TranspilerCtx *ctx,
                                     const char *name,
                                     const MIRBasicBlock *block,
                                     const char *block_reason)
{
    if (ctx == NULL || ctx->backend_error != NULL || block == NULL)
        return;
    transpiler_set_mir_topology_invalid(
        ctx,
        "MIR pin cleanup emission failed in function '%s' at block %llu: %s",
        name != NULL ? name : "<function>",
        (unsigned long long) block->id,
        block_reason != NULL && block_reason[0] != '\0'
            ? block_reason
            : "unknown reason");
}

static bool
transpiler_emit_mir_branch_terminator(ASTNode *node,
                                      const MIRRoutine *mir_routine,
                                      const MIRBasicBlock *block,
                                      size_t block_index,
                                      const MIRInstruction *inst,
                                      const char *name,
                                      TranspilerCtx *ctx,
                                      TranspilerSSANameMap *block_ssa_map,
                                      char *block_reason,
                                      size_t block_reason_cap)
{
    char *cond = transpiler_mir_render_branch_condition(
        node, mir_routine, inst, block->succ_true, ctx, block_ssa_map);
    const char *cond_text = cond != NULL ? cond : "false";

    if (block->has_succ_true) {
        transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent, block_index,
            block, &mir_routine->blocks[block->succ_true]);
    }
    if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                            block_reason,
                                            block_reason_cap)) {
        free(cond);
        transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
        return false;
    }

    write_indent(ctx);
    transpiler_write_condition_head(ctx, "if", cond_text, " {\n");
    write_indent_to(ctx->out, ctx->indent + 1);
    codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                  name, block->succ_true);
    write_indent(ctx);
    codebuf_write(ctx->out, "} else {\n");
    if (block->has_succ_false) {
        transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent + 1,
            block_index, block, &mir_routine->blocks[block->succ_false]);
    }
    write_indent_to(ctx->out, ctx->indent + 1);
    codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                  name, block->succ_false);
    write_indent(ctx);
    codebuf_write(ctx->out, "}\n");
    free(cond);
    return true;
}

static bool
transpiler_emit_mir_return_terminator(const MIRBasicBlock *block,
                                      const MIRInstruction *inst,
                                      const char *name,
                                      TranspilerCtx *ctx,
                                      TranspilerSSANameMap *block_ssa_map,
                                      char *block_reason,
                                      size_t block_reason_cap)
{
    char *ret_expr = NULL;

    transpiler_emit_defers_from(ctx, 0);
    if (inst->expr0 != NULL) {
        const char *saved_expected_type = ctx->expected_type;
        if (ctx->current_return_type[0] != '\0'
            && strcmp(ctx->current_return_type, "Void") != 0
            && strcmp(ctx->current_return_type, "void") != 0) {
            ctx->expected_type = ctx->current_return_type;
        }
        ret_expr = emit_expression_with_ssa_map(inst->expr0, ctx, block_ssa_map);
        ctx->expected_type = saved_expected_type;
    }
    if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                            block_reason,
                                            block_reason_cap)) {
        free(ret_expr);
        transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
        return false;
    }

    write_indent(ctx);
    if (inst->expr0 != NULL) {
        codebuf_write(ctx->out, "return %s;\n",
                      ret_expr != NULL ? ret_expr : "0");
    } else {
        codebuf_write(ctx->out, "return;\n");
    }
    free(ret_expr);
    return true;
}

static bool
transpiler_emit_mir_explicit_terminator(ASTNode *node,
                                        const MIRRoutine *mir_routine,
                                        const MIRBasicBlock *block,
                                        size_t block_index,
                                        const char *name,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *block_ssa_map,
                                        bool *terminator_emitted,
                                        char *block_reason,
                                        size_t block_reason_cap)
{
    if (terminator_emitted != NULL)
        *terminator_emitted = false;
    for (size_t j = 0; j < block->instruction_count; j++) {
        const MIRInstruction *inst = &block->instructions[j];
        if (inst->kind == MIR_INST_RESOURCE_OP)
            continue;
        if (inst->kind == MIR_INST_BRANCH) {
            if (!transpiler_emit_mir_branch_terminator(
                    node, mir_routine, block, block_index, inst, name, ctx,
                    block_ssa_map, block_reason, block_reason_cap)) {
                return false;
            }
            if (terminator_emitted != NULL)
                *terminator_emitted = true;
            return true;
        }
        if (inst->kind == MIR_INST_RETURN) {
            if (!transpiler_emit_mir_return_terminator(
                    block, inst, name, ctx, block_ssa_map,
                    block_reason, block_reason_cap)) {
                return false;
            }
            if (terminator_emitted != NULL)
                *terminator_emitted = true;
            return true;
        }
    }
    return true;
}

static bool
transpiler_emit_mir_fallthrough_terminator(const MIRRoutine *mir_routine,
                                           const MIRBasicBlock *block,
                                           size_t block_index,
                                           const char *name,
                                           TranspilerCtx *ctx,
                                           char *block_reason,
                                           size_t block_reason_cap)
{
    if (block->has_succ_true) {
        transpiler_emit_mir_phi_copies(ctx->out, ctx, ctx->indent,
            block_index, block, &mir_routine->blocks[block->succ_true]);
        if (!transpiler_mir_emit_loop_backedge_increment(ctx->out, ctx,
                                                         mir_routine,
                                                         block)) {
            return false;
        }
        if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                                block_reason,
                                                block_reason_cap)) {
            transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
            return false;
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
                      name, block->succ_true);
        return true;
    }

    if (!block->has_succ_false) {
        transpiler_emit_defers_from(ctx, 0);
        if (!transpiler_emit_mir_pin_exit_local(ctx->out, ctx, block,
                                                block_reason,
                                                block_reason_cap)) {
            transpiler_mir_set_pin_cleanup_error(ctx, name, block, block_reason);
            return false;
        }
        write_indent(ctx);
        if (strcmp(ctx->current_return_type, "Void") == 0) {
            codebuf_write(ctx->out, "return;\n");
        } else {
            codebuf_write(ctx->out, "__builtin_unreachable();\n");
        }
    }
    return true;
}

#endif /* PGY_TRANSPILER_MIR_TERMINATOR_EMIT_H */
