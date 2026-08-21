#include "transpiler_intent_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_block_intent_helpers.h"
#include "transpiler_block_intent_rebind_helpers.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_intent_context.h"
#include "transpiler_intent_cleanup_emit.h"
#include "transpiler_intent_emit_metadata_helpers.h"
#include "transpiler_intent_failure_emit.h"
#include "transpiler_intent_participant.h"
#include "transpiler_intent_prologue_emit.h"
#include "transpiler_intent_step_completion_emit.h"
#include "transpiler_intent_typed_execution.h"
#include "transpiler_intent_zone_binding_emit.h"
#include "transpiler_intent_zone_slot.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_intent_query.h"
#include "transpiler_mir_emit_state.h"
#include "transpiler_symbols.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

void
emit_intent_decl(ASTNode *node, CodeBuf *buf, TranspilerCtx *ctx)
{
    TranspilerMirEmitState saved_emit_state;
    bool has_compensate_steps = false;
    bool needs_cleanup_done_label = false;
    bool mir_only_intent = false;
    ASTNode **mir_steps = NULL;
    ASTNode **step_nodes = NULL;
    const char **mir_step_names = NULL;
    size_t step_count = 0;
    IntentBindingMetadataView binding_metadata = {0};
    size_t mir_binding_count = 0;
    const MIRRoutine *mir_routine = NULL;
    bool emit_cleanup_from_mir = false;
    bool mir_requires_routine = false;
    const char *intent_name = NULL;
    ASTNode **decl_steps = NULL;
    size_t decl_step_count = 0;
    IntentRollbackPolicy rollback_policy = INTENT_ROLLBACK_NONE;
    ASTNode *success_expr = NULL;

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;

    intent_name = ast_intent_decl_name(node);
    decl_steps = ast_intent_decl_steps(node, &decl_step_count);
    rollback_policy = ast_intent_decl_rollback_policy(node);
    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);
    ctx->out = buf;
    transpiler_set_current_return_type_local(ctx, "Bool");
    ctx->current_return_callable_type = NULL;
    emit_cleanup_from_mir =
        transpiler_active_can_emit_intent_cleanup_from_mir(ctx, node);
    mir_requires_routine = transpiler_active_has_mir(ctx) && decl_step_count > 0;
    mir_routine = transpiler_find_mir_intent(ctx, node);
    mir_only_intent = mir_routine != NULL;
    if (mir_routine != NULL) {
        transpiler_set_current_return_type_local(
            ctx, mir_routine_return_type_name(mir_routine));
        if (mir_routine_has_admitted_intent_execution_plan(mir_routine)) {
            mir_binding_count = transpiler_collect_mir_intent_bindings(
                mir_routine, &binding_metadata);
            if (!transpiler_emit_typed_intent_execution(
                    node, ctx, mir_routine, &binding_metadata)) {
                goto intent_emit_fail;
            }
            transpiler_free_intent_emit_metadata(
                NULL, NULL, NULL, NULL, NULL,
                &binding_metadata, NULL);
            transpiler_restore_mir_emit_state_from_snapshot_local(
                ctx, &saved_emit_state);
            return;
        }
        success_expr = transpiler_find_mir_intent_check_expr(
            mir_routine, intent_name, "success");
        if (transpiler_mir_intent_has_stmt(
                mir_routine, intent_name, "IntentCheck", "success")
            && success_expr == NULL) {
            PGY_MIR_INTENT_CARRIER_FAIL(
                "MIR-only C path missing intent success check carrier");
        }
        mir_binding_count = transpiler_collect_mir_intent_bindings(
            mir_routine, &binding_metadata);
        step_count = transpiler_collect_mir_intent_step_names(
            mir_routine, &mir_step_names);
        mir_steps = transpiler_build_mir_intent_step_sources(
            node, mir_step_names, step_count);
    } else {
        success_expr = ast_intent_decl_success_expr(node);
    }
    if (step_count == 0) {
        step_nodes = decl_steps;
        step_count = decl_step_count;
    } else {
        step_nodes = mir_steps;
    }
    if (mir_requires_routine && mir_routine == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing routine for intent '%s'",
            intent_name != NULL ? intent_name : "(anonymous-intent)");
        transpiler_free_intent_emit_metadata(
            mir_steps, NULL, NULL,
            NULL, NULL, &binding_metadata, mir_step_names);
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
        return;
    }
    if (mir_requires_routine && step_count == 0) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing intent step sequence for '%s'",
            intent_name != NULL ? intent_name : "(anonymous-intent)");
        transpiler_free_intent_emit_metadata(
            mir_steps, NULL, NULL,
            NULL, NULL, &binding_metadata, mir_step_names);
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
        return;
    }
    if (mir_requires_routine) {
        for (size_t i = 0; i < step_count; i++) {
            if (mir_steps != NULL && mir_steps[i] != NULL)
                continue;
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing intent step source mapping for '%s'",
                mir_step_names != NULL && mir_step_names[i] != NULL
                    ? mir_step_names[i]
                    : "(anonymous-step)");
            transpiler_free_intent_emit_metadata(
                mir_steps, NULL, NULL,
                NULL, NULL, &binding_metadata, mir_step_names);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
            return;
        }
    }
    if (mir_only_intent && mir_binding_count > 0) {
        for (size_t i = 0; i < mir_binding_count; i++) {
            if (!intent_binding_metadata_view_has_complete_row(
                    &binding_metadata, i)) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path has incomplete ordered intent binding metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous-intent)");
                transpiler_free_intent_emit_metadata(
                    mir_steps, NULL, NULL,
                    NULL, NULL, &binding_metadata, mir_step_names);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                    &saved_emit_state);
                return;
            }
            if (!intent_binding_metadata_kind_is_supported(
                    intent_binding_metadata_view_kind_at(
                        &binding_metadata, i))) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path has invalid ordered intent binding metadata for '%s'",
                    intent_name != NULL ? intent_name : "(anonymous-intent)");
                transpiler_free_intent_emit_metadata(
                    mir_steps, NULL, NULL,
                    NULL, NULL, &binding_metadata, mir_step_names);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx,
                    &saved_emit_state);
                return;
            }
        }
    }
    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
        if (step != NULL && step->type == AST_INTENT_STEP
            && ((mir_only_intent && mir_routine != NULL
                 && transpiler_mir_intent_has_stmt(mir_routine,
                     step_name != NULL ? step_name : ast_intent_step_name(step),
                     "IntentEval", "compensate"))
                || (!mir_only_intent && ast_intent_step_compensate_expr_count(step) > 0))) {
            has_compensate_steps = true;
            break;
        }
    }
    if (rollback_policy == INTENT_ROLLBACK_NONE)
        has_compensate_steps = false;
    needs_cleanup_done_label = !emit_cleanup_from_mir && has_compensate_steps
        && rollback_policy == INTENT_ROLLBACK_CURRENT;

    if (!transpiler_emit_intent_signature_and_entry(
            node, ctx, has_compensate_steps, step_count,
            &binding_metadata,
            emit_cleanup_from_mir, mir_routine)) {
        goto intent_emit_fail;
    }

    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = step_nodes[i];
        ASTNode *saved_host_decl = transpiler_current_host_decl_local(ctx);
        const char *saved_overlay_receiver = ctx->current_overlay_receiver_expr;
        const char *step_name = (mir_step_names != NULL) ? mir_step_names[i] : NULL;
        ASTNode *pre_expr = NULL;
        ASTNode *guard_expr = NULL;
        ASTNode *post_expr = NULL;
        ASTNode *expect_expr = NULL;
        ASTNode *invariant_pre_expr = NULL;
        ASTNode *invariant_post_expr = NULL;
        ASTNode **on_exprs = NULL;
        size_t on_expr_count = 0;
        ASTNode *subintent_expr = NULL;
        const char *step_zone_name = NULL;
        const char *step_zone_alias = NULL;
        const char *step_from_alias = NULL;
        const char *step_causes_effect = NULL;
        const char **who_aliases = NULL;
        size_t who_alias_count = 0;
        const char **authorized_aliases = NULL;
        size_t authorized_alias_count = 0;
        const char **dispatch_aliases = NULL;
        size_t dispatch_alias_count = 0;
        const char *outcome_binding_name = NULL;
        const char *outcome_binding_type_name = NULL;
        char outcome_binding_c_type[256] = {0};
        bool has_outcome_binding = false;
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (step_name == NULL)
            step_name = ast_intent_step_name(step);
        if (mir_routine != NULL) {
            pre_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "pre");
            guard_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "guard");
            post_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "post");
            expect_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "expect");
            invariant_pre_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "invariant-pre");
            invariant_post_expr = transpiler_find_mir_intent_check_expr(mir_routine, step_name, "invariant-post");
            on_expr_count = transpiler_collect_mir_intent_eval_exprs(
                mir_routine, step_name, "on", &on_exprs);
            subintent_expr = transpiler_find_mir_intent_eval_expr(mir_routine, step_name, "intent");
            step_zone_name = transpiler_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneWhere");
            step_zone_alias = transpiler_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneAlias");
            step_from_alias = transpiler_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneFrom");
            step_causes_effect = transpiler_find_mir_intent_meta_arg(mir_routine, step_name, "IntentCauses");
            who_alias_count = transpiler_collect_mir_intent_who_aliases(mir_routine, step_name, &who_aliases);
            authorized_alias_count = transpiler_collect_mir_intent_authorized_aliases(
                mir_routine, step_name, &authorized_aliases);
            dispatch_alias_count = transpiler_collect_mir_intent_dispatch_aliases(mir_routine, step_name, &dispatch_aliases);
        }
        if (mir_only_intent) {
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "pre")
                && pre_expr == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent pre check carrier");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "guard")
                && guard_expr == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent guard check carrier");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "post")
                && post_expr == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent post check carrier");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "expect")
                && expect_expr == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent expect check carrier");
            if ((transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "invariant-pre")
                 || transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "invariant-post"))
                && (invariant_pre_expr == NULL || invariant_post_expr == NULL))
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent invariant check carrier");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentEval", "intent")
                && subintent_expr == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent subintent eval carrier");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentEval", "on")
                && on_expr_count == 0)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent on-eval carrier");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneWhere", NULL)
                && step_zone_name == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent zone where metadata");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneAlias", NULL)
                && step_zone_alias == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent zone alias metadata");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneFrom", NULL)
                && step_from_alias == NULL)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent transfer-from metadata");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentWho", NULL)
                && who_alias_count == 0)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent who metadata");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentAuthorizedBy", NULL)
                && authorized_alias_count == 0)
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent authorized-by metadata");
            if (transpiler_mir_intent_has_stmt(mir_routine, step_name, "IntentDispatch", NULL)
                && dispatch_alias_count == 0) {
                PGY_MIR_INTENT_CARRIER_FAIL("MIR-only C path missing intent dispatch carrier");
            }
        } else {
            if (pre_expr == NULL)
                pre_expr = ast_intent_step_pre_expr(step);
            if (guard_expr == NULL)
                guard_expr = ast_intent_step_guard_expr(step);
            if (post_expr == NULL)
                post_expr = ast_intent_step_post_expr(step);
            if (expect_expr == NULL)
                expect_expr = ast_intent_step_expect_expr(step);
            if (invariant_pre_expr == NULL)
                invariant_pre_expr = ast_intent_step_invariant_expr(step);
            if (invariant_post_expr == NULL)
                invariant_post_expr = ast_intent_step_invariant_expr(step);
            if (on_expr_count == 0) {
                on_exprs = ast_intent_step_on_exprs(step, NULL);
                on_expr_count = ast_intent_step_on_expr_count(step);
            }
            if (subintent_expr == NULL)
                subintent_expr = ast_intent_step_intent_expr(step);
            if (step_zone_name == NULL
                && ast_intent_step_where_type(step) != NULL
                && ast_intent_step_where_type(step)->type == AST_TYPE) {
                step_zone_name = ast_type_name(ast_intent_step_where_type(step));
            }
            if (step_zone_alias == NULL)
                step_zone_alias = intent_step_effective_zone_alias(step);
            if (step_from_alias == NULL)
                step_from_alias = ast_intent_step_transfer_from_alias(step);
            if (step_causes_effect == NULL)
                step_causes_effect = ast_intent_step_causes_effect(step);
            if (who_alias_count == 0) {
                who_aliases = (const char **)ast_intent_step_who_names(step, NULL);
                who_alias_count = ast_intent_step_who_count(step);
            }
            if (authorized_alias_count == 0) {
                authorized_aliases = (const char **)ast_intent_step_authorized_by(step, NULL);
                authorized_alias_count = ast_intent_step_authorized_by_count(step);
            }
            if (dispatch_alias_count == 0) {
                dispatch_aliases = (const char **)ast_intent_step_who_names(step, NULL);
                dispatch_alias_count = ast_intent_step_who_count(step);
            }
        }
        outcome_binding_name = ast_intent_step_outcome_binding_name(step);
        outcome_binding_type_name =
            ast_intent_step_outcome_binding_type_name(step);
        has_outcome_binding = outcome_binding_name != NULL
            || outcome_binding_type_name != NULL;
        if (has_outcome_binding
            && (outcome_binding_name == NULL
                || outcome_binding_name[0] == '\0'
                || outcome_binding_type_name == NULL
                || outcome_binding_type_name[0] == '\0')) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_INTENT_STEP,
                PGY_FIX_CHECK_INTENT_STEP_LOWERING,
                "C intent step '%s' has incomplete outcome binding name/type metadata",
                step_name != NULL ? step_name : "<step>");
            if (mir_only_intent) {
                free(on_exprs);
                free((void *)who_aliases);
                free((void *)authorized_aliases);
                free((void *)dispatch_aliases);
            }
            goto intent_emit_fail;
        }
        if (has_outcome_binding && on_expr_count != 1) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_INTENT_STEP,
                PGY_FIX_CHECK_INTENT_STEP_LOWERING,
                "C intent step '%s' outcome binding '%s' requires exactly one on expression",
                step_name != NULL ? step_name : "<step>",
                outcome_binding_name);
            if (mir_only_intent) {
                free(on_exprs);
                free((void *)who_aliases);
                free((void *)authorized_aliases);
                free((void *)dispatch_aliases);
            }
            goto intent_emit_fail;
        }
        if (has_outcome_binding
            && !transpiler_require_type_name_c_type_copy(
                ctx, outcome_binding_type_name,
                "intent outcome binding", outcome_binding_c_type,
                sizeof(outcome_binding_c_type))) {
            if (mir_only_intent) {
                free(on_exprs);
                free((void *)who_aliases);
                free((void *)authorized_aliases);
                free((void *)dispatch_aliases);
            }
            goto intent_emit_fail;
        }
        if (step_zone_name != NULL) {
            ASTNode *step_zone_decl =
                find_zone_decl_in_program_view(ctx, step_zone_name);
            PGY_BIND_INTENT_STEP_CONTEXT(step_zone_decl, step_zone_alias);
        } else {
            PGY_BIND_INTENT_STEP_CONTEXT(NULL, step_zone_alias);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "/* intent step: %s */\n",
            step_name != NULL ? step_name : "<step>");
        if (ctx->uses_intent_observability) {
            write_indent(ctx);
            codebuf_write(ctx->out, "pgy_intent_trace_step_export(__intent_handle, \"%s\", \"%s\");\n",
                step_name != NULL ? step_name : "<step>",
                step_zone_name != NULL ? step_zone_name : "<zone>");
            for (size_t j = 0; j < who_alias_count; j++) {
                const char *alias = who_aliases[j];
                const char *slot_name = (mir_only_intent && step_zone_name != NULL)
                    ? resolve_intent_zone_slot_name_for_zone_with_bindings(
                        ctx, node, step_zone_name, alias, &binding_metadata)
                    : resolve_intent_zone_slot_name(ctx, node, step, alias);
                write_indent(ctx);
                codebuf_write(ctx->out, "pgy_intent_trace_bind_export(__intent_handle, \"%s\", \"%s\");\n",
                    alias != NULL ? alias : "<participant>",
                    slot_name != NULL ? slot_name : "<unbound>");
            }
        }
        emit_intent_step_validate_authority(ctx->out, ctx,
            intent_name, step_name, step_zone_name, step_zone_alias,
            authorized_aliases, authorized_alias_count,
            emit_cleanup_from_mir,
            mir_routine != NULL ? mir_routine->cleanup_block : 0);
        if (mir_only_intent) {
            emit_intent_step_bind_bound_zone_with_metadata(
                ctx->out, ctx, node, step_zone_name, step_zone_alias, step_from_alias,
                who_aliases, who_alias_count,
                &binding_metadata, true);
            if (mir_only_intent && ctx->backend_error != NULL) {
                free(on_exprs);
                free((void *)who_aliases);
                free((void *)authorized_aliases);
                free((void *)dispatch_aliases);
                goto intent_emit_fail;
            }
            rebound_aliases = emit_intent_step_rebind_bound_zone_aliases_with_metadata(
                ctx->out, ctx, node, step_zone_name, step_zone_alias,
                who_aliases, who_alias_count, i,
                &binding_metadata);
            if (mir_only_intent && ctx->backend_error != NULL) {
                free(on_exprs);
                free((void *)who_aliases);
                free((void *)authorized_aliases);
                free((void *)dispatch_aliases);
                goto intent_emit_fail;
            }
        } else {
            emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step, true);
            rebound_aliases = emit_intent_step_rebind_bound_zone_aliases(ctx->out, ctx, node, step, i);
        }

        if (pre_expr != NULL) {
            char *pre = emit_expression(pre_expr, ctx);
            emit_intent_step_condition_failure(ctx->out, ctx, pre, "pre",
                step_name, intent_name, emit_cleanup_from_mir,
                mir_routine != NULL ? mir_routine->cleanup_block : 0);
            free(pre);
        }

        if (invariant_pre_expr != NULL) {
            char *invariant = emit_expression(invariant_pre_expr, ctx);
            emit_intent_step_condition_failure(ctx->out, ctx, invariant,
                "invariant-pre", step_name, intent_name,
                emit_cleanup_from_mir,
                mir_routine != NULL ? mir_routine->cleanup_block : 0);
            free(invariant);
        }

        if (has_outcome_binding) {
            char *on_expr = emit_expression(on_exprs[0], ctx);
            if (on_expr == NULL || on_expr[0] == '\0'
                || ctx->backend_error != NULL) {
                if (ctx->backend_error == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_INTENT_STEP,
                        PGY_FIX_CHECK_INTENT_STEP_LOWERING,
                        "C intent step '%s' cannot lower bound on expression for '%s'",
                        step_name != NULL ? step_name : "<step>",
                        outcome_binding_name);
                }
                free(on_expr);
                PGY_RESTORE_INTENT_STEP_CONTEXT(
                    saved_host_decl, saved_overlay_receiver);
                if (mir_only_intent) {
                    free(on_exprs);
                    free((void *)who_aliases);
                    free((void *)authorized_aliases);
                    free((void *)dispatch_aliases);
                }
                goto intent_emit_fail;
            }
            write_indent(ctx);
            codebuf_write(ctx->out, "%s const %s = %s;\n",
                outcome_binding_c_type, outcome_binding_name, on_expr);
            free(on_expr);
            /* Keep the step-local type fact alive through cleanup emission so
             * expect/post and the compensation tail lower the same binding. */
            register_typed_var(
                ctx, outcome_binding_name, outcome_binding_type_name);
            if (ctx->backend_error != NULL) {
                PGY_RESTORE_INTENT_STEP_CONTEXT(
                    saved_host_decl, saved_overlay_receiver);
                if (mir_only_intent) {
                    free(on_exprs);
                    free((void *)who_aliases);
                    free((void *)authorized_aliases);
                    free((void *)dispatch_aliases);
                }
                goto intent_emit_fail;
            }
        } else if (on_expr_count > 0) {
            for (size_t j = 0; j < on_expr_count; j++) {
                char *on_expr = emit_expression(on_exprs[j], ctx);
                if (on_expr != NULL && on_expr[0] != '\0') {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "%s;\n", on_expr);
                }
                free(on_expr);
            }
        }
        if (subintent_expr != NULL) {
            char *intent_expr = emit_expression(subintent_expr, ctx);
            emit_intent_step_condition_failure(ctx->out, ctx, intent_expr,
                "intent", step_name, intent_name,
                emit_cleanup_from_mir,
                mir_routine != NULL ? mir_routine->cleanup_block : 0);
            free(intent_expr);
        } else if (on_expr_count == 0) {
            for (size_t j = 0; j < dispatch_alias_count; j++) {
                const char *alias = dispatch_aliases[j];
                const char *subject_name = NULL;
                const MIRDeclMethod *action_meta = NULL;
                if (mir_only_intent) {
                    subject_name = intent_zone_binding_type_name_with_bindings(
                        node, alias, &binding_metadata);
                    if (subject_name == NULL) {
                        transpiler_set_mir_inventory_missing(ctx,
                            "MIR-only C path missing intent dispatch participant metadata for '%s'",
                            alias != NULL ? alias : "(anonymous-participant)");
                        free(on_exprs);
                        free((void *)who_aliases);
                        free((void *)authorized_aliases);
                        free((void *)dispatch_aliases);
                        goto intent_emit_fail;
                    }
                } else {
                    ASTNode *involves = find_intent_participant_local(node, alias);
                    ASTNode *subject_type = NULL;
                    if (involves != NULL && involves->type == AST_INTENT_INVOLVES)
                        subject_type = ast_intent_involves_subject_type(involves);
                    if (subject_type != NULL && subject_type->type == AST_TYPE) {
                        subject_name = ast_type_name(subject_type);
                    }
                }
                action_meta = find_subject_action_metadata(ctx,
                    subject_name, step_name);
                if (subject_name != NULL && action_meta != NULL
                    && intent_action_metadata_has_only_self(action_meta)) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "%s_%s(%s);\n",
                        subject_name, step_name, alias);
                }
            }
        }
        emit_intent_step_mark_caused_effect(ctx->out, ctx,
            step_zone_name, step_zone_alias, step_causes_effect);
        if (rebound_aliases) {
            if (mir_only_intent)
                emit_intent_step_sync_effective_zone_with_metadata(ctx->out, ctx, step_zone_name, step_zone_alias);
            else
                emit_intent_step_sync_effective_zone(ctx->out, ctx, step);
            if (mir_only_intent && ctx->backend_error != NULL) {
                free(on_exprs);
                if (mir_only_intent)
                    free((void *)who_aliases);
                if (mir_only_intent)
                    free((void *)authorized_aliases);
                if (mir_only_intent)
                    free((void *)dispatch_aliases);
                goto intent_emit_fail;
            }
        } else {
            if (mir_only_intent) {
                emit_intent_step_bind_bound_zone_with_metadata(
                    ctx->out, ctx, node, step_zone_name, step_zone_alias, step_from_alias,
                    who_aliases, who_alias_count,
                    &binding_metadata, true);
                if (ctx->backend_error != NULL) {
                    free(on_exprs);
                    free((void *)who_aliases);
                    free((void *)authorized_aliases);
                    free((void *)dispatch_aliases);
                    goto intent_emit_fail;
                }
            } else {
                emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step, true);
            }
        }
        if (rebound_aliases) {
            if (mir_only_intent) {
                emit_intent_step_restore_bound_zone_aliases_with_metadata(
                    ctx->out, ctx, node, step_zone_name,
                    who_aliases, who_alias_count, i,
                    &binding_metadata);
                if (ctx->backend_error != NULL) {
                    free(on_exprs);
                    free((void *)who_aliases);
                    free((void *)authorized_aliases);
                    free((void *)dispatch_aliases);
                    goto intent_emit_fail;
                }
            } else {
                emit_intent_step_restore_bound_zone_aliases(ctx->out, ctx, node, step, i);
            }
        }
        PGY_RESTORE_INTENT_STEP_CONTEXT(saved_host_decl,
                                        saved_overlay_receiver);

        transpiler_emit_intent_step_completion(
            ctx, i, has_compensate_steps,
            guard_expr, expect_expr, post_expr, invariant_post_expr,
            step_name, intent_name, emit_cleanup_from_mir,
            mir_routine != NULL ? mir_routine->cleanup_block : 0);
        free(on_exprs);
        if (mir_only_intent)
            free((void *)who_aliases);
        if (mir_only_intent)
            free((void *)authorized_aliases);
        if (mir_only_intent)
            free((void *)dispatch_aliases);
    }

    write_indent(ctx);
    if (success_expr != NULL) {
        char *success = emit_expression(success_expr, ctx);
        if (success == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_MATCH_PREDICATE_SIGNATURE_IN_HOST,
                "C intent '%s' cannot lower its success predicate; silent true fallback is disabled",
                intent_name != NULL ? intent_name : "<intent>");
            goto intent_emit_fail;
        }
        codebuf_write(ctx->out, "__intent_result = %s;\n", success);
        free(success);
    } else {
        codebuf_write(ctx->out, "__intent_result = true;\n");
    }
    write_indent(ctx);
    if (emit_cleanup_from_mir) {
        codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
            intent_name, mir_routine->cleanup_block);
    } else {
        codebuf_write(ctx->out, "goto __intent_cleanup;\n");
    }

    if (!transpiler_emit_intent_cleanup_tail(
            node, ctx, mir_only_intent, emit_cleanup_from_mir,
            has_compensate_steps, needs_cleanup_done_label,
            step_nodes, step_count, mir_step_names, mir_routine,
            &binding_metadata)) {
        goto intent_emit_fail;
    }

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    transpiler_free_intent_emit_metadata(
        mir_steps, NULL, NULL,
        NULL, NULL, &binding_metadata, mir_step_names);
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
    return;

intent_emit_fail:
#undef PGY_MIR_INTENT_CARRIER_FAIL
#undef PGY_BIND_INTENT_STEP_CONTEXT
#undef PGY_RESTORE_INTENT_STEP_CONTEXT
    transpiler_free_intent_emit_metadata(
        mir_steps, NULL, NULL,
        NULL, NULL, &binding_metadata, mir_step_names);
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}
