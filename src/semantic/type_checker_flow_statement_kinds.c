#include "type_checker_internal.h"
#include "type_checker_flow_internal.h"
#include "type_checker_flow_effects.h"
#include "diag_codes.h"

FlowFlags
type_check_with_stmt_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    ASTNode *slot_type_node;
    const char *alias;
    bool is_secure;
    Type *inner;
    Type *slot_type;
    Symbol *sym;
    FlowFlags flags;

    if (node == NULL || node->type != AST_WITH_STMT || ctx == NULL)
        return FLOW_FALLTHROUGH;

    scope_enter(&ctx->scope, SCOPE_WITH);

    slot_type_node = ast_with_slot_type(node);
    alias = ast_with_alias(node);
    is_secure = ast_with_is_secure(node);
    inner = flow_resolve_type_ref(slot_type_node, ctx);
    slot_type = type_create_slot(inner, is_secure);

    sym = symbol_create_slot(alias, slot_type, is_secure, NULL,
                             node->line, node->column);
    scope_declare(ctx->scope, sym);
    scope_register_slot(ctx->scope, sym);

    flags = type_check_block_flow(ast_with_body(node), ctx, loop_flow);

    scope_auto_release_slots(ctx->scope);
    semantic_future_require_scope_retired(
        ctx->scope, node, ctx, "with scope exit");
    scope_exit(&ctx->scope);
    return flags;
}

FlowFlags
type_check_unsafe_block_flow(ASTNode *node, SemanticContext *ctx,
                             LoopFlowState *loop_flow)
{
    ASTNode *body;

    if (node == NULL || node->type != AST_UNSAFE_BLOCK)
        return FLOW_FALLTHROUGH;

    semantic_record_effect(ctx, EFFECT_UNSAFE);
    if (ctx != NULL && ctx->in_parallel) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_PARALLEL_SECURE_FORBIDDEN,
            PGY_CAUSE_PARALLEL_SECURE_IN_TASK,
            PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL,
            node,
            "Unsafe block is not permitted inside a parallel task; raw memory "
            "operations across concurrent tasks compound undefined behavior. "
            "Serialize the unsafe work outside the parallel block.");
        return FLOW_FALLTHROUGH;
    }

    body = ast_unsafe_block_body(node);
    return body != NULL
        ? type_check_block_flow(body, ctx, loop_flow)
        : FLOW_FALLTHROUGH;
}

FlowFlags
type_check_defer_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || node->type != AST_DEFER_STMT)
        return FLOW_FALLTHROUGH;

    (void)type_check_defer_body_flow(ast_defer_body(node), ctx);
    return FLOW_FALLTHROUGH | FLOW_HAS_DEFER;
}

FlowFlags
type_check_namespace_flow(ASTNode *node, SemanticContext *ctx,
                          LoopFlowState *loop_flow)
{
    FlowFlags flags = FLOW_FALLTHROUGH;

    if (node == NULL || node->type != AST_NAMESPACE_DECL)
        return flags;

    for (size_t i = 0; i < ast_namespace_statement_count(node); i++) {
        ASTNode *stmt = ast_namespace_statement(node, i);
        if (!flow_has_fallthrough(flags)) {
            flow_record_unreachable_statement(ctx, stmt);
            break;
        }
        flags = flow_record_statement_result(
            flags,
            type_check_statement_flow(stmt, ctx, loop_flow));
    }
    return flags;
}

FlowFlags
type_check_parallel_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_parallel_block_flow(node, ctx);
    return FLOW_FALLTHROUGH;
}

FlowFlags
type_check_async_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_async_block(node, ctx);
    return FLOW_FALLTHROUGH;
}

FlowFlags
type_check_select_stmt_flow_kind(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_select_stmt(node, ctx);
    return FLOW_FALLTHROUGH;
}

FlowFlags
type_check_let_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_let_decl(node, ctx);
    return FLOW_FALLTHROUGH;
}

FlowFlags
type_check_destructure_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_let_destructure_stmt(node, ctx);
    return FLOW_FALLTHROUGH;
}

FlowFlags
type_check_return_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_return_stmt(node, ctx);
    return FLOW_RETURN;
}

FlowFlags
type_check_event_stmt_flow(ASTNode *node, SemanticContext *ctx,
                           const char *event_kind)
{
    (void)type_check_event_subscription(node, ctx, event_kind);
    return FLOW_FALLTHROUGH;
}

FlowFlags
type_check_event_invoke_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_event_invoke_stmt(node, ctx);
    return FLOW_FALLTHROUGH;
}

FlowFlags
type_check_use_stmt_flow(ASTNode *node, SemanticContext *ctx)
{
    (void)type_check_use_decl(node, ctx);
    return FLOW_FALLTHROUGH;
}
