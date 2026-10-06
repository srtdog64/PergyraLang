/*
 * Intent compensation coverage (docs/173 INT-2).
 *
 * Under `rollback: full` a failed intent runs the compensation of every
 * completed step in reverse order.  When the intent claims that rollback,
 * an effectful step without a `compensate:` clause would stay applied while
 * the intent reports a full rollback, so the step must either compensate or
 * declare, with a reason, that its effect cannot be undone
 * (`irreversible: "...";`).
 *
 * The intent claims full rollback when it writes `rollback: full` or
 * compensates any step under the default policy.  An intent that compensates
 * nothing and leaves the policy implicit has no rollback to be partial
 * (measured 2026-10-06: 55 such intents, 0 partial ones; demanding a marker
 * on each would be the ritual docs/173 R-2 warns about).
 *
 * A step is effectful when it executes something: an `on:` action, a
 * `transfer:`/`move` handoff, or a nested `intent:`.  Rollback reaches the
 * effect only if something can fail after it in the same run: a later step,
 * a non-constant post-action check of the step (guard/expect/post/invariant
 * run after `on:`), or a non-constant intent completion.  The check runs
 * after the step sequence so inherited/derived step facts are final.
 */

#include "type_checker_internal.h"
#include "type_checker_intent_compensation_coverage_internal.h"
#include "diag_codes.h"

static bool
intent_step_is_effectful(ASTNode *step)
{
    return ast_intent_step_on_expr_count(step) > 0
        || ast_intent_step_transfer_from_alias(step) != NULL
        || ast_intent_step_intent_expr(step) != NULL;
}

static bool
intent_check_can_fail(ASTNode *check)
{
    return check != NULL
        && !(check->type == AST_BOOLEAN && ast_boolean_value(check));
}

static bool
intent_step_post_action_check_can_fail(ASTNode *step)
{
    return intent_check_can_fail(ast_intent_step_guard_expr(step))
        || intent_check_can_fail(ast_intent_step_expect_expr(step))
        || intent_check_can_fail(ast_intent_step_post_expr(step))
        || intent_check_can_fail(ast_intent_step_invariant_expr(step));
}

/* A typed intent can fail through its step outcome protocol; a legacy one
 * through a completion predicate other than the reified literal `true`. */
static bool
intent_completion_can_fail(ASTNode *intent)
{
    if (ast_intent_decl_has_typed_result(intent))
        return true;
    return intent_check_can_fail(ast_intent_decl_success_expr(intent));
}

static bool
intent_claims_full_rollback(ASTNode *intent, ASTNode **steps,
                            size_t step_count)
{
    if (ast_intent_decl_rollback_policy(intent) != INTENT_ROLLBACK_FULL)
        return false;
    if (ast_intent_decl_rollback_declared(intent))
        return true;
    for (size_t i = 0; i < step_count; i++) {
        if (steps[i] != NULL && steps[i]->type == AST_INTENT_STEP
            && ast_intent_step_compensate_expr_count(steps[i]) > 0)
            return true;
    }
    return false;
}

/* `irreversible:` is a claim about an effect; it contradicts a compensation
 * and means nothing on a step that changes nothing. */
static bool
intent_step_irreversible_marker_valid(ASTNode *step, const char *step_name,
                                      const char *intent_name,
                                      SemanticContext *ctx)
{
    if (ast_intent_step_irreversible_reason(step) == NULL)
        return true;
    if (ast_intent_step_compensate_expr_count(step) > 0) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP, PGY_FIX_CHECK_INTENT_STEP_LOWERING, step,
            "Intent step '%s' in '%s' declares both 'compensate:' and 'irreversible:'.\n"
            "Reason:\n"
            "- 'irreversible:' says the effect cannot be undone; a compensation says it can\n"
            "Fix:\n"
            "- keep 'compensate:' if the effect can be undone\n"
            "- or remove 'compensate:' and keep 'irreversible:'",
            step_name, intent_name);
        return false;
    }
    if (!intent_step_is_effectful(step)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP, PGY_FIX_CHECK_INTENT_STEP_LOWERING, step,
            "Intent step '%s' in '%s' is marked 'irreversible:' but performs no effect.\n"
            "Reason:\n"
            "- the step has no on:, transfer:/move or intent: clause, so there is nothing to undo\n"
            "Fix:\n"
            "- remove the 'irreversible:' clause",
            step_name, intent_name);
        return false;
    }
    return true;
}

static const char *
intent_step_failure_after_effect(ASTNode *step, bool has_later_step,
                                 bool completion_can_fail)
{
    if (has_later_step)
        return "a later step";
    if (intent_step_post_action_check_can_fail(step))
        return "a guard/expect/post/invariant check of this step";
    if (completion_can_fail)
        return "the intent completion";
    return NULL;
}

void
type_check_intent_compensation_coverage(ASTNode *intent, SemanticContext *ctx)
{
    size_t step_count = 0;
    ASTNode **steps;
    const char *intent_name;
    bool claims_full;
    bool completion_can_fail;

    if (intent == NULL || intent->type != AST_INTENT_DECL || ctx == NULL)
        return;
    intent_name = ast_intent_decl_name(intent) != NULL
        ? ast_intent_decl_name(intent) : "<intent>";
    steps = ast_intent_decl_steps(intent, &step_count);
    claims_full = intent_claims_full_rollback(intent, steps, step_count);
    completion_can_fail = intent_completion_can_fail(intent);
    for (size_t i = 0; i < step_count; i++) {
        ASTNode *step = steps[i];
        const char *step_name;
        const char *failure_after;

        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        step_name = ast_intent_step_name(step) != NULL
            ? ast_intent_step_name(step) : "<step>";
        if (!intent_step_irreversible_marker_valid(step, step_name,
                intent_name, ctx))
            continue;
        if (!claims_full || !intent_step_is_effectful(step)
            || ast_intent_step_compensate_expr_count(step) > 0
            || ast_intent_step_irreversible_reason(step) != NULL)
            continue;
        failure_after = intent_step_failure_after_effect(step,
            i + 1 < step_count, completion_can_fail);
        if (failure_after == NULL)
            continue;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INTENT_STEP_INVALID,
            PGY_CAUSE_INTENT_STEP, PGY_FIX_CHECK_INTENT_STEP_LOWERING, step,
            "Intent step '%s' in '%s' has an effect but no compensation under 'rollback: full'.\n"
            "Reason:\n"
            "- '%s' claims a full rollback: it declares 'rollback: full' or compensates another step\n"
            "- %s can fail after this step's effect, which would then stay applied\n"
            "Fix:\n"
            "- add 'compensate: <undo>;' to the step\n"
            "- or declare 'irreversible: \"<why it cannot be undone>\";'\n"
            "- or choose 'rollback: current' / 'rollback: none' for the intent",
            step_name, intent_name, intent_name, failure_after);
    }
}
