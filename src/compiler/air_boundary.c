#include "air_internal.h"
#include "io_boundary_builtin.h"

typedef struct
{
    ASTNodeType     ast_type;
    AIRBoundaryKind kind;
    const char     *source_name;
} AIRAstBoundaryRule;

static const AIRAstBoundaryRule kAstBoundaryRules[] = {
    {AST_PARALLEL_BLOCK, AIR_BOUNDARY_PARALLEL, "parallel"},
    {AST_ASYNC_BLOCK, AIR_BOUNDARY_PARALLEL, "async"},
    {AST_SPAWN_EXPR, AIR_BOUNDARY_PARALLEL, "spawn"},
    {AST_AWAIT_EXPR, AIR_BOUNDARY_PARALLEL, "await"},
    {AST_TASK_GROUP, AIR_BOUNDARY_PARALLEL, "task-group"},
    {AST_CHANNEL_SEND, AIR_BOUNDARY_CHANNEL, "channel-send"},
    {AST_CHANNEL_RECV, AIR_BOUNDARY_CHANNEL, "channel-recv"},
    {AST_SELECT_STMT, AIR_BOUNDARY_CHANNEL, "select"},
    {AST_WITH_STMT, AIR_BOUNDARY_EXECUTION, "with"},
    {AST_UNSAFE_BLOCK, AIR_BOUNDARY_EXECUTION, "unsafe"},
    {AST_DEFER_STMT, AIR_BOUNDARY_EXECUTION, "defer"},
    {AST_EVENT_SUBSCRIBE, AIR_BOUNDARY_EXECUTION, "event-subscribe"},
    {AST_EVENT_UNSUBSCRIBE, AIR_BOUNDARY_EXECUTION, "event-unsubscribe"},
};

bool
air_step_has_zone_boundary(const DIRIntentStep *step)
{
    return step != NULL && step->where_type_name != NULL;
}

bool
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

static const AIRAstBoundaryRule *
air_ast_boundary_rule_for_node(const ASTNode *node)
{
    if (node == NULL)
        return NULL;
    for (size_t i = 0; i < sizeof(kAstBoundaryRules) / sizeof(kAstBoundaryRules[0]); i++) {
        if (kAstBoundaryRules[i].ast_type == node->type)
            return &kAstBoundaryRules[i];
    }
    return NULL;
}

static bool
air_call_is_io_boundary(const ASTNode *node)
{
    const char *name = air_call_callee_name(node);
    return pgy_compiler_io_boundary_builtin_is_stable(name);
}

AIRBoundaryKind
air_boundary_kind_from_ast(const ASTNode *node)
{
    const AIRAstBoundaryRule *rule;

    if (node == NULL)
        return AIR_BOUNDARY_UNKNOWN;
    if (node->type == AST_BLOCK && node->data.block.is_pin_block)
        return AIR_BOUNDARY_EXECUTION;
    if (node->type == AST_CALL)
        return air_call_is_io_boundary(node)
            ? AIR_BOUNDARY_IO
            : AIR_BOUNDARY_UNKNOWN;
    rule = air_ast_boundary_rule_for_node(node);
    return rule != NULL ? rule->kind : AIR_BOUNDARY_UNKNOWN;
}

AIRSyncClass
air_boundary_sync_from_kind(AIRBoundaryKind kind)
{
    switch (kind) {
    case AIR_BOUNDARY_PARALLEL:
    case AIR_BOUNDARY_CHANNEL:
        return AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_IO:
        return AIR_SYNC_EITHER;
    case AIR_BOUNDARY_EXECUTION:
    case AIR_BOUNDARY_ZONE:
        return AIR_SYNC_SYNC;
    /* World handoff is an async abstraction boundary by AIR contract. */
    case AIR_BOUNDARY_WORLD:
        return AIR_SYNC_ASYNC;
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return AIR_SYNC_UNKNOWN;
    }
}

const char *
air_boundary_source_from_ast(const ASTNode *node)
{
    AIRBoundaryKind kind = air_boundary_kind_from_ast(node);
    const AIRAstBoundaryRule *rule;

    if (kind == AIR_BOUNDARY_IO) {
        const char *name = air_call_callee_name(node);
        return name != NULL ? name : "io";
    }
    if (kind == AIR_BOUNDARY_EXECUTION
        && node != NULL
        && node->type == AST_BLOCK
        && node->data.block.is_pin_block) {
        return "pin";
    }
    rule = air_ast_boundary_rule_for_node(node);
    if (rule != NULL)
        return rule->source_name;
    return kind == AIR_BOUNDARY_EXECUTION ? "execution" : "boundary";
}
