#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "type_checker_flow_internal.h"
#include "type_checker_flow_effects.h"
#include "type_checker_flow_loops.h"

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

FlowFlags
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
        return type_check_parallel_stmt_flow(node, ctx);
    case AST_UNSAFE_BLOCK:
        return type_check_unsafe_block_flow(node, ctx, loop_flow);
    case AST_TRANSACTION_BLOCK: {
        ASTNode *txn_body = ast_transaction_block_body(node);
        return txn_body != NULL
            ? type_check_block_flow(txn_body, ctx, loop_flow)
            : FLOW_FALLTHROUGH;
    }
    case AST_FAIL_STMT: {
        ASTNode *fail_reason = ast_fail_stmt_reason(node);
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
