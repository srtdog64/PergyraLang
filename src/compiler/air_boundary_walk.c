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
    if (!air_walk_child(ctx, node->data.match_case.pattern))
        return false;
    if (!air_walk_child_array(ctx,
                              node->data.match_case.patterns,
                              node->data.match_case.pattern_count)) {
        return false;
    }
    return air_walk_child(ctx, node->data.match_case.guard)
        && air_walk_child(ctx, node->data.match_case.body);
}

static bool
air_walk_select_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    if (!air_walk_child_array(ctx,
                              node->data.select_stmt.cases,
                              node->data.select_stmt.case_count)) {
        return false;
    }
    return air_walk_child(ctx, node->data.select_stmt.default_case);
}

static bool
air_walk_match_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    if (!air_walk_child(ctx, node->data.match_stmt.subject))
        return false;
    if (!air_walk_child_array(ctx,
                              node->data.match_stmt.cases,
                              node->data.match_stmt.case_count)) {
        return false;
    }
    return air_walk_child(ctx, node->data.match_stmt.default_body);
}

static bool
air_walk_party_instance_boundaries(AIRBoundaryWalkCtx *ctx, ASTNode *node)
{
    for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
        if (!air_walk_child(ctx,
                            node->data.party_instance.assignments[i].value)) {
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
        return air_walk_child_array(ctx,
                                    node->data.block.statements,
                                    node->data.block.count);
    case AST_LET_DECL:
        return air_walk_child(ctx, node->data.let_decl.initializer);
    case AST_LET_DESTRUCTURE:
        return air_walk_child(ctx, node->data.let_destructure.initializer);
    case AST_WITH_STMT:
        return air_walk_child(ctx, node->data.with_stmt.body);
    case AST_FOR_LOOP:
        return air_walk_child(ctx, node->data.for_loop.range_start)
            && air_walk_child(ctx, node->data.for_loop.range_end)
            && air_walk_child(ctx, node->data.for_loop.iterable)
            && air_walk_child(ctx, node->data.for_loop.body);
    case AST_WHILE_LOOP:
        return air_walk_child(ctx, node->data.while_loop.condition)
            && air_walk_child(ctx, node->data.while_loop.body);
    case AST_PARALLEL_BLOCK:
        return air_walk_child_array(ctx,
                                    node->data.parallel.tasks,
                                    node->data.parallel.task_count);
    case AST_ASYNC_BLOCK:
        return air_walk_child_array(ctx,
                                    node->data.async_block.statements,
                                    node->data.async_block.statement_count);
    case AST_SPAWN_EXPR:
        if (!air_walk_child(ctx, node->data.spawn_expr.function))
            return false;
        return air_walk_child_array(ctx,
                                    node->data.spawn_expr.arguments,
                                    node->data.spawn_expr.arg_count);
    case AST_CALL:
        if (!air_walk_child(ctx, node->data.call.callee))
            return false;
        return air_walk_child_array(ctx,
                                    node->data.call.arguments,
                                    node->data.call.arg_count);
    case AST_MEMBER_ACCESS:
        return air_walk_child(ctx, node->data.member.object);
    case AST_ARRAY_ACCESS:
        return air_walk_child(ctx, node->data.array_access.array)
            && air_walk_child(ctx, node->data.array_access.index);
    case AST_ARRAY_LITERAL:
        return air_walk_child_array(ctx,
                                    node->data.array_literal.elements,
                                    node->data.array_literal.count);
    case AST_TUPLE_LITERAL:
        return air_walk_child_array(ctx,
                                    node->data.tuple_literal.elements,
                                    node->data.tuple_literal.count);
    case AST_ASSIGNMENT:
        return air_walk_child(ctx, node->data.assignment.target)
            && air_walk_child(ctx, node->data.assignment.value);
    case AST_BINARY:
        return air_walk_child(ctx, node->data.binary.left)
            && air_walk_child(ctx, node->data.binary.right);
    case AST_UNARY:
        return air_walk_child(ctx, node->data.unary.operand);
    case AST_AWAIT_EXPR:
        return air_walk_child(ctx, node->data.await_expr.expression);
    case AST_CHANNEL_SEND:
        return air_walk_child(ctx, node->data.channel_send.channel)
            && air_walk_child(ctx, node->data.channel_send.value);
    case AST_CHANNEL_RECV:
        return air_walk_child(ctx, node->data.channel_recv.channel);
    case AST_SELECT_STMT:
        return air_walk_select_boundaries(ctx, node);
    case AST_MATCH_STMT:
        return air_walk_match_boundaries(ctx, node);
    case AST_MATCH_CASE:
        return air_walk_match_case_boundaries(ctx, node);
    case AST_IF_STMT:
        return air_walk_child(ctx, node->data.if_stmt.condition)
            && air_walk_child(ctx, node->data.if_stmt.then_branch)
            && air_walk_child(ctx, node->data.if_stmt.else_branch);
    case AST_RETURN:
        return air_walk_child(ctx, node->data.return_stmt.value);
    case AST_TASK_GROUP:
        return air_walk_child_array(ctx,
                                    node->data.task_group.tasks,
                                    node->data.task_group.task_count);
    case AST_EVENT_INVOKE:
        if (!air_walk_child(ctx, node->data.event_invoke.event))
            return false;
        return air_walk_child_array(ctx,
                                    node->data.event_invoke.arguments,
                                    node->data.event_invoke.arg_count);
    case AST_EVENT_SUBSCRIBE:
    case AST_EVENT_UNSUBSCRIBE:
        return air_walk_child(ctx, node->data.event_op.event)
            && air_walk_child(ctx, node->data.event_op.handler);
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
        return air_walk_child(ctx, node->data.lambda_expr.body);
    case AST_UNSAFE_BLOCK:
        return air_walk_child(ctx, node->data.unsafe_block.body);
    case AST_DEFER_STMT:
        return air_walk_child(ctx, node->data.defer_stmt.body);
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
