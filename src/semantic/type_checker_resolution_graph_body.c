#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
resolution_body_strdup_fmt(const char *fmt, ...)
{
    va_list ap;
    va_list ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

static void
semantic_type_resolution_precollect_expr_type_refs(ASTNode *expr,
                                                   SemanticContext *ctx,
                                                   const ASTNode *owner,
                                                   const char *owner_name)
{
    if (expr == NULL || ctx == NULL)
        return;

    switch (expr->type) {
    case AST_LAMBDA_EXPR:
        for (size_t i = 0; i < ast_lambda_param_count(expr); i++) {
            ASTNode *param = ast_lambda_param(expr, i);
            char *consumer_name;

            if (param == NULL || param->type != AST_LET_DECL)
                continue;

            consumer_name = resolution_body_strdup_fmt(
                "lambda %s.%s",
                owner_name != NULL ? owner_name : "<lambda>",
                ast_let_name(param) != NULL
                    ? ast_let_name(param) : "<param>");
            if (consumer_name != NULL) {
                semantic_type_resolution_collect_type_refs(
                    ast_let_type(param),
                    ctx,
                    param,
                    consumer_name,
                    "lambda parameter type lookup");
                free(consumer_name);
            }
        }
        semantic_type_resolution_collect_type_refs(
            ast_lambda_return_type(expr),
            ctx,
            expr,
            owner_name != NULL ? owner_name : "<lambda>",
            "lambda return type lookup");
        if (ast_lambda_body(expr) != NULL
            && ast_lambda_body(expr)->type == AST_BLOCK) {
            semantic_type_resolution_precollect_body_type_refs(
                ast_lambda_body(expr), ctx, owner, owner_name);
        } else {
            semantic_type_resolution_precollect_expr_type_refs(
                ast_lambda_body(expr), ctx, owner, owner_name);
        }
        return;

    case AST_CALL:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_call_callee(expr), ctx, owner, owner_name);
        for (size_t i = 0; i < ast_call_arg_count(expr); i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                ast_call_argument(expr, i), ctx, owner, owner_name);
        }
        if (ast_call_generic_args(expr) != NULL) {
            for (size_t i = 0; i < ast_call_generic_arg_count(expr); i++) {
                GenericParam *arg = ast_call_generic_arg(expr, i);
                ASTNode *constraint = ast_generic_param_constraint(arg);
                if (constraint != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        constraint,
                        ctx,
                        expr,
                        owner_name != NULL ? owner_name : "<call>",
                        "call type-argument lookup");
                }
            }
        }
        return;

    case AST_BINARY:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_binary_left(expr), ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            ast_binary_right(expr), ctx, owner, owner_name);
        return;

    case AST_UNARY:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_unary_operand(expr), ctx, owner, owner_name);
        return;

    case AST_MEMBER_ACCESS:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_member_object(expr), ctx, owner, owner_name);
        return;

    case AST_ARRAY_ACCESS:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_array_access_array(expr), ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            ast_array_access_index(expr), ctx, owner, owner_name);
        return;

    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(expr); i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                ast_array_literal_element(expr, i), ctx, owner, owner_name);
        }
        return;

    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(expr); i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                ast_tuple_literal_element(expr, i), ctx, owner, owner_name);
        }
        return;

    case AST_ASSIGNMENT:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_assignment_target(expr), ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            ast_assignment_value(expr), ctx, owner, owner_name);
        return;

    case AST_AWAIT_EXPR:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_await_expression(expr), ctx, owner, owner_name);
        return;

    case AST_CHANNEL_SEND:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_channel_send_channel(expr), ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            ast_channel_send_value(expr), ctx, owner, owner_name);
        return;

    case AST_CHANNEL_RECV:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_channel_recv_channel(expr), ctx, owner, owner_name);
        return;

    case AST_SPAWN_EXPR:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_spawn_function(expr), ctx, owner, owner_name);
        for (size_t i = 0; i < ast_spawn_arg_count(expr); i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                ast_spawn_argument(expr, i), ctx, owner, owner_name);
        }
        return;

    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_event_op_event(expr), ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            ast_event_op_handler(expr), ctx, owner, owner_name);
        return;

    case AST_EVENT_INVOKE:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_event_invoke_event(expr), ctx, owner, owner_name);
        for (size_t i = 0; i < ast_event_invoke_arg_count(expr); i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                ast_event_invoke_argument(expr, i), ctx, owner, owner_name);
        }
        return;

    case AST_BLOCK:
        semantic_type_resolution_precollect_body_type_refs(
            expr, ctx, owner, owner_name);
        return;

    default:
        return;
    }
}

void
semantic_type_resolution_precollect_body_type_refs(ASTNode *stmt,
                                                   SemanticContext *ctx,
                                                   const ASTNode *owner,
                                                   const char *owner_name)
{
    char *consumer_name;

    if (stmt == NULL || ctx == NULL)
        return;

    switch (stmt->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(stmt); i++) {
            semantic_type_resolution_precollect_body_type_refs(
                ast_block_statement(stmt, i), ctx, owner, owner_name);
        }
        return;

    case AST_LET_DECL:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_let_initializer(stmt), ctx, owner, owner_name);
        consumer_name = resolution_body_strdup_fmt(
            "body %s.%s",
            owner_name != NULL ? owner_name : "<body>",
            ast_let_name(stmt) != NULL
                ? ast_let_name(stmt) : "<local>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                ast_let_type(stmt),
                ctx,
                stmt,
                consumer_name,
                "local let annotation lookup");
            free(consumer_name);
        }
        return;

    case AST_WITH_STMT:
        consumer_name = resolution_body_strdup_fmt(
            "body %s.%s",
            owner_name != NULL ? owner_name : "<body>",
            ast_with_alias(stmt) != NULL
                ? ast_with_alias(stmt) : "<with>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                ast_with_slot_type(stmt),
                ctx,
                stmt,
                consumer_name,
                "with slot type lookup");
            free(consumer_name);
        }
        semantic_type_resolution_precollect_body_type_refs(
            ast_with_body(stmt), ctx, owner, owner_name);
        return;

    case AST_IF_STMT:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_if_condition(stmt), ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            ast_if_then_branch(stmt), ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            ast_if_else_branch(stmt), ctx, owner, owner_name);
        return;

    case AST_WHILE_LOOP:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_while_condition(stmt), ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            ast_while_body(stmt), ctx, owner, owner_name);
        return;

    case AST_FOR_LOOP:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_for_range_start(stmt), ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            ast_for_range_end(stmt), ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            ast_for_iterable(stmt), ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            ast_for_body(stmt), ctx, owner, owner_name);
        return;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < ast_parallel_task_count(stmt); i++) {
            semantic_type_resolution_precollect_body_type_refs(
                ast_parallel_task(stmt, i), ctx, owner, owner_name);
        }
        return;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(stmt); i++) {
            semantic_type_resolution_precollect_body_type_refs(
                ast_async_block_statement(stmt, i), ctx, owner, owner_name);
        }
        return;

    case AST_TASK_GROUP:
        for (size_t i = 0; i < ast_task_group_task_count(stmt); i++) {
            semantic_type_resolution_precollect_body_type_refs(
                ast_task_group_task(stmt, i), ctx, owner, owner_name);
        }
        return;

    case AST_MATCH_STMT:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_match_subject(stmt), ctx, owner, owner_name);
        for (size_t i = 0; i < ast_match_case_count(stmt); i++) {
            semantic_type_resolution_precollect_body_type_refs(
                ast_match_case_at(stmt, i), ctx, owner, owner_name);
        }
        semantic_type_resolution_precollect_body_type_refs(
            ast_match_default_body(stmt), ctx, owner, owner_name);
        return;

    case AST_MATCH_CASE:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_match_case_guard(stmt), ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            ast_match_case_body(stmt), ctx, owner, owner_name);
        return;

    case AST_UNSAFE_BLOCK:
        semantic_type_resolution_precollect_body_type_refs(
            ast_unsafe_block_body(stmt), ctx, owner, owner_name);
        return;

    case AST_DEFER_STMT:
        semantic_type_resolution_precollect_body_type_refs(
            ast_defer_body(stmt), ctx, owner, owner_name);
        return;

    case AST_RETURN:
        semantic_type_resolution_precollect_expr_type_refs(
            ast_return_value(stmt), ctx, owner, owner_name);
        return;

    case AST_ASSIGNMENT:
    case AST_CALL:
    case AST_LAMBDA_EXPR:
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
    case AST_EVENT_INVOKE:
    case AST_SPAWN_EXPR:
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt, ctx, owner, owner_name);
        return;

    default:
        return;
    }
}
