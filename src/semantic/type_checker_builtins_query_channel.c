#include <stdio.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_query.h"
#include "type_checker_channel_transport_internal.h"
#include "type_checker_ownership_internal.h"

static const char *
builtin_type_name_or_unknown(const Type *type)
{
    return (type != NULL && type->name != NULL) ? type->name : "<unknown>";
}

static Type *
channel_builtin_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static Type *
channel_builtin_element_type(ASTNode *expr, size_t channel_arg_index,
                             const char *name, SemanticContext *ctx)
{
    Type *ch_type = channel_builtin_normalize_type(type_check_expression(
        ast_call_argument(expr, channel_arg_index), ctx));
    if (!type_is_constructed_named(ch_type, "Channel")) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_CHANNEL_TRANSPORT_INVALID,
            PGY_CAUSE_CHANNEL_TRANSPORT_RULE_VIOLATION,
            PGY_FIX_ALIGN_CHANNEL_ELEMENT_TYPE,
            ast_call_argument(expr, channel_arg_index),
            "%s requires Channel<T>, got '%s'", name,
            builtin_type_name_or_unknown(ch_type));
        return TYPE_UNKNOWN;
    }
    return channel_builtin_normalize_type(type_get_constructed_arg(ch_type, 0));
}

static Type *
channel_builtin_recv_result(Type *element_type, const char *name,
                            ASTNode *site, SemanticContext *ctx)
{
    if (type_is_constructed_named(element_type, "Token")) {
        char message[256];
        snprintf(message, sizeof(message),
            "%s cannot yield Token values yet", name);
        semantic_report_channel_transport_policy(
            site, ctx, message,
            "receive would materialize authority-bearing token state outside the closed flow",
            "receive a plain value instead");
        return TYPE_UNKNOWN;
    }
    if (type_is_constructed_named(element_type, "Slice")) {
        char message[256];
        snprintf(message, sizeof(message),
            "%s cannot yield borrowed Slice values yet", name);
        semantic_report_channel_transport_policy(
            site, ctx, message,
            "receive would materialize a borrowed view without closed backing-owner provenance\n"
            "- beta slice transport needs explicit owner/copy/pin evidence before it can be trusted",
            "receive an owning Array<T> copy or projection/value result instead\n"
            "- keep Slice<T> use local to the producing synchronous boundary");
        return TYPE_UNKNOWN;
    }
    OwnershipTypeClass element_ownership =
        semantic_classify_ownership_type(element_type, ctx);

    if (element_ownership != OWNERSHIP_TYPE_COPY_ONLY) {
        char message[256];
        char reason[256];
        snprintf(message, sizeof(message),
            "%s does not support %s channels yet",
            name, semantic_ownership_value_label(element_ownership));
        snprintf(reason, sizeof(reason),
            "%s receive remains restricted on this builtin channel surface",
            semantic_ownership_provenance_label(element_ownership));
        semantic_report_channel_transport_policy(
            site, ctx, message,
            reason,
            "use blocking '<-' receive and bind the result first");
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_OPTION, element_type);
}

Type *
type_check_channel_send_builtin(ASTNode *expr, const char *name,
                                bool has_timeout, bool detailed_status,
                                SemanticContext *ctx)
{
    if (!check_call_arity(expr, has_timeout ? 3 : 2, name, ctx))
        return TYPE_UNKNOWN;

    semantic_record_body_summary(ctx, BODY_SUMMARY_SENDS_CHANNEL);
    semantic_record_effect(ctx, EFFECT_REMOTE);
    if (semantic_reject_active_slot_view_boundary(expr, ctx,
            "channel handoff boundary",
            "channel send may hand the value to another execution frontier",
            "move the channel send")) {
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }
    Type *element_type = channel_builtin_element_type(expr, 0, name, ctx);
    Type *value_type = channel_builtin_normalize_type(
        type_check_expression(ast_call_argument(expr, 1), ctx));
    OwnershipTypeClass element_ownership =
        semantic_classify_ownership_type(element_type, ctx);
    OwnershipTypeClass value_ownership =
        semantic_classify_ownership_type(value_type, ctx);

    if (has_timeout) {
        Type *timeout_type = channel_builtin_normalize_type(
            type_check_expression(ast_call_argument(expr, 2), ctx));
        require_assignable(timeout_type, TYPE_INT,
            ast_call_argument(expr, 2), ctx);
    }

    if (element_type == TYPE_UNKNOWN) {
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (type_is_constructed_named(element_type, "Token")
        || type_is_constructed_named(value_type, "Token")) {
        char message[256];
        snprintf(message, sizeof(message),
            "%s cannot transport Token values yet", name);
        semantic_report_channel_transport_policy(
            ast_call_argument(expr, 1), ctx, message,
            "authority-bearing token state must stay local to the authorized flow at this channel surface",
            "keep the token local to the authorized flow\n"
            "- or send a plain projection/value instead");
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (type_is_constructed_named(element_type, "Slice")
        || type_is_constructed_named(value_type, "Slice")) {
        char message[256];
        snprintf(message, sizeof(message),
            "%s does not support borrowed Slice transport yet", name);
        semantic_report_channel_transport_policy(
            ast_call_argument(expr, 1), ctx, message,
            "Slice<T> is a borrowed view over another owner\n"
            "- non-blocking channel send may outlive or reorder the backing owner provenance\n"
            "- beta slice transport needs explicit owner/copy/pin evidence before it can be trusted",
            "send an owning Array<T> copy or projection/value result instead\n"
            "- keep Slice<T> use local to the current synchronous boundary");
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || value_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        char message[256];
        snprintf(message, sizeof(message),
            "%s does not support slot handle (movable) sends yet", name);
        semantic_report_channel_transport_policy(
            ast_call_argument(expr, 1), ctx, message,
            "slot handle (movable) transfer on this builtin channel surface is still restricted",
            "use blocking 'ch <- value' so ownership transfer stays explicit");
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
        || value_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
        if (semantic_validate_channel_transport_ownership(
                ast_call_argument(expr, 1), value_type, ctx,
                name,
                OWNERSHIP_TYPE_SUBJECT_IDENTITY,
                element_ownership, value_ownership,
                "subject",
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                "subject",
                "bind the subject first in a local variable")) {
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_BORROW_TRACKED
        || value_ownership == OWNERSHIP_TYPE_BORROW_TRACKED) {
        if (semantic_validate_channel_transport_ownership(
                ast_call_argument(expr, 1), value_type, ctx,
                name,
                OWNERSHIP_TYPE_BORROW_TRACKED,
                element_ownership, value_ownership,
                "boundary value",
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                "boundary value",
                "bind the boundary value first in a local variable")) {
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
    }

    if (element_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE
        || value_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
        if (semantic_validate_channel_transport_ownership(
                ast_call_argument(expr, 1), value_type, ctx,
                name,
                OWNERSHIP_TYPE_ANCHORED_HANDLE,
                element_ownership, value_ownership,
                "slot handle (anchored)",
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                "slot handle (anchored)",
                "bind the slot handle (anchored) first in a local variable")) {
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
    }

    require_assignable(value_type, element_type, ast_call_argument(expr, 1), ctx);
    return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                           : TYPE_BOOL;
}

Type *
type_check_channel_recv_builtin(ASTNode *expr, const char *name,
                                bool has_timeout, SemanticContext *ctx)
{
    if (!check_call_arity(expr, has_timeout ? 2 : 1, name, ctx))
        return TYPE_UNKNOWN;

    semantic_record_effect(ctx, EFFECT_REMOTE);
    if (semantic_reject_active_slot_view_boundary(expr, ctx,
            "channel handoff boundary",
            "channel receive may observe work from another execution frontier",
            "move the channel receive")) {
        return TYPE_UNKNOWN;
    }
    Type *element_type = channel_builtin_element_type(expr, 0, name, ctx);
    if (has_timeout) {
        Type *timeout_type = channel_builtin_normalize_type(
            type_check_expression(ast_call_argument(expr, 1), ctx));
        require_assignable(timeout_type, TYPE_INT,
            ast_call_argument(expr, 1), ctx);
    }
    /* Token / movable / anchored / capability rejection lives in
     * channel_builtin_recv_result so the rule lives in one place. */
    return channel_builtin_recv_result(
        element_type, name, ast_call_argument(expr, 0), ctx);
}

Type *
type_check_channel_close_builtin(ASTNode *expr, SemanticContext *ctx)
{
    Type *element_type;
    OwnershipTypeClass element_ownership;

    if (!check_call_arity(expr, 1, "ChannelClose", ctx))
        return TYPE_UNKNOWN;

    semantic_record_effect(ctx, EFFECT_REMOTE);
    if (semantic_reject_active_slot_view_boundary(expr, ctx,
            "channel close boundary",
            "channel close may discard or unblock work on another execution frontier",
            "move channel close")) {
        return TYPE_UNKNOWN;
    }
    element_type = channel_builtin_element_type(expr, 0, "ChannelClose", ctx);
    if (element_type == TYPE_UNKNOWN)
        return TYPE_UNKNOWN;

    if (type_is_constructed_named(element_type, "Token")) {
        semantic_report_channel_transport_policy(
            ast_call_argument(expr, 0), ctx,
            "ChannelClose cannot close Token channels yet",
            "closing a channel may discard queued authority-bearing token state",
            "keep token channels local and drain/handle token state explicitly");
        return TYPE_UNKNOWN;
    }

    element_ownership = semantic_classify_ownership_type(element_type, ctx);
    if (element_ownership != OWNERSHIP_TYPE_COPY_ONLY) {
        char message[256];
        char reason[256];
        snprintf(message, sizeof(message),
            "ChannelClose does not support %s channels yet",
            semantic_ownership_value_label(element_ownership));
        snprintf(reason, sizeof(reason),
            "closing a channel may discard queued %s without a cleanup summary",
            semantic_ownership_provenance_label(element_ownership));
        semantic_report_channel_transport_policy(
            ast_call_argument(expr, 0), ctx, message, reason,
            "drain the channel with blocking '<-' into named bindings first");
        return TYPE_UNKNOWN;
    }

    return TYPE_VOID;
}

