#include "transpiler_mir_stmt_emit.h"

#include <stdlib.h>

#include "../compiler/mir.h"
#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_reason.h"
#include "transpiler_projection_sync.h"
#include "transpiler_symbols.h"

bool
transpiler_mir_resource_has_mirroring_stmt_in_block(
    const MIRBasicBlock *block,
    const MIRInstruction *resource_inst)
{
    ASTNode *resource_stmt;

    if (block == NULL || resource_inst == NULL)
        return false;
    if (resource_inst->kind != MIR_INST_RESOURCE_OP)
        return false;
    if (mir_instruction_resource_op_keeps_residual_statement_emit(resource_inst))
        return false;
    resource_stmt = mir_instruction_source_payload(resource_inst);
    if (resource_stmt == NULL)
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *cand = &block->instructions[i];
        if (cand == resource_inst)
            continue;
        if (cand->kind != MIR_INST_STMT)
            continue;
        if (mir_instruction_source_payload(cand) == resource_stmt)
            return true;
    }
    return false;
}

bool
transpiler_mir_stmt_is_mirrored_resource(TranspilerCtx *ctx,
                                         const MIRBasicBlock *block,
                                         ASTNode *stmt)
{
    if (ctx == NULL || !transpiler_active_has_mir(ctx)
        || block == NULL || stmt == NULL)
        return false;

    for (size_t ri = 0; ri < block->instruction_count; ri++) {
        const MIRInstruction *resource_inst = &block->instructions[ri];
        bool resource_is_secure = false;
        if (resource_inst->kind != MIR_INST_RESOURCE_OP)
            continue;
        if (mir_instruction_source_payload(resource_inst) != stmt)
            continue;
        if (stmt->type == AST_PARALLEL_BLOCK
            || stmt->type == AST_ASYNC_BLOCK
            || stmt->type == AST_SPAWN_EXPR
            || stmt->type == AST_AWAIT_EXPR) {
            /*
             * These resource ops are observability hooks, not semantic
             * replacements. The residual statement must still lower the
             * runtime body (tasks, sends, awaits, etc.).
             */
            continue;
        }
        if (mir_instruction_resource_op_keeps_residual_statement_emit(
                resource_inst)) {
            /*
             * AIR/RIR IO and channel ops are boundary evidence and
             * observability hooks. They do not emit the concrete builtin or
             * channel runtime call; the residual source statement owns that.
             */
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

bool
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
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: unable to render call statement with SSA mapping",
                     (unsigned long long) block->id);
        }
        return false;
    }

    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s;\n", expr);
    emit_zone_action_effect_runtime(buf, stmt, ctx);
    free(expr);
    return true;
}
