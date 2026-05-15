/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * FSM, Timer, and Cooldown builtin family.
 */

#include <stdlib.h>
#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

static void
state_tool_check_prefix_args(ASTNode *expr, SemanticContext *ctx, size_t count)
{
    if (expr == NULL)
        return;
    if (ast_call_arg_count(expr) < count)
        return;
    for (size_t i = 0; i < count; i++)
        type_check_expression(ast_call_argument(expr, i), ctx);
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
    { "CooldownNew", 1, STATE_TOOL_RET_UNKNOWN },
    { "CooldownReady", 1, STATE_TOOL_RET_BOOL },
    { "CooldownTick", 2, STATE_TOOL_RET_VOID },
    { "CooldownTrigger", 1, STATE_TOOL_RET_VOID },
    { "FsmAddState", 2, STATE_TOOL_RET_INT },
    { "FsmCurrent", 1, STATE_TOOL_RET_INT },
    { "FsmCurrentName", 1, STATE_TOOL_RET_STRING },
    { "FsmNew", 0, STATE_TOOL_RET_UNKNOWN },
    { "FsmStep", 2, STATE_TOOL_RET_BOOL },
    { "FsmTransition", 4, STATE_TOOL_RET_VOID },
    { "TimerDone", 1, STATE_TOOL_RET_BOOL },
    { "TimerNew", 1, STATE_TOOL_RET_UNKNOWN },
    { "TimerRemaining", 1, STATE_TOOL_RET_INT },
    { "TimerReset", 1, STATE_TOOL_RET_VOID },
    { "TimerTick", 2, STATE_TOOL_RET_VOID },
};

static int
state_tool_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const StateToolBuiltinSpec *spec = (const StateToolBuiltinSpec *)entry;

    return strcmp(name, spec->name);
}

static const StateToolBuiltinSpec *
state_tool_find_spec(const char *name)
{
    const StateToolBuiltinSpec *match;

    if (name == NULL)
        return NULL;
    match = (const StateToolBuiltinSpec *)bsearch(
        &name, state_tool_specs,
        sizeof(state_tool_specs) / sizeof(state_tool_specs[0]),
        sizeof(state_tool_specs[0]), state_tool_compare);
    return match;
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
