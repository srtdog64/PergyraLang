#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "diag_codes.h"
#include "type_checker_flow_internal.h"
#include "type_checker_flow_effects.h"
#include "type_checker_flow_loops.h"

static FlowFlags type_check_statement_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);
static FlowFlags type_check_if_stmt_flow(ASTNode *node,
                                         SemanticContext *ctx,
                                         LoopFlowState *loop_flow);
static FlowFlags type_check_match_stmt_flow(ASTNode *node,
                                            SemanticContext *ctx,
                                            LoopFlowState *loop_flow);
static FlowFlags type_check_with_stmt_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);

static FlowFlags
flow_terminating_flags(FlowFlags flags)
{
    return flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN);
}

static FlowFlags
flow_record_statement_result(FlowFlags current, FlowFlags statement)
{
    current &= ~FLOW_FALLTHROUGH;
    current |= statement & (FLOW_FALLTHROUGH
                          | FLOW_BREAK
                          | FLOW_CONTINUE
                          | FLOW_RETURN);
    return current;
}

static bool
flow_has_fallthrough(FlowFlags flags)
{
    return (flags & FLOW_FALLTHROUGH) != 0;
}

static Type *
flow_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_annotation_nullable(ctx, type_ref);
}

Type *
flow_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

bool
flow_condition_is_static_bool(const ASTNode *node)
{
    return node != NULL && node->type == AST_BOOLEAN;
}

static bool
flow_expr_is_static_literal(const ASTNode *node)
{
    return node != NULL
        && (node->type == AST_NUMBER
            || node->type == AST_STRING
            || node->type == AST_BOOLEAN);
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

bool
flow_ast_contains_defer_stmt(const ASTNode *node)
{
    if (node == NULL)
        return false;

    switch (node->type) {
    case AST_DEFER_STMT:
        return true;
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++) {
            if (flow_ast_contains_defer_stmt(node->data.block.statements[i]))
                return true;
        }
        return false;
    case AST_IF_STMT:
        return flow_ast_contains_defer_stmt(node->data.if_stmt.then_branch)
            || flow_ast_contains_defer_stmt(node->data.if_stmt.else_branch);
    case AST_WHILE_LOOP:
        return flow_ast_contains_defer_stmt(node->data.while_loop.body);
    case AST_FOR_LOOP:
        return flow_ast_contains_defer_stmt(node->data.for_loop.body);
    case AST_MATCH_STMT:
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
            if (flow_ast_contains_defer_stmt(node->data.match_stmt.cases[i]))
                return true;
        }
        return flow_ast_contains_defer_stmt(node->data.match_stmt.default_body);
    case AST_MATCH_CASE:
        return flow_ast_contains_defer_stmt(node->data.match_case.body);
    default:
        return false;
    }
}

void
flow_reject_dynamic_defer_control(SemanticContext *ctx,
                                  ASTNode *site,
                                  const char *control_kind)
{
    if (ctx == NULL || site == NULL || ctx->has_error)
        return;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_DEFER_DYNAMIC_CONTROL,
        PGY_CAUSE_DEFER_DYNAMIC_CONTROL,
        PGY_FIX_MOVE_DEFER_OUTSIDE_DYNAMIC_CONTROL,
        site,
        "defer inside dynamic %s control is not beta-stable; move the defer outside the dynamic control or make the control condition compile-time static",
        control_kind != NULL ? control_kind : "flow");
}

FlowFlags
type_check_block_flow(ASTNode *node, SemanticContext *ctx,
                      LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    if (node->type != AST_BLOCK)
        return type_check_statement_flow(node, ctx, loop_flow);

    FlowFlags flags = FLOW_FALLTHROUGH;
    for (size_t i = 0; i < node->data.block.count; i++) {
        if (!flow_has_fallthrough(flags)) {
            flow_record_unreachable_statement(ctx, node->data.block.statements[i]);
            break;
        }

        FlowFlags stmt_flags =
            type_check_statement_flow(node->data.block.statements[i], ctx, loop_flow);

        flags = flow_record_statement_result(flags, stmt_flags);
    }

    return flags;
}

static FlowFlags
type_check_if_stmt_flow(ASTNode *node, SemanticContext *ctx,
                        LoopFlowState *loop_flow)
{
    Type *cond = flow_normalize_type(
        type_check_expression(node->data.if_stmt.condition, ctx));
    uint32_t effect_base = ctx->current_function_effects;
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;
    FlowFlags then_flags = FLOW_NONE;
    uint32_t then_effect_delta = EFFECT_NONE;
    uint32_t else_effect_delta = EFFECT_NONE;

    if (!flow_condition_is_static_bool(node->data.if_stmt.condition)
        && (flow_ast_contains_defer_stmt(node->data.if_stmt.then_branch)
            || flow_ast_contains_defer_stmt(node->data.if_stmt.else_branch))) {
        flow_reject_dynamic_defer_control(ctx, node, "if");
    }

    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_CONDITION_NON_BOOL, PGY_FIX_CONVERT_CONDITION_TO_BOOL,
            node,
            "If condition must be Bool, got '%s'", cond->name);
    }

    restore_resource_states(&base);
    ctx->current_function_effects = effect_base;
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    then_flags = type_check_block_flow(node->data.if_stmt.then_branch, ctx, loop_flow);
    scope_exit(&ctx->scope);
    then_effect_delta = effect_delta_from_baseline(effect_base,
        ctx->current_function_effects);
    flags |= flow_terminating_flags(then_flags);
    if (flow_has_fallthrough(then_flags)) {
        ResourceConsumeSnapshot then_snap = snapshot_resource_states(ctx);
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &then_snap);
        destroy_resource_snapshot(&then_snap);
        flags |= FLOW_FALLTHROUGH;
    }

    if (node->data.if_stmt.else_branch != NULL) {
        FlowFlags else_flags = FLOW_NONE;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        else_flags =
            type_check_statement_flow(node->data.if_stmt.else_branch, ctx, loop_flow);
        scope_exit(&ctx->scope);
        else_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        flags |= flow_terminating_flags(else_flags);
        if (flow_has_fallthrough(else_flags)) {
            ResourceConsumeSnapshot else_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &else_snap);
            destroy_resource_snapshot(&else_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
        else_effect_delta = EFFECT_NONE;
    }

    flow_record_branch_effect_conflict_labeled(ctx, node,
        then_effect_delta, "then branch",
        else_effect_delta,
        node->data.if_stmt.else_branch != NULL ? "else branch" : "implicit fallthrough path");
    ctx->current_function_effects = type_effect_mask_join(
        effect_base,
        type_effect_mask_join(then_effect_delta, else_effect_delta));

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_match_stmt_flow(ASTNode *node, SemanticContext *ctx,
                           LoopFlowState *loop_flow)
{
    Type *subj_type = type_check_expression(node->data.match_stmt.subject, ctx);
    uint32_t effect_base = ctx->current_function_effects;
    uint32_t merged_effect_delta = EFFECT_NONE;
    uint32_t previous_case_delta = EFFECT_NONE;
    bool have_previous_case_delta = false;
    ResourceConsumeSnapshot base = snapshot_resource_states(ctx);
    ResourceConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;

    if (flow_ast_contains_defer_stmt(node)
        && !flow_expr_is_static_literal(node->data.match_stmt.subject)) {
        flow_reject_dynamic_defer_control(ctx, node, "match");
    }

    if (!flow_match_subject_is_beta_supported(subj_type)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_MATCH_PATTERN_INVALID,
            PGY_CAUSE_MATCH_PATTERN_SHAPE,
            PGY_FIX_ALIGN_PATTERN_ARITY_OR_KIND,
            node->data.match_stmt.subject,
            "Match subject type '%s' is not beta-stable; supported subjects are Int, Long, Bool, enum, Option<T>, and Result<T>",
            subj_type != NULL && subj_type->name != NULL ? subj_type->name : "<unknown>");
    }

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];
        uint32_t case_effect_delta = EFFECT_NONE;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);

        if (mc->data.match_case.pattern != NULL) {
            type_check_match_case_patterns(mc, subj_type, ctx);
        }

        if (mc->data.match_case.guard != NULL) {
            Type *guard_type = flow_normalize_type(
                type_check_expression(mc->data.match_case.guard, ctx));
            if (!type_equals(guard_type, TYPE_BOOL)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                    PGY_CAUSE_CONDITION_NON_BOOL, PGY_FIX_CONVERT_CONDITION_TO_BOOL,
                    mc->data.match_case.guard,
                    "Case guard must be Bool, got '%s'", guard_type->name);
            }
        }

        FlowFlags case_flags =
            type_check_block_flow(mc->data.match_case.body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        case_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        if (merged_effect_delta != EFFECT_NONE)
            flow_record_branch_effect_conflict_labeled(ctx, mc,
                merged_effect_delta, "merged prior cases",
                case_effect_delta, "current case");
        else if (have_previous_case_delta)
            flow_record_branch_effect_conflict_labeled(ctx, mc,
                previous_case_delta, "previous case",
                case_effect_delta, "current case");
        merged_effect_delta =
            type_effect_mask_join(merged_effect_delta, case_effect_delta);
        previous_case_delta = case_effect_delta;
        have_previous_case_delta = true;
        flags |= flow_terminating_flags(case_flags);
        if (flow_has_fallthrough(case_flags)) {
            ResourceConsumeSnapshot case_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &case_snap);
            destroy_resource_snapshot(&case_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    }

    if (node->data.match_stmt.default_body != NULL) {
        FlowFlags default_flags = FLOW_NONE;
        uint32_t default_effect_delta = EFFECT_NONE;
        restore_resource_states(&base);
        ctx->current_function_effects = effect_base;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        default_flags =
            type_check_block_flow(node->data.match_stmt.default_body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        default_effect_delta = effect_delta_from_baseline(effect_base,
            ctx->current_function_effects);
        if (merged_effect_delta != EFFECT_NONE)
            flow_record_branch_effect_conflict_labeled(ctx, node,
                merged_effect_delta, "merged explicit cases",
                default_effect_delta, "default case");
        else if (have_previous_case_delta)
            flow_record_branch_effect_conflict_labeled(ctx, node,
                previous_case_delta, "previous case",
                default_effect_delta, "default case");
        merged_effect_delta =
            type_effect_mask_join(merged_effect_delta, default_effect_delta);
        flags |= flow_terminating_flags(default_flags);
        if (flow_has_fallthrough(default_flags)) {
            ResourceConsumeSnapshot default_snap = snapshot_resource_states(ctx);
            merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &default_snap);
            destroy_resource_snapshot(&default_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else if (!match_stmt_has_total_case_coverage(node, subj_type, ctx)) {
        merge_resource_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    check_match_redundancy(node, subj_type, ctx);
    check_match_exhaustiveness(node, subj_type, ctx);
    ctx->current_function_effects =
        type_effect_mask_join(effect_base, merged_effect_delta);

    if (has_fallthrough)
        restore_resource_states(&fallthrough);
    else
        restore_resource_states(&base);

    destroy_resource_snapshot(&base);
    destroy_resource_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_with_stmt_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    scope_enter(&ctx->scope, SCOPE_WITH);

    ASTNode *slot_type_node = node->data.with_stmt.slot_type;
    const char *alias = node->data.with_stmt.alias;
    bool is_secure = node->data.with_stmt.is_secure;

    Type *inner = flow_normalize_type(flow_resolve_type_ref(slot_type_node, ctx));
    Type *slot_type = type_create_slot(inner, is_secure);

    Symbol *sym = symbol_create_slot(alias, slot_type, is_secure, NULL,
                                     node->line, node->column);
    scope_declare(ctx->scope, sym);
    scope_register_slot(ctx->scope, sym);

    FlowFlags flags = type_check_block_flow(node->data.with_stmt.body, ctx, loop_flow);

    scope_auto_release_slots(ctx->scope);
    scope_exit(&ctx->scope);
    return flags;
}

static FlowFlags
type_check_statement_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    switch (node->type) {
    case AST_BLOCK: {
        FlowFlags flags;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        flags = type_check_block_flow(node, ctx, loop_flow);
        scope_exit(&ctx->scope);
        return flags;
    }
    case AST_IF_STMT:
        return type_check_if_stmt_flow(node, ctx, loop_flow);
    case AST_MATCH_STMT:
        return type_check_match_stmt_flow(node, ctx, loop_flow);
    case AST_WITH_STMT:
        return type_check_with_stmt_flow(node, ctx, loop_flow);
    case AST_WHILE_LOOP:
        return type_check_while_loop_flow(node, ctx);
    case AST_FOR_LOOP:
        return type_check_for_loop_flow(node, ctx);
    case AST_PARALLEL_BLOCK:
        (void)type_check_parallel_block_flow(node, ctx);
        return FLOW_FALLTHROUGH;
    case AST_UNSAFE_BLOCK:
        if (node->data.unsafe_block.body != NULL)
            return type_check_block_flow(node->data.unsafe_block.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_DEFER_STMT:
        (void)type_check_defer_body_flow(node->data.defer_stmt.body, ctx);
        return FLOW_FALLTHROUGH;
    case AST_RETURN:
        type_check_return_stmt(node, ctx);
        return FLOW_RETURN;
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node, "'break' used outside of loop");
            return FLOW_NONE;
        }
        if (node->data.break_stmt.label != NULL) {
            bool found = false;
            for (int i = ctx->loop_depth - 1; i >= 0; i--) {
                if (ctx->loop_labels[i] != NULL
                    && strcmp(ctx->loop_labels[i], node->data.break_stmt.label) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node, "Unknown loop label '%s' in break",
                    node->data.break_stmt.label);
                return FLOW_NONE;
            }
        }
        {
            ResourceConsumeSnapshot snap = snapshot_resource_states_from_scope(
                loop_flow != NULL && loop_flow->loop_scope != NULL
                    ? loop_flow->loop_scope
                    : ctx->scope,
                ctx);
            loop_flow_record(loop_flow, true, &snap);
            destroy_resource_snapshot(&snap);
        }
        return FLOW_BREAK;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node, "'continue' used outside of loop");
            return FLOW_NONE;
        }
        if (node->data.continue_stmt.label != NULL) {
            bool found = false;
            for (int i = ctx->loop_depth - 1; i >= 0; i--) {
                if (ctx->loop_labels[i] != NULL
                    && strcmp(ctx->loop_labels[i], node->data.continue_stmt.label) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_LOOP_CONTROL_INVALID, PGY_CAUSE_LOOP_CONTROL, PGY_FIX_MOVE_INTO_LOOP_OR_FIX_LABEL, node, "Unknown loop label '%s' in continue",
                    node->data.continue_stmt.label);
                return FLOW_NONE;
            }
        }
        {
            ResourceConsumeSnapshot snap = snapshot_resource_states_from_scope(
                loop_flow != NULL && loop_flow->loop_scope != NULL
                    ? loop_flow->loop_scope
                    : ctx->scope,
                ctx);
            loop_flow_record(loop_flow, false, &snap);
            destroy_resource_snapshot(&snap);
        }
        return FLOW_CONTINUE;
    default:
        type_check_statement(node, ctx);
        return FLOW_FALLTHROUGH;
    }
}

#include "type_checker_flow_parallel.h"

bool
type_check_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    (void)type_check_block_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
semantic_check_body_flow(ASTNode *body, SemanticContext *ctx,
                         bool *must_return_out)
{
    FlowFlags flags = type_check_block_flow(body, ctx, NULL);
    if (must_return_out != NULL)
        *must_return_out =
            ((flags & FLOW_RETURN) != 0)
            && !flow_has_fallthrough(flags);
    return !ctx->has_error;
}

bool
type_check_if_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_if_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}


bool
type_check_match_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_match_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_with_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_with_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}
