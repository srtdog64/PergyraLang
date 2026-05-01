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

Type *
type_check_state_tool_builtin(ASTNode *expr, const char *name,
                              SemanticContext *ctx, bool *handled_out)
{
    if (handled_out != NULL)
        *handled_out = true;

    if (strcmp(name, "FsmNew") == 0)
        return TYPE_UNKNOWN;
    if (strcmp(name, "FsmAddState") == 0) {
        state_tool_check_prefix_args(expr, ctx, 2);
        return TYPE_INT;
    }
    if (strcmp(name, "FsmTransition") == 0) {
        state_tool_check_prefix_args(expr, ctx, 4);
        return TYPE_VOID;
    }
    if (strcmp(name, "FsmStep") == 0) {
        state_tool_check_prefix_args(expr, ctx, 2);
        return TYPE_BOOL;
    }
    if (strcmp(name, "FsmCurrent") == 0) {
        state_tool_check_prefix_args(expr, ctx, 1);
        return TYPE_INT;
    }
    if (strcmp(name, "FsmCurrentName") == 0) {
        state_tool_check_prefix_args(expr, ctx, 1);
        return TYPE_STRING;
    }
    if (strcmp(name, "TimerNew") == 0) {
        state_tool_check_prefix_args(expr, ctx, 1);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "TimerTick") == 0) {
        state_tool_check_prefix_args(expr, ctx, 2);
        return TYPE_VOID;
    }
    if (strcmp(name, "TimerRemaining") == 0) {
        state_tool_check_prefix_args(expr, ctx, 1);
        return TYPE_INT;
    }
    if (strcmp(name, "TimerDone") == 0 || strcmp(name, "CooldownReady") == 0) {
        state_tool_check_prefix_args(expr, ctx, 1);
        return TYPE_BOOL;
    }
    if (strcmp(name, "TimerReset") == 0 || strcmp(name, "CooldownTrigger") == 0) {
        state_tool_check_prefix_args(expr, ctx, 1);
        return TYPE_VOID;
    }
    if (strcmp(name, "CooldownNew") == 0) {
        state_tool_check_prefix_args(expr, ctx, 1);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "CooldownTick") == 0) {
        state_tool_check_prefix_args(expr, ctx, 2);
        return TYPE_VOID;
    }

    if (handled_out != NULL)
        *handled_out = false;
    return NULL;
}
