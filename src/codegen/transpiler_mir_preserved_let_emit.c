#include "transpiler_mir_preserved_let_emit.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "codegen_match_variant_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_format.h"
#include "transpiler_inventory_view.h"
#include "transpiler_let_box_emit.h"
#include "transpiler_let_channel_emit.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_mir_block_emit_helpers.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_ssa_names.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_specialization_registry.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_require.h"

static size_t
transpiler_mir_preserved_source_local_def_count(const MIRRoutine *routine,
                                                const char *base_name)
{
    size_t count = 0;

    if (routine == NULL || base_name == NULL)
        return 0;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t i = 0; i < block->source_local_def_count; i++) {
            const char *name = block->source_local_defs[i];
            if (name != NULL && strcmp(name, base_name) == 0)
                count++;
        }
    }
    return count;
}

TranspilerMIRLocalLetEmitResult
transpiler_emit_mir_source_local_let_def_inst(
    CodeBuf *buf,
    const MIRRoutine *mir_routine,
    const MIRBasicBlock *block,
    const MIRInstruction *inst,
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

    if (!mir_instruction_uses_source_local_decl_emit(inst)
        || inst->arg0 == NULL
        || inst->result_name == NULL
        || inst->expr0 == NULL) {
        return TRANSPILE_MIR_LOCAL_LET_NOT_HANDLED;
    }

    saved_type_hint = ctx->active_type_hint;
    let_name = inst->arg0;
    let_type = inst->expr1;
    let_init = inst->expr0;

    if (inst->abi_type_name != NULL && inst->abi_type_name[0] != '\0') {
        rendered_type_hint =
            transpiler_canonical_effective_local_type_name(
                ctx, inst->abi_type_name);
        ctx->active_type_hint = rendered_type_hint;
    } else if (mir_routine != NULL
               && transpiler_mir_preserved_source_local_def_count(
                   mir_routine, let_name) == 1) {
        const char *source_type =
            transpiler_mir_routine_source_local_type_name(
                mir_routine, let_name);
        if (source_type != NULL && source_type[0] != '\0') {
            rendered_type_hint =
                transpiler_canonical_effective_local_type_name(
                    ctx, source_type);
            ctx->active_type_hint = rendered_type_hint;
        }
    } else if (let_type != NULL && let_type->type == AST_TYPE) {
        rendered_type_hint =
            transpiler_render_effective_local_type_name(ctx, let_type);
        ctx->active_type_hint = rendered_type_hint;
    } else if (mir_routine != NULL) {
        const char *source_type =
            transpiler_mir_routine_source_local_type_name(
                mir_routine, let_name);
        if (source_type != NULL && source_type[0] != '\0') {
            rendered_type_hint =
                transpiler_canonical_effective_local_type_name(
                    ctx, source_type);
            ctx->active_type_hint = rendered_type_hint;
        }
    }
    if (rendered_type_hint == NULL && let_type != NULL) {
        rendered_type_hint =
            transpiler_render_effective_local_type_name(ctx, let_type);
        ctx->active_type_hint = rendered_type_hint;
    }
    if (rendered_type_hint != NULL) {
        local_type_name_owned =
            transpiler_canonical_effective_local_type_name(
                ctx, rendered_type_hint);
    } else if (let_init != NULL) {
        const char *inferred = transpiler_expr_infer_type_name(ctx, let_init);
        if (inferred != NULL)
            local_type_name_owned = pergyra_strdup(inferred);
    }
    if (transpiler_type_name_is_slot_like(local_type_name_owned)) {
        if (transpiler_type_name_is_view_like(local_type_name_owned)) {
            char *ann_type_name = local_type_name_owned;
            local_type_name_owned = NULL;
            if (block->is_pin_region
                && block->pin_view_name != NULL
                && block->pin_source_name != NULL
                && strcmp(block->pin_view_name, let_name) == 0) {
                if (!transpiler_ssa_name_map_set(ssa_map_out, let_name,
                                                 block->pin_source_name)) {
                    ctx->active_type_hint = saved_type_hint;
                    free(rendered_type_hint);
                    free(ann_type_name);
                    return TRANSPILE_MIR_LOCAL_LET_FAILED;
                }
                register_view_like_var(
                    ctx, let_name, ann_type_name, block->pin_source_name,
                    lookup_slot_is_secure(ctx, block->pin_source_name), false);
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(ann_type_name);
                return ctx->backend_error != NULL
                    ? TRANSPILE_MIR_LOCAL_LET_FAILED
                    : TRANSPILE_MIR_LOCAL_LET_HANDLED;
            }
            if (transpiler_try_emit_let_slot_view_or_move(
                    ctx, let_name, let_init, let_type, &ann_type_name)) {
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(ann_type_name);
                return ctx->backend_error != NULL
                    ? TRANSPILE_MIR_LOCAL_LET_FAILED
                    : TRANSPILE_MIR_LOCAL_LET_HANDLED;
            }
            local_type_name_owned = ann_type_name;
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);
            free(local_type_name_owned);
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "MIR block %llu emission failed: source-local view let '%s' requires MIR-owned local-decl emit facts",
                    (unsigned long long) block->id,
                    let_name != NULL ? let_name : "<binding>");
            }
            return TRANSPILE_MIR_LOCAL_LET_FAILED;
        }
        if (transpiler_block_has_claim_for_slot_local(block, let_name)) {
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);
            free(local_type_name_owned);
            return TRANSPILE_MIR_LOCAL_LET_HANDLED;
        }
        {
            char *ann_type_name = local_type_name_owned;
            local_type_name_owned = NULL;
            if (transpiler_try_emit_let_slot_claim(
                    NULL, ctx, let_name, let_init, let_type, &ann_type_name)
                || transpiler_try_emit_let_slot_sugar(
                    ctx, let_name, let_init, let_type, &ann_type_name)) {
                ctx->active_type_hint = saved_type_hint;
                free(rendered_type_hint);
                free(ann_type_name);
                return ctx->backend_error != NULL
                    ? TRANSPILE_MIR_LOCAL_LET_FAILED
                    : TRANSPILE_MIR_LOCAL_LET_HANDLED;
            }
            local_type_name_owned = ann_type_name;
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);
            free(local_type_name_owned);
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                    "MIR block %llu emission failed: source-local slot let '%s' requires MIR-owned local-decl emit facts",
                    (unsigned long long) block->id,
                    let_name != NULL ? let_name : "<binding>");
            }
        }
        return TRANSPILE_MIR_LOCAL_LET_FAILED;
    }
    if (local_type_name_owned != NULL
        && transpiler_type_name_is_channel(local_type_name_owned)) {
        char *ann_type_name = local_type_name_owned;
        local_type_name_owned = NULL;
        if (transpiler_try_emit_channel_let(
                ctx, let_name, let_init, &ann_type_name)) {
            ctx->active_type_hint = saved_type_hint;
            free(rendered_type_hint);
            free(ann_type_name);
            return ctx->backend_error != NULL
                ? TRANSPILE_MIR_LOCAL_LET_FAILED
                : TRANSPILE_MIR_LOCAL_LET_HANDLED;
        }
        local_type_name_owned = ann_type_name;
        ctx->active_type_hint = saved_type_hint;
        free(rendered_type_hint);
        free(local_type_name_owned);
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                "MIR block %llu emission failed: source-local channel let '%s' requires MIR-owned local-decl emit facts",
                (unsigned long long) block->id,
                let_name != NULL ? let_name : "<binding>");
        }
        return TRANSPILE_MIR_LOCAL_LET_FAILED;
    }

    if (local_type_name_owned != NULL) {
        ensure_type_specializations_from_type_name_to(ctx, ctx->out,
                                                      local_type_name_owned);
    } else if (let_type != NULL) {
        ensure_type_specializations_from_ast_to(ctx, ctx->out, let_type);
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
        TranspilerBoxArrayLetCtor box_array_ctor;
        bool box_array_handled;
        ctx->expected_type = local_type_name_owned;
        if (let_type != NULL && let_type->type == AST_EVENT_HANDLER_TYPE)
            ctx->expected_callable_type = let_type;
        box_array_handled = transpiler_try_render_box_array_let_ctor(
            ctx, let_name, let_init, let_type, local_type_name_owned,
            &box_array_ctor);
        if (box_array_handled) {
            rhs = box_array_ctor.rhs;
            box_array_ctor.rhs = NULL;
        } else {
            rhs = emit_expression_with_ssa_map(let_init, ctx, ssa_map_out);
        }
        if (local_type_name_owned == NULL && box_array_ctor.surface_type != NULL)
            local_type_name_owned = pergyra_strdup(box_array_ctor.surface_type);
        transpiler_box_array_let_ctor_destroy(&box_array_ctor);
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

    /* Captured-lambda local: the initializer lowered to a closure literal, so
     * declare the variable with the closure struct type (keyed off the lambda's
     * stable_id, matching transpiler_emit_captured_lambda) and register that
     * type so the call site dispatches structurally (docs/135 Stage A). */
    if (let_init != NULL && let_init->type == AST_LAMBDA_EXPR
        && ast_lambda_capture_count(let_init) > 0) {
        char *clo_type = strdup_fmt("pgy_lambda_clo_%u",
            (unsigned) let_init->stable_id);
        bool ok = clo_type != NULL
            && transpiler_ssa_name_map_set(ssa_map_out, let_name,
                   inst->result_name);
        if (ok) {
            write_indent_to(buf, ctx->indent);
            codebuf_write(buf, "%s %s = %s;\n", clo_type, lhs, rhs);
            register_typed_var(ctx, let_name, clo_type);
        }
        free(clo_type);
        free(lhs);
        free(rhs);
        free(local_type_name_owned);
        return ok ? TRANSPILE_MIR_LOCAL_LET_HANDLED
                  : TRANSPILE_MIR_LOCAL_LET_FAILED;
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
