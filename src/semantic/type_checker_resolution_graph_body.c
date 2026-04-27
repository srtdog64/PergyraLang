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
        for (size_t i = 0; i < expr->data.lambda_expr.param_count; i++) {
            ASTNode *param = expr->data.lambda_expr.params[i];
            char *consumer_name;

            if (param == NULL || param->type != AST_LET_DECL)
                continue;

            consumer_name = resolution_body_strdup_fmt(
                "lambda %s.%s",
                owner_name != NULL ? owner_name : "<lambda>",
                param->data.let_decl.name != NULL
                    ? param->data.let_decl.name : "<param>");
            if (consumer_name != NULL) {
                semantic_type_resolution_collect_type_refs(
                    param->data.let_decl.type,
                    ctx,
                    param,
                    consumer_name,
                    "lambda parameter type lookup");
                free(consumer_name);
            }
        }
        semantic_type_resolution_collect_type_refs(
            expr->data.lambda_expr.return_type,
            ctx,
            expr,
            owner_name != NULL ? owner_name : "<lambda>",
            "lambda return type lookup");
        if (expr->data.lambda_expr.body != NULL
            && expr->data.lambda_expr.body->type == AST_BLOCK) {
            semantic_type_resolution_precollect_body_type_refs(
                expr->data.lambda_expr.body, ctx, owner, owner_name);
        } else {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.lambda_expr.body, ctx, owner, owner_name);
        }
        return;

    case AST_CALL:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.call.callee, ctx, owner, owner_name);
        for (size_t i = 0; i < expr->data.call.arg_count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.call.arguments[i], ctx, owner, owner_name);
        }
        if (expr->data.call.generic_args != NULL) {
            for (size_t i = 0; i < expr->data.call.generic_args->count; i++) {
                GenericParam *arg = expr->data.call.generic_args->params[i];
                if (arg != NULL) {
                    semantic_type_resolution_collect_type_refs(
                        arg->constraint,
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
            expr->data.binary.left, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.binary.right, ctx, owner, owner_name);
        return;

    case AST_UNARY:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.unary.operand, ctx, owner, owner_name);
        return;

    case AST_MEMBER_ACCESS:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.member.object, ctx, owner, owner_name);
        return;

    case AST_ARRAY_ACCESS:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.array_access.array, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.array_access.index, ctx, owner, owner_name);
        return;

    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < expr->data.array_literal.count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.array_literal.elements[i], ctx, owner, owner_name);
        }
        return;

    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < expr->data.tuple_literal.count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.tuple_literal.elements[i], ctx, owner, owner_name);
        }
        return;

    case AST_ASSIGNMENT:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.assignment.target, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.assignment.value, ctx, owner, owner_name);
        return;

    case AST_AWAIT_EXPR:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.await_expr.expression, ctx, owner, owner_name);
        return;

    case AST_CHANNEL_SEND:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.channel_send.channel, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.channel_send.value, ctx, owner, owner_name);
        return;

    case AST_CHANNEL_RECV:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.channel_recv.channel, ctx, owner, owner_name);
        return;

    case AST_SPAWN_EXPR:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.spawn_expr.function, ctx, owner, owner_name);
        for (size_t i = 0; i < expr->data.spawn_expr.arg_count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.spawn_expr.arguments[i], ctx, owner, owner_name);
        }
        return;

    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.event_op.event, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.event_op.handler, ctx, owner, owner_name);
        return;

    case AST_EVENT_INVOKE:
        semantic_type_resolution_precollect_expr_type_refs(
            expr->data.event_invoke.event, ctx, owner, owner_name);
        for (size_t i = 0; i < expr->data.event_invoke.arg_count; i++) {
            semantic_type_resolution_precollect_expr_type_refs(
                expr->data.event_invoke.arguments[i], ctx, owner, owner_name);
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
        for (size_t i = 0; i < stmt->data.block.count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.block.statements[i], ctx, owner, owner_name);
        }
        return;

    case AST_LET_DECL:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.let_decl.initializer, ctx, owner, owner_name);
        consumer_name = resolution_body_strdup_fmt(
            "body %s.%s",
            owner_name != NULL ? owner_name : "<body>",
            stmt->data.let_decl.name != NULL
                ? stmt->data.let_decl.name : "<local>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                stmt->data.let_decl.type,
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
            stmt->data.with_stmt.alias != NULL
                ? stmt->data.with_stmt.alias : "<with>");
        if (consumer_name != NULL) {
            semantic_type_resolution_collect_type_refs(
                stmt->data.with_stmt.slot_type,
                ctx,
                stmt,
                consumer_name,
                "with slot type lookup");
            free(consumer_name);
        }
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.with_stmt.body, ctx, owner, owner_name);
        return;

    case AST_IF_STMT:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.if_stmt.condition, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.if_stmt.then_branch, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.if_stmt.else_branch, ctx, owner, owner_name);
        return;

    case AST_WHILE_LOOP:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.while_loop.condition, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.while_loop.body, ctx, owner, owner_name);
        return;

    case AST_FOR_LOOP:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.for_loop.range_start, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.for_loop.range_end, ctx, owner, owner_name);
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.for_loop.iterable, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.for_loop.body, ctx, owner, owner_name);
        return;

    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < stmt->data.parallel.task_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.parallel.tasks[i], ctx, owner, owner_name);
        }
        return;

    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < stmt->data.async_block.statement_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.async_block.statements[i], ctx, owner, owner_name);
        }
        return;

    case AST_TASK_GROUP:
        for (size_t i = 0; i < stmt->data.task_group.task_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.task_group.tasks[i], ctx, owner, owner_name);
        }
        return;

    case AST_MATCH_STMT:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.match_stmt.subject, ctx, owner, owner_name);
        for (size_t i = 0; i < stmt->data.match_stmt.case_count; i++) {
            semantic_type_resolution_precollect_body_type_refs(
                stmt->data.match_stmt.cases[i], ctx, owner, owner_name);
        }
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.match_stmt.default_body, ctx, owner, owner_name);
        return;

    case AST_MATCH_CASE:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.match_case.guard, ctx, owner, owner_name);
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.match_case.body, ctx, owner, owner_name);
        return;

    case AST_UNSAFE_BLOCK:
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.unsafe_block.body, ctx, owner, owner_name);
        return;

    case AST_DEFER_STMT:
        semantic_type_resolution_precollect_body_type_refs(
            stmt->data.defer_stmt.body, ctx, owner, owner_name);
        return;

    case AST_RETURN:
        semantic_type_resolution_precollect_expr_type_refs(
            stmt->data.return_stmt.value, ctx, owner, owner_name);
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
