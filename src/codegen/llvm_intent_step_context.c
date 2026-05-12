/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * LLVM backend intent step metadata/carrier context.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_intent_internal.h"

#include <string.h>

static bool
llvm_intent_step_context_fail(LLVMGenCtx *ctx, const char *message)
{
    llvm_set_mir_intent_carrier_missing(ctx, "%s", message);
    return false;
}

bool
llvm_intent_step_context_load(LLVMGenCtx *ctx,
                              ASTNode *intent,
                              const MIRRoutine *mir_routine,
                              ASTNode *step,
                              const char *step_name,
                              bool mir_only_intent,
                              LLVMIntentStepContext *out)
{
    if (out == NULL)
        return false;
    memset(out, 0, sizeof(*out));
    if (step == NULL || step->type != AST_INTENT_STEP)
        return true;

    if (mir_routine != NULL) {
        out->pre_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "pre");
        out->guard_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "guard");
        out->post_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "post");
        out->expect_expr = llvm_find_mir_intent_check_expr(mir_routine, step_name, "expect");
        out->invariant_pre_expr =
            llvm_find_mir_intent_check_expr(mir_routine, step_name, "invariant-pre");
        out->invariant_post_expr =
            llvm_find_mir_intent_check_expr(mir_routine, step_name, "invariant-post");
        out->on_expr_count = llvm_collect_mir_intent_eval_exprs(
            mir_routine, ctx, step_name, "on", &out->on_exprs);
        out->subintent_expr =
            llvm_find_mir_intent_eval_expr(mir_routine, ctx, step_name, "intent");
        out->zone_type_name =
            llvm_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneWhere");
        out->zone_alias =
            llvm_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneAlias");
        out->from_alias =
            llvm_find_mir_intent_meta_arg(mir_routine, step_name, "IntentZoneFrom");
        out->causes_effect =
            llvm_find_mir_intent_meta_arg(mir_routine, step_name, "IntentCauses");
        out->who_alias_count = llvm_collect_mir_intent_who_aliases(
            mir_routine, ctx, step_name, &out->who_aliases);
        out->authorized_alias_count = llvm_collect_mir_intent_authorized_aliases(
            mir_routine, ctx, step_name, &out->authorized_aliases);
        out->dispatch_alias_count = llvm_collect_mir_intent_dispatch_aliases(
            mir_routine, ctx, step_name, &out->dispatch_aliases);
    }

    if (mir_only_intent) {
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "pre")
            && out->pre_expr == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent pre check carrier");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "guard")
            && out->guard_expr == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent guard check carrier");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "post")
            && out->post_expr == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent post check carrier");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "expect")
            && out->expect_expr == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent expect check carrier");
        if ((llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "invariant-pre")
             || llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentCheck", "invariant-post"))
            && (out->invariant_pre_expr == NULL || out->invariant_post_expr == NULL))
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent invariant check carrier");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentEval", "intent")
            && out->subintent_expr == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent subintent eval carrier");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentEval", "on")
            && out->on_expr_count == 0)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent on-eval carrier");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneWhere", NULL)
            && out->zone_type_name == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent zone where metadata");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneAlias", NULL)
            && out->zone_alias == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent zone alias metadata");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentZoneFrom", NULL)
            && out->from_alias == NULL)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent transfer-from metadata");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentWho", NULL)
            && out->who_alias_count == 0)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent who metadata");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentAuthorizedBy", NULL)
            && out->authorized_alias_count == 0)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent authorized-by metadata");
        if (llvm_mir_intent_has_stmt(mir_routine, step_name, "IntentDispatch", NULL)
            && out->dispatch_alias_count == 0)
            return llvm_intent_step_context_fail(
                ctx, "MIR-only LLVM path missing intent dispatch carrier");
        return true;
    }

    if (out->pre_expr == NULL)
        out->pre_expr = step->data.intent_step.pre_expr;
    if (out->guard_expr == NULL)
        out->guard_expr = step->data.intent_step.guard_expr;
    if (out->post_expr == NULL)
        out->post_expr = step->data.intent_step.post_expr;
    if (out->expect_expr == NULL)
        out->expect_expr = step->data.intent_step.expect_expr;
    if (out->invariant_pre_expr == NULL)
        out->invariant_pre_expr = step->data.intent_step.invariant_expr;
    if (out->invariant_post_expr == NULL)
        out->invariant_post_expr = step->data.intent_step.invariant_expr;
    if (out->subintent_expr == NULL)
        out->subintent_expr = step->data.intent_step.intent_expr;
    if (out->zone_type_name == NULL
        && step->data.intent_step.where_type != NULL
        && step->data.intent_step.where_type->type == AST_TYPE)
        out->zone_type_name = step->data.intent_step.where_type->data.type.name;
    if (out->zone_alias == NULL)
        out->zone_alias = llvm_intent_step_effective_zone_alias(step);
    if (out->from_alias == NULL)
        out->from_alias = step->data.intent_step.transfer_from_alias;
    if (out->causes_effect == NULL)
        out->causes_effect = step->data.intent_step.causes_effect;
    if (out->who_alias_count == 0) {
        out->who_alias_count = step->data.intent_step.who_count;
        out->who_aliases = (const char **)step->data.intent_step.who_names;
    }
    if (out->authorized_alias_count == 0) {
        out->authorized_alias_count = step->data.intent_step.authorized_by_count;
        out->authorized_aliases = (const char **)step->data.intent_step.authorized_by;
    }
    if (out->dispatch_alias_count == 0) {
        out->dispatch_alias_count = step->data.intent_step.who_count;
        out->dispatch_aliases = (const char **)step->data.intent_step.who_names;
    }
    (void)intent;
    return true;
}

#endif
