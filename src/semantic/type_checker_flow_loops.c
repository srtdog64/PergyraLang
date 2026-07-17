#include "diag_codes.h"
#include "type_checker_flow_loops.h"
#include "type_checker_flow_effects.h"
#include "type_checker_flow_loop_summary.h"

static size_t
for_loop_known_iteration_cap(const ASTNode *node, bool *known)
{
    if (known != NULL)
        *known = false;
    ASTNode *range_start = ast_for_range_start(node);
    ASTNode *range_end = ast_for_range_end(node);
    if (node == NULL
        || range_start == NULL
        || range_end == NULL) {
        return 0;
    }
    if (range_start->type != AST_NUMBER
        || range_end->type != AST_NUMBER) {
        return 0;
    }

    double start = ast_number_value(range_start);
    double end = ast_number_value(range_end);
    if (known != NULL)
        *known = true;
    if (end <= start)
        return 0;
    if ((end - start) <= 1.0)
        return 1;
    return 2;
}

static void
destroy_loop_flow_state(LoopFlowState *loop_flow)
{
    if (loop_flow == NULL)
        return;
    destroy_resource_snapshot(&loop_flow->break_states);
    destroy_resource_snapshot(&loop_flow->continue_states);
    loop_flow->has_break_states = false;
    loop_flow->has_continue_states = false;
}

FlowFlags
type_check_for_loop_flow(ASTNode *node, SemanticContext *ctx)
{
    size_t diagnostic_base = ctx->diagnostic_count;
    uint32_t effect_base = ctx->current_function_effects;
    uint32_t merged_effect_delta = EFFECT_NONE;
    uint32_t previous_iter_delta = EFFECT_NONE;
    bool have_previous_iter_delta = false;
    ASTNode *iterable = ast_for_iterable(node);
    ASTNode *range_start = ast_for_range_start(node);
    ASTNode *range_end = ast_for_range_end(node);
    ASTNode *body = ast_for_body(node);
    size_t header_diagnostic_base = ctx->diagnostic_count;
    Type *iterable_type_fact = TYPE_INT;
    bool iteration_fact_valid = true;
    bool has_iteration_owner =
        semantic_current_routine_syntax_id(ctx) != 0;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    if (ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
        ctx->loop_labels[ctx->loop_depth] = ast_for_label(node);
    ctx->loop_depth++;

    Type *var_type = TYPE_INT;
    if (iterable != NULL) {
        Type *coll_type = flow_normalize_type(
            type_check_expression(iterable, ctx));
        iterable_type_fact = coll_type;
        if (type_is_constructed_named(coll_type, "Array")
            || type_is_constructed_named(coll_type, "Slice")
            || type_is_constructed_named(coll_type, "List")) {
            var_type = flow_normalize_type(type_get_constructed_arg(coll_type, 0));
        } else if (coll_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_FOR_IN_NON_ITERABLE, PGY_FIX_USE_ARRAY_SLICE_OR_LIST,
                iterable,
                "for-in requires Array<T>, Slice<T>, or List<T>, got '%s'",
                type_name_or_unknown(coll_type));
            iteration_fact_valid = false;
        } else {
            iteration_fact_valid = false;
        }
    }

    Symbol *loop_var = symbol_create_variable(
        ast_for_variable(node), var_type, node->line, node->column);
    symbol_mark_declaration(loop_var, ast_node_stable_id(node), false);
    scope_declare(ctx->scope, loop_var);

    if (range_start != NULL) {
        Type *t = flow_normalize_type(
            type_check_expression(range_start, ctx));
        require_assignable(t, TYPE_INT, range_start, ctx);
    }
    if (range_end != NULL) {
        Type *t = flow_normalize_type(
            type_check_expression(range_end, ctx));
        require_assignable(t, TYPE_INT, range_end, ctx);
    }

    if (iteration_fact_valid && has_iteration_owner
        && ctx->diagnostic_count == header_diagnostic_base
        && !semantic_iteration_type_fact_record(
                ctx, node, var_type, iterable_type_fact,
                iterable != NULL && iterable->type != AST_IDENTIFIER)) {
        semantic_error(ctx, node,
            "Iteration type fact allocation or identity binding failed");
    }

    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot merged = copy_resource_snapshot(&base);
    ResourceConsumeSnapshot entry = copy_resource_snapshot(&base);
    bool known_iterations = false;
    size_t known_cap = for_loop_known_iteration_cap(node, &known_iterations);
    bool has_break_exit = false;
    bool body_must_return = false;
    bool dynamic_defer_rejected = false;
    size_t max_iterations = (known_iterations && known_cap <= 1)
        ? 1
        : (base.count + 1);
    if (max_iterations == 0)
        max_iterations = 1;

    if (!base.valid || !merged.valid || !entry.valid) {
        semantic_error(ctx, node,
            "Resource snapshot allocation failed before for-loop analysis");
        ctx->loop_depth--;
        if (ctx->loop_depth >= 0 && ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
            ctx->loop_labels[ctx->loop_depth] = NULL;
        scope_exit(&ctx->scope);
        ctx->current_function_effects = effect_base;
        destroy_resource_snapshot(&base);
        destroy_resource_snapshot(&merged);
        destroy_resource_snapshot(&entry);
        return FLOW_FALLTHROUGH;
    }

    FlowFlags cached_flags = FLOW_NONE;
    if (loop_flow_summary_try_apply(ctx, node, &base, effect_base,
                                    &cached_flags)) {
        ctx->loop_depth--;
        if (ctx->loop_depth >= 0 && ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
            ctx->loop_labels[ctx->loop_depth] = NULL;
        scope_exit(&ctx->scope);
        destroy_resource_snapshot(&base);
        destroy_resource_snapshot(&merged);
        destroy_resource_snapshot(&entry);
        return cached_flags;
    }

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        ResourceConsumeSnapshot backedge = {0};
        bool has_backedge = false;
        FlowFlags body_flags = FLOW_NONE;
        uint32_t iter_effect_delta = EFFECT_NONE;

        loop_flow.loop_scope = ctx->scope;
        restore_resource_states(&entry);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        loop_flow_summary_note_body_check(ctx, node, "for");
        body_flags = type_check_block_flow(body, ctx, &loop_flow);
        scope_exit(&ctx->scope);
        if (!dynamic_defer_rejected
            && (body_flags & FLOW_HAS_DEFER) != 0
            && (!known_iterations || known_cap > 1)) {
            flow_reject_dynamic_defer_control(ctx, node, "for");
            dynamic_defer_rejected = true;
        }
        iter_effect_delta =
            effect_delta_from_baseline(effect_base, ctx->current_function_effects);
        flow_merge_effect_delta(ctx, node,
            &merged_effect_delta, &previous_iter_delta,
            &have_previous_iter_delta,
            iter_effect_delta);
        if (body_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot body_snap = snapshot_resource_states(ctx);
            if (!body_snap.valid) {
                semantic_error(ctx, body != NULL ? body : node,
                    "Resource snapshot allocation failed while checking for-loop body");
                destroy_resource_snapshot(&body_snap);
                destroy_loop_flow_state(&loop_flow);
                break;
            }
            merge_resource_states_or(&merged, &body_snap);
            merge_resource_snapshots_or(&backedge, &has_backedge, &body_snap);
            destroy_resource_snapshot(&body_snap);
        }

        if (loop_flow.has_continue_states)
            merge_resource_snapshots_or(&backedge, &has_backedge,
                                        &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_resource_states_or(&merged, &loop_flow.break_states);
        if (loop_flow.has_break_states)
            has_break_exit = true;
        if (!has_backedge && (body_flags & FLOW_RETURN) != 0)
            body_must_return = true;

        destroy_loop_flow_state(&loop_flow);

        if ((has_backedge && !backedge.valid) || !merged.valid) {
            semantic_error(ctx, node,
                "Resource snapshot merge failed while checking for-loop flow");
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (!has_backedge) {
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (resource_snapshot_availability_equal(&entry, &backedge)) {
            destroy_resource_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_resource_snapshot(&entry);
        entry = backedge;
    }

    FlowFlags result_flags = FLOW_FALLTHROUGH;
    if (known_iterations && known_cap > 0 && !has_break_exit && body_must_return)
        result_flags = FLOW_RETURN;
    if (ctx->diagnostic_count == diagnostic_base) {
        loop_flow_summary_record(ctx, node, &base, &merged,
                                 effect_base, merged_effect_delta,
                                 result_flags);
    }

    ctx->loop_depth--;
    if (ctx->loop_depth >= 0 && ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
        ctx->loop_labels[ctx->loop_depth] = NULL;
    restore_resource_states(&merged);
    scope_exit(&ctx->scope);
    ctx->current_function_effects =
        type_effect_mask_join(effect_base, merged_effect_delta);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&merged);
    destroy_resource_snapshot(&entry);
    return result_flags;
}

bool
type_check_for_loop(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_for_loop_flow(node, ctx);
    return !ctx->has_error;
}

FlowFlags
type_check_while_loop_flow(ASTNode *node, SemanticContext *ctx)
{
    size_t diagnostic_base = ctx->diagnostic_count;
    uint32_t effect_base = ctx->current_function_effects;
    uint32_t merged_effect_delta = EFFECT_NONE;
    uint32_t previous_iter_delta = EFFECT_NONE;
    bool have_previous_iter_delta = false;
    bool condition_static_value = false;
    bool condition_is_static_bool =
        flow_static_bool_value(ast_while_condition(node),
                               &condition_static_value);
    bool condition_static_true =
        condition_is_static_bool && condition_static_value;
    bool condition_static_false =
        condition_is_static_bool && !condition_static_value;
    bool has_break_exit = false;
    bool body_must_return = false;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    if (ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
        ctx->loop_labels[ctx->loop_depth] = ast_while_label(node);
    ctx->loop_depth++;

    if (condition_static_false) {
        Type *cond = flow_normalize_type(
            type_check_expression(ast_while_condition(node), ctx));
        if (!type_equals(cond, TYPE_BOOL)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_CONDITION_NON_BOOL,
                PGY_FIX_CONVERT_CONDITION_TO_BOOL,
                node,
                "While condition must be Bool, got '%s'",
                type_name_or_unknown(cond));
        }
        ctx->loop_depth--;
        if (ctx->loop_depth >= 0 && ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
            ctx->loop_labels[ctx->loop_depth] = NULL;
        scope_exit(&ctx->scope);
        ctx->current_function_effects = effect_base;
        return FLOW_FALLTHROUGH;
    }

    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot merged = copy_resource_snapshot(&base);
    ResourceConsumeSnapshot entry = copy_resource_snapshot(&base);
    bool dynamic_defer_rejected = false;
    size_t max_iterations = base.count + 1;
    if (max_iterations == 0)
        max_iterations = 1;

    if (!base.valid || !merged.valid || !entry.valid) {
        semantic_error(ctx, node,
            "Resource snapshot allocation failed before while-loop analysis");
        ctx->loop_depth--;
        if (ctx->loop_depth >= 0 && ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
            ctx->loop_labels[ctx->loop_depth] = NULL;
        scope_exit(&ctx->scope);
        ctx->current_function_effects = effect_base;
        destroy_resource_snapshot(&base);
        destroy_resource_snapshot(&merged);
        destroy_resource_snapshot(&entry);
        return FLOW_FALLTHROUGH;
    }

    FlowFlags cached_flags = FLOW_NONE;
    if (loop_flow_summary_try_apply(ctx, node, &base, effect_base,
                                    &cached_flags)) {
        ctx->loop_depth--;
        if (ctx->loop_depth >= 0 && ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
            ctx->loop_labels[ctx->loop_depth] = NULL;
        scope_exit(&ctx->scope);
        destroy_resource_snapshot(&base);
        destroy_resource_snapshot(&merged);
        destroy_resource_snapshot(&entry);
        return cached_flags;
    }

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        ResourceConsumeSnapshot backedge = {0};
        bool has_backedge = false;
        FlowFlags body_flags = FLOW_NONE;
        uint32_t iter_effect_delta = EFFECT_NONE;

        loop_flow.loop_scope = ctx->scope;
        restore_resource_states(&entry);
        ctx->current_function_effects = effect_base;
        Type *cond = flow_normalize_type(
            type_check_expression(ast_while_condition(node), ctx));
        if (!type_equals(cond, TYPE_BOOL)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_CONDITION_NON_BOOL,
                PGY_FIX_CONVERT_CONDITION_TO_BOOL,
                node,
                "While condition must be Bool, got '%s'",
                type_name_or_unknown(cond));
        }

        scope_enter(&ctx->scope, SCOPE_BLOCK);
        loop_flow_summary_note_body_check(ctx, node, "while");
        body_flags = type_check_block_flow(ast_while_body(node), ctx, &loop_flow);
        scope_exit(&ctx->scope);
        if (!dynamic_defer_rejected
            && (body_flags & FLOW_HAS_DEFER) != 0
            && !flow_condition_is_static_bool(ast_while_condition(node))) {
            flow_reject_dynamic_defer_control(ctx, node, "while");
            dynamic_defer_rejected = true;
        }
        iter_effect_delta =
            effect_delta_from_baseline(effect_base, ctx->current_function_effects);
        flow_merge_effect_delta(ctx, node,
            &merged_effect_delta, &previous_iter_delta,
            &have_previous_iter_delta,
            iter_effect_delta);
        if (body_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot body_snap = snapshot_resource_states(ctx);
            if (!body_snap.valid) {
                semantic_error(ctx, ast_while_body(node) != NULL
                    ? ast_while_body(node)
                    : node,
                    "Resource snapshot allocation failed while checking while-loop body");
                destroy_resource_snapshot(&body_snap);
                destroy_loop_flow_state(&loop_flow);
                break;
            }
            merge_resource_states_or(&merged, &body_snap);
            merge_resource_snapshots_or(&backedge, &has_backedge, &body_snap);
            destroy_resource_snapshot(&body_snap);
        }

        if (loop_flow.has_continue_states)
            merge_resource_snapshots_or(&backedge, &has_backedge,
                                        &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_resource_states_or(&merged, &loop_flow.break_states);
        if (loop_flow.has_break_states)
            has_break_exit = true;
        if (!has_backedge && (body_flags & FLOW_RETURN) != 0)
            body_must_return = true;

        destroy_loop_flow_state(&loop_flow);

        if ((has_backedge && !backedge.valid) || !merged.valid) {
            semantic_error(ctx, node,
                "Resource snapshot merge failed while checking while-loop flow");
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (!has_backedge) {
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (resource_snapshot_availability_equal(&entry, &backedge)) {
            destroy_resource_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_resource_snapshot(&entry);
        entry = backedge;
    }

    FlowFlags result_flags = FLOW_FALLTHROUGH;
    if (!has_break_exit && condition_static_true && body_must_return)
        result_flags = FLOW_RETURN;
    if (ctx->diagnostic_count == diagnostic_base) {
        loop_flow_summary_record(ctx, node, &base, &merged,
                                 effect_base, merged_effect_delta,
                                 result_flags);
    }

    ctx->loop_depth--;
    if (ctx->loop_depth >= 0 && ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
        ctx->loop_labels[ctx->loop_depth] = NULL;
    restore_resource_states(&merged);
    scope_exit(&ctx->scope);
    ctx->current_function_effects =
        type_effect_mask_join(effect_base, merged_effect_delta);
    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&merged);
    destroy_resource_snapshot(&entry);
    return result_flags;
}

bool
type_check_while_loop(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_while_loop_flow(node, ctx);
    return !ctx->has_error;
}
