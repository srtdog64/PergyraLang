#include "type_checker_internal.h"
#include "type_checker_flow_internal.h"
#include "diag_codes.h"
#include "../parser/ast_analysis.h"

static Type *
async_decl_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static bool
async_symbol_is_local_storage(const Symbol *sym)
{
    return sym != NULL
        && (sym->kind == SYMBOL_VARIABLE
            || sym->kind == SYMBOL_SLOT
            || sym->kind == SYMBOL_TOKEN);
}

static bool
semantic_reject_detached_async_capture(ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL || ctx == NULL)
        return false;

    for (Scope *scope = ctx->scope; scope != NULL; scope = scope->parent) {
        if (scope->kind == SCOPE_GLOBAL)
            continue;
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *sym = scope->symbols[i];
            if (!async_symbol_is_local_storage(sym)
                || sym->name == NULL
                || !ast_contains_free_identifier_ref(node, sym->name)) {
                continue;
            }
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_MOVE_INTO_ASYNC_FUNCTION,
                node,
                "Detached async block cannot capture local '%s' by pointer.\n"
                "Reason:\n"
                "- async block may continue after the current synchronous frame advances\n"
                "- beta detached async has no closed closure-capture ownership model yet\n"
                "- generated C/LLVM must not depend on shallow-copied local storage\n"
                "Fix:\n"
                "- move the body into a named async function with explicit parameters\n"
                "- or pass a copied value through an explicit handoff boundary",
                sym->name);
            return true;
        }
    }
    return false;
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
    if (semantic_reject_detached_async_capture(node, ctx))
        return false;

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
                semantic_future_require_scope_retired(
                    ctx->scope, c, ctx, "select case exit");
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
