/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR (Abstraction Intent Representation) synthesis and drift checks.
 */

#include "air.h"

#include "../semantic/diag_codes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
air_vformat(const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0)
        return NULL;

    char *buffer = (char *)malloc((size_t)needed + 1);
    if (buffer == NULL)
        return NULL;
    vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    return buffer;
}

static void
air_set_error(char **error_message, const char *fmt, ...)
{
    if (error_message == NULL)
        return;
    va_list args;
    va_start(args, fmt);
    *error_message = air_vformat(fmt, args);
    va_end(args);
}

static char *
air_strdup_owned(const char *text)
{
    size_t len;
    char *copy;

    if (text == NULL)
        text = "";
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static const char *
air_program_owned_name(AIRProgram *air, const char *text)
{
    char *copy;
    char **next;

    if (air == NULL || text == NULL)
        return NULL;

    copy = air_strdup_owned(text);
    if (copy == NULL)
        return NULL;

    next = (char **)realloc(air->owned_names,
                            sizeof(char *) * (air->owned_name_count + 1));
    if (next == NULL) {
        free(copy);
        return NULL;
    }

    air->owned_names = next;
    air->owned_names[air->owned_name_count++] = copy;
    return copy;
}

static bool
air_assign_owned_name(AIRProgram *air, const char **slot, const char *text)
{
    if (slot == NULL)
        return false;
    *slot = NULL;
    if (text == NULL)
        return true;
    *slot = air_program_owned_name(air, text);
    return *slot != NULL;
}

static bool
air_assign_first_owned_name(AIRProgram *air,
                            const char **slot,
                            const char *text,
                            char **error_message,
                            const char *what)
{
    if (slot == NULL || *slot != NULL || text == NULL)
        return true;
    if (!air_assign_owned_name(air, slot, text)) {
        air_set_error(error_message, "AIR %s evidence name allocation failed", what);
        return false;
    }
    return true;
}

static bool
air_assign_authority_names(AIRProgram *air,
                           AIRBoundaryNode *boundary,
                           const char **names,
                           size_t name_count)
{
    if (boundary == NULL)
        return false;
    boundary->authority_names = NULL;
    boundary->authority_name_count = 0;
    if (name_count == 0)
        return true;

    boundary->authority_names = (const char **)calloc(name_count, sizeof(char *));
    if (boundary->authority_names == NULL)
        return false;

    for (size_t i = 0; i < name_count; i++) {
        const char *copy = air_program_owned_name(air, names != NULL ? names[i] : NULL);
        if (copy == NULL)
            return false;
        boundary->authority_names[i] = copy;
    }
    boundary->authority_name_count = name_count;
    return true;
}

static void
air_clear_drifts(AIRProgram *air)
{
    if (air == NULL)
        return;
    for (size_t i = 0; i < air->drift_count; i++)
        free((char *)air->drifts[i].message);
    free(air->drifts);
    air->drifts = NULL;
    air->drift_count = 0;
}

static const char *
air_dir_node_name(const DIRProgram *dir, size_t node_id)
{
    if (dir == NULL)
        return NULL;
    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].id == node_id)
            return dir->nodes[i].name;
    }
    return NULL;
}

static ASTNode *
air_dir_node_ast(const DIRProgram *dir, size_t node_id)
{
    if (dir == NULL)
        return NULL;
    for (size_t i = 0; i < dir->node_count; i++) {
        if (dir->nodes[i].id == node_id)
            return dir->nodes[i].ast;
    }
    return NULL;
}

static AIRSyncClass
air_sync_from_dir_step(const DIRIntentStep *step)
{
    if (step == NULL)
        return AIR_SYNC_UNKNOWN;
    if (step->transfer_from_alias != NULL || step->transfer_to_alias != NULL)
        return AIR_SYNC_ASYNC;
    return AIR_SYNC_SYNC;
}

static AIRFailureClass
air_failure_from_dir_step(const DIRIntentStep *step)
{
    if (step == NULL)
        return AIR_FAILURE_UNKNOWN;
    if (step->causes_effect_name != NULL)
        return AIR_FAILURE_COMPENSABLE;
    return AIR_FAILURE_RECOVERABLE;
}

static bool
air_step_has_zone_boundary(const DIRIntentStep *step)
{
    return step != NULL && step->where_type_name != NULL;
}

static bool
air_step_has_world_boundary(const DIRIntentStep *step)
{
    return step != NULL
        && (step->transfer_from_alias != NULL || step->transfer_to_alias != NULL);
}

static const char *
air_call_callee_name(const ASTNode *node)
{
    if (node == NULL || node->type != AST_CALL || node->data.call.callee == NULL)
        return NULL;
    if (node->data.call.callee->type == AST_IDENTIFIER)
        return node->data.call.callee->data.identifier.name;
    if (node->data.call.callee->type == AST_MEMBER_ACCESS)
        return node->data.call.callee->data.member.name;
    return NULL;
}

static bool
air_call_is_io_boundary(const ASTNode *node)
{
    const char *name = air_call_callee_name(node);
    if (name == NULL)
        return false;
    return strcmp(name, "ReadFile") == 0
        || strcmp(name, "WriteFile") == 0
        || strcmp(name, "ReadLine") == 0;
}

static AIRBoundaryKind
air_boundary_from_ast_node(const ASTNode *node)
{
    if (node == NULL)
        return AIR_BOUNDARY_UNKNOWN;
    switch (node->type) {
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_SPAWN_EXPR:
        return AIR_BOUNDARY_PARALLEL;
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
    case AST_SELECT_STMT:
        return AIR_BOUNDARY_CHANNEL;
    case AST_CALL:
        return air_call_is_io_boundary(node) ? AIR_BOUNDARY_IO : AIR_BOUNDARY_UNKNOWN;
    default:
        return AIR_BOUNDARY_UNKNOWN;
    }
}

static AIRSyncClass
air_sync_from_boundary_kind(AIRBoundaryKind kind)
{
    switch (kind) {
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_CHANNEL:
        return AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_IO:
        return AIR_SYNC_EITHER;
    case AIR_BOUNDARY_ZONE:
    case AIR_BOUNDARY_WORLD:
        return AIR_SYNC_SYNC;
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return AIR_SYNC_UNKNOWN;
    }
}

static const char *
air_boundary_source_from_ast(const ASTNode *node)
{
    AIRBoundaryKind kind = air_boundary_from_ast_node(node);
    if (kind == AIR_BOUNDARY_IO) {
        const char *name = air_call_callee_name(node);
        return name != NULL ? name : "io";
    }
    switch (kind) {
    case AIR_BOUNDARY_PARALLEL:
        if (node != NULL && node->type == AST_SPAWN_EXPR)
            return "spawn";
        if (node != NULL && node->type == AST_ASYNC_BLOCK)
            return "async";
        return "parallel";
    case AIR_BOUNDARY_CHANNEL:
        if (node != NULL && node->type == AST_CHANNEL_SEND)
            return "channel-send";
        if (node != NULL && node->type == AST_CHANNEL_RECV)
            return "channel-recv";
        return "select";
    default:
        return "boundary";
    }
}

static bool
air_sync_conflicts(AIRSyncClass expected, AIRSyncClass actual)
{
    if (expected == AIR_SYNC_UNKNOWN || actual == AIR_SYNC_UNKNOWN)
        return false;
    if (expected == AIR_SYNC_EITHER || actual == AIR_SYNC_EITHER)
        return false;
    return expected != actual;
}

static size_t
air_count_expr_boundaries(ASTNode *node)
{
    size_t count = 0;
    if (node == NULL)
        return 0;
    if (air_boundary_from_ast_node(node) != AIR_BOUNDARY_UNKNOWN)
        count++;
    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < node->data.block.count; i++)
            count += air_count_expr_boundaries(node->data.block.statements[i]);
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

static size_t
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

    kind = air_boundary_from_ast_node(node);
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
        boundary->sync_class = air_sync_from_boundary_kind(kind);
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
    (void)air;
    return true;
}

static bool
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

static bool
air_strict_evidence_enabled(void)
{
    const char *value = getenv("PGY_AIR_STRICT_EVIDENCE");
    if (value == NULL || value[0] == '\0')
        return true;
    if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 || strcmp(value, "off") == 0)
        return false;
    return true;
}

static bool
air_append_drift(AIRProgram *air,
                 AIRDriftKind kind,
                 size_t intent_index,
                 size_t boundary_index,
                 const char *message,
                 char **error_message)
{
    char *message_copy = air_strdup_owned(message);
    AIRDrift *next;

    if (message_copy == NULL) {
        air_set_error(error_message, "AIR drift message allocation failed");
        return false;
    }
    next = (AIRDrift *)realloc(air->drifts, sizeof(AIRDrift) * (air->drift_count + 1));
    if (next == NULL) {
        free(message_copy);
        air_set_error(error_message, "AIR drift allocation failed");
        return false;
    }
    air->drifts = next;
    air->drifts[air->drift_count].kind = kind;
    air->drifts[air->drift_count].intent_index = intent_index;
    air->drifts[air->drift_count].boundary_index = boundary_index;
    air->drifts[air->drift_count].message = message_copy;
    air->drift_count++;
    return true;
}

static bool
air_name_matches(const char *a, const char *b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static bool
air_boundary_authority_matches(const AIRBoundaryNode *boundary, const char *authority_name)
{
    if (boundary == NULL || authority_name == NULL)
        return false;
    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        if (air_name_matches(boundary->authority_names[i], authority_name))
            return true;
    }
    return false;
}

static bool
air_format_authority_names(const AIRBoundaryNode *boundary,
                           char *out,
                           size_t out_size)
{
    size_t used = 0;
    bool emitted = false;

    if (out == NULL || out_size == 0)
        return false;
    out[0] = '\0';
    if (boundary == NULL || boundary->authority_names == NULL)
        return false;

    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        const char *name = boundary->authority_names[i];
        int written;

        if (name == NULL || name[0] == '\0')
            continue;
        written = snprintf(out + used,
                           out_size - used,
                           "%s%s",
                           emitted ? ", " : "",
                           name);
        if (written < 0)
            return emitted;
        if ((size_t)written >= out_size - used) {
            out[out_size - 1] = '\0';
            return true;
        }
        used += (size_t)written;
        emitted = true;
    }
    return emitted;
}

static bool
air_hir_routine_matches_boundary(const HIRRoutine *routine,
                                 const AIRIntentNode *intent,
                                 const AIRBoundaryNode *boundary)
{
    if (routine == NULL || intent == NULL || boundary == NULL)
        return false;
    if (routine->kind == HIR_TOPLEVEL_INTENT)
        return true;
    return air_name_matches(routine->owner_name, intent->intent_owner)
        || air_name_matches(routine->name, intent->step_name)
        || air_name_matches(routine->name, boundary->source_name);
}

static bool
air_collect_hir_evidence(AIRProgram *air, const HIRProgram *hir, char **error_message)
{
    if (air == NULL || hir == NULL)
        return true;
    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            const AIRIntentNode *intent = &air->intents[boundary->intent_index];
            if (air_hir_routine_matches_boundary(routine, intent, boundary)) {
                const char *routine_name = routine->name != NULL
                    ? routine->name
                    : routine->owner_name;
                if (!air_assign_first_owned_name(air,
                                                 &boundary->hir_routine_evidence_name,
                                                 routine_name,
                                                 error_message,
                                                 "HIR routine")) {
                    return false;
                }
                boundary->has_hir_routine_evidence = true;
                air->hir_routine_evidence_count++;
            }
        }
    }
    return true;
}

static bool
air_rir_scope_matches_boundary(const RIRScope *scope, const AIRBoundaryNode *boundary)
{
    if (scope == NULL || boundary == NULL)
        return false;
    if (!(scope->kind == RIR_SCOPE_INTENT
          || scope->kind == RIR_SCOPE_ZONE
          || scope->kind == RIR_SCOPE_WORLD)) {
        return false;
    }
    if (boundary->kind == AIR_BOUNDARY_PARALLEL
        || boundary->kind == AIR_BOUNDARY_IO
        || boundary->kind == AIR_BOUNDARY_CHANNEL) {
        return air_name_matches(scope->name, boundary->source_name);
    }
    if (boundary->kind == AIR_BOUNDARY_WORLD) {
        return air_name_matches(scope->name, boundary->source_name)
            || air_name_matches(scope->name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->source_name);
    }
    return air_name_matches(scope->name, boundary->source_name)
        || air_name_matches(scope->owner_name, boundary->owner_name)
        || air_name_matches(scope->owner_name, boundary->source_name);
}

static bool
air_rir_scope_provides_boundary_evidence(const RIRScope *scope,
                                         const AIRBoundaryNode *boundary)
{
    if (!air_rir_scope_matches_boundary(scope, boundary))
        return false;

    if (boundary->kind != AIR_BOUNDARY_WORLD)
        return true;

    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        if (op->kind == RIR_OP_CLAIM
            && air_name_matches(op->subject, boundary->source_name)) {
            return true;
        }
        if (op->kind == RIR_OP_MOVE
            && air_name_matches(op->arg0, boundary->source_name)) {
            return true;
        }
    }
    return false;
}

static bool
air_collect_rir_evidence(AIRProgram *air, const RIRProgram *rir, char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        const char *scope_name = scope->name != NULL ? scope->name : scope->owner_name;
        for (size_t j = 0; j < scope->fact_count; j++) {
            if (scope->facts[j].kind == RIR_FACT_AUTHORITY) {
                air->rir_authority_evidence_count++;
            }
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            if (scope->ops[j].kind == RIR_OP_AUTHORIZE) {
                air->rir_authority_evidence_count++;
            }
        }
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            if (!air_rir_scope_provides_boundary_evidence(scope, boundary))
                continue;
            if (!air_assign_first_owned_name(air,
                                             &boundary->rir_boundary_evidence_scope,
                                             scope_name,
                                             error_message,
                                             "RIR boundary")) {
                return false;
            }
            boundary->has_rir_boundary_evidence = true;
            air->rir_boundary_evidence_count++;
            for (size_t k = 0; k < scope->fact_count; k++) {
                if (scope->facts[k].kind == RIR_FACT_AUTHORITY
                    && air_boundary_authority_matches(boundary, scope->facts[k].name)) {
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->facts[k].name,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                    break;
                }
            }
            for (size_t k = 0; !boundary->has_rir_authority_evidence && k < scope->op_count; k++) {
                if (scope->ops[k].kind == RIR_OP_AUTHORIZE
                    && air_boundary_authority_matches(boundary, scope->ops[k].subject)) {
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->ops[k].subject,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                    break;
                }
            }
        }
    }
    return true;
}

AIRProgram *
air_synthesize(const HIRProgram *hir,
               const DIRProgram *dir,
               const RIRProgram *rir,
               char **error_message)
{
    if (dir == NULL) {
        air_set_error(error_message, "AIR synthesis requires DIR input");
        return NULL;
    }

    AIRProgram *air = (AIRProgram *)calloc(1, sizeof(AIRProgram));
    if (air == NULL) {
        air_set_error(error_message, "AIR program allocation failed");
        return NULL;
    }
    air->strict_evidence = air_strict_evidence_enabled();

    size_t intent_node_count = 0;
    size_t boundary_node_count = 0;
    for (size_t i = 0; i < dir->intent_count; i++) {
        intent_node_count += dir->intents[i].step_count;
        for (size_t j = 0; j < dir->intents[i].step_count; j++) {
            if (air_step_has_zone_boundary(&dir->intents[i].steps[j]))
                boundary_node_count++;
            if (air_step_has_world_boundary(&dir->intents[i].steps[j]))
                boundary_node_count++;
            boundary_node_count += air_count_step_expr_boundaries(&dir->intents[i].steps[j]);
        }
    }

    if (intent_node_count > 0) {
        air->intents = (AIRIntentNode *)calloc(intent_node_count, sizeof(AIRIntentNode));
        if (air->intents == NULL) {
            air_destroy(air);
            air_set_error(error_message, "AIR intent allocation failed");
            return NULL;
        }
    }
    if (boundary_node_count > 0) {
        air->boundaries = (AIRBoundaryNode *)calloc(boundary_node_count, sizeof(AIRBoundaryNode));
        if (air->boundaries == NULL) {
            air_destroy(air);
            air_set_error(error_message, "AIR boundary allocation failed");
            return NULL;
        }
    }
    air->intent_count = intent_node_count;
    air->boundary_count = boundary_node_count;

    size_t intent_index = 0;
    size_t boundary_index = 0;
    for (size_t i = 0; i < dir->intent_count; i++) {
        const DIRIntentInfo *info = &dir->intents[i];
        const char *owner_source = air_dir_node_name(dir, info->node_id);
        const char *owner = air_program_owned_name(air, owner_source);
        ASTNode *owner_ast = air_dir_node_ast(dir, info->node_id);
        if (owner_source != NULL && owner == NULL) {
            air_destroy(air);
            air_set_error(error_message, "AIR owner name allocation failed");
            return NULL;
        }
        for (size_t j = 0; j < info->step_count; j++) {
            const DIRIntentStep *step = &info->steps[j];
            AIRSyncClass sync_class = air_sync_from_dir_step(step);
            air->intents[intent_index].intent_owner = owner;
            if (!air_assign_owned_name(air, &air->intents[intent_index].step_name, step->name)) {
                air_destroy(air);
                air_set_error(error_message, "AIR intent step name allocation failed");
                return NULL;
            }
            air->intents[intent_index].step_index = step->index;
            air->intents[intent_index].ast = step->ast != NULL ? step->ast : owner_ast;
            air->intents[intent_index].sync_class = sync_class;
            air->intents[intent_index].failure_class = air_failure_from_dir_step(step);

            if (air_step_has_zone_boundary(step)) {
                air->boundaries[boundary_index].kind = AIR_BOUNDARY_ZONE;
                air->boundaries[boundary_index].owner_name = owner;
                if (!air_assign_owned_name(air,
                                           &air->boundaries[boundary_index].source_name,
                                           step->where_type_name)
                    || !air_assign_authority_names(air,
                                                   &air->boundaries[boundary_index],
                                                   step->authorized_by,
                                                   step->authorized_by_count)) {
                    air_destroy(air);
                    air_set_error(error_message, "AIR zone boundary allocation failed");
                    return NULL;
                }
                air->boundaries[boundary_index].intent_index = intent_index;
                air->boundaries[boundary_index].step_index = step->index;
                air->boundaries[boundary_index].ast = step->ast != NULL ? step->ast : owner_ast;
                air->boundaries[boundary_index].sync_class = sync_class;
                air->boundaries[boundary_index].authority_required = step->authorized_by_count > 0;
                boundary_index++;
            }
            if (air_step_has_world_boundary(step)) {
                air->boundaries[boundary_index].kind = AIR_BOUNDARY_WORLD;
                air->boundaries[boundary_index].owner_name = owner;
                if (!air_assign_owned_name(air,
                                           &air->boundaries[boundary_index].source_name,
                                           step->transfer_to_alias != NULL
                                               ? step->transfer_to_alias
                                               : step->transfer_from_alias)
                    || !air_assign_authority_names(air,
                                                   &air->boundaries[boundary_index],
                                                   step->authorized_by,
                                                   step->authorized_by_count)) {
                    air_destroy(air);
                    air_set_error(error_message, "AIR world boundary allocation failed");
                    return NULL;
                }
                air->boundaries[boundary_index].intent_index = intent_index;
                air->boundaries[boundary_index].step_index = step->index;
                air->boundaries[boundary_index].ast = step->ast != NULL ? step->ast : owner_ast;
                air->boundaries[boundary_index].sync_class = sync_class;
                air->boundaries[boundary_index].authority_required = step->authorized_by_count > 0;
                boundary_index++;
            }
            if (!air_append_step_expr_boundaries(air,
                                                air->boundaries,
                                                &boundary_index,
                                                intent_index,
                                                owner,
                                                step)) {
                air_destroy(air);
                air_set_error(error_message, "AIR boundary synthesis failed for intent step %s", step->name);
                return NULL;
            }
            intent_index++;
        }
    }
    if (intent_index != intent_node_count || boundary_index != boundary_node_count) {
        air_destroy(air);
        air_set_error(error_message,
                      "AIR synthesis count mismatch: intents %zu/%zu boundaries %zu/%zu",
                      intent_index,
                      intent_node_count,
                      boundary_index,
                      boundary_node_count);
        return NULL;
    }
    if (!air_collect_hir_evidence(air, hir, error_message)
        || !air_collect_rir_evidence(air, rir, error_message)) {
        air_destroy(air);
        return NULL;
    }

    if (!air_validate(air, error_message)) {
        air_destroy(air);
        return NULL;
    }
    if (!air_check_drift(air, error_message)) {
        air_destroy(air);
        return NULL;
    }
    return air;
}

bool
air_validate(const AIRProgram *air, char **error_message)
{
    if (air == NULL) {
        air_set_error(error_message, "AIR validation requires a program");
        return false;
    }
    for (size_t i = 0; i < air->intent_count; i++) {
        if (air->intents[i].step_name == NULL) {
            air_set_error(error_message, "AIR intent node %zu has no step name", i);
            return false;
        }
    }
    for (size_t i = 0; i < air->boundary_count; i++) {
        if (air->boundaries[i].kind == AIR_BOUNDARY_UNKNOWN) {
            air_set_error(error_message, "AIR boundary node %zu has unknown kind", i);
            return false;
        }
        if (air->boundaries[i].intent_index >= air->intent_count) {
            air_set_error(error_message,
                          "AIR boundary node %zu references missing intent node %zu",
                          i,
                          air->boundaries[i].intent_index);
            return false;
        }
    }
    return true;
}

bool
air_check_drift(AIRProgram *air, char **error_message)
{
    if (!air_validate(air, error_message))
        return false;

    air_clear_drifts(air);

    for (size_t i = 0; i < air->boundary_count; i++) {
        AIRBoundaryNode *boundary = &air->boundaries[i];
        AIRIntentNode *intent = &air->intents[boundary->intent_index];
        if (air_sync_conflicts(intent->sync_class, boundary->sync_class)) {
            if (!air_append_drift(air,
                                  AIR_DRIFT_SYNC_ASYNC_CONFLICT,
                                  boundary->intent_index,
                                  i,
                                  PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT
                                  ": intent sync class conflicts with boundary implementation sync class",
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence && !boundary->has_rir_boundary_evidence) {
            char message[512];
            snprintf(message,
                     sizeof(message),
                     PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                     ": AIR boundary has no matching RIR boundary evidence; implementation boundary '%s' (%s)",
                     boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                     air_boundary_kind_name(boundary->kind));
            if (!air_append_drift(air,
                                  AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                  boundary->intent_index,
                                  i,
                                  message,
                                  error_message)) {
                return false;
            }
        }
        if (air->strict_evidence
            && boundary->authority_required
            && !boundary->has_rir_authority_evidence) {
            char authority_names[256];
            char message[512];
            const char *drift_message =
                PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                ": AIR authority boundary has no matching RIR authority evidence";

            if (air_format_authority_names(boundary,
                                           authority_names,
                                           sizeof(authority_names))) {
                snprintf(message,
                         sizeof(message),
                         PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING
                         ": AIR authority boundary has no matching RIR authority evidence; expected authority participant(s): %s",
                         authority_names);
                drift_message = message;
            }
            if (!air_append_drift(air,
                                  AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING,
                                  boundary->intent_index,
                                  i,
                                  drift_message,
                                  error_message)) {
                return false;
            }
        }
    }
    return true;
}

void
air_destroy(AIRProgram *air)
{
    if (air == NULL)
        return;
    air_clear_drifts(air);
    for (size_t i = 0; i < air->boundary_count; i++)
        free((void *)air->boundaries[i].authority_names);
    for (size_t i = 0; i < air->owned_name_count; i++)
        free(air->owned_names[i]);
    free(air->owned_names);
    free(air->intents);
    free(air->boundaries);
    free(air);
}

void
air_dump(const AIRProgram *air, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (air == NULL) {
        fprintf(out, "AIRProgram(null)\n");
        return;
    }
    fprintf(out, "AIRProgram intents=%zu boundaries=%zu drifts=%zu strict_evidence=%s\n",
            air->intent_count,
            air->boundary_count,
            air->drift_count,
            air->strict_evidence ? "yes" : "no");
    fprintf(out, "  evidence hir_routines=%zu rir_boundaries=%zu rir_authority=%zu\n",
            air->hir_routine_evidence_count,
            air->rir_boundary_evidence_count,
            air->rir_authority_evidence_count);
    for (size_t i = 0; i < air->intent_count; i++) {
        const AIRIntentNode *intent = &air->intents[i];
        fprintf(out,
                "  intent[%zu] owner=%s step=%s index=%zu sync=%s failure=%s\n",
                i,
                intent->intent_owner != NULL ? intent->intent_owner : "<anonymous>",
                intent->step_name != NULL ? intent->step_name : "<unnamed>",
                intent->step_index,
                air_sync_class_name(intent->sync_class),
                air_failure_class_name(intent->failure_class));
    }
    for (size_t i = 0; i < air->boundary_count; i++) {
        const AIRBoundaryNode *boundary = &air->boundaries[i];
        fprintf(out,
                "  boundary[%zu] kind=%s owner=%s source=%s intent=%zu step=%zu sync=%s authority=%s\n",
                i,
                air_boundary_kind_name(boundary->kind),
                boundary->owner_name != NULL ? boundary->owner_name : "<anonymous>",
                boundary->source_name != NULL ? boundary->source_name : "<unknown>",
                boundary->intent_index,
                boundary->step_index,
                air_sync_class_name(boundary->sync_class),
                boundary->authority_required ? "yes" : "no");
        fprintf(out,
                "    evidence hir=%s(%s) rir_boundary=%s(%s) rir_authority=%s(%s)\n",
                boundary->has_hir_routine_evidence ? "yes" : "no",
                boundary->hir_routine_evidence_name != NULL
                    ? boundary->hir_routine_evidence_name
                    : "<none>",
                boundary->has_rir_boundary_evidence ? "yes" : "no",
                boundary->rir_boundary_evidence_scope != NULL
                    ? boundary->rir_boundary_evidence_scope
                    : "<none>",
                boundary->has_rir_authority_evidence ? "yes" : "no",
                boundary->rir_authority_evidence_name != NULL
                    ? boundary->rir_authority_evidence_name
                    : "<none>");
    }
}

const char *
air_sync_class_name(AIRSyncClass sync_class)
{
    switch (sync_class) {
    case AIR_SYNC_UNKNOWN: return "unknown";
    case AIR_SYNC_SYNC: return "sync";
    case AIR_SYNC_ASYNC: return "async";
    case AIR_SYNC_EITHER: return "either";
    }
    return "invalid";
}

const char *
air_failure_class_name(AIRFailureClass failure_class)
{
    switch (failure_class) {
    case AIR_FAILURE_UNKNOWN: return "unknown";
    case AIR_FAILURE_RECOVERABLE: return "recoverable";
    case AIR_FAILURE_FATAL: return "fatal";
    case AIR_FAILURE_COMPENSABLE: return "compensable";
    }
    return "invalid";
}

const char *
air_boundary_kind_name(AIRBoundaryKind kind)
{
    switch (kind) {
    case AIR_BOUNDARY_UNKNOWN: return "unknown";
    case AIR_BOUNDARY_ZONE: return "zone";
    case AIR_BOUNDARY_WORLD: return "world";
    case AIR_BOUNDARY_PARALLEL: return "parallel";
    case AIR_BOUNDARY_IO: return "io";
    case AIR_BOUNDARY_CHANNEL: return "channel";
    }
    return "invalid";
}

const char *
air_drift_kind_name(AIRDriftKind kind)
{
    switch (kind) {
    case AIR_DRIFT_NONE: return "none";
    case AIR_DRIFT_SYNC_ASYNC_CONFLICT: return "sync_async_conflict";
    case AIR_DRIFT_BOUNDARY_EVIDENCE_MISSING: return "boundary_evidence_missing";
    }
    return "invalid";
}
