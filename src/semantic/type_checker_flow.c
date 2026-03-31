/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker control-flow and ownership analysis
 */

#include <stdlib.h>
#include <string.h>
#include "type_checker_internal.h"

typedef struct
{
    Symbol **symbols;
    bool    *states;
    size_t   count;
} QubitConsumeSnapshot;

typedef enum
{
    FLOW_NONE        = 0,
    FLOW_FALLTHROUGH = 1 << 0,
    FLOW_BREAK       = 1 << 1,
    FLOW_CONTINUE    = 1 << 2,
    FLOW_RETURN      = 1 << 3
} FlowFlags;

typedef struct
{
    QubitConsumeSnapshot break_states;
    QubitConsumeSnapshot continue_states;
    bool                 has_break_states;
    bool                 has_continue_states;
} LoopFlowState;

static FlowFlags type_check_statement_flow(ASTNode *node,
                                           SemanticContext *ctx,
                                           LoopFlowState *loop_flow);
static FlowFlags type_check_block_flow(ASTNode *node,
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

static QubitConsumeSnapshot
snapshot_qubit_states(SemanticContext *ctx)
{
    QubitConsumeSnapshot snap = {0};
    Scope *scope = ctx != NULL ? ctx->scope : NULL;

    while (scope != NULL) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            if (sym == NULL || !type_is_qubit(sym->type))
                continue;

            Symbol **new_symbols = realloc(snap.symbols,
                (snap.count + 1) * sizeof(Symbol *));
            bool *new_states = realloc(snap.states,
                (snap.count + 1) * sizeof(bool));
            if (new_symbols == NULL || new_states == NULL) {
                free(new_symbols);
                free(new_states);
                free(snap.symbols);
                free(snap.states);
                snap.symbols = NULL;
                snap.states = NULL;
                snap.count = 0;
                return snap;
            }

            snap.symbols = new_symbols;
            snap.states = new_states;
            snap.symbols[snap.count] = sym;
            snap.states[snap.count] = sym->is_consumed;
            snap.count++;
        }
        scope = scope->parent;
    }

    return snap;
}

static void
restore_qubit_states(const QubitConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    for (size_t i = 0; i < snap->count; i++) {
        if (snap->symbols[i] != NULL)
            snap->symbols[i]->is_consumed = snap->states[i];
    }
}

static void
merge_qubit_states_or(QubitConsumeSnapshot *dst,
                      const QubitConsumeSnapshot *src)
{
    if (dst == NULL || src == NULL)
        return;
    size_t count = dst->count < src->count ? dst->count : src->count;
    for (size_t i = 0; i < count; i++)
        dst->states[i] = dst->states[i] || src->states[i];
}

static void
destroy_qubit_snapshot(QubitConsumeSnapshot *snap)
{
    if (snap == NULL)
        return;
    free(snap->symbols);
    free(snap->states);
    snap->symbols = NULL;
    snap->states = NULL;
    snap->count = 0;
}

static bool
qubit_snapshots_equal(const QubitConsumeSnapshot *a,
                      const QubitConsumeSnapshot *b)
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

static QubitConsumeSnapshot
copy_qubit_snapshot(const QubitConsumeSnapshot *src)
{
    QubitConsumeSnapshot dst = {0};
    if (src == NULL || src->count == 0)
        return dst;

    dst.symbols = calloc(src->count, sizeof(Symbol *));
    dst.states = calloc(src->count, sizeof(bool));
    if (dst.symbols == NULL || dst.states == NULL) {
        free(dst.symbols);
        free(dst.states);
        dst.symbols = NULL;
        dst.states = NULL;
        return dst;
    }

    memcpy(dst.symbols, src->symbols, src->count * sizeof(Symbol *));
    memcpy(dst.states, src->states, src->count * sizeof(bool));
    dst.count = src->count;
    return dst;
}

static void
merge_qubit_snapshots_or(QubitConsumeSnapshot *dst,
                         bool *dst_initialized,
                         const QubitConsumeSnapshot *src)
{
    if (dst == NULL || dst_initialized == NULL || src == NULL)
        return;

    if (!*dst_initialized) {
        *dst = copy_qubit_snapshot(src);
        *dst_initialized = true;
        return;
    }

    merge_qubit_states_or(dst, src);
}

static void
loop_flow_record(LoopFlowState *loop_flow,
                 bool is_break,
                 const QubitConsumeSnapshot *state)
{
    if (loop_flow == NULL || state == NULL)
        return;

    if (is_break) {
        merge_qubit_snapshots_or(&loop_flow->break_states,
                                 &loop_flow->has_break_states,
                                 state);
        return;
    }

    merge_qubit_snapshots_or(&loop_flow->continue_states,
                             &loop_flow->has_continue_states,
                             state);
}

static void
destroy_loop_flow_state(LoopFlowState *loop_flow)
{
    if (loop_flow == NULL)
        return;
    destroy_qubit_snapshot(&loop_flow->break_states);
    destroy_qubit_snapshot(&loop_flow->continue_states);
    loop_flow->has_break_states = false;
    loop_flow->has_continue_states = false;
}

static FlowFlags
type_check_block_flow(ASTNode *node, SemanticContext *ctx,
                      LoopFlowState *loop_flow)
{
    if (node == NULL)
        return FLOW_FALLTHROUGH;

    if (node->type != AST_BLOCK)
        return type_check_statement_flow(node, ctx, loop_flow);

    FlowFlags flags = FLOW_FALLTHROUGH;
    for (size_t i = 0; i < node->data.block.count; i++) {
        if ((flags & FLOW_FALLTHROUGH) == 0)
            break;

        FlowFlags stmt_flags =
            type_check_statement_flow(node->data.block.statements[i], ctx, loop_flow);

        flags &= ~FLOW_FALLTHROUGH;
        flags |= (stmt_flags & (FLOW_FALLTHROUGH
                              | FLOW_BREAK
                              | FLOW_CONTINUE
                              | FLOW_RETURN));
    }

    return flags;
}

static FlowFlags
type_check_if_stmt_flow(ASTNode *node, SemanticContext *ctx,
                        LoopFlowState *loop_flow)
{
    Type *cond = type_check_expression(node->data.if_stmt.condition, ctx);
    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;
    FlowFlags then_flags = FLOW_NONE;

    if (!type_equals(cond, TYPE_BOOL)) {
        semantic_error(ctx, node,
            "If condition must be Bool, got '%s'", cond->name);
    }

    restore_qubit_states(&base);
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    then_flags = type_check_block_flow(node->data.if_stmt.then_branch, ctx, loop_flow);
    scope_exit(&ctx->scope);
    flags |= (then_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
    if (then_flags & FLOW_FALLTHROUGH) {
        QubitConsumeSnapshot then_snap = snapshot_qubit_states(ctx);
        merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &then_snap);
        destroy_qubit_snapshot(&then_snap);
        flags |= FLOW_FALLTHROUGH;
    }

    if (node->data.if_stmt.else_branch != NULL) {
        FlowFlags else_flags = FLOW_NONE;
        restore_qubit_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        else_flags =
            type_check_statement_flow(node->data.if_stmt.else_branch, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (else_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (else_flags & FLOW_FALLTHROUGH) {
            QubitConsumeSnapshot else_snap = snapshot_qubit_states(ctx);
            merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &else_snap);
            destroy_qubit_snapshot(&else_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    if (has_fallthrough)
        restore_qubit_states(&fallthrough);
    else
        restore_qubit_states(&base);

    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&fallthrough);
    return flags;
}

static FlowFlags
type_check_match_stmt_flow(ASTNode *node, SemanticContext *ctx,
                           LoopFlowState *loop_flow)
{
    Type *subj_type = type_check_expression(node->data.match_stmt.subject, ctx);
    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot fallthrough = {0};
    bool has_fallthrough = false;
    FlowFlags flags = FLOW_NONE;

    for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
        ASTNode *mc = node->data.match_stmt.cases[i];

        restore_qubit_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);

        if (mc->data.match_case.pattern != NULL) {
            Type *pat_type = type_check_expression(mc->data.match_case.pattern, ctx);
            if (!type_is_assignable(pat_type, subj_type) &&
                !type_is_assignable(subj_type, pat_type)) {
                semantic_error(ctx, mc->data.match_case.pattern,
                    "Case pattern type '%s' incompatible with match subject '%s'",
                    pat_type->name, subj_type->name);
            }
        }

        if (mc->data.match_case.guard != NULL) {
            Type *guard_type = type_check_expression(mc->data.match_case.guard, ctx);
            if (!type_equals(guard_type, TYPE_BOOL)) {
                semantic_error(ctx, mc->data.match_case.guard,
                    "Case guard must be Bool, got '%s'", guard_type->name);
            }
        }

        FlowFlags case_flags =
            type_check_block_flow(mc->data.match_case.body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (case_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (case_flags & FLOW_FALLTHROUGH) {
            QubitConsumeSnapshot case_snap = snapshot_qubit_states(ctx);
            merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &case_snap);
            destroy_qubit_snapshot(&case_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    }

    if (node->data.match_stmt.default_body != NULL) {
        FlowFlags default_flags = FLOW_NONE;
        restore_qubit_states(&base);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        default_flags =
            type_check_block_flow(node->data.match_stmt.default_body, ctx, loop_flow);
        scope_exit(&ctx->scope);
        flags |= (default_flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN));
        if (default_flags & FLOW_FALLTHROUGH) {
            QubitConsumeSnapshot default_snap = snapshot_qubit_states(ctx);
            merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &default_snap);
            destroy_qubit_snapshot(&default_snap);
            flags |= FLOW_FALLTHROUGH;
        }
    } else {
        merge_qubit_snapshots_or(&fallthrough, &has_fallthrough, &base);
        flags |= FLOW_FALLTHROUGH;
    }

    if (has_fallthrough)
        restore_qubit_states(&fallthrough);
    else
        restore_qubit_states(&base);

    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&fallthrough);
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

    Type *inner = resolve_type_node(slot_type_node, ctx);
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
    case AST_BLOCK:
        return type_check_block_flow(node, ctx, loop_flow);
    case AST_IF_STMT:
        return type_check_if_stmt_flow(node, ctx, loop_flow);
    case AST_MATCH_STMT:
        return type_check_match_stmt_flow(node, ctx, loop_flow);
    case AST_WITH_STMT:
        return type_check_with_stmt_flow(node, ctx, loop_flow);
    case AST_UNSAFE_BLOCK:
        if (node->data.unsafe_block.body != NULL)
            return type_check_block_flow(node->data.unsafe_block.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_DEFER_STMT:
        if (node->data.defer_stmt.body != NULL)
            return type_check_block_flow(node->data.defer_stmt.body, ctx, loop_flow);
        return FLOW_FALLTHROUGH;
    case AST_RETURN:
        type_check_return_stmt(node, ctx);
        return FLOW_RETURN;
    case AST_BREAK:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'break' used outside of loop");
            return FLOW_NONE;
        }
        {
            QubitConsumeSnapshot snap = snapshot_qubit_states(ctx);
            loop_flow_record(loop_flow, true, &snap);
            destroy_qubit_snapshot(&snap);
        }
        return FLOW_BREAK;
    case AST_CONTINUE:
        if (ctx->loop_depth <= 0) {
            semantic_error(ctx, node, "'continue' used outside of loop");
            return FLOW_NONE;
        }
        {
            QubitConsumeSnapshot snap = snapshot_qubit_states(ctx);
            loop_flow_record(loop_flow, false, &snap);
            destroy_qubit_snapshot(&snap);
        }
        return FLOW_CONTINUE;
    default:
        type_check_statement(node, ctx);
        return FLOW_FALLTHROUGH;
    }
}

bool
type_check_block(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL)
        return true;

    (void)type_check_block_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_if_stmt(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_if_stmt_flow(node, ctx, NULL);
    return !ctx->has_error;
}

bool
type_check_for_loop(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    ctx->loop_depth++;

    Symbol *loop_var = symbol_create_variable(
        node->data.for_loop.variable, TYPE_INT, node->line, node->column);
    scope_declare(ctx->scope, loop_var);

    if (node->data.for_loop.range_start != NULL) {
        Type *t = type_check_expression(node->data.for_loop.range_start, ctx);
        require_assignable(t, TYPE_INT, node->data.for_loop.range_start, ctx);
    }
    if (node->data.for_loop.range_end != NULL) {
        Type *t = type_check_expression(node->data.for_loop.range_end, ctx);
        require_assignable(t, TYPE_INT, node->data.for_loop.range_end, ctx);
    }

    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot merged = copy_qubit_snapshot(&base);
    QubitConsumeSnapshot entry = copy_qubit_snapshot(&base);
    bool known_iterations = false;
    size_t known_cap = for_loop_known_iteration_cap(node, &known_iterations);
    size_t max_iterations = (known_iterations && known_cap <= 1)
        ? 1
        : (base.count + 1);
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        QubitConsumeSnapshot backedge = {0};
        bool has_backedge = false;

        restore_qubit_states(&entry);
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        {
            FlowFlags body_flags =
                type_check_block_flow(node->data.for_loop.body, ctx, &loop_flow);
            if (body_flags & FLOW_FALLTHROUGH) {
                QubitConsumeSnapshot body_snap = snapshot_qubit_states(ctx);
                merge_qubit_states_or(&merged, &body_snap);
                merge_qubit_snapshots_or(&backedge, &has_backedge, &body_snap);
                destroy_qubit_snapshot(&body_snap);
            }
        }
        scope_exit(&ctx->scope);

        if (loop_flow.has_continue_states)
            merge_qubit_snapshots_or(&backedge, &has_backedge,
                                     &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_qubit_states_or(&merged, &loop_flow.break_states);

        destroy_loop_flow_state(&loop_flow);

        if (!has_backedge) {
            destroy_qubit_snapshot(&backedge);
            break;
        }

        if (qubit_snapshots_equal(&entry, &backedge)) {
            destroy_qubit_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_qubit_snapshot(&entry);
        entry = backedge;
    }

    ctx->loop_depth--;
    scope_exit(&ctx->scope);

    restore_qubit_states(&merged);
    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&merged);
    destroy_qubit_snapshot(&entry);
    return !ctx->has_error;
}

bool
type_check_while_loop(ASTNode *node, SemanticContext *ctx)
{
    scope_enter(&ctx->scope, SCOPE_BLOCK);
    ctx->loop_depth++;

    QubitConsumeSnapshot base = snapshot_qubit_states(ctx);
    QubitConsumeSnapshot merged = copy_qubit_snapshot(&base);
    QubitConsumeSnapshot entry = copy_qubit_snapshot(&base);
    size_t max_iterations = base.count + 1;
    if (max_iterations == 0)
        max_iterations = 1;

    for (size_t iter = 0; iter < max_iterations; iter++) {
        LoopFlowState loop_flow = {0};
        QubitConsumeSnapshot backedge = {0};
        bool has_backedge = false;

        restore_qubit_states(&entry);
        Type *cond = type_check_expression(node->data.while_loop.condition, ctx);
        if (!type_equals(cond, TYPE_BOOL)) {
            semantic_error(ctx, node,
                "While condition must be Bool, got '%s'", cond->name);
        }

        scope_enter(&ctx->scope, SCOPE_BLOCK);
        {
            FlowFlags body_flags =
                type_check_block_flow(node->data.while_loop.body, ctx, &loop_flow);
            if (body_flags & FLOW_FALLTHROUGH) {
                QubitConsumeSnapshot body_snap = snapshot_qubit_states(ctx);
                merge_qubit_states_or(&merged, &body_snap);
                merge_qubit_snapshots_or(&backedge, &has_backedge, &body_snap);
                destroy_qubit_snapshot(&body_snap);
            }
        }
        scope_exit(&ctx->scope);

        if (loop_flow.has_continue_states)
            merge_qubit_snapshots_or(&backedge, &has_backedge,
                                     &loop_flow.continue_states);
        if (loop_flow.has_break_states)
            merge_qubit_states_or(&merged, &loop_flow.break_states);

        destroy_loop_flow_state(&loop_flow);

        if (!has_backedge) {
            destroy_qubit_snapshot(&backedge);
            break;
        }

        if (qubit_snapshots_equal(&entry, &backedge)) {
            destroy_qubit_snapshot(&entry);
            entry = backedge;
            break;
        }

        destroy_qubit_snapshot(&entry);
        entry = backedge;
    }

    ctx->loop_depth--;
    scope_exit(&ctx->scope);

    restore_qubit_states(&merged);
    destroy_qubit_snapshot(&base);
    destroy_qubit_snapshot(&merged);
    destroy_qubit_snapshot(&entry);
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
