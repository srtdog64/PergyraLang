#include "type_checker_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "type_checker_flow_internal.h"
#include "type_checker_flow_effects.h"

#include <string.h>

static bool
flow_expr_is_static_literal(const ASTNode *node)
{
    return node != NULL
        && (node->type == AST_NUMBER
            || node->type == AST_STRING
            || node->type == AST_BOOLEAN);
}

static bool
flow_static_literals_equal(const ASTNode *subject,
                           const ASTNode *pattern,
                           bool *equal_out)
{
    if (!flow_expr_is_static_literal(subject)
        || !flow_expr_is_static_literal(pattern)
        || subject->type != pattern->type) {
        return false;
    }

    bool equal = false;
    switch (subject->type) {
    case AST_NUMBER:
        equal = ast_number_value(subject) == ast_number_value(pattern);
        break;
    case AST_STRING: {
        const char *subject_value = ast_string_value(subject);
        const char *pattern_value = ast_string_value(pattern);
        equal = subject_value != NULL && pattern_value != NULL
            && strcmp(subject_value, pattern_value) == 0;
        break;
    }
    case AST_BOOLEAN:
        equal = ast_boolean_value(subject) == ast_boolean_value(pattern);
        break;
    default:
        return false;
    }

    if (equal_out != NULL)
        *equal_out = equal;
    return true;
}

/* Return true only when reachability is proven. Unknown pattern or guard
 * shapes remain conservatively reachable and are not allowed to suppress a
 * later case/default path. */
static bool
flow_match_case_static_reachability(const ASTNode *subject,
                                    const ASTNode *match_case,
                                    bool *reachable_out)
{
    ASTNode *guard = ast_match_case_guard(match_case);
    bool guard_value = true;
    if (guard != NULL
        && flow_static_bool_value(guard, &guard_value)
        && !guard_value) {
        if (reachable_out != NULL)
            *reachable_out = false;
        return true;
    }

    size_t pattern_count = ast_match_case_pattern_count(match_case);
    bool saw_unknown_pattern = false;
    bool matched = false;
    if (pattern_count == 0 && ast_match_case_pattern(match_case) != NULL)
        pattern_count = 1;

    for (size_t i = 0; i < pattern_count; i++) {
        ASTNode *pattern = ast_match_case_pattern_count(match_case) > 0
            ? ast_match_case_pattern_at(match_case, i)
            : ast_match_case_pattern(match_case);
        bool equal = false;
        if (!flow_static_literals_equal(subject, pattern, &equal)) {
            saw_unknown_pattern = true;
        } else if (equal) {
            matched = true;
            break;
        }
    }

    if (!matched && saw_unknown_pattern)
        return false;
    if (!matched) {
        if (reachable_out != NULL)
            *reachable_out = false;
        return true;
    }
    if (guard != NULL
        && !flow_static_bool_value(guard, &guard_value)) {
        return false;
    }

    if (reachable_out != NULL)
        *reachable_out = guard_value;
    return true;
}

static bool
flow_match_subject_is_beta_supported(const Type *type)
{
    if (type == NULL || type == TYPE_UNKNOWN)
        return true;
    if (type_equals(type, TYPE_INT)
        || type_equals(type, TYPE_LONG)
        || type_equals(type, TYPE_BOOL))
        return true;
    if (type->kind == TYPE_KIND_ENUM)
        return true;
    if (type_is_constructed_named(type, "Option")
        || type_is_constructed_named(type, "Result"))
        return true;
    return false;
}

FlowFlags
type_check_if_stmt_flow(ASTNode *node, SemanticContext *ctx,
                        LoopFlowState *loop_flow)
{
    Type *cond = flow_normalize_type(
        type_check_expression(ast_if_condition(node), ctx));
    uint32_t effect_base = ctx->current_function_effects;
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    bool branch_has_defer = false;
    FlowFlags flags = FLOW_NONE;
    FlowFlags then_flags = FLOW_NONE;
    uint32_t then_effect_delta = EFFECT_NONE;
    uint32_t else_effect_delta = EFFECT_NONE;
    bool condition_value = false;
    bool condition_known = flow_static_bool_value(
        ast_if_condition(node), &condition_value);
    bool then_reachable = !condition_known || condition_value;
    bool else_reachable = !condition_known || !condition_value;

    if (!base.valid) {
        semantic_error(ctx, node,
            "Resource snapshot allocation failed before if/else analysis");
        destroy_resource_snapshot(&base);
        return FLOW_NONE;
    }

    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_CONDITION_NON_BOOL, PGY_FIX_CONVERT_CONDITION_TO_BOOL,
            node,
            "If condition must be Bool, got '%s'", cond->name);
    }

    restore_resource_states(&base);
    ctx->current_function_effects = effect_base;
    if (!then_reachable)
        ctx->future_lifecycle_unreachable_depth++;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    then_flags = type_check_block_flow(ast_if_then_branch(node), ctx, loop_flow);
    semantic_future_require_scope_retired(
        ctx->scope, ast_if_then_branch(node), ctx, "if branch exit");
    scope_exit(&ctx->scope);
    if (!then_reachable)
        ctx->future_lifecycle_unreachable_depth--;
    then_effect_delta = effect_delta_from_baseline(effect_base,
        ctx->current_function_effects);
    if ((then_flags & FLOW_HAS_DEFER) != 0)
        branch_has_defer = true;
    if (then_reachable)
        flags |= flow_terminating_flags(then_flags);
    if (then_reachable && flow_has_fallthrough(then_flags)) {
        ResourceConsumeSnapshot then_snap = snapshot_resource_states(ctx);
        if (!then_snap.valid) {
            semantic_error(ctx, ast_if_then_branch(node) != NULL
                ? ast_if_then_branch(node)
                : node,
                "Resource snapshot allocation failed while checking if branch");
        }
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &then_snap);
        destroy_resource_snapshot(&then_snap);
        flags |= FLOW_FALLTHROUGH;
    }

    if (ast_if_else_branch(node) != NULL) {
        FlowFlags else_flags = FLOW_NONE;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        if (!else_reachable)
            ctx->future_lifecycle_unreachable_depth++;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        else_flags =
            type_check_statement_flow(ast_if_else_branch(node), ctx, loop_flow);
        semantic_future_require_scope_retired(
            ctx->scope, ast_if_else_branch(node), ctx, "else branch exit");
        scope_exit(&ctx->scope);
        if (!else_reachable)
            ctx->future_lifecycle_unreachable_depth--;
        else_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        if ((else_flags & FLOW_HAS_DEFER) != 0)
            branch_has_defer = true;
        if (else_reachable)
            flags |= flow_terminating_flags(else_flags);
        if (else_reachable && flow_has_fallthrough(else_flags)) {
            ResourceConsumeSnapshot else_snap = snapshot_resource_states(ctx);
            if (!else_snap.valid) {
                semantic_error(ctx, ast_if_else_branch(node),
                    "Resource snapshot allocation failed while checking else branch");
            }
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &else_snap);
            destroy_resource_snapshot(&else_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else if (else_reachable) {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
        else_effect_delta = EFFECT_NONE;
    }

    if (!flow_condition_is_static_bool(ast_if_condition(node))
        && branch_has_defer) {
        flow_reject_dynamic_defer_control(ctx, node, "if");
    }

    if (then_reachable && else_reachable) {
        flow_record_branch_effect_conflict_labeled(ctx, node,
            then_effect_delta, "then branch",
            else_effect_delta,
            ast_if_else_branch(node) != NULL
                ? "else branch"
                : "implicit fallthrough path");
    }
    ctx->current_function_effects = type_effect_mask_join(effect_base,
        type_effect_mask_join(
            then_reachable ? then_effect_delta : EFFECT_NONE,
            else_reachable ? else_effect_delta : EFFECT_NONE));

    if (has_fallthrough && !fallthrough.valid) {
        semantic_error(ctx, node,
            "Resource snapshot merge failed while joining if/else branches");
    }

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}

FlowFlags
type_check_match_stmt_flow(ASTNode *node, SemanticContext *ctx,
                           LoopFlowState *loop_flow)
{
    ASTNode *subject = ast_match_subject(node);
    Type *subj_type = type_check_expression(subject, ctx);
    uint32_t effect_base = ctx->current_function_effects;
    uint32_t merged_effect_delta = EFFECT_NONE;
    uint32_t previous_case_delta = EFFECT_NONE;
    bool have_previous_case_delta = false;
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    bool match_has_defer = false;
    bool prior_static_match = false;
    FlowFlags flags = FLOW_NONE;

    if (!base.valid) {
        semantic_error(ctx, node,
            "Resource snapshot allocation failed before match analysis");
        destroy_resource_snapshot(&base);
        return FLOW_NONE;
    }

    if (!flow_match_subject_is_beta_supported(subj_type)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            subject,
            "Match subject type '%s' is not beta-stable; supported subjects are Int, Long, Bool, enum, Option<T>, and Result<T>",
            subj_type != NULL && subj_type->name != NULL
                ? subj_type->name
                : "<unknown>");
    }

    for (size_t i = 0; i < ast_match_case_count(node); i++) {
        ASTNode *mc = ast_match_case_at(node, i);
        uint32_t case_effect_delta = EFFECT_NONE;
        bool case_reachable = true;
        bool case_reachability_known =
            flow_match_case_static_reachability(subject, mc,
                                                &case_reachable);
        if (prior_static_match)
            case_reachable = false;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        if (!case_reachable)
            ctx->future_lifecycle_unreachable_depth++;
        scope_enter(&ctx->scope, SCOPE_BLOCK);

        if (ast_match_case_pattern(mc) != NULL)
            type_check_match_case_patterns(mc, subj_type, ctx);

        if (ast_match_case_guard(mc) != NULL) {
            Type *guard_type = flow_normalize_type(
                type_check_expression(ast_match_case_guard(mc), ctx));
            if (!type_equals(guard_type, TYPE_BOOL)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_CONDITION_NON_BOOL,
                    PGY_FIX_CONVERT_CONDITION_TO_BOOL,
                    ast_match_case_guard(mc),
                    "Case guard must be Bool, got '%s'", guard_type->name);
            }
        }

        FlowFlags case_flags =
            type_check_block_flow(ast_match_case_body(mc), ctx, loop_flow);
        semantic_future_require_scope_retired(
            ctx->scope, mc, ctx, "match case exit");
        scope_exit(&ctx->scope);
        if (!case_reachable)
            ctx->future_lifecycle_unreachable_depth--;
        if ((case_flags & FLOW_HAS_DEFER) != 0)
            match_has_defer = true;
        case_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        if (case_reachable && merged_effect_delta != EFFECT_NONE) {
            flow_record_branch_effect_conflict_labeled(ctx, mc,
                merged_effect_delta, "merged prior cases",
                case_effect_delta, "current case");
        } else if (case_reachable && have_previous_case_delta) {
            flow_record_branch_effect_conflict_labeled(ctx, mc,
                previous_case_delta, "previous case",
                case_effect_delta, "current case");
        }
        if (case_reachable) {
            merged_effect_delta =
                type_effect_mask_join(merged_effect_delta, case_effect_delta);
            previous_case_delta = case_effect_delta;
            have_previous_case_delta = true;
            flags |= flow_terminating_flags(case_flags);
        }
        if (case_reachable && flow_has_fallthrough(case_flags)) {
            ResourceConsumeSnapshot case_snap = snapshot_resource_states(ctx);
            if (!case_snap.valid) {
                semantic_error(ctx, mc,
                    "Resource snapshot allocation failed while checking match case");
            }
            merge_resource_snapshots_or(&fallthrough,
                                        &has_fallthrough,
                                        &case_snap);
            destroy_resource_snapshot(&case_snap);
            flags |= FLOW_FALLTHROUGH;
        }
        if (!prior_static_match && case_reachability_known && case_reachable)
            prior_static_match = true;
    }

    if (ast_match_default_body(node) != NULL) {
        FlowFlags default_flags = FLOW_NONE;
        uint32_t default_effect_delta = EFFECT_NONE;
        bool default_reachable = !prior_static_match;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        if (!default_reachable)
            ctx->future_lifecycle_unreachable_depth++;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        default_flags =
            type_check_block_flow(ast_match_default_body(node), ctx, loop_flow);
        semantic_future_require_scope_retired(
            ctx->scope, node, ctx, "match default exit");
        scope_exit(&ctx->scope);
        if (!default_reachable)
            ctx->future_lifecycle_unreachable_depth--;
        if ((default_flags & FLOW_HAS_DEFER) != 0)
            match_has_defer = true;
        default_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        if (default_reachable && merged_effect_delta != EFFECT_NONE) {
            flow_record_branch_effect_conflict_labeled(ctx, node,
                merged_effect_delta, "merged explicit cases",
                default_effect_delta, "default case");
        } else if (default_reachable && have_previous_case_delta) {
            flow_record_branch_effect_conflict_labeled(ctx, node,
                previous_case_delta, "previous case",
                default_effect_delta, "default case");
        }
        if (default_reachable) {
            merged_effect_delta = type_effect_mask_join(
                merged_effect_delta, default_effect_delta);
            flags |= flow_terminating_flags(default_flags);
        }
        if (default_reachable && flow_has_fallthrough(default_flags)) {
            ResourceConsumeSnapshot default_snap = snapshot_resource_states(ctx);
            if (!default_snap.valid) {
                semantic_error(ctx, ast_match_default_body(node),
                    "Resource snapshot allocation failed while checking match default");
            }
            merge_resource_snapshots_or(&fallthrough,
                                        &has_fallthrough,
                                        &default_snap);
            destroy_resource_snapshot(&default_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else if (!prior_static_match
               && !match_stmt_has_total_case_coverage(node, subj_type, ctx)) {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    if (match_has_defer && !flow_expr_is_static_literal(subject))
        flow_reject_dynamic_defer_control(ctx, node, "match");

    check_match_redundancy(node, subj_type, ctx);
    check_match_exhaustiveness(node, subj_type, ctx);
    ctx->current_function_effects =
        type_effect_mask_join(effect_base, merged_effect_delta);

    if (has_fallthrough && !fallthrough.valid) {
        semantic_error(ctx, node,
            "Resource snapshot merge failed while joining match cases");
    }

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}
