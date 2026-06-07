#include "transpiler_mir_cfg_control_emit.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_channel_type_query.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_mir_cfg_policy.h"
#include "transpiler_mir_match_condition_emit.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"

static void
transpiler_mir_loop_binding_name(const MIRInstruction *inst,
                                 const char *variable,
                                 char *buf,
                                 size_t buf_size)
{
    uint32_t stable_id;

    if (buf == NULL || buf_size == 0)
        return;
    buf[0] = '\0';
    stable_id = mir_instruction_source_stable_id(inst);
    if (stable_id == 0) {
        snprintf(buf, buf_size, "_pgy_for_%s",
                 variable != NULL ? variable : "it");
        return;
    }
    snprintf(buf, buf_size, "_pgy_for_%s_%u",
             variable != NULL ? variable : "it", stable_id);
}

static void
transpiler_mir_loop_index_name(const MIRInstruction *inst,
                               const char *variable,
                               char *buf,
                               size_t buf_size)
{
    uint32_t stable_id;

    if (buf == NULL || buf_size == 0)
        return;
    buf[0] = '\0';
    stable_id = mir_instruction_source_stable_id(inst);
    if (stable_id == 0) {
        snprintf(buf, buf_size, "_pgy_idx_%s",
                 variable != NULL ? variable : "it");
        return;
    }
    snprintf(buf, buf_size, "_pgy_idx_%s_%u",
             variable != NULL ? variable : "it", stable_id);
}

static bool
transpiler_mir_set_loop_binding_name(TranspilerCtx *ctx,
                                     TranspilerSSANameMap *ssa_map,
                                     const char *variable,
                                     const char *loop_name)
{
    const char *stable_name;

    if (ssa_map == NULL)
        return true;
    if (ctx == NULL || variable == NULL || loop_name == NULL)
        return false;
    stable_name = transpiler_scratch_strdup(ctx, loop_name);
    if (stable_name == NULL)
        return false;
    if (ctx->match_binding_alias_map != NULL) {
        (void)transpiler_ssa_name_map_set(
            (TranspilerSSANameMap *)ctx->match_binding_alias_map,
            variable, stable_name);
    }
    return transpiler_ssa_name_map_set(ssa_map, variable, stable_name);
}

bool
transpiler_mir_emit_for_loop_init_inst(CodeBuf *buf,
                                       const MIRInstruction *inst,
                                       TranspilerCtx *ctx,
                                       TranspilerSSANameMap *ssa_map)
{
    char *start;
    const char *variable;
    char loop_name[256];
    char idx_name[256];

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
    transpiler_mir_loop_binding_name(inst, variable, loop_name,
                                     sizeof(loop_name));
    if (inst->branch_shape == MIR_BRANCH_FOR_IN) {
        transpiler_mir_loop_index_name(inst, variable, idx_name,
                                       sizeof(idx_name));
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "size_t %s = 0;\n", idx_name);
        if (!transpiler_mir_set_loop_binding_name(ctx, ssa_map, variable,
                                                  loop_name)) {
            return false;
        }
        return true;
    }

    start = emit_expression_with_ssa_map(inst->expr0, ctx, ssa_map);
    if (start == NULL) {
        if (ctx->backend_error == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "MIR for-range start expression could not be emitted");
        }
        return false;
    }
    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "int32_t %s = %s;\n", loop_name, start);
    if (!transpiler_mir_set_loop_binding_name(ctx, ssa_map, variable,
                                              loop_name)) {
        free(start);
        return false;
    }
    register_typed_var(ctx, variable, "Int");
    register_typed_var(ctx, loop_name, "Int");
    free(start);
    return true;
}

static bool
transpiler_mir_for_in_element_type(TranspilerCtx *ctx,
                                   ASTNode *iterable,
                                   const char **collection_type_out,
                                   char *element_type_buf,
                                   size_t element_type_buf_size,
                                   char *inner_type_buf,
                                   size_t inner_type_buf_size)
{
    const char *collection_type;

    if (collection_type_out != NULL)
        *collection_type_out = NULL;
    if (ctx == NULL || iterable == NULL)
        return false;

    collection_type = infer_expression_type_name(ctx, iterable);
    if (collection_type_out != NULL)
        *collection_type_out = collection_type;
    if (collection_type == NULL)
        return false;
    if (!transpiler_type_name_is_array_or_slice(collection_type)
        && !transpiler_type_name_is_list(collection_type)) {
        return false;
    }
    if (!slot_inner_type_name_copy(collection_type, inner_type_buf,
            inner_type_buf_size))
        return false;
    return transpiler_require_type_name_c_type_copy(ctx, inner_type_buf,
        "MIR for-in element",
        element_type_buf, element_type_buf_size);
}

static char *
transpiler_mir_render_for_loop_condition_inst(
    const MIRInstruction *inst,
    TranspilerCtx *ctx,
    const TranspilerSSANameMap *ssa_map)
{
    char *end;
    char *cond;
    const char *variable;
    char loop_name[256];
    char idx_name[256];

    if (inst == NULL || ctx == NULL)
        return NULL;
    if (inst->branch_shape != MIR_BRANCH_FOR_RANGE
        && inst->branch_shape != MIR_BRANCH_FOR_IN)
        return NULL;
    variable = inst->arg0;
    if (variable == NULL)
        return NULL;
    transpiler_mir_loop_binding_name(inst, variable, loop_name,
                                     sizeof(loop_name));
    if (inst->branch_shape == MIR_BRANCH_FOR_IN) {
        const char *collection_type = NULL;
        const char *length_field;
        char element_type_buf[128];
        char inner_type_buf[128];
        char *collection;
        if (!transpiler_mir_for_in_element_type(ctx, inst->expr0,
                &collection_type, element_type_buf, sizeof(element_type_buf),
                inner_type_buf, sizeof(inner_type_buf))) {
            if (ctx->backend_error == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "MIR for-in condition requires Array<T>, Slice<T>, or List<T> iterable metadata");
            }
            return NULL;
        }
        length_field = transpiler_mir_for_in_length_field(collection_type);
        collection = emit_expression_with_ssa_map(inst->expr0, ctx, ssa_map);
        if (collection == NULL) {
            if (ctx->backend_error == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "MIR for-in condition could not emit iterable expression");
            }
            return NULL;
        }
        transpiler_mir_loop_index_name(inst, variable, idx_name,
                                       sizeof(idx_name));
        cond = strdup_fmt("%s < %s.%s",
            idx_name,
            collection,
            length_field);
        free(collection);
        return cond;
    }

    end = emit_expression_with_ssa_map(inst->expr1, ctx, ssa_map);
    if (end == NULL) {
        if (ctx->backend_error == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_INSPECT_MIR_INVENTORY,
                "MIR for-range end expression could not be emitted");
        }
        return NULL;
    }
    cond = strdup_fmt("%s < %s", loop_name, end);
    free(end);
    return cond;
}

bool
transpiler_mir_emit_for_in_body_binding(CodeBuf *buf,
                                        const MIRRoutine *routine,
                                        const MIRBasicBlock *block,
                                        TranspilerCtx *ctx,
                                        TranspilerSSANameMap *ssa_map)
{
    const MIRInstruction *branch_inst;
    const char *variable;
    const char *collection_type = NULL;
    char element_type[128];
    char inner_type[128];
    char *collection;
    char loop_name[256];
    char idx_name[256];

    if (buf == NULL || routine == NULL || block == NULL || ctx == NULL)
        return true;

    branch_inst = transpiler_mir_find_incoming_loop_branch(routine, block);
    bool is_body_entry = branch_inst != NULL;
    if (branch_inst == NULL)
        branch_inst = transpiler_mir_find_backedge_loop_branch(routine, block);
    if (branch_inst == NULL)
        return true;
    variable = branch_inst->arg0;
    if (variable == NULL)
        return true;
    transpiler_mir_loop_binding_name(branch_inst, variable, loop_name,
                                     sizeof(loop_name));
    if (branch_inst->branch_shape == MIR_BRANCH_FOR_RANGE) {
        if (!transpiler_mir_set_loop_binding_name(ctx, ssa_map, variable,
                                                  loop_name)) {
            return false;
        }
        register_typed_var(ctx, variable, "Int");
        register_typed_var(ctx, loop_name, "Int");
        return true;
    }

    if (!transpiler_mir_for_in_element_type(ctx, branch_inst->expr0,
            &collection_type, element_type, sizeof(element_type),
            inner_type, sizeof(inner_type))) {
        return false;
    }

    if (is_body_entry) {
        collection = emit_expression_with_ssa_map(branch_inst->expr0, ctx,
                                                  ssa_map);
        if (collection == NULL) {
            if (ctx->backend_error == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_INSPECT_MIR_INVENTORY,
                    "MIR for-in body binding could not emit iterable expression");
            }
            return false;
        }
        transpiler_mir_loop_index_name(branch_inst, variable, idx_name,
                                       sizeof(idx_name));
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s %s = %s.data[%s];\n",
            element_type,
            loop_name,
            collection,
            idx_name);
        free(collection);
    }
    if (!transpiler_mir_set_loop_binding_name(ctx, ssa_map, variable,
                                              loop_name)) {
        return false;
    }
    if (ctx->match_binding_alias_map != NULL) {
        const char *stable_alias = transpiler_scratch_strdup(ctx, loop_name);
        if (stable_alias != NULL) {
            (void)transpiler_ssa_name_map_set(
                (TranspilerSSANameMap *)ctx->match_binding_alias_map,
                variable, stable_alias);
        }
    }
    register_typed_var(ctx, variable, inner_type);
    register_typed_var(ctx, loop_name, inner_type);
    return true;
}

bool
transpiler_mir_emit_loop_backedge_increment(CodeBuf *buf,
                                            TranspilerCtx *ctx,
                                            const MIRRoutine *routine,
                                            const MIRBasicBlock *block)
{
    const MIRBasicBlock *target;
    const MIRInstruction *branch_inst;
    const char *variable;
    char loop_name[256];
    char idx_name[256];

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
    transpiler_mir_loop_binding_name(branch_inst, variable, loop_name,
                                     sizeof(loop_name));
    transpiler_mir_loop_index_name(branch_inst, variable, idx_name,
                                   sizeof(idx_name));

    write_indent_to(buf, ctx->indent);
    if (branch_inst->branch_shape == MIR_BRANCH_FOR_IN) {
        codebuf_write(buf, "%s = %s + 1;\n", idx_name, idx_name);
    } else {
        codebuf_write(buf, "%s = %s + 1;\n", loop_name, loop_name);
    }
    return true;
}

static ASTNode *
transpiler_mir_recv_expr_channel(ASTNode *node)
{
    if (node == NULL || node->type != AST_CHANNEL_RECV)
        return NULL;
    return ast_channel_recv_channel(node);
}

static ASTNode *
transpiler_mir_assignment_recv_channel(ASTNode *node)
{
    if (node == NULL || node->type != AST_ASSIGNMENT)
        return NULL;
    return transpiler_mir_recv_expr_channel(ast_assignment_value(node));
}

static ASTNode *
transpiler_mir_select_case_channel(ASTNode *node)
{
    ASTNode *first;

    if (node == NULL || node->type != AST_BLOCK
        || ast_block_statement_count(node) == 0)
        return NULL;
    first = ast_block_statement(node, 0);
    if (first == NULL)
        return NULL;
    if (first->type == AST_CHANNEL_RECV)
        return ast_channel_recv_channel(first);
    return transpiler_mir_assignment_recv_channel(first);
}

static char *
transpiler_mir_render_channel_ready_condition(
    ASTNode *channel,
    TranspilerCtx *ctx,
    const TranspilerSSANameMap *ssa_map)
{
    char inner_buf[64];
    const char *inner;
    char *channel_expr;
    char *result;

    if (channel == NULL || ctx == NULL)
        return NULL;
    inner = transpiler_require_channel_inner_type(ctx, channel,
        "MIR select dispatch", inner_buf, sizeof(inner_buf));
    if (inner == NULL)
        return NULL;
    if (channel->type == AST_IDENTIFIER && ast_identifier_name(channel) != NULL) {
        channel_expr = emit_expression_with_ssa_map(channel, ctx, NULL);
    } else {
        channel_expr = emit_expression_with_ssa_map(channel, ctx, ssa_map);
    }
    if (channel_expr == NULL)
        return NULL;
    result = strdup_fmt("pgy_channel_ready_%s(&%s)", inner, channel_expr);
    free(channel_expr);
    return result;
}

static char *
transpiler_mir_render_select_case_condition(
    ASTNode *case_node,
    const MIRRoutine *routine,
    size_t target_block,
    TranspilerCtx *ctx,
    const TranspilerSSANameMap *ssa_map)
{
    ASTNode *channel = transpiler_mir_select_case_channel(case_node);

    if (channel != NULL)
        return transpiler_mir_render_channel_ready_condition(channel, ctx,
                                                            ssa_map);
    if (routine == NULL || target_block >= routine->block_count)
        return NULL;

    const MIRBasicBlock *target = &routine->blocks[target_block];
    for (size_t i = 0; i < target->instruction_count; i++) {
        const MIRInstruction *inst = &target->instructions[i];
        if (inst->kind != MIR_INST_DEF)
            continue;
        channel = transpiler_mir_recv_expr_channel(inst->expr0);
        if (channel != NULL)
            return transpiler_mir_render_channel_ready_condition(channel, ctx,
                                                                ssa_map);
    }
    return NULL;
}

char *
transpiler_mir_render_branch_condition(const MIRRoutine *routine,
                                       const MIRInstruction *inst,
                                       size_t target_block,
                                       TranspilerCtx *ctx,
                                       TranspilerSSANameMap *ssa_map)
{
    ASTNode *condition;

    if (inst == NULL)
        return pergyra_strdup("true");
    if (inst->branch_shape == MIR_BRANCH_FOR_RANGE
        || inst->branch_shape == MIR_BRANCH_FOR_IN)
        return transpiler_mir_render_for_loop_condition_inst(inst, ctx, ssa_map);
    if (inst->branch_shape == MIR_BRANCH_MATCH_CASE) {
        if (!mir_instruction_has_required_branch_condition_fact(inst))
            return pergyra_strdup("true");
        condition = mir_instruction_source_payload(inst);
        if (condition == NULL)
            return pergyra_strdup("true");
        return transpiler_mir_render_match_case_condition(inst, ctx, ssa_map);
    }
    if (inst->branch_shape == MIR_BRANCH_SELECT_DISPATCH) {
        char *select_cond;
        if (!mir_instruction_has_required_branch_condition_fact(inst)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C MIR select branch requires branch condition fact");
            return NULL;
        }
        select_cond = transpiler_mir_render_select_case_condition(
            mir_instruction_source_payload(inst), routine, target_block, ctx,
            ssa_map);
        return select_cond;
    }
    condition = inst->expr0;
    if (condition == NULL)
        return pergyra_strdup("true");
    return emit_expression_with_ssa_map(condition, ctx, ssa_map);
}
