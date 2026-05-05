/* C backend MIR CFG control helpers.
 *
 * These helpers keep explicit CFG containers out of the generic AST statement
 * and expression emitters. MIR owns the control-flow edges; C lowering only
 * renders the loop/match branch conditions and edge-local maintenance.
 */

#include "transpiler_mir_cfg_policy.h"

static bool
transpiler_mir_emit_for_loop_init_inst(CodeBuf *buf,
                                       const MIRInstruction *inst,
                                       TranspilerCtx *ctx,
                                       const TranspilerSSANameMap *ssa_map)
{
    char *start;
    const char *variable;

    if (buf == NULL || inst == NULL || ctx == NULL)
        return true;
    if (inst->kind != MIR_INST_LOOP_INIT)
        return true;
    if (inst->branch_shape != MIR_BRANCH_FOR_RANGE
        && inst->branch_shape != MIR_BRANCH_FOR_IN)
        return false;
    variable = inst->arg0;
    if (variable == NULL)
        return true;
    if (inst->branch_shape == MIR_BRANCH_FOR_IN) {
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "size_t _pgy_idx_%s = 0;\n", variable);
        return true;
    }

    start = emit_expression_with_ssa_map(inst->expr0, ctx, ssa_map);
    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "int32_t %s = %s;\n",
                  variable,
                  start != NULL ? start : "0");
    register_typed_var(ctx, variable, "Int");
    free(start);
    return true;
}

static const char *
transpiler_mir_for_in_element_type(TranspilerCtx *ctx,
                                   ASTNode *iterable,
                                   const char **collection_type_out)
{
    const char *collection_type;

    if (collection_type_out != NULL)
        *collection_type_out = NULL;
    if (ctx == NULL || iterable == NULL)
        return NULL;

    collection_type = infer_expression_type_name(ctx, iterable);
    if (collection_type_out != NULL)
        *collection_type_out = collection_type;
    if (collection_type == NULL)
        return NULL;
    if (strncmp(collection_type, "Array<", 6) != 0
        && strncmp(collection_type, "Slice<", 6) != 0
        && strncmp(collection_type, "List<", 5) != 0) {
        return NULL;
    }
    return pergyra_type_to_c(slot_inner_type_name(collection_type));
}

static char *
transpiler_mir_render_for_loop_condition_inst(const MIRInstruction *inst,
                                              TranspilerCtx *ctx,
                                              const TranspilerSSANameMap *ssa_map)
{
    char *end;
    char *cond;
    const char *variable;

    if (inst == NULL || ctx == NULL)
        return NULL;
    if (inst->branch_shape != MIR_BRANCH_FOR_RANGE
        && inst->branch_shape != MIR_BRANCH_FOR_IN)
        return NULL;
    variable = inst->arg0;
    if (variable == NULL)
        return NULL;
    if (inst->branch_shape == MIR_BRANCH_FOR_IN) {
        const char *collection_type = NULL;
        const char *length_field;
        char *collection;
        (void)transpiler_mir_for_in_element_type(ctx, inst->expr0, &collection_type);
        length_field = transpiler_mir_for_in_length_field(collection_type);
        collection = emit_expression_with_ssa_map(inst->expr0, ctx, ssa_map);
        cond = strdup_fmt("_pgy_idx_%s < %s.%s",
            variable,
            collection != NULL ? collection : "0",
            length_field);
        free(collection);
        return cond;
    }

    end = emit_expression_with_ssa_map(inst->expr1, ctx, ssa_map);
    cond = strdup_fmt("%s < %s", variable, end != NULL ? end : "0");
    free(end);
    return cond;
}

static bool
transpiler_mir_emit_for_in_body_binding(CodeBuf *buf,
                                        const MIRRoutine *routine,
                                        const MIRBasicBlock *block,
                                        TranspilerCtx *ctx,
                                        const TranspilerSSANameMap *ssa_map)
{
    const MIRInstruction *branch_inst;
    const char *variable;
    const char *collection_type = NULL;
    const char *element_type;
    char *collection;

    if (buf == NULL || routine == NULL || block == NULL || ctx == NULL)
        return true;

    branch_inst = transpiler_mir_find_incoming_for_in_branch(routine, block);
    if (branch_inst == NULL)
        return true;
    variable = branch_inst->arg0;
    if (variable == NULL)
        return true;

    element_type = transpiler_mir_for_in_element_type(
        ctx, branch_inst->expr0, &collection_type);
    if (element_type == NULL)
        return false;

    collection = emit_expression_with_ssa_map(branch_inst->expr0, ctx, ssa_map);
    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s %s = %s.data[_pgy_idx_%s];\n",
        element_type,
        variable,
        collection != NULL ? collection : "0",
        variable);
    register_typed_var(ctx, variable, slot_inner_type_name(collection_type));
    free(collection);
    return true;
}

static bool
transpiler_mir_emit_loop_backedge_increment(CodeBuf *buf,
                                            TranspilerCtx *ctx,
                                            const MIRRoutine *routine,
                                            const MIRBasicBlock *block)
{
    const MIRBasicBlock *target;
    const MIRInstruction *branch_inst;
    const char *variable;

    if (buf == NULL || ctx == NULL || routine == NULL || block == NULL)
        return true;
    if (!block->has_succ_true || block->succ_true >= routine->block_count)
        return true;
    if (block->id <= block->succ_true)
        return true;

    target = &routine->blocks[block->succ_true];
    if (target == block)
        return true;

    branch_inst = transpiler_mir_find_loop_branch_inst(target);
    if (branch_inst == NULL)
        return true;
    variable = branch_inst->arg0;
    if (variable == NULL)
        return true;

    write_indent_to(buf, ctx->indent);
    if (branch_inst->branch_shape == MIR_BRANCH_FOR_IN) {
        codebuf_write(buf, "_pgy_idx_%s = _pgy_idx_%s + 1;\n",
            variable,
            variable);
    } else {
        codebuf_write(buf, "%s = %s + 1;\n", variable, variable);
    }
    return true;
}

#include "transpiler_mir_match_condition_emit.h"

static char *
transpiler_mir_render_branch_condition(ASTNode *func_decl,
                                       const MIRInstruction *inst,
                                       TranspilerCtx *ctx,
                                       const TranspilerSSANameMap *ssa_map)
{
    ASTNode *condition = inst != NULL ? inst->ast : NULL;
    if (condition == NULL)
        return pergyra_strdup("true");
    if (inst->branch_shape == MIR_BRANCH_FOR_RANGE
        || inst->branch_shape == MIR_BRANCH_FOR_IN)
        return transpiler_mir_render_for_loop_condition_inst(inst, ctx, ssa_map);
    if (inst->branch_shape == MIR_BRANCH_MATCH_CASE)
        return transpiler_mir_render_match_case_condition(func_decl, condition,
                                                         ctx, ssa_map);
    return emit_expression_with_ssa_map(condition, ctx, ssa_map);
}
