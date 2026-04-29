#include "air_internal.h"

#include <string.h>

static size_t
air_count_expr_boundaries(ASTNode *node)
{
    size_t count = 0;
    if (node == NULL)
        return 0;
    if (air_boundary_kind_from_ast(node) != AIR_BOUNDARY_UNKNOWN)
        count++;
    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            count += air_count_expr_boundaries(node->data.block.statements[i]);
        break;
    case AST_WITH_STMT:
        count += air_count_expr_boundaries(node->data.with_stmt.body);
        break;
    case AST_FOR_LOOP:
        count += air_count_expr_boundaries(node->data.for_loop.range_start);
        count += air_count_expr_boundaries(node->data.for_loop.range_end);
        count += air_count_expr_boundaries(node->data.for_loop.iterable);
        count += air_count_expr_boundaries(node->data.for_loop.body);
        break;
    case AST_WHILE_LOOP:
        count += air_count_expr_boundaries(node->data.while_loop.condition);
        count += air_count_expr_boundaries(node->data.while_loop.body);
        break;
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            count += air_count_expr_boundaries(node->data.parallel.tasks[i]);
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            count += air_count_expr_boundaries(node->data.async_block.statements[i]);
        break;
    case AST_SPAWN_EXPR:
        count += air_count_expr_boundaries(node->data.spawn_expr.function);
        for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++)
            count += air_count_expr_boundaries(node->data.spawn_expr.arguments[i]);
        break;
    case AST_CALL:
        count += air_count_expr_boundaries(node->data.call.callee);
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            count += air_count_expr_boundaries(node->data.call.arguments[i]);
        break;
    case AST_MEMBER_ACCESS:
        count += air_count_expr_boundaries(node->data.member.object);
        break;
    case AST_ARRAY_ACCESS:
        count += air_count_expr_boundaries(node->data.array_access.array);
        count += air_count_expr_boundaries(node->data.array_access.index);
        break;
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++)
            count += air_count_expr_boundaries(node->data.array_literal.elements[i]);
        break;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < node->data.tuple_literal.count; i++)
            count += air_count_expr_boundaries(node->data.tuple_literal.elements[i]);
        break;
    case AST_ASSIGNMENT:
        count += air_count_expr_boundaries(node->data.assignment.target);
        count += air_count_expr_boundaries(node->data.assignment.value);
        break;
    case AST_BINARY:
        count += air_count_expr_boundaries(node->data.binary.left);
        count += air_count_expr_boundaries(node->data.binary.right);
        break;
    case AST_UNARY:
        count += air_count_expr_boundaries(node->data.unary.operand);
        break;
    case AST_AWAIT_EXPR:
        count += air_count_expr_boundaries(node->data.await_expr.expression);
        break;
    case AST_CHANNEL_SEND:
        count += air_count_expr_boundaries(node->data.channel_send.channel);
        count += air_count_expr_boundaries(node->data.channel_send.value);
        break;
    case AST_CHANNEL_RECV:
        count += air_count_expr_boundaries(node->data.channel_recv.channel);
        break;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++)
            count += air_count_expr_boundaries(node->data.select_stmt.cases[i]);
        count += air_count_expr_boundaries(node->data.select_stmt.default_case);
        break;
    case AST_MATCH_STMT:
        count += air_count_expr_boundaries(node->data.match_stmt.subject);
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            count += air_count_expr_boundaries(node->data.match_stmt.cases[i]);
        count += air_count_expr_boundaries(node->data.match_stmt.default_body);
        break;
    case AST_MATCH_CASE:
        count += air_count_expr_boundaries(node->data.match_case.pattern);
        for (size_t i = 0; i < node->data.match_case.pattern_count; i++)
            count += air_count_expr_boundaries(node->data.match_case.patterns[i]);
        count += air_count_expr_boundaries(node->data.match_case.guard);
        count += air_count_expr_boundaries(node->data.match_case.body);
        break;
    case AST_IF_STMT:
        count += air_count_expr_boundaries(node->data.if_stmt.condition);
        count += air_count_expr_boundaries(node->data.if_stmt.then_branch);
        count += air_count_expr_boundaries(node->data.if_stmt.else_branch);
        break;
    case AST_RETURN:
        count += air_count_expr_boundaries(node->data.return_stmt.value);
        break;
    case AST_TASK_GROUP:
        for (size_t i = 0; i < node->data.task_group.task_count; i++)
            count += air_count_expr_boundaries(node->data.task_group.tasks[i]);
        break;
    case AST_EVENT_INVOKE:
        count += air_count_expr_boundaries(node->data.event_invoke.event);
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++)
            count += air_count_expr_boundaries(node->data.event_invoke.arguments[i]);
        break;
    case AST_LAMBDA_EXPR:
        count += air_count_expr_boundaries(node->data.lambda_expr.body);
        break;
    case AST_UNSAFE_BLOCK:
        count += air_count_expr_boundaries(node->data.unsafe_block.body);
        break;
    case AST_DEFER_STMT:
        count += air_count_expr_boundaries(node->data.defer_stmt.body);
        break;
    default:
        break;
    }
    return count;
}

size_t
air_count_step_expr_boundaries(const DIRIntentStep *step)
{
    size_t count = 0;
    ASTNode *ast;

    if (step == NULL || step->ast == NULL || step->ast->type != AST_INTENT_STEP)
        return 0;
    ast = step->ast;
    count += air_count_expr_boundaries(ast->data.intent_step.using_expr);
    count += air_count_expr_boundaries(ast->data.intent_step.intent_expr);
    count += air_count_expr_boundaries(ast->data.intent_step.pre_expr);
    count += air_count_expr_boundaries(ast->data.intent_step.guard_expr);
    count += air_count_expr_boundaries(ast->data.intent_step.post_expr);
    count += air_count_expr_boundaries(ast->data.intent_step.invariant_expr);
    count += air_count_expr_boundaries(ast->data.intent_step.expect_expr);
    for (size_t i = 0; i < ast->data.intent_step.on_expr_count; i++)
        count += air_count_expr_boundaries(ast->data.intent_step.on_exprs[i]);
    for (size_t i = 0; i < ast->data.intent_step.compensate_expr_count; i++)
        count += air_count_expr_boundaries(ast->data.intent_step.compensate_exprs[i]);
    return count;
}

static bool
air_append_expr_boundaries(AIRProgram *air,
                           AIRBoundaryNode *boundaries,
                           size_t *boundary_index,
                           size_t intent_index,
                           const char *owner,
                           const DIRIntentStep *step,
                           ASTNode *node)
{
    AIRBoundaryKind kind;

    if (node == NULL)
        return true;

    kind = air_boundary_kind_from_ast(node);
    if (kind != AIR_BOUNDARY_UNKNOWN) {
        AIRBoundaryNode *boundary = &boundaries[*boundary_index];
        const char *source = air_boundary_source_from_ast(node);
        memset(boundary, 0, sizeof(*boundary));
        boundary->kind = kind;
        if (!air_assign_owned_name(air, &boundary->owner_name, owner)
            || !air_assign_owned_name(air, &boundary->source_name, source))
            return false;
        boundary->intent_index = intent_index;
        boundary->step_index = step != NULL ? step->index : 0;
        boundary->ast = (node->line > 0 || step == NULL || step->ast == NULL)
            ? node
            : step->ast;
        boundary->sync_class = air_boundary_sync_from_kind(kind);
        (*boundary_index)++;
    }

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.block.statements[i]))
                return false;
        break;
    case AST_FOR_LOOP:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.for_loop.range_start)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.for_loop.range_end)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.for_loop.iterable)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.for_loop.body);
    case AST_WHILE_LOOP:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.while_loop.condition)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.while_loop.body);
    case AST_PARALLEL_BLOCK:
        for (size_t i = 0; i < node->data.parallel.task_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.parallel.tasks[i]))
                return false;
        break;
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < node->data.async_block.statement_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.async_block.statements[i]))
                return false;
        break;
    case AST_SPAWN_EXPR:
        if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.spawn_expr.function))
            return false;
        for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.spawn_expr.arguments[i]))
                return false;
        break;
    case AST_CALL:
        if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.call.callee))
            return false;
        for (size_t i = 0; i < node->data.call.arg_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.call.arguments[i]))
                return false;
        break;
    case AST_MEMBER_ACCESS:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.member.object);
    case AST_ARRAY_ACCESS:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.array_access.array)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.array_access.index);
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < node->data.array_literal.count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.array_literal.elements[i]))
                return false;
        break;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < node->data.tuple_literal.count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.tuple_literal.elements[i]))
                return false;
        break;
    case AST_ASSIGNMENT:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.assignment.target)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.assignment.value);
    case AST_BINARY:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.binary.left)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.binary.right);
    case AST_UNARY:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.unary.operand);
    case AST_AWAIT_EXPR:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.await_expr.expression);
    case AST_CHANNEL_SEND:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.channel_send.channel)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.channel_send.value);
    case AST_CHANNEL_RECV:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.channel_recv.channel);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < node->data.select_stmt.case_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.select_stmt.cases[i]))
                return false;
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.select_stmt.default_case);
    case AST_MATCH_STMT:
        if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.match_stmt.subject))
            return false;
        for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.match_stmt.cases[i]))
                return false;
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.match_stmt.default_body);
    case AST_MATCH_CASE:
        if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.match_case.pattern))
            return false;
        for (size_t i = 0; i < node->data.match_case.pattern_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.match_case.patterns[i]))
                return false;
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.match_case.guard)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.match_case.body);
    case AST_IF_STMT:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.if_stmt.condition)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.if_stmt.then_branch)
            && air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.if_stmt.else_branch);
    case AST_WITH_STMT:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.with_stmt.body);
    case AST_RETURN:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.return_stmt.value);
    case AST_TASK_GROUP:
        for (size_t i = 0; i < node->data.task_group.task_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.task_group.tasks[i]))
                return false;
        break;
    case AST_EVENT_INVOKE:
        if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.event_invoke.event))
            return false;
        for (size_t i = 0; i < node->data.event_invoke.arg_count; i++)
            if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.event_invoke.arguments[i]))
                return false;
        break;
    case AST_LAMBDA_EXPR:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.lambda_expr.body);
    case AST_UNSAFE_BLOCK:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.unsafe_block.body);
    case AST_DEFER_STMT:
        return air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, node->data.defer_stmt.body);
    default:
        break;
    }
    return true;
}

bool
air_append_step_expr_boundaries(AIRProgram *air,
                                AIRBoundaryNode *boundaries,
                                size_t *boundary_index,
                                size_t intent_index,
                                const char *owner,
                                const DIRIntentStep *step)
{
    ASTNode *ast;

    if (step == NULL || step->ast == NULL || step->ast->type != AST_INTENT_STEP)
        return true;
    ast = step->ast;
    if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.using_expr))
        return false;
    if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.intent_expr))
        return false;
    if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.pre_expr))
        return false;
    if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.guard_expr))
        return false;
    if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.post_expr))
        return false;
    if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.invariant_expr))
        return false;
    if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.expect_expr))
        return false;
    for (size_t i = 0; i < ast->data.intent_step.on_expr_count; i++) {
        if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.on_exprs[i]))
            return false;
    }
    for (size_t i = 0; i < ast->data.intent_step.compensate_expr_count; i++) {
        if (!air_append_expr_boundaries(air, boundaries, boundary_index, intent_index, owner, step, ast->data.intent_step.compensate_exprs[i]))
            return false;
    }
    return true;
}
