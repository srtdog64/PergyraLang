/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Channel state query builtins.
 */

#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

static Type *
channel_state_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

typedef struct
{
    const char *name;
    bool returns_bool;
} ChannelStateBuiltinSpec;

static const ChannelStateBuiltinSpec channel_state_specs[] = {
    { "ChannelCapacity", false },
    { "ChannelClosed", true },
    { "ChannelFull", true },
    { "ChannelLength", false },
    { "ChannelReady", true },
    { "ChannelSpace", false },
};

static const ChannelStateBuiltinSpec *
channel_state_find_spec(const char *name)
{
    if (name == NULL)
        return NULL;
    for (size_t i = 0; i < sizeof(channel_state_specs) / sizeof(channel_state_specs[0]); i++) {
        if (strcmp(channel_state_specs[i].name, name) == 0)
            return &channel_state_specs[i];
    }
    return NULL;
}

Type *
type_check_channel_state_builtin(ASTNode *expr, const char *name,
                                 SemanticContext *ctx, bool *handled_out)
{
    const ChannelStateBuiltinSpec *spec = channel_state_find_spec(name);
    Type *ch_type;

    if (handled_out != NULL)
        *handled_out = false;
    if (spec == NULL)
        return NULL;
    if (handled_out != NULL)
        *handled_out = true;

    if (!check_call_arity(expr, 1, name, ctx))
        return TYPE_UNKNOWN;
    semantic_record_effect(ctx, EFFECT_REMOTE);
    ch_type = channel_state_normalize_type(
        type_check_expression(expr->data.call.arguments[0], ctx));
    if (!type_is_constructed_named(ch_type, "Channel")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
            PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            expr->data.call.arguments[0],
            "%s requires Channel<T>, got '%s'",
            name,
            type_name_or_unknown(ch_type));
        return TYPE_UNKNOWN;
    }
    return spec->returns_bool ? TYPE_BOOL : TYPE_INT;
}
