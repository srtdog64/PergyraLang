#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "type_checker_flow_internal.h"
#include "type_checker_flow_effects.h"
#include "type_checker_flow_loops.h"
#include "../common/numeric_parse.h"

/* PGY_DEBUG_SEMANTIC_TIMING statement census: per-kind INCLUSIVE time for
 * body statements (parents such as blocks/ifs double-count their children;
 * leaf statement kinds read as self-time). Mirrors the expression census in
 * type_checker_expr.c. */
#ifdef _WIN32
#include <windows.h>
#endif

typedef struct
{
    size_t visits_by_kind[512];
    double seconds_by_kind[512];
} StmtVisitCensus;

static StmtVisitCensus *
stmt_visit_census(void)
{
    static _Thread_local StmtVisitCensus census;
    return &census;
}

static double
stmt_census_now(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

static bool
stmt_census_enabled(void)
{
    static _Thread_local int enabled = -1;

    if (enabled < 0)
        enabled = getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL ? 1 : 0;
    return enabled == 1;
}

void
type_check_stmt_debug_visit_report(void)
{
    StmtVisitCensus *census = stmt_visit_census();

    if (!stmt_census_enabled())
        return;
    for (size_t i = 0; i < 512; i++) {
        if (census->visits_by_kind[i] > 5000
            || census->seconds_by_kind[i] > 0.2) {
            fprintf(stderr,
                    "[semantic timing]   stmt kind=%zu visits=%zu"
                    " inclusive=%.3fs\n",
                    i, census->visits_by_kind[i], census->seconds_by_kind[i]);
        }
    }
}

FlowFlags
flow_terminating_flags(FlowFlags flags)
{
    return flags & (FLOW_BREAK | FLOW_CONTINUE | FLOW_RETURN);
}

FlowFlags
flow_record_statement_result(FlowFlags current, FlowFlags statement)
{
    current &= ~FLOW_FALLTHROUGH;
    current |= statement & (FLOW_FALLTHROUGH
                          | FLOW_BREAK
                          | FLOW_CONTINUE
                          | FLOW_RETURN
                          | FLOW_HAS_DEFER);
    return current;
}

bool
flow_has_fallthrough(FlowFlags flags)
{
    return (flags & FLOW_FALLTHROUGH) != 0;
}

Type *
flow_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

Type *
flow_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    Type *resolved;

    if (type_ref == NULL || ctx == NULL)
        return TYPE_UNKNOWN;
    resolved = semantic_type_resolution_lookup_metadata_type_ref(ctx, type_ref);
    return flow_normalize_type(resolved);
}

bool
flow_condition_is_static_bool(const ASTNode *node)
{
    return node != NULL && node->type == AST_BOOLEAN;
}

bool
flow_static_bool_value(const ASTNode *node, bool *value_out)
{
    if (!flow_condition_is_static_bool(node))
        return false;
    if (value_out != NULL)
        *value_out = ast_boolean_value(node);
    return true;
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

/* =================================================================
 * Semantic termination contract (docs/189 C8).
 *
 * The parser caps recursion at 400 and operators at 4096, but semantic
 * analysis had no bound of any kind: the census below is an env-gated
 * print-only instrument, and the 2026-07 escape-analysis hang proved the
 * "semantic must terminate on every input" contract can break in
 * practice. Two deterministic backstops close the class fail-closed:
 *
 * - a statement-flow depth cap (512 -- above the parser's 400, so any
 *   parser-accepted input can never trip it; it only fires for a
 *   non-parser AST producer or a future transform that deepens trees);
 * - a global step budget over statement-flow + expression dispatches
 *   (default 1<<28 steps, PGY_SEMANTIC_STEP_BUDGET overrides, 0
 *   disables). Deterministic by construction -- unlike a wall-clock
 *   watchdog, the same input always trips at the same step.
 *
 * Once tripped, every subsequent dispatch returns immediately so the
 * recursion unwinds and the compile ends with a diagnostic instead of a
 * hang. Thread-local so the LSP's per-request analyses stay independent.
 * Honest coverage note: subsystems that do not route through these two
 * dispatchers (e.g. the escape analyzer's own walks) are outside this
 * budget; their guards live with them.
 * ================================================================= */

#define SEMANTIC_STMT_FLOW_MAX_DEPTH 512
#define SEMANTIC_STEP_BUDGET_DEFAULT ((size_t)1 << 28)

typedef struct
{
    size_t stmt_flow_depth;
    size_t steps_used;
    size_t step_budget;
    bool contract_tripped;
} SemanticTerminationContractState;

static SemanticTerminationContractState *
semantic_termination_contract_state(void)
{
    static _Thread_local SemanticTerminationContractState state;
    return &state;
}

void
semantic_termination_contract_reset(void)
{
    const char *env = getenv("PGY_SEMANTIC_STEP_BUDGET");
    SemanticTerminationContractState *state =
        semantic_termination_contract_state();
    size_t configured_budget;

    state->stmt_flow_depth = 0;
    state->steps_used = 0;
    state->contract_tripped = false;
    state->step_budget = SEMANTIC_STEP_BUDGET_DEFAULT;
    if (env != NULL && env[0] != '\0'
        && pgy_parse_size_strict_allow_zero(env, &configured_budget))
        state->step_budget = configured_budget;
}

bool
semantic_termination_contract_tick(SemanticContext *ctx, ASTNode *site)
{
    SemanticTerminationContractState *state =
        semantic_termination_contract_state();

    if (state->contract_tripped)
        return false;
    if (state->step_budget == 0)
        return true;
    if (++state->steps_used <= state->step_budget)
        return true;
    state->contract_tripped = true;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_SEMANTIC_STEP_BUDGET,
        PGY_FIX_REPORT_INPUT_OR_RAISE_STEP_BUDGET,
        site,
        "semantic analysis exceeded its termination-contract step budget (%llu steps).\n"
        "Reason:\n"
        "- the compiler must terminate on every input; this deterministic budget is the fail-closed backstop for analysis blow-ups\n"
        "Fix:\n"
        "- report this input as a compiler performance defect\n"
        "- or raise PGY_SEMANTIC_STEP_BUDGET (0 disables) if the program is legitimately this large",
        (unsigned long long)state->step_budget);
    return false;
}

static bool
semantic_stmt_flow_depth_enter(SemanticContext *ctx, ASTNode *site)
{
    SemanticTerminationContractState *state =
        semantic_termination_contract_state();

    if (state->contract_tripped)
        return false;
    if (++state->stmt_flow_depth <= SEMANTIC_STMT_FLOW_MAX_DEPTH)
        return true;
    state->stmt_flow_depth--;
    state->contract_tripped = true;
    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_TYPE_MISMATCH,
        PGY_CAUSE_SEMANTIC_STEP_BUDGET,
        PGY_FIX_REPORT_INPUT_OR_RAISE_STEP_BUDGET,
        site,
        "semantic statement nesting exceeded the analyzer's depth cap (%d).\n"
        "Reason:\n"
        "- the parser caps nesting at 400, so parser-accepted programs cannot reach this; an AST this deep indicates a non-parser producer or a compiler defect\n"
        "Fix:\n"
        "- flatten the nesting, or report this input as a compiler defect",
        SEMANTIC_STMT_FLOW_MAX_DEPTH);
    return false;
}

static void
semantic_stmt_flow_depth_leave(void)
{
    SemanticTerminationContractState *state =
        semantic_termination_contract_state();
    if (state->stmt_flow_depth > 0)
        state->stmt_flow_depth--;
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
    for (size_t i = 0; i < ast_block_statement_count(node); i++) {
        if (!flow_has_fallthrough(flags)) {
            flow_record_unreachable_statement(ctx, ast_block_statement(node, i));
            break;
        }

        FlowFlags stmt_flags =
            type_check_statement_flow(ast_block_statement(node, i), ctx, loop_flow);

        flags = flow_record_statement_result(flags, stmt_flags);
    }

    return flags;
}

static FlowFlags
type_check_statement_flow_dispatch(ASTNode *node, SemanticContext *ctx,
                                   LoopFlowState *loop_flow)
{
    switch (node->type) {
    case AST_BLOCK: {
        FlowFlags flags;
        scope_enter(&ctx->scope, SCOPE_BLOCK);
        flags = type_check_block_flow(node, ctx, loop_flow);
        semantic_future_require_scope_retired(
            ctx->scope, node, ctx, "block exit");
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
        return type_check_parallel_stmt_flow(node, ctx);
    case AST_UNSAFE_BLOCK:
        return type_check_unsafe_block_flow(node, ctx, loop_flow);
    case AST_TRANSACTION_BLOCK: {
        ASTNode *txn_body = ast_transaction_block_body(node);
        FlowFlags txn_flags;

        if (txn_body == NULL)
            return FLOW_FALLTHROUGH;
        ctx->transaction_depth++;
        txn_flags = type_check_block_flow(txn_body, ctx, loop_flow);
        ctx->transaction_depth--;
        return txn_flags;
    }
    case AST_FAIL_STMT: {
        ASTNode *fail_reason = ast_fail_stmt_reason(node);

        if (ctx != NULL && ctx->transaction_depth <= 0 && !ctx->has_error)
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_TRANSACTION_CONTROL_INVALID,
                PGY_CAUSE_TRANSACTION_CONTROL,
                PGY_FIX_MOVE_INTO_TRANSACTION, node,
                "'fail' used outside of transaction");
        if (fail_reason != NULL)
            type_check_expression(fail_reason, ctx);
        return FLOW_FALLTHROUGH;
    }
    case AST_DEFER_STMT:
        return type_check_defer_stmt_flow(node, ctx);
    case AST_ASYNC_BLOCK:
        return type_check_async_stmt_flow(node, ctx);
    case AST_SELECT_STMT:
        return type_check_select_stmt_flow_kind(node, ctx);
    case AST_LET_DECL:
        return type_check_let_stmt_flow(node, ctx);
    case AST_LET_DESTRUCTURE:
        return type_check_destructure_stmt_flow(node, ctx);
    case AST_RETURN:
        return type_check_return_stmt_flow(node, ctx);
    case AST_GIVE_STMT:
        return type_check_give_stmt_flow(node, ctx);
    case AST_BREAK:
        return type_check_loop_control_flow(node, ctx, loop_flow, true);
    case AST_CONTINUE:
        return type_check_loop_control_flow(node, ctx, loop_flow, false);
    case AST_EVENT_SUBSCRIBE:
        return type_check_event_stmt_flow(node, ctx, "subscription");
    case AST_EVENT_UNSUBSCRIBE:
        return type_check_event_stmt_flow(node, ctx, "unsubscription");
    case AST_EVENT_INVOKE:
        return type_check_event_invoke_stmt_flow(node, ctx);
    case AST_USE_DECL:
        return type_check_use_stmt_flow(node, ctx);
    case AST_NAMESPACE_DECL:
        return type_check_namespace_flow(node, ctx, loop_flow);
    case AST_BIND_STMT:
        type_check_bind_stmt(node, ctx);
        return FLOW_FALLTHROUGH;
    case AST_IMPORT_DECL:
        return FLOW_FALLTHROUGH;
    default:
        type_check_expression(node, ctx);
        return FLOW_FALLTHROUGH;
    }
}

FlowFlags
type_check_statement_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    double t0;
    FlowFlags flags;

    if (node == NULL)
        return FLOW_FALLTHROUGH;
    if (!semantic_termination_contract_tick(ctx, node))
        return FLOW_FALLTHROUGH;
    if (!semantic_stmt_flow_depth_enter(ctx, node))
        return FLOW_FALLTHROUGH;
    if (!stmt_census_enabled()) {
        flags = type_check_statement_flow_dispatch(node, ctx, loop_flow);
        semantic_stmt_flow_depth_leave();
        return flags;
    }
    StmtVisitCensus *census = stmt_visit_census();

    if ((size_t)node->type < 512)
        census->visits_by_kind[(size_t)node->type]++;
    t0 = stmt_census_now();
    flags = type_check_statement_flow_dispatch(node, ctx, loop_flow);
    if ((size_t)node->type < 512)
        census->seconds_by_kind[(size_t)node->type] += stmt_census_now() - t0;
    semantic_stmt_flow_depth_leave();
    return flags;
}

FlowFlags
type_check_statement_flow_boundary(ASTNode *node, SemanticContext *ctx)
{
    return type_check_statement_flow(node, ctx, NULL);
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
semantic_check_body_flow_summary(ASTNode *body,
                                 SemanticContext *ctx,
                                 SemanticBodyFlowSummary *summary_out)
{
    FlowFlags flags = type_check_block_flow(body, ctx, NULL);
    if (summary_out != NULL) {
        summary_out->has_fallthrough = flow_has_fallthrough(flags);
        summary_out->has_return = (flags & FLOW_RETURN) != 0;
        summary_out->has_break = (flags & FLOW_BREAK) != 0;
        summary_out->has_continue = (flags & FLOW_CONTINUE) != 0;
        summary_out->has_defer = (flags & FLOW_HAS_DEFER) != 0;
        summary_out->must_return =
            summary_out->has_return && !summary_out->has_fallthrough;
    }
    return !ctx->has_error;
}

bool
semantic_check_body_flow(ASTNode *body, SemanticContext *ctx,
                         bool *must_return_out)
{
    SemanticBodyFlowSummary summary = {0};
    bool ok = semantic_check_body_flow_summary(body, ctx, &summary);
    if (must_return_out != NULL)
        *must_return_out = summary.must_return;
    return ok;
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
