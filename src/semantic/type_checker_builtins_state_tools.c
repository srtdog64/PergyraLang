/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * FSM, Timer, and Cooldown builtin family.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

static void
state_tool_check_prefix_args(ASTNode *expr, SemanticContext *ctx, size_t count)
{
    if (expr == NULL)
        return;
    if (expr->data.call.arg_count < count)
        return;
    for (size_t i = 0; i < count; i++)
        type_check_expression(expr->data.call.arguments[i], ctx);
}

typedef enum
{
    STATE_TOOL_RET_UNKNOWN,
    STATE_TOOL_RET_INT,
    STATE_TOOL_RET_VOID,
    STATE_TOOL_RET_BOOL,
    STATE_TOOL_RET_STRING
} StateToolReturnKind;

typedef struct
{
    const char *name;
    size_t prefix_arg_count;
    StateToolReturnKind return_kind;
} StateToolBuiltinSpec;

static const StateToolBuiltinSpec state_tool_specs[] = {
    { "FsmNew", 0, STATE_TOOL_RET_UNKNOWN },
    { "FsmAddState", 2, STATE_TOOL_RET_INT },
    { "FsmTransition", 4, STATE_TOOL_RET_VOID },
    { "FsmStep", 2, STATE_TOOL_RET_BOOL },
    { "FsmCurrent", 1, STATE_TOOL_RET_INT },
    { "FsmCurrentName", 1, STATE_TOOL_RET_STRING },
    { "TimerNew", 1, STATE_TOOL_RET_UNKNOWN },
    { "TimerTick", 2, STATE_TOOL_RET_VOID },
    { "TimerRemaining", 1, STATE_TOOL_RET_INT },
    { "TimerDone", 1, STATE_TOOL_RET_BOOL },
    { "TimerReset", 1, STATE_TOOL_RET_VOID },
    { "CooldownNew", 1, STATE_TOOL_RET_UNKNOWN },
    { "CooldownTick", 2, STATE_TOOL_RET_VOID },
    { "CooldownReady", 1, STATE_TOOL_RET_BOOL },
    { "CooldownTrigger", 1, STATE_TOOL_RET_VOID },
};

static const StateToolBuiltinSpec *
state_tool_find_spec(const char *name)
{
    if (name == NULL)
        return NULL;
    for (size_t i = 0; i < sizeof(state_tool_specs) / sizeof(state_tool_specs[0]); i++) {
        if (strcmp(state_tool_specs[i].name, name) == 0)
            return &state_tool_specs[i];
    }
    return NULL;
}

static Type *
state_tool_return_type(StateToolReturnKind kind)
{
    switch (kind) {
    case STATE_TOOL_RET_INT:
        return TYPE_INT;
    case STATE_TOOL_RET_VOID:
        return TYPE_VOID;
    case STATE_TOOL_RET_BOOL:
        return TYPE_BOOL;
    case STATE_TOOL_RET_STRING:
        return TYPE_STRING;
    case STATE_TOOL_RET_UNKNOWN:
    default:
        return TYPE_UNKNOWN;
    }
}

Type *
type_check_state_tool_builtin(ASTNode *expr, const char *name,
                              SemanticContext *ctx, bool *handled_out)
{
    const StateToolBuiltinSpec *spec = state_tool_find_spec(name);

    if (handled_out != NULL)
        *handled_out = spec != NULL;
    if (spec == NULL)
        return NULL;

    state_tool_check_prefix_args(expr, ctx, spec->prefix_arg_count);
    return state_tool_return_type(spec->return_kind);
}
