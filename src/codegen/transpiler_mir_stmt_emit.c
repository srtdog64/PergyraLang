#include "transpiler_mir_stmt_emit.h"

#include <stdlib.h>

#include "../compiler/mir.h"
#include "../parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_reason.h"
#include "transpiler_projection_sync.h"
#include "transpiler_symbols.h"

/* Emit the domain-lifecycle runtime guard carried by this MIR call fact
 * (doc/12 section 2.3). The receiver is rendered through the same SSA map the
 * call uses, so &(recv) is the call's lvalue. Construction state defaults to
 * the initial index (absent == state 0) in the runtime side-map, so no separate
 * init line is emitted. */
static void
transpiler_emit_mir_lifecycle_guard(CodeBuf *buf,
                                    const MIRInstruction *inst,
                                    ASTNode *stmt,
                                    TranspilerCtx *ctx,
                                    TranspilerSSANameMap *ssa_map)
{
    ASTNode *callee;
    ASTNode *obj;
    char    *recv;

    if (!mir_instruction_has_lifecycle_guard(inst)
        || stmt == NULL
        || stmt->type != AST_CALL) {
        return;
    }
    callee = ast_call_callee(stmt);
    obj = callee != NULL ? ast_member_object(callee) : NULL;
    if (obj == NULL || obj->type != AST_IDENTIFIER)
        return;
    recv = emit_expression_with_ssa_map(obj, ctx, ssa_map);
    if (recv == NULL || recv[0] == '\0') {
        free(recv);
        return;
    }
    write_indent_to(buf, ctx->indent);
    if (mir_instruction_lifecycle_guard_kind(inst)
            == MIR_LIFECYCLE_GUARD_CHECK) {
        codebuf_write(buf,
            "pgy_runtime_lifecycle_guard_export(&(%s), %uu, %d, \"%s\", \"%s\");\n",
            recv,
            (unsigned)mir_instruction_lifecycle_valid_mask(inst),
            mir_instruction_lifecycle_to_state(inst),
            mir_instruction_lifecycle_op(inst) != NULL
                ? mir_instruction_lifecycle_op(inst)
                : "",
            mir_instruction_lifecycle_subject(inst) != NULL
                ? mir_instruction_lifecycle_subject(inst)
                : "");
    } else {
        codebuf_write(buf,
            "pgy_runtime_lifecycle_set_export(&(%s), %d);\n",
            recv,
            mir_instruction_lifecycle_to_state(inst));
    }
    free(recv);
}

bool
transpiler_mir_resource_has_mirroring_stmt_in_block(
    const MIRBasicBlock *block,
    const MIRInstruction *resource_inst)
{
    if (block == NULL || resource_inst == NULL)
        return false;
    if (resource_inst->kind != MIR_INST_RESOURCE_OP)
        return false;
    if (mir_instruction_resource_op_keeps_residual_statement_emit(resource_inst))
        return false;
    if (!mir_instruction_has_source_statement_order(resource_inst))
        return false;
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *cand = &block->instructions[i];
        if (cand == resource_inst)
            continue;
        if (cand->kind != MIR_INST_STMT)
            continue;
        if (mir_instructions_share_source_statement(resource_inst, cand))
            return true;
    }
    return false;
}

bool
transpiler_mir_stmt_is_mirrored_resource(TranspilerCtx *ctx,
                                         const MIRBasicBlock *block,
                                         const MIRInstruction *stmt_inst)
{
    if (ctx == NULL || !transpiler_active_has_mir(ctx)
        || block == NULL || stmt_inst == NULL)
        return false;

    for (size_t ri = 0; ri < block->instruction_count; ri++) {
        const MIRInstruction *resource_inst = &block->instructions[ri];
        bool resource_is_secure = false;
        if (resource_inst->kind != MIR_INST_RESOURCE_OP)
            continue;
        if (!mir_instructions_share_source_statement(resource_inst, stmt_inst))
            continue;
        if (mir_instruction_source_matches_ast_type(stmt_inst, AST_PARALLEL_BLOCK)
            || mir_instruction_source_matches_ast_type(stmt_inst, AST_ASYNC_BLOCK)
            || mir_instruction_source_matches_ast_type(stmt_inst, AST_SPAWN_EXPR)
            || mir_instruction_source_matches_ast_type(stmt_inst, AST_AWAIT_EXPR)) {
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
                                   const MIRInstruction *inst,
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

    transpiler_emit_mir_lifecycle_guard(buf, inst, stmt, ctx, ssa_map);
    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s;\n", expr);
    emit_zone_action_effect_runtime(buf, stmt, ctx);
    free(expr);
    return true;
}
