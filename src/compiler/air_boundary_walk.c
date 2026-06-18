#include "air_internal.h"

#include <string.h>

#include "../parser/ast_api.h"

typedef struct
{
    AIRProgram          *air;
    AIRBoundaryNode    *boundaries;
    size_t             *boundary_index;
    size_t              intent_index;
    const char         *owner;
    const DIRIntentStep *step;
    size_t              count;
    bool                append;
} AIRBoundaryWalkCtx;

static bool air_walk_expr_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node);

static bool
air_walk_child(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    return air_walk_expr_boundaries(ctx, node);
}

static bool
air_walk_child_array(AIRBoundaryWalkCtx *ctx, ASTNode **nodes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (!air_walk_child(ctx, nodes[i]))
            return false;
    }
    return true;
}

static bool
air_append_current_boundary(AIRBoundaryWalkCtx *ctx,
                            ASTNode *node,
                            AIRBoundaryKind kind)
{
    AIRBoundaryNode *boundary;
    const char *source;

    if (!ctx->append) {
        ctx->count++;
        return true;
    }
    if (ctx->boundaries == NULL || ctx->boundary_index == NULL)
        return false;

    boundary = &ctx->boundaries[*ctx->boundary_index];
    source = air_boundary_source_from_ast(node);
    memset(boundary, 0, sizeof(*boundary));
    boundary->kind = kind;
    if (!air_assign_owned_name(ctx->air, &boundary->owner_name, ctx->owner)
        || !air_assign_owned_name(ctx->air, &boundary->source_name, source)) {
        return false;
    }
    boundary->intent_index = ctx->intent_index;
    boundary->step_index = ctx->step != NULL ? ctx->step->index : 0;
    boundary->ast = (node->line > 0 || ctx->step == NULL || ctx->step->ast == NULL)
        ? node
        : ctx->step->ast;
    boundary->sync_class = air_boundary_sync_from_kind(kind);
    (*ctx->boundary_index)++;
    return true;
}

static bool
air_walk_match_case_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    size_t pattern_count = 0;
    ASTNode **patterns = ast_match_case_patterns(node, &pattern_count);
    if (!air_walk_child(ctx, ast_match_case_pattern(node)))
        return false;
    if (!air_walk_child_array(ctx, patterns, pattern_count)) {
        return false;
    }
    return air_walk_child(ctx, ast_match_case_guard(node))
        && air_walk_child(ctx, ast_match_case_body(node));
}

static bool
air_walk_select_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    size_t case_count = 0;
    ASTNode **cases = ast_select_cases(node, &case_count);

    if (!air_walk_child_array(ctx, cases, case_count)) {
        return false;
    }
    return air_walk_child(ctx, ast_select_default_case(node));
}

static bool
air_walk_match_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    size_t case_count = 0;
    ASTNode **cases = ast_match_cases(node, &case_count);
    if (!air_walk_child(ctx, ast_match_subject(node)))
        return false;
    if (!air_walk_child_array(ctx, cases, case_count)) {
        return false;
    }
    return air_walk_child(ctx, ast_match_default_body(node));
}

static bool
air_walk_party_instance_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    for (size_t i = 0; i < ast_party_instance_assignment_count(node); i++) {
        if (!air_walk_child(ctx,
                            ast_party_instance_assignment_value(node, i))) {
            return false;
        }
    }
    return true;
}

static bool
air_walk_expr_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    AIRBoundaryKind kind;

    if (ctx == NULL || node == NULL)
        return true;

    kind = air_boundary_kind_from_ast(node);
    if (kind != AIR_BOUNDARY_UNKNOWN
        && !air_append_current_boundary(ctx, node, kind)) {
        return false;
    }

    switch (node->type) {
    case AST_BLOCK:
        {
            size_t statement_count = 0;
            ASTNode **statements = ast_block_statements(node, &statement_count);
            return air_walk_child_array(ctx, statements, statement_count);
        }
    case AST_LET_DECL:
        return air_walk_child(ctx, ast_let_initializer(node));
    case AST_LET_DESTRUCTURE:
        return air_walk_child(ctx, ast_let_destructure_initializer(node));
    case AST_WITH_STMT:
        return air_walk_child(ctx, ast_with_body(node));
    case AST_FOR_LOOP:
        return air_walk_child(ctx, ast_for_range_start(node))
            && air_walk_child(ctx, ast_for_range_end(node))
            && air_walk_child(ctx, ast_for_iterable(node))
            && air_walk_child(ctx, ast_for_body(node));
    case AST_WHILE_LOOP:
        return air_walk_child(ctx, ast_while_condition(node))
            && air_walk_child(ctx, ast_while_body(node));
    case AST_PARALLEL_BLOCK:
        {
            size_t task_count = 0;
            ASTNode **tasks = ast_parallel_tasks(node, &task_count);
            return air_walk_child_array(ctx, tasks, task_count);
        }
    case AST_ASYNC_BLOCK:
        {
            size_t statement_count = 0;
            ASTNode **statements =
                ast_async_block_statements(node, &statement_count);
            return air_walk_child_array(ctx, statements, statement_count);
        }
    case AST_SPAWN_EXPR:
        if (!air_walk_child(ctx, ast_spawn_function(node)))
            return false;
        {
            size_t arg_count = 0;
            ASTNode **args = ast_spawn_arguments(node, &arg_count);
            return air_walk_child_array(ctx, args, arg_count);
        }
    case AST_CALL:
        if (!air_walk_child(ctx, ast_call_callee(node)))
            return false;
        return air_walk_child_array(ctx,
                                    ast_call_arguments(node, NULL),
                                    ast_call_arg_count(node));
    case AST_MEMBER_ACCESS:
        return air_walk_child(ctx, ast_member_object(node));
    case AST_ARRAY_ACCESS:
        return air_walk_child(ctx, ast_array_access_array(node))
            && air_walk_child(ctx, ast_array_access_index(node));
    case AST_ARRAY_LITERAL:
        for (size_t i = 0; i < ast_array_literal_count(node); i++) {
            if (!air_walk_child(ctx, ast_array_literal_element(node, i)))
                return false;
        }
        return true;
    case AST_TUPLE_LITERAL:
        for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
            if (!air_walk_child(ctx, ast_tuple_literal_element(node, i)))
                return false;
        }
        return true;
    case AST_ASSIGNMENT:
        return air_walk_child(ctx, ast_assignment_target(node))
            && air_walk_child(ctx, ast_assignment_value(node));
    case AST_BINARY:
        return air_walk_child(ctx, ast_binary_left(node))
            && air_walk_child(ctx, ast_binary_right(node));
    case AST_UNARY:
        return air_walk_child(ctx, ast_unary_operand(node));
    case AST_AWAIT_EXPR:
        return air_walk_child(ctx, ast_await_expression(node));
    case AST_CHANNEL_SEND:
        return air_walk_child(ctx, ast_channel_send_channel(node))
            && air_walk_child(ctx, ast_channel_send_value(node));
    case AST_CHANNEL_RECV:
        return air_walk_child(ctx, ast_channel_recv_channel(node));
    case AST_SELECT_STMT:
        return air_walk_select_boundaries(ctx, node);
    case AST_MATCH_STMT:
        return air_walk_match_boundaries(ctx, node);
    case AST_MATCH_CASE:
        return air_walk_match_case_boundaries(ctx, node);
    case AST_IF_STMT:
        return air_walk_child(ctx, ast_if_condition(node))
            && air_walk_child(ctx, ast_if_then_branch(node))
            && air_walk_child(ctx, ast_if_else_branch(node));
    case AST_RETURN:
        return air_walk_child(ctx, ast_return_value(node));
    case AST_TASK_GROUP:
        {
            size_t task_count = 0;
            ASTNode **tasks = ast_task_group_tasks(node, &task_count);
            return air_walk_child_array(ctx, tasks, task_count);
        }
    case AST_EVENT_INVOKE:
        if (!air_walk_child(ctx, ast_event_invoke_event(node)))
            return false;
        return air_walk_child_array(ctx,
                                    ast_event_invoke_arguments(node, NULL),
                                    ast_event_invoke_arg_count(node));
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return air_walk_child(ctx, ast_event_op_event(node))
            && air_walk_child(ctx, ast_event_op_handler(node));
    case AST_PARTY_SHARED:
        return air_walk_child(ctx, ast_party_shared_initializer(node));
    case AST_PARTY_INSTANCE:
        return air_walk_party_instance_boundaries(ctx, node);
    case AST_WORLD_SYSTEMIC:
        return air_walk_child(ctx, ast_world_roster_initializer(node));
    case AST_WORLD_ZONE:
        return air_walk_child(ctx, ast_world_zone_initializer(node));
    case AST_DOMAIN_SLOT:
        return air_walk_child(ctx, ast_domain_slot_initializer(node));
    case AST_LAMBDA_EXPR:
        return air_walk_child(ctx, ast_lambda_body(node));
    case AST_UNSAFE_BLOCK:
        return air_walk_child(ctx, ast_unsafe_block_body(node));
    case AST_TRANSACTION_BLOCK:
        return air_walk_child(ctx, ast_transaction_block_body(node));
    case AST_DEFER_STMT:
        return air_walk_child(ctx, ast_defer_body(node));
    default:
        return true;
    }
}

static bool
air_walk_step_expr_boundaries(AIRBoundaryWalkCtx *ctx, const DIRIntentStep *step)
{
    ASTNode *ast;

    if (step == NULL || step->ast == NULL || step->ast->type != AST_INTENT_STEP)
        return true;
    ast = step->ast;
    if (!air_walk_child(ctx, ast_intent_step_using_expr(ast))
        || !air_walk_child(ctx, ast_intent_step_intent_expr(ast))
        || !air_walk_child(ctx, ast_intent_step_pre_expr(ast))
        || !air_walk_child(ctx, ast_intent_step_guard_expr(ast))
        || !air_walk_child(ctx, ast_intent_step_post_expr(ast))
        || !air_walk_child(ctx, ast_intent_step_invariant_expr(ast))
        || !air_walk_child(ctx, ast_intent_step_expect_expr(ast))) {
        return false;
    }
    if (!air_walk_child_array(ctx,
                              ast_intent_step_on_exprs(ast, NULL),
                              ast_intent_step_on_expr_count(ast))) {
        return false;
    }
    return air_walk_child_array(ctx,
                                ast_intent_step_compensate_exprs(ast, NULL),
                                ast_intent_step_compensate_expr_count(ast));
}

size_t
air_count_step_expr_boundaries(const DIRIntentStep *step)
{
    AIRBoundaryWalkCtx ctx;

    memset(&ctx, 0, sizeof(ctx));
    if (!air_walk_step_expr_boundaries(&ctx, step))
        return 0;
    return ctx.count;
}

bool
air_append_step_expr_boundaries(AIRProgram *air,
                                AIRBoundaryNode *boundaries,
                                size_t *boundary_index,
                                size_t intent_index,
                                const char *owner,
                                const DIRIntentStep *step)
{
    AIRBoundaryWalkCtx ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.air = air;
    ctx.boundaries = boundaries;
    ctx.boundary_index = boundary_index;
    ctx.intent_index = intent_index;
    ctx.owner = owner;
    ctx.step = step;
    ctx.append = true;
    return air_walk_step_expr_boundaries(&ctx, step);
}
