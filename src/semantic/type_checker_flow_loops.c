#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_flow_loops.h"
#include "type_checker_flow_effects.h"

static bool
resource_snapshot_count_fits(size_t count)
{
    return count <= SIZE_MAX / sizeof(Symbol *)
        && count <= SIZE_MAX / sizeof(bool)
        && count <= SIZE_MAX / sizeof(SlotState)
        && count <= SIZE_MAX / sizeof(QubitSemanticState)
        && count <= SIZE_MAX / sizeof(int32_t);
}

static bool
resource_snapshots_equal(const ResourceConsumeSnapshot *a,
                         const ResourceConsumeSnapshot *b)
{
    if (a == NULL || b == NULL)
        return a == b;
    if (a->count != b->count)
        return false;
    for (size_t i = 0; i < a->count; i++) {
        if (a->symbols[i] != b->symbols[i])
            return false;
        if (a->states[i] != b->states[i])
            return false;
        if (a->used_states[i] != b->used_states[i])
            return false;
        if (a->slot_states[i] != b->slot_states[i])
            return false;
        if (a->sem_states[i] != b->sem_states[i])
            return false;
        if (a->pool_ids[i] != b->pool_ids[i])
            return false;
    }
    return true;
}

static size_t
for_loop_known_iteration_cap(const ASTNode *node, bool *known)
{
    if (known != NULL)
        *known = false;
    if (node == NULL
        || node->data.for_loop.range_start == NULL
        || node->data.for_loop.range_end == NULL) {
        return 0;
    }
    if (node->data.for_loop.range_start->type != AST_NUMBER
        || node->data.for_loop.range_end->type != AST_NUMBER) {
        return 0;
    }

    double start = node->data.for_loop.range_start->data.number.value;
    double end = node->data.for_loop.range_end->data.number.value;
    if (known != NULL)
        *known = true;
    if (end <= start)
        return 0;
    if ((end - start) <= 1.0)
        return 1;
    return 2;
}

static ResourceConsumeSnapshot
copy_resource_snapshot(const ResourceConsumeSnapshot *src)
{
    ResourceConsumeSnapshot dst = {0};
    if (src == NULL || src->count == 0)
        return dst;
    if (!resource_snapshot_count_fits(src->count))
        return dst;

    dst.symbols    = calloc(src->count, sizeof(Symbol *));
    dst.states     = calloc(src->count, sizeof(bool));
    dst.used_states = calloc(src->count, sizeof(bool));
    dst.slot_states = calloc(src->count, sizeof(SlotState));
    dst.sem_states = calloc(src->count, sizeof(QubitSemanticState));
    dst.pool_ids   = calloc(src->count, sizeof(int32_t));
    if (dst.symbols == NULL || dst.states == NULL
        || dst.used_states == NULL || dst.slot_states == NULL
        || dst.sem_states == NULL || dst.pool_ids == NULL) {
        destroy_resource_snapshot(&dst);
        return dst;
    }

    memcpy(dst.symbols, src->symbols, src->count * sizeof(Symbol *));
    memcpy(dst.states, src->states, src->count * sizeof(bool));
    memcpy(dst.used_states, src->used_states, src->count * sizeof(bool));
    memcpy(dst.slot_states, src->slot_states, src->count * sizeof(SlotState));
    memcpy(dst.sem_states, src->sem_states, src->count * sizeof(QubitSemanticState));
    memcpy(dst.pool_ids, src->pool_ids, src->count * sizeof(int32_t));
    dst.count = src->count;
    dst.capacity = src->count;
    return dst;
}

void
merge_resource_snapshots_or(ResourceConsumeSnapshot *dst,
                            bool *dst_initialized,
                            const ResourceConsumeSnapshot *src)
{
    if (dst == NULL || dst_initialized == NULL || src == NULL)
        return;

    if (!*dst_initialized) {
        ResourceConsumeSnapshot copy = copy_resource_snapshot(src);
        if (src->count > 0 && copy.count != src->count) {
            destroy_resource_snapshot(&copy);
            return;
        }
        *dst = copy;
        *dst_initialized = true;
        return;
    }

    merge_resource_states_or(dst, src);
}

void
loop_flow_record(LoopFlowState *loop_flow,
                 bool is_break,
                 const ResourceConsumeSnapshot *state)
{
    if (loop_flow == NULL || state == NULL)
        return;

    if (is_break) {
        merge_resource_snapshots_or(&loop_flow->break_states,
                                    &loop_flow->has_break_states,
                                    state);
        return;
    }

    merge_resource_snapshots_or(&loop_flow->continue_states,
                                &loop_flow->has_continue_states,
                                state);
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
    uint32_t effect_base = ctx->current_function_effects;
    uint32_t merged_effect_delta = EFFECT_NONE;
    uint32_t previous_iter_delta = EFFECT_NONE;
    bool have_previous_iter_delta = false;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    if (ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
        ctx->loop_labels[ctx->loop_depth] = node->data.for_loop.label;
    ctx->loop_depth++;

    Type *var_type = TYPE_INT;
    if (node->data.for_loop.iterable != NULL) {
        Type *coll_type = flow_normalize_type(
            type_check_expression(node->data.for_loop.iterable, ctx));
        if (type_is_constructed_named(coll_type, "Array")
            || type_is_constructed_named(coll_type, "Slice")
            || type_is_constructed_named(coll_type, "List")) {
            var_type = flow_normalize_type(type_get_constructed_arg(coll_type, 0));
        } else if (coll_type != TYPE_UNKNOWN) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_FOR_IN_NON_ITERABLE, PGY_FIX_USE_ARRAY_SLICE_OR_LIST,
                node->data.for_loop.iterable,
                "for-in requires Array<T>, Slice<T>, or List<T>, got '%s'",
                type_name_or_unknown(coll_type));
        }
    }

    Symbol *loop_var = symbol_create_variable(
        node->data.for_loop.variable, var_type, node->line, node->column);
    scope_declare(ctx->scope, loop_var);

    if (node->data.for_loop.range_start != NULL) {
        Type *t = flow_normalize_type(
            type_check_expression(node->data.for_loop.range_start, ctx));
        require_assignable(t, TYPE_INT, node->data.for_loop.range_start, ctx);
    }
    if (node->data.for_loop.range_end != NULL) {
        Type *t = flow_normalize_type(
            type_check_expression(node->data.for_loop.range_end, ctx));
        require_assignable(t, TYPE_INT, node->data.for_loop.range_end, ctx);
    }

    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot merged = copy_resource_snapshot(&base);
    ResourceConsumeSnapshot entry = copy_resource_snapshot(&base);
    bool known_iterations = false;
    size_t known_cap = for_loop_known_iteration_cap(node, &known_iterations);
    bool has_break_exit = false;
    bool body_must_return = false;
    if (flow_ast_contains_defer_stmt(node->data.for_loop.body)
        && (!known_iterations || known_cap > 1)) {
        flow_reject_dynamic_defer_control(ctx, node, "for");
    }
    size_t max_iterations = (known_iterations && known_cap <= 1)
        ? 1
        : (base.count + 1);
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        ResourceConsumeSnapshot backedge = {0};
        bool has_backedge = false;
        FlowFlags body_flags = FLOW_NONE;
        uint32_t previous_merged_effect_delta = merged_effect_delta;
        uint32_t iter_effect_delta = EFFECT_NONE;

        loop_flow.loop_scope = ctx->scope;
        restore_resource_states(&entry);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        body_flags = type_check_block_flow(node->data.for_loop.body, ctx, &loop_flow);
        scope_exit(&ctx->scope);
        iter_effect_delta =
            effect_delta_from_baseline(effect_base, ctx->current_function_effects);
        flow_merge_effect_delta(ctx, node,
            &merged_effect_delta, &previous_iter_delta,
            &have_previous_iter_delta,
            iter_effect_delta);
        if (body_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot body_snap = snapshot_resource_states(ctx);
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

        if (!has_backedge) {
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (resource_snapshots_equal(&entry, &backedge)
            && type_effect_mask_compare(previous_merged_effect_delta,
                                        merged_effect_delta) == 0) {
            destroy_resource_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_resource_snapshot(&entry);
        entry = backedge;
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
    if (known_iterations && known_cap > 0 && !has_break_exit && body_must_return)
        return FLOW_RETURN;
    return FLOW_FALLTHROUGH;
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
    uint32_t effect_base = ctx->current_function_effects;
    uint32_t merged_effect_delta = EFFECT_NONE;
    uint32_t previous_iter_delta = EFFECT_NONE;
    bool have_previous_iter_delta = false;
    bool condition_static_value = false;
    bool condition_is_static_bool =
        flow_static_bool_value(node->data.while_loop.condition,
                               &condition_static_value);
    bool condition_static_true =
        condition_is_static_bool && condition_static_value;
    bool condition_static_false =
        condition_is_static_bool && !condition_static_value;
    bool has_break_exit = false;
    bool body_must_return = false;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    if (ctx->loop_depth < SEMANTIC_MAX_LOOP_DEPTH)
        ctx->loop_labels[ctx->loop_depth] = node->data.while_loop.label;
    ctx->loop_depth++;

    if (condition_static_false) {
        Type *cond = flow_normalize_type(
            type_check_expression(node->data.while_loop.condition, ctx));
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
    if (flow_ast_contains_defer_stmt(node->data.while_loop.body)
        && !flow_condition_is_static_bool(node->data.while_loop.condition)) {
        flow_reject_dynamic_defer_control(ctx, node, "while");
    }
    size_t max_iterations = base.count + 1;
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        ResourceConsumeSnapshot backedge = {0};
        bool has_backedge = false;
        FlowFlags body_flags = FLOW_NONE;
        uint32_t previous_merged_effect_delta = merged_effect_delta;
        uint32_t iter_effect_delta = EFFECT_NONE;

        loop_flow.loop_scope = ctx->scope;
        restore_resource_states(&entry);
        ctx->current_function_effects = effect_base;
        Type *cond = flow_normalize_type(
            type_check_expression(node->data.while_loop.condition, ctx));
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
        body_flags = type_check_block_flow(node->data.while_loop.body, ctx, &loop_flow);
        scope_exit(&ctx->scope);
        iter_effect_delta =
            effect_delta_from_baseline(effect_base, ctx->current_function_effects);
        flow_merge_effect_delta(ctx, node,
            &merged_effect_delta, &previous_iter_delta,
            &have_previous_iter_delta,
            iter_effect_delta);
        if (body_flags & FLOW_FALLTHROUGH) {
            ResourceConsumeSnapshot body_snap = snapshot_resource_states(ctx);
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

        if (!has_backedge) {
            destroy_resource_snapshot(&backedge);
            break;
        }

        if (resource_snapshots_equal(&entry, &backedge)
            && type_effect_mask_compare(previous_merged_effect_delta,
                                        merged_effect_delta) == 0) {
            destroy_resource_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_resource_snapshot(&entry);
        entry = backedge;
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
    if (has_break_exit)
        return FLOW_FALLTHROUGH;
    if (condition_static_true && body_must_return)
        return FLOW_RETURN;
    return FLOW_FALLTHROUGH;
}

bool
type_check_while_loop(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_while_loop_flow(node, ctx);
    return !ctx->has_error;
}
