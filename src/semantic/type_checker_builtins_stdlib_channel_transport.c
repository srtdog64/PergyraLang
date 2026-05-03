/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Type Checker stdlib channel transport builtin dispatch.
 */

#include <string.h>

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"

typedef enum StdlibChannelTransportKind {
    STDLIB_CHANNEL_TRANSPORT_UNKNOWN = 0,
    STDLIB_CHANNEL_TRY_RECV,
    STDLIB_CHANNEL_RECV_TIMEOUT,
    STDLIB_CHANNEL_TRY_SEND,
    STDLIB_CHANNEL_SEND_TIMEOUT,
    STDLIB_CHANNEL_TRY_SEND_STATUS,
    STDLIB_CHANNEL_SEND_TIMEOUT_STATUS
} StdlibChannelTransportKind;

typedef struct StdlibChannelTransportSpec {
    const char *name;
    StdlibChannelTransportKind kind;
} StdlibChannelTransportSpec;

static StdlibChannelTransportKind
stdlib_channel_transport_kind(const char *name)
{
    static const StdlibChannelTransportSpec specs[] = {
        { "RecvTimeout", STDLIB_CHANNEL_RECV_TIMEOUT },
        { "SendTimeout", STDLIB_CHANNEL_SEND_TIMEOUT },
        { "SendTimeoutStatus", STDLIB_CHANNEL_SEND_TIMEOUT_STATUS },
        { "TryRecv", STDLIB_CHANNEL_TRY_RECV },
        { "TrySend", STDLIB_CHANNEL_TRY_SEND },
        { "TrySendStatus", STDLIB_CHANNEL_TRY_SEND_STATUS }
    };
    size_t i;

    if (name == NULL)
        return STDLIB_CHANNEL_TRANSPORT_UNKNOWN;
    for (i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        if (strcmp(name, specs[i].name) == 0)
            return specs[i].kind;
    }
    return STDLIB_CHANNEL_TRANSPORT_UNKNOWN;
}

Type *
type_check_stdlib_channel_transport_call(ASTNode *expr, const char *name,
                                         SemanticContext *ctx,
                                         bool *handled_out)
{
    StdlibChannelTransportKind kind = stdlib_channel_transport_kind(name);

    if (kind == STDLIB_CHANNEL_TRANSPORT_UNKNOWN) {
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }
    if (handled_out != NULL)
        *handled_out = true;

    switch (kind) {
    case STDLIB_CHANNEL_TRY_RECV:
        return type_check_channel_recv_builtin(expr, name, false, ctx);
    case STDLIB_CHANNEL_RECV_TIMEOUT:
        return type_check_channel_recv_builtin(expr, name, true, ctx);
    case STDLIB_CHANNEL_TRY_SEND:
        return type_check_channel_send_builtin(expr, name, false, false, ctx);
    case STDLIB_CHANNEL_SEND_TIMEOUT:
        return type_check_channel_send_builtin(expr, name, true, false, ctx);
    case STDLIB_CHANNEL_TRY_SEND_STATUS:
        return type_check_channel_send_builtin(expr, name, false, true, ctx);
    case STDLIB_CHANNEL_SEND_TIMEOUT_STATUS:
        return type_check_channel_send_builtin(expr, name, true, true, ctx);
    default:
        if (handled_out != NULL)
            *handled_out = false;
        return TYPE_UNKNOWN;
    }
}
