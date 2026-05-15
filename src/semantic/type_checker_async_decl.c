#include "type_checker_internal.h"
#include "type_checker_flow_internal.h"
#include "diag_codes.h"

static Type *
async_decl_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

bool
type_check_async_block(ASTNode *node, SemanticContext *ctx)
{
    bool saved_async = ctx->in_async_func;

    if (semantic_reject_active_slot_view_boundary(node, ctx,
            "async block boundary",
            "async block execution may resume after the current synchronous frame advances",
            "move async block")) {
        return false;
    }

    ctx->in_async_func = true;

    for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
        (void)type_check_statement_flow_boundary(
            ast_async_block_statement(node, i), ctx);
    }

    ctx->in_async_func = saved_async;
    return !ctx->has_error;
}

bool
type_check_select_stmt(ASTNode *node, SemanticContext *ctx)
{
    for (size_t i = 0; i < ast_select_case_count(node); i++) {
        ASTNode *c = ast_select_case(node, i);
        if (c != NULL) {
            bool valid_case = false;
            if (c->type == AST_BLOCK && ast_block_statement_count(c) > 0) {
                ASTNode *first = ast_block_statement(c, 0);
                ASTNode *recv_expr = NULL;
                const char *bind_name = NULL;

                if (first != NULL && first->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    recv_expr = first;
                } else if (first != NULL && first->type == AST_ASSIGNMENT
                           && ast_assignment_target(first) != NULL
                           && ast_assignment_target(first)->type == AST_IDENTIFIER
                           && ast_assignment_value(first) != NULL
                           && ast_assignment_value(first)->type == AST_CHANNEL_RECV) {
                    valid_case = true;
                    bind_name = ast_identifier_name(ast_assignment_target(first));
                    recv_expr = ast_assignment_value(first);
                }

                if (!valid_case) {
                    semantic_error_with_hints(ctx, PGY_CODE_SEM_SELECT_CASE_INVALID, PGY_CAUSE_SELECT_CASE_SHAPE, PGY_FIX_START_WITH_CHANNEL_RECV, c,
                        "select case must begin with a channel receive pattern");
                    (void)type_check_statement_flow_boundary(c, ctx);
                    continue;
                }

                scope_enter(&ctx->scope, SCOPE_BLOCK);
                if (recv_expr != NULL) {
                    Type *recv_type = async_decl_normalize_type(
                        type_check_expression(recv_expr, ctx));
                    if (bind_name != NULL) {
                        Symbol *binding = symbol_create_variable(
                            bind_name, recv_type, first->line, first->column);
                        scope_declare(ctx->scope, binding);
                    }
                }
                for (size_t j = 1; j < ast_block_statement_count(c); j++)
                    (void)type_check_statement_flow_boundary(
                        ast_block_statement(c, j), ctx);
                scope_exit(&ctx->scope);
            } else {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_SELECT_CASE_INVALID, PGY_CAUSE_SELECT_CASE_SHAPE, PGY_FIX_START_WITH_CHANNEL_RECV, c,
                    "select case must begin with a channel receive pattern");
                (void)type_check_statement_flow_boundary(c, ctx);
            }
        }
    }

    if (ast_select_default_case(node))
        (void)type_check_statement_flow_boundary(
            ast_select_default_case(node), ctx);

    return !ctx->has_error;
}
