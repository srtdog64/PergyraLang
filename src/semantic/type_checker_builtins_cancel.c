/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Cancel builtin ownership policy.
 */

#include "type_checker_internal.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_ownership_internal.h"
#include "diag_codes.h"

static Type *
cancel_future_payload_type(Type *task_type)
{
    if (!type_is_future_like(task_type))
        return NULL;
    return type_get_constructed_arg(task_type, 0);
}

bool
type_check_cancel_rejects_payload(ASTNode *site, Type *task_type,
                                  SemanticContext *ctx)
{
    Type *payload_type;
    OwnershipTypeClass payload_ownership;

    if (site == NULL || ctx == NULL)
        return false;

    payload_type = cancel_future_payload_type(task_type);
    if (payload_type == NULL)
        return false;

    if (type_is_constructed_named(payload_type, "Token")) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REMOTE_FUTURE_MISUSE,
            PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS,
            PGY_FIX_AWAIT_FUTURE,
            site,
            "Cancel does not support authority-bearing Token future payloads yet.\n"
            "Reason:\n"
            "- cancellation may discard a payload before authority provenance is observed\n"
            "- beta cancellation lacks a task-boundary cleanup summary for Token<T>\n"
            "Fix:\n"
            "- await the task and handle the Result/value explicitly\n"
            "- or cancel a copy-only status future instead");
        return true;
    }

    payload_ownership = semantic_classify_ownership_type(payload_type, ctx);
    if (payload_ownership == OWNERSHIP_TYPE_COPY_ONLY)
        return false;

    semantic_error_with_hints(ctx,
        PGY_CODE_SEM_REMOTE_FUTURE_MISUSE,
        PGY_CAUSE_REMOTE_FUTURE_DIRECT_ACCESS,
        PGY_FIX_AWAIT_FUTURE,
        site,
        "Cancel does not support %s future payloads yet.\n"
        "Reason:\n"
        "- cancellation may drop a task before the ownership-bearing payload is observed\n"
        "- beta cancellation lacks a task-boundary cleanup summary for %s\n"
        "Fix:\n"
        "- await the task and bind/release the payload explicitly\n"
        "- or return a copy-only cancellation/status value from the task",
        semantic_ownership_value_label(payload_ownership),
        semantic_ownership_provenance_label(payload_ownership));
    return true;
}
