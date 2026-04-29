#include "air_internal.h"

#include <string.h>

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

static bool
air_call_is_io_boundary(const ASTNode *node)
{
    const char *name = air_call_callee_name(node);
    static const char *io_names[] = {
        "FileOpen",
        "FileRead",
        "FileWrite",
        "FileClose",
        "ReadFile",
        "WriteFile",
        "Input",
        "ReadLine",
        "Now",
        "Sleep",
    };
    if (name == NULL)
        return false;
    for (size_t i = 0; i < sizeof(io_names) / sizeof(io_names[0]); i++) {
        if (strcmp(name, io_names[i]) == 0)
            return true;
    }
    return false;
}

AIRBoundaryKind
air_boundary_kind_from_ast(const ASTNode *node)
{
    if (node == NULL)
        return AIR_BOUNDARY_UNKNOWN;
    if (node->type == AST_BLOCK && node->data.block.is_pin_block)
        return AIR_BOUNDARY_EXECUTION;
    switch (node->type) {
    case AST_PARALLEL_BLOCK:
    case AST_ASYNC_BLOCK:
    case AST_SPAWN_EXPR:
    case AST_AWAIT_EXPR:
    case AST_TASK_GROUP:
        return AIR_BOUNDARY_PARALLEL;
    case AST_CHANNEL_SEND:
    case AST_CHANNEL_RECV:
    case AST_SELECT_STMT:
        return AIR_BOUNDARY_CHANNEL;
    case AST_WITH_STMT:
    case AST_UNSAFE_BLOCK:
    case AST_DEFER_STMT:
        return AIR_BOUNDARY_EXECUTION;
    case AST_CALL:
        return air_call_is_io_boundary(node) ? AIR_BOUNDARY_IO : AIR_BOUNDARY_UNKNOWN;
    default:
        return AIR_BOUNDARY_UNKNOWN;
    }
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
    case AIR_BOUNDARY_WORLD:
        return AIR_SYNC_SYNC;
    case AIR_BOUNDARY_UNKNOWN:
    default:
        return AIR_SYNC_UNKNOWN;
    }
}

const char *
air_boundary_source_from_ast(const ASTNode *node)
{
    AIRBoundaryKind kind = air_boundary_kind_from_ast(node);
    if (kind == AIR_BOUNDARY_IO) {
        const char *name = air_call_callee_name(node);
        return name != NULL ? name : "io";
    }
    switch (kind) {
    case AIR_BOUNDARY_PARALLEL:
        if (node != NULL && node->type == AST_AWAIT_EXPR)
            return "await";
        if (node != NULL && node->type == AST_SPAWN_EXPR)
            return "spawn";
        if (node != NULL && node->type == AST_ASYNC_BLOCK)
            return "async";
        if (node != NULL && node->type == AST_TASK_GROUP)
            return "task-group";
        return "parallel";
    case AIR_BOUNDARY_CHANNEL:
        if (node != NULL && node->type == AST_CHANNEL_SEND)
            return "channel-send";
        if (node != NULL && node->type == AST_CHANNEL_RECV)
            return "channel-recv";
        return "select";
    case AIR_BOUNDARY_EXECUTION:
        if (node != NULL && node->type == AST_BLOCK && node->data.block.is_pin_block)
            return "pin";
        if (node != NULL && node->type == AST_WITH_STMT)
            return "with";
        if (node != NULL && node->type == AST_UNSAFE_BLOCK)
            return "unsafe";
        if (node != NULL && node->type == AST_DEFER_STMT)
            return "defer";
        return "execution";
    default:
        return "boundary";
    }
}
