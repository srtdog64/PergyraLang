#include "collection_ownership_fact_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

bool
collection_call_is_unshadowed_builtin(const ASTNode *expr,
                                      const char *expected_name)
{
    ASTNode *callee;
    const char *name;

    if (expr == NULL || expr->type != AST_CALL || expected_name == NULL)
        return false;
    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    return name != NULL && strcmp(name, expected_name) == 0
        && ast_identifier_binding_syntax_id(callee) == 0;
}

static ASTNode *
collection_direct_let_for_binding(ASTNode *body, uint32_t binding_syntax_id)
{
    if (body == NULL || body->type != AST_BLOCK || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt != NULL && stmt->type == AST_LET_DECL
            && ast_node_stable_id(stmt) == binding_syntax_id)
            return stmt;
    }
    return NULL;
}

static bool
collection_direct_result_allocator_binding(ASTNode *body,
                                           uint32_t binding_syntax_id)
{
    ASTNode *decl = collection_direct_let_for_binding(
        body, binding_syntax_id);
    return decl != NULL && !ast_let_is_mutable(decl)
        && collection_call_is_unshadowed_builtin(
            ast_let_initializer(decl), "AllocatorResult");
}

static bool
collection_callable_type_returns_owned_string(const ASTNode *call,
                                              SemanticContext *ctx,
                                              uint32_t *producer_id_out)
{
    ASTNode *callee;
    ASTNode *decl;
    Symbol *symbol;
    const char *name;
    uint32_t producer_id;

    if (producer_id_out != NULL)
        *producer_id_out = 0;
    if (call == NULL || call->type != AST_CALL || ctx == NULL)
        return false;
    producer_id = ast_call_semantic_callee_decl_id(call);
    callee = ast_call_callee(call);
    name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    if (producer_id == 0 || name == NULL)
        return false;
    decl = semantic_find_callable_decl_by_name(ctx, name);
    symbol = scope_lookup(ctx->scope, name);
    if (decl == NULL || decl->type != AST_FUNC_DECL
        || ast_node_stable_id(decl) != producer_id
        || symbol == NULL || symbol->kind != SYMBOL_FUNCTION
        || symbol->decl_syntax_id != producer_id
        || symbol->type == NULL || symbol->type->kind != TYPE_KIND_FUNCTION
        || !type_function_has_body_summary(symbol->type)
        || !type_equals(type_function_return_type(symbol->type), TYPE_STRING)
        || (type_function_body_summary(symbol->type)
            & BODY_SUMMARY_RETURNS_OWNED_STRING) == 0) {
        return false;
    }
    if (producer_id_out != NULL)
        *producer_id_out = producer_id;
    return true;
}

static bool
collection_owned_string_expression_ready(ASTNode *body,
                                         const ASTNode *expression,
                                         SemanticContext *ctx,
                                         unsigned depth)
{
    ASTNode *decl;
    ASTNode *allocator;
    uint32_t binding_id;

    if (body == NULL || expression == NULL || depth > 8)
        return false;
    if (expression->type == AST_IDENTIFIER) {
        binding_id = ast_identifier_binding_syntax_id(expression);
        decl = collection_direct_let_for_binding(body, binding_id);
        /* A named initializer is only a straight-line provenance proof.
         * Control-dependent rebinding needs a value fact, not the old let. */
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            ASTNode *stmt = ast_block_statement(body, i);
            if (stmt == NULL)
                return false;
            if (stmt->type == AST_ASSIGNMENT) {
                ASTNode *target = ast_assignment_target(stmt);
                if (target != NULL && target->type == AST_IDENTIFIER
                    && ast_identifier_binding_syntax_id(target) == binding_id)
                    return false;
            } else if (stmt->type != AST_RETURN && stmt->type != AST_LET_DECL
                       && stmt->type != AST_CALL
                       && stmt->type != AST_DEFER_STMT) {
                return false;
            }
        }
        return decl != NULL && !ast_let_is_mutable(decl)
            && collection_owned_string_expression_ready(
                body, ast_let_initializer(decl), ctx, depth + 1);
    }
    if (expression->type != AST_CALL)
        return false;
    if (collection_call_is_unshadowed_builtin(expression, "Concat"))
        return ast_call_arg_count(expression) == 2;
    if (collection_call_is_unshadowed_builtin(
            expression, "TextBuilderFinish")) {
        if (ast_call_arg_count(expression) != 2)
            return false;
        allocator = ast_call_argument(expression, 1);
        return allocator != NULL && allocator->type == AST_IDENTIFIER
            && collection_direct_result_allocator_binding(
                body, ast_identifier_binding_syntax_id(allocator));
    }
    return collection_callable_type_returns_owned_string(
        expression, ctx, NULL);
}

bool
semantic_collection_owned_string_call_result(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out)
{
    return collection_callable_type_returns_owned_string(
        expression, ctx, producer_syntax_id_out);
}

static bool
collection_owned_string_returns_ready(ASTNode *node, ASTNode *function_body,
                                      SemanticContext *ctx,
                                      bool *saw_return, unsigned depth)
{
    if (node == NULL)
        return true;
    if (depth > 8)
        return false;
    if (node->type == AST_RETURN) {
        *saw_return = true;
        return collection_owned_string_expression_ready(
            function_body, ast_return_value(node), ctx, 0);
    }
    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            ASTNode *stmt = ast_block_statement(node, i);
            if (stmt == NULL || !collection_owned_string_returns_ready(
                    stmt, function_body, ctx, saw_return, depth + 1)) {
                return false;
            }
        }
        return true;
    }
    if (node->type == AST_IF_STMT) {
        return collection_owned_string_returns_ready(
                   ast_if_then_branch(node), function_body, ctx,
                   saw_return, depth + 1)
            && collection_owned_string_returns_ready(
                   ast_if_else_branch(node), function_body, ctx,
                   saw_return, depth + 1);
    }
    /* Lambda bodies are not enclosing-function returns. Other control owners
     * remain unadmitted until their return facts are explicitly supported. */
    return node->type == AST_LET_DECL || node->type == AST_CALL
        || node->type == AST_ASSIGNMENT || node->type == AST_DEFER_STMT;
}

void
semantic_collection_record_owned_string_result_summary(
    ASTNode *function_decl,
    SemanticContext *ctx)
{
    bool saw_return = false;
    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL
        || ctx == NULL || !ctx->tracking_function_effects
        || !type_equals(ctx->current_return, TYPE_STRING)) {
        return;
    }
    ASTNode *body = ast_func_body(function_decl);
    if (body != NULL && body->type == AST_BLOCK
        && collection_owned_string_returns_ready(
            body, body, ctx, &saw_return, 0) && saw_return) {
        semantic_record_body_summary(
            ctx, BODY_SUMMARY_RETURNS_OWNED_STRING);
    }
}
