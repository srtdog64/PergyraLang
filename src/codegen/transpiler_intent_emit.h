/* C backend intent declaration emission owner.
 * Included inside transpiler.c after base declaration emitters. */

#include "transpiler_intent_prologue_emit.h"
#include "transpiler_intent_cleanup_emit.h"

static void
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
    const char **participant_aliases = NULL;
    const char **participant_types = NULL;
    size_t participant_count = 0;
    const MIRRoutine *mir_routine = NULL;
    bool emit_cleanup_from_mir = false;

#define PGY_MIR_INTENT_CARRIER_FAIL(MSG) \
    do { \
        transpiler_set_backend_error_with_hints(ctx, \
            PGY_CODE_MIR_INTENT_CARRIER_MISSING, \
            PGY_CAUSE_MIR_INTENT_CARRIER_MISSING, \
            PGY_FIX_CHECK_INTENT_STEP_LOWERING, \
            (MSG)); \
        goto intent_emit_fail; \
    } while (0)
#define PGY_BIND_INTENT_STEP_CONTEXT(STEP_ZONE_DECL, STEP_ZONE_ALIAS) \
    do { \
        if ((STEP_ZONE_DECL) != NULL) \
            transpiler_bind_current_host_decl_local(ctx, (STEP_ZONE_DECL)); \
        if ((STEP_ZONE_ALIAS) != NULL) \
            ctx->current_overlay_receiver_expr = (STEP_ZONE_ALIAS); \
    } while (0)
#define PGY_RESTORE_INTENT_STEP_CONTEXT(SAVED_HOST_DECL, SAVED_OVERLAY_RECEIVER) \
    do { \
        transpiler_bind_current_host_decl_local(ctx, (SAVED_HOST_DECL)); \
        ctx->current_overlay_receiver_expr = (SAVED_OVERLAY_RECEIVER); \
    } while (0)

    if (node == NULL || node->type != AST_INTENT_DECL || buf == NULL || ctx == NULL)
        return;

    transpiler_capture_mir_emit_state_local(ctx, &saved_emit_state);
    ctx->out = buf;
    g_type_render_ctx = ctx;
    mir_only_intent = ctx->mir != NULL;
    transpiler_set_current_return_type_local(ctx, "Bool");
    emit_cleanup_from_mir = transpiler_can_emit_intent_cleanup_from_mir(ctx, node, &mir_routine);
    if (mir_routine != NULL) {
        participant_count = transpiler_collect_mir_intent_participants(
            mir_routine, &participant_aliases, &participant_types);
        (void)transpiler_collect_mir_intent_step_names(mir_routine, &mir_step_names);
    }
    if (mir_routine != NULL)
        step_count = transpiler_collect_mir_intent_steps(mir_routine, &mir_steps);
    if (step_count == 0) {
        step_nodes = node->data.intent_decl.steps;
        step_count = node->data.intent_decl.step_count;
    } else {
        step_nodes = mir_steps;
    }
    if (mir_only_intent && node->data.intent_decl.step_count > 0 && mir_routine == NULL) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "MIR-only C path missing routine for intent '%s'",
                node->data.intent_decl.name != NULL
                    ? node->data.intent_decl.name
                    : "(anonymous-intent)");
        }
        free(mir_steps);
        free((void *)participant_aliases);
        free((void *)participant_types);
        free((void *)mir_step_names);
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
        return;
    }
    if (mir_only_intent && node->data.intent_decl.step_count > 0 && step_count == 0) {
        if (ctx->backend_error == NULL) {
            ctx->backend_error = strdup_fmt(
                "MIR-only C path missing intent step sequence for '%s'",
                node->data.intent_decl.name != NULL
                    ? node->data.intent_decl.name
                    : "(anonymous-intent)");
        }
        free(mir_steps);
        free((void *)participant_aliases);
        free((void *)participant_types);
        free((void *)mir_step_names);
        transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
        return;
    }
    if (mir_only_intent && node->data.intent_decl.involve_count > 0) {
        if (participant_count < node->data.intent_decl.involve_count) {
            if (ctx->backend_error == NULL) {
                ctx->backend_error = strdup_fmt(
                    "MIR-only C path missing intent participant metadata for '%s'",
                    node->data.intent_decl.name != NULL
                        ? node->data.intent_decl.name
                        : "(anonymous-intent)");
            }
            free(mir_steps);
            free((void *)participant_aliases);
            free((void *)participant_types);
            free((void *)mir_step_names);
            transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
            return;
        }
        for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
            if (participant_aliases == NULL || participant_types == NULL
                || participant_aliases[i] == NULL || participant_types[i] == NULL) {
                if (ctx->backend_error == NULL) {
                    ctx->backend_error = strdup_fmt(
                        "MIR-only C path has incomplete intent participant metadata for '%s'",
                        node->data.intent_decl.name != NULL
                            ? node->data.intent_decl.name
                            : "(anonymous-intent)");
                }
                free(mir_steps);
                free((void *)participant_aliases);
                free((void *)participant_types);
                free((void *)mir_step_names);
                transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
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
                     step_name != NULL ? step_name : step->data.intent_step.name,
                     "IntentEval", "compensate"))
                || (!mir_only_intent && step->data.intent_step.compensate_expr_count > 0))) {
            has_compensate_steps = true;
            break;
        }
    }
    if (node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_NONE)
        has_compensate_steps = false;
    needs_cleanup_done_label = !emit_cleanup_from_mir && has_compensate_steps
        && node->data.intent_decl.rollback_policy == INTENT_ROLLBACK_CURRENT;

    if (!transpiler_emit_intent_signature_and_entry(
            node, ctx, mir_only_intent, has_compensate_steps, step_count,
            participant_aliases, participant_types, participant_count,
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
        bool rebound_aliases = false;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        if (step_name == NULL)
            step_name = step->data.intent_step.name;
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
                pre_expr = step->data.intent_step.pre_expr;
            if (guard_expr == NULL)
                guard_expr = step->data.intent_step.guard_expr;
            if (post_expr == NULL)
                post_expr = step->data.intent_step.post_expr;
            if (expect_expr == NULL)
                expect_expr = step->data.intent_step.expect_expr;
            if (invariant_pre_expr == NULL)
                invariant_pre_expr = step->data.intent_step.invariant_expr;
            if (invariant_post_expr == NULL)
                invariant_post_expr = step->data.intent_step.invariant_expr;
            if (on_expr_count == 0) {
                on_exprs = step->data.intent_step.on_exprs;
                on_expr_count = step->data.intent_step.on_expr_count;
            }
            if (subintent_expr == NULL)
                subintent_expr = step->data.intent_step.intent_expr;
            if (step_zone_name == NULL
                && step->data.intent_step.where_type != NULL
                && step->data.intent_step.where_type->type == AST_TYPE) {
                step_zone_name = step->data.intent_step.where_type->data.type.name;
            }
            if (step_zone_alias == NULL)
                step_zone_alias = intent_step_effective_zone_alias(step);
            if (step_from_alias == NULL)
                step_from_alias = step->data.intent_step.transfer_from_alias;
            if (step_causes_effect == NULL)
                step_causes_effect = step->data.intent_step.causes_effect;
            if (who_alias_count == 0) {
                who_aliases = (const char **)step->data.intent_step.who_names;
                who_alias_count = step->data.intent_step.who_count;
            }
            if (authorized_alias_count == 0) {
                authorized_aliases = (const char **)step->data.intent_step.authorized_by;
                authorized_alias_count = step->data.intent_step.authorized_by_count;
            }
            if (dispatch_alias_count == 0) {
                dispatch_aliases = (const char **)step->data.intent_step.who_names;
                dispatch_alias_count = step->data.intent_step.who_count;
            }
        }
        if (step_zone_name != NULL) {
            ASTNode *step_zone_decl = find_zone_decl(ctx, step_zone_name);
            PGY_BIND_INTENT_STEP_CONTEXT(step_zone_decl, step_zone_alias);
        } else {
            PGY_BIND_INTENT_STEP_CONTEXT(NULL, step_zone_alias);
        }

        write_indent(ctx);
        codebuf_write(ctx->out, "/* intent step: %s */\n",
            step_name != NULL ? step_name : "<step>");
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_intent_trace_step_export(__intent_handle, \"%s\", \"%s\");\n",
            step_name != NULL ? step_name : "<step>",
            step_zone_name != NULL ? step_zone_name : "<zone>");
        for (size_t j = 0; j < who_alias_count; j++) {
            const char *alias = who_aliases[j];
            const char *slot_name = (mir_only_intent && step_zone_name != NULL)
                ? resolve_intent_zone_slot_name_for_zone(ctx, node, step_zone_name, alias)
                : resolve_intent_zone_slot_name(ctx, node, step, alias);
            write_indent(ctx);
            codebuf_write(ctx->out, "pgy_intent_trace_bind_export(__intent_handle, \"%s\", \"%s\");\n",
                alias != NULL ? alias : "<participant>",
                slot_name != NULL ? slot_name : "<unbound>");
        }
        emit_intent_step_validate_authority(ctx->out, ctx,
            node->data.intent_decl.name, step_name, step_zone_name, step_zone_alias,
            authorized_aliases, authorized_alias_count,
            emit_cleanup_from_mir,
            mir_routine != NULL ? mir_routine->cleanup_block : 0);
        if (mir_only_intent) {
            emit_intent_step_bind_bound_zone_with_metadata(
                ctx->out, ctx, node, step_zone_name, step_zone_alias, step_from_alias,
                who_aliases, who_alias_count,
                participant_aliases, participant_types, participant_count);
            rebound_aliases = emit_intent_step_rebind_bound_zone_aliases_with_metadata(
                ctx->out, ctx, node, step_zone_name, step_zone_alias,
                who_aliases, who_alias_count, i,
                participant_aliases, participant_types, participant_count);
        } else {
            emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
            rebound_aliases = emit_intent_step_rebind_bound_zone_aliases(ctx->out, ctx, node, step, i);
        }

        if (pre_expr != NULL) {
            char *pre = emit_expression(pre_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", pre != NULL ? pre : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"pre:%s\"); __intent_result = false; ",
                step_name != NULL ? step_name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(pre);
        }

        if (invariant_pre_expr != NULL) {
            char *invariant = emit_expression(invariant_pre_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", invariant != NULL ? invariant : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"invariant-pre:%s\"); __intent_result = false; ",
                step_name != NULL ? step_name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(invariant);
        }

        if (on_expr_count > 0) {
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
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", intent_expr != NULL ? intent_expr : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"intent:%s\"); __intent_result = false; ",
                step_name != NULL ? step_name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(intent_expr);
        } else if (on_expr_count == 0) {
            for (size_t j = 0; j < dispatch_alias_count; j++) {
                const char *alias = dispatch_aliases[j];
                const char *subject_name = NULL;
                ASTNode *action_decl = NULL;
                if (mir_only_intent) {
                    subject_name = intent_zone_binding_type_name_with_metadata(
                        node, alias, participant_aliases, participant_types, participant_count);
                } else {
                    ASTNode *involves = find_intent_participant_local(node, alias);
                    if (involves != NULL && involves->data.intent_involves.subject_type != NULL
                        && involves->data.intent_involves.subject_type->type == AST_TYPE) {
                        subject_name = involves->data.intent_involves.subject_type->data.type.name;
                    }
                }
                action_decl = find_subject_action_decl(ctx,
                    subject_name, step_name);
                if (subject_name != NULL && action_decl != NULL
                    && intent_action_has_only_self(action_decl)) {
                    write_indent(ctx);
                    codebuf_write(ctx->out, "%s_%s(%s);\n",
                        subject_name, step_name, alias);
                }
            }
        }
        if (step_causes_effect == NULL) {
            step_causes_effect = infer_intent_step_causes_from_on_exprs(
                ctx, node, on_exprs, on_expr_count,
                participant_aliases, participant_types, participant_count);
        }
        emit_intent_step_mark_caused_effect(ctx->out, ctx,
            step_zone_name, step_zone_alias, step_causes_effect);
        if (rebound_aliases) {
            if (mir_only_intent)
                emit_intent_step_sync_effective_zone_with_metadata(ctx->out, ctx, step_zone_name, step_zone_alias);
            else
                emit_intent_step_sync_effective_zone(ctx->out, ctx, step);
        } else {
            if (mir_only_intent) {
                emit_intent_step_bind_bound_zone_with_metadata(
                    ctx->out, ctx, node, step_zone_name, step_zone_alias, step_from_alias,
                    who_aliases, who_alias_count,
                    participant_aliases, participant_types, participant_count);
            } else {
                emit_intent_step_bind_bound_zone(ctx->out, ctx, node, step);
            }
        }
        if (rebound_aliases) {
            if (mir_only_intent) {
                emit_intent_step_restore_bound_zone_aliases_with_metadata(
                    ctx->out, ctx, node, step_zone_name, who_aliases, who_alias_count, i);
            } else {
                emit_intent_step_restore_bound_zone_aliases(ctx->out, ctx, node, step, i);
            }
        }
        PGY_RESTORE_INTENT_STEP_CONTEXT(saved_host_decl,
                                        saved_overlay_receiver);

        if (has_compensate_steps) {
            write_indent(ctx);
            codebuf_write(ctx->out, "__intent_step_completed[%zu] = true;\n", i);
        }

        if (guard_expr != NULL) {
            char *guard = emit_expression(guard_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", guard != NULL ? guard : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"guard:%s\"); __intent_result = false; ",
                step_name != NULL ? step_name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(guard);
        }

        if (expect_expr != NULL) {
            char *expect = emit_expression(expect_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", expect != NULL ? expect : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"expect:%s\"); __intent_result = false; ",
                step_name != NULL ? step_name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(expect);
        }

        if (post_expr != NULL) {
            char *post = emit_expression(post_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", post != NULL ? post : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"post:%s\"); __intent_result = false; ",
                step_name != NULL ? step_name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(post);
        }

        if (invariant_post_expr != NULL) {
            char *invariant = emit_expression(invariant_post_expr, ctx);
            write_indent(ctx);
            codebuf_write(ctx->out, "if (!(%s)) { ", invariant != NULL ? invariant : "false");
            codebuf_write(ctx->out, "__intent_failed = true; ");
            codebuf_write(ctx->out,
                "pgy_intent_trace_fail_export(__intent_handle, \"invariant-post:%s\"); __intent_result = false; ",
                step_name != NULL ? step_name : "<step>");
            if (emit_cleanup_from_mir) {
                codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu; }\n",
                    node->data.intent_decl.name, mir_routine->cleanup_block);
            } else {
                codebuf_write(ctx->out, "goto __intent_cleanup; }\n");
            }
            free(invariant);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "pgy_intent_trace_step_ok_export(__intent_handle, \"%s\");\n",
            step_name != NULL ? step_name : "<step>");
        free(on_exprs);
        if (mir_only_intent)
            free((void *)who_aliases);
        if (mir_only_intent)
            free((void *)authorized_aliases);
        if (mir_only_intent)
            free((void *)dispatch_aliases);
    }

    write_indent(ctx);
    if (node->data.intent_decl.success_expr != NULL) {
        char *success = emit_expression(node->data.intent_decl.success_expr, ctx);
        codebuf_write(ctx->out, "__intent_result = %s;\n", success != NULL ? success : "true");
        free(success);
    } else {
        codebuf_write(ctx->out, "__intent_result = true;\n");
    }
    write_indent(ctx);
    if (emit_cleanup_from_mir) {
        codebuf_write(ctx->out, "goto _pgy_mir_bb_%s_%zu;\n",
            node->data.intent_decl.name, mir_routine->cleanup_block);
    } else {
        codebuf_write(ctx->out, "goto __intent_cleanup;\n");
    }

    if (!transpiler_emit_intent_cleanup_tail(
            node, ctx, mir_only_intent, emit_cleanup_from_mir,
            has_compensate_steps, needs_cleanup_done_label,
            step_nodes, step_count, mir_step_names, mir_routine,
            participant_aliases, participant_types, participant_count)) {
        goto intent_emit_fail;
    }

    ctx->indent--;
    codebuf_write(ctx->out, "}\n");
    free(mir_steps);
    free((void *)participant_aliases);
    free((void *)participant_types);
    free((void *)mir_step_names);
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
    return;

intent_emit_fail:
#undef PGY_MIR_INTENT_CARRIER_FAIL
#undef PGY_BIND_INTENT_STEP_CONTEXT
#undef PGY_RESTORE_INTENT_STEP_CONTEXT
    free(mir_steps);
    free((void *)participant_aliases);
    free((void *)participant_types);
    free((void *)mir_step_names);
    transpiler_restore_mir_emit_state_from_snapshot_local(ctx, &saved_emit_state);
}
