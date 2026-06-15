#include "transpiler_mir_preserved_let_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_block_emit_helpers.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"
#include "transpiler_type_result_mapping_helpers.h"

bool
transpiler_emit_mir_preserved_let_stmt(CodeBuf *buf,
                                       const ASTNode *func_decl,
                                       const MIRRoutine *mir_routine,
                                       const MIRBasicBlock *block,
                                       ASTNode *stmt,
                                       TranspilerCtx *ctx,
                                       TranspilerSSANameMap *ssa_map_out,
                                       bool *handled_out,
                                       char *reason,
                                       size_t reason_cap)
{
    const char *versioned_local;
    const char *let_name;
    ASTNode *let_type;
    ASTNode *let_init;

    if (handled_out != NULL)
        *handled_out = false;
    if (buf == NULL || func_decl == NULL || mir_routine == NULL
        || block == NULL || stmt == NULL || ctx == NULL
        || ssa_map_out == NULL || stmt->type != AST_LET_DECL) {
        return true;
    }
    let_name = ast_let_name(stmt);
    let_type = ast_let_type(stmt);
    let_init = ast_let_initializer(stmt);
    if (let_name == NULL || let_init == NULL)
        return true;

    versioned_local = transpiler_find_block_exit_ssa_name(block, let_name);
    if (versioned_local == NULL)
        versioned_local = transpiler_find_block_renamed_ssa_name(block, let_name);
    if (versioned_local == NULL) {
        versioned_local = transpiler_resolve_ssa_name(
            (const TranspilerSSANameMap *)ssa_map_out, let_name);
    }
    if (versioned_local == NULL) {
        if (transpiler_find_routine_exit_ssa_name(
                mir_routine, let_name) != NULL) {
            if (handled_out != NULL)
                *handled_out = true;
        }
        return true;
    }

    {
        char *lhs = transpiler_render_ssa_name(ctx, versioned_local);
        ASTNode *saved_expected_callable_type = ctx->expected_callable_type;
        char *rhs;
        char *rendered_type = NULL;
        const char *value_type = NULL;

        if (let_type != NULL && let_type->type == AST_EVENT_HANDLER_TYPE)
            ctx->expected_callable_type = let_type;
        rhs = emit_expression_with_ssa_map(let_init, ctx, ssa_map_out);
        ctx->expected_callable_type = saved_expected_callable_type;

        if (let_type != NULL) {
            const char *source_type =
                transpiler_mir_routine_source_local_type_name(
                    mir_routine, let_name);
            if (source_type != NULL && source_type[0] != '\0') {
                value_type = source_type;
            } else {
                rendered_type = transpiler_render_effective_local_type_name(
                    ctx, let_type);
                value_type = rendered_type;
            }
        } else {
            value_type = transpiler_expr_infer_type_name(ctx, let_init);
        }

        if (value_type != NULL && strcmp(value_type, "Unknown") != 0) {
            const char *binding_type_name =
                transpiler_find_local_type_name(ctx, func_decl, let_name);
            if (transpiler_type_name_is_view_like(binding_type_name)
                || transpiler_type_name_is_view_like(value_type)) {
                emit_statement(stmt, ctx);
                free(lhs);
                free(rhs);
                free(rendered_type);
                if (handled_out != NULL)
                    *handled_out = true;
                return true;
            }
            if (is_slot_var(ctx, let_name)
                || (binding_type_name != NULL
                    && (transpiler_type_name_is_slot_like(binding_type_name)
                        || transpiler_type_name_is_claim_shape(binding_type_name)
                        || transpiler_type_name_is_channel(binding_type_name)))
                || (value_type != NULL
                    && (transpiler_type_name_is_slot_like(value_type)
                        || transpiler_type_name_is_claim_shape(value_type)
                        || transpiler_type_name_is_channel(value_type)))) {
                free(lhs);
                free(rhs);
                free(rendered_type);
                if (handled_out != NULL)
                    *handled_out = true;
                return true;
            }
        }

        if (lhs == NULL || rhs == NULL) {
            free(lhs);
            free(rhs);
            free(rendered_type);
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                         "MIR block %zu emission failed: unable to materialize preserved let-binding '%s'",
                         block->id, let_name);
            }
            return false;
        }

        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s = %s;\n", lhs, rhs);
        free(lhs);
        free(rhs);

        if (!transpiler_ssa_name_map_set(ssa_map_out, let_name, versioned_local)) {
            free(rendered_type);
            return false;
        }
        if (value_type != NULL && value_type[0] != '\0')
            register_typed_var(ctx, let_name, value_type);
        free(rendered_type);
    }

    if (handled_out != NULL)
        *handled_out = true;
    return true;
}

TranspilerMIRLocalLetEmitResult
transpiler_emit_mir_source_local_let_def_inst(
    CodeBuf *buf,
    const MIRRoutine *mir_routine,
    const MIRBasicBlock *block,
    const MIRInstruction *inst,
    ASTNode *stmt,
    TranspilerCtx *ctx,
    TranspilerSSANameMap *ssa_map_out,
    char *reason,
    size_t reason_cap)
{
    const char *saved_type_hint;
    const char *let_name;
    ASTNode *let_type;
    ASTNode *let_init;
    char *rendered_type_hint = NULL;
    char *local_type_name_owned = NULL;
    char *lhs = NULL;
    char *rhs = NULL;

    if (!transpiler_mir_def_uses_source_local_decl_emit(inst, stmt)
        || ast_let_name(stmt) == NULL
        || inst->arg0 == NULL
        || inst->result_name == NULL
        || strcmp(inst->arg0, ast_let_name(stmt)) != 0) {
        return TRANSPILE_MIR_LOCAL_LET_NOT_HANDLED;
    }

    saved_type_hint = ctx->active_type_hint;
    let_name = ast_let_name(stmt);
    let_type = ast_let_type(stmt);
    let_init = ast_let_initializer(stmt);

    if (mir_routine != NULL) {
        const char *source_type =
            transpiler_mir_routine_source_local_type_name(
                mir_routine, let_name);
        if (source_type != NULL && source_type[0] != '\0') {
            rendered_type_hint = pergyra_strdup(source_type);
            ctx->active_type_hint = rendered_type_hint;
        }
    }
    if (rendered_type_hint == NULL && let_type != NULL) {
        rendered_type_hint =
            transpiler_render_effective_local_type_name(ctx, let_type);
        ctx->active_type_hint = rendered_type_hint;
    }
    if (rendered_type_hint != NULL) {
        local_type_name_owned = pergyra_strdup(rendered_type_hint);
    } else if (let_init != NULL) {
        const char *inferred = transpiler_expr_infer_type_name(ctx, let_init);
        if (inferred != NULL)
            local_type_name_owned = pergyra_strdup(inferred);
    }
    if (transpiler_type_name_is_slot_like(local_type_name_owned)) {
        if (transpiler_type_name_is_view_like(local_type_name_owned)) {
            emit_statement(stmt, ctx);
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);
            free(local_type_name_owned);
            return TRANSPILE_MIR_LOCAL_LET_HANDLED;
        }
        if (transpiler_block_has_claim_for_slot_local(block, let_name)) {
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);
            free(local_type_name_owned);
            return TRANSPILE_MIR_LOCAL_LET_HANDLED;
        }
        emit_statement(stmt, ctx);
        ctx->active_type_hint = saved_type_hint;
        free(rendered_type_hint);
        free(local_type_name_owned);
        return TRANSPILE_MIR_LOCAL_LET_HANDLED;
    }
    if (local_type_name_owned != NULL
        && transpiler_type_name_is_channel(local_type_name_owned)) {
        emit_statement(stmt, ctx);
        ctx->active_type_hint = saved_type_hint;
        free(rendered_type_hint);
        free(local_type_name_owned);
        return TRANSPILE_MIR_LOCAL_LET_HANDLED;
    }

    if (let_type != NULL) {
        ensure_type_specializations_from_ast_to(ctx, ctx->out, let_type);
    } else if (local_type_name_owned != NULL) {
        ensure_result_specialization_from_type_name_to(ctx, ctx->out,
                                                       local_type_name_owned);
    }

    lhs = transpiler_render_ssa_name(ctx, inst->result_name);
    if (let_init != NULL
        && let_init->type == AST_UNARY
        && ast_unary_operator(let_init).type == TOKEN_QUESTION) {
        ASTNode *operand = ast_unary_operand(let_init);
        const char *result_type = transpiler_expr_infer_type_name(ctx, operand);
        char result_c_type_buf[256];
        const char *result_c_type = NULL;
        char *operand_expr = NULL;
        int try_id;
        bool current_returns_result;

        ctx->active_type_hint = saved_type_hint;
        free(rendered_type_hint);
        if (lhs == NULL || result_type == NULL
            || !transpiler_type_name_is_result(result_type)) {
            free(lhs);
            free(local_type_name_owned);
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "MIR block %llu emission failed: '?' let binding '%s' requires Result<T,E> operand",
                    (unsigned long long) block->id,
                    let_name != NULL ? let_name : "<binding>");
            }
            return TRANSPILE_MIR_LOCAL_LET_FAILED;
        }
        current_returns_result = ctx->current_return_type[0] != '\0'
            && transpiler_type_name_is_result(ctx->current_return_type);

        if (!transpiler_require_type_name_c_type_copy(ctx, result_type,
                "MIR preserved try operand Result", result_c_type_buf,
                sizeof(result_c_type_buf))) {
            free(lhs);
            free(local_type_name_owned);
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "MIR block %llu emission failed: '?' operand type '%s' has no stable C rendering",
                    (unsigned long long) block->id, result_type);
            }
            return TRANSPILE_MIR_LOCAL_LET_FAILED;
        }
        result_c_type = result_c_type_buf;
        operand_expr = emit_expression_with_ssa_map(operand, ctx, ssa_map_out);
        if (operand_expr == NULL) {
            free(lhs);
            free(local_type_name_owned);
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "MIR block %llu emission failed: unable to render '?' operand for '%s'",
                    (unsigned long long) block->id,
                    let_name != NULL ? let_name : "<binding>");
            }
            return TRANSPILE_MIR_LOCAL_LET_FAILED;
        }
        try_id = ctx->tmp_counter++;
        const char *ok_tag =
            pgy_codegen_match_variant_c_result_tag(PGY_MATCH_VARIANT_OK);
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s __try_%d = %s;\n",
                      result_c_type, try_id, operand_expr);
        write_indent_to(buf, ctx->indent);
        if (current_returns_result) {
            codebuf_write(buf,
                          "if (__try_%d.tag != %s) return __try_%d;\n",
                          try_id, ok_tag, try_id);
        } else {
            codebuf_write(buf,
                          "if (__try_%d.tag != %s) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR);\n",
                          try_id, ok_tag);
        }
        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s = __try_%d.ok;\n", lhs, try_id);
        free(operand_expr);
        free(lhs);

        if (!transpiler_ssa_name_map_set(ssa_map_out, let_name,
                                         inst->result_name)) {
            free(local_type_name_owned);
            return TRANSPILE_MIR_LOCAL_LET_FAILED;
        }
        if (local_type_name_owned != NULL)
            register_typed_var(ctx, let_name, local_type_name_owned);
        free(local_type_name_owned);
        return TRANSPILE_MIR_LOCAL_LET_HANDLED;
    }

    {
        const char *saved_expected_type = ctx->expected_type;
        ASTNode *saved_expected_callable_type = ctx->expected_callable_type;
        ctx->expected_type = local_type_name_owned;
        if (let_type != NULL && let_type->type == AST_EVENT_HANDLER_TYPE)
            ctx->expected_callable_type = let_type;
        rhs = emit_expression_with_ssa_map(let_init, ctx, ssa_map_out);
        ctx->expected_callable_type = saved_expected_callable_type;
        ctx->expected_type = saved_expected_type;
    }
    ctx->active_type_hint = saved_type_hint;
    free(rendered_type_hint);

    if (rhs == NULL) {
        free(lhs);
        free(local_type_name_owned);
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                "MIR block %llu emission failed: unable to render initializer for '%s'",
                (unsigned long long) block->id,
                let_name != NULL ? let_name : "<binding>");
        }
        return TRANSPILE_MIR_LOCAL_LET_FAILED;
    }

    write_indent_to(buf, ctx->indent);
    codebuf_write(buf, "%s = %s;\n", lhs, rhs);
    free(lhs);
    free(rhs);

    if (!transpiler_ssa_name_map_set(ssa_map_out, let_name,
                                     inst->result_name)) {
        free(local_type_name_owned);
        return TRANSPILE_MIR_LOCAL_LET_FAILED;
    }
    if (local_type_name_owned != NULL)
        register_typed_var(ctx, let_name, local_type_name_owned);
    free(local_type_name_owned);
    return TRANSPILE_MIR_LOCAL_LET_HANDLED;
}
