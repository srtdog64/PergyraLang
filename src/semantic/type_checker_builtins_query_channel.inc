static Type *
channel_builtin_element_type(ASTNode *expr, size_t channel_arg_index,
                             const char *name, SemanticContext *ctx)
{
    Type *ch_type = type_check_expression(
        expr->data.call.arguments[channel_arg_index], ctx);
    if (!type_is_constructed_named(ch_type, "Channel")) {
        semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "align-channel-element-type",
            expr->data.call.arguments[channel_arg_index],
            "%s requires Channel<T>, got '%s'", name,
            ch_type != NULL ? ch_type->name : "<null>");
        return TYPE_UNKNOWN;
    }
    return type_get_constructed_arg(ch_type, 0);
}

static Type *
channel_builtin_recv_result(Type *element_type, const char *name,
                            ASTNode *site, SemanticContext *ctx)
{
    OwnershipTypeClass element_ownership =
        semantic_classify_ownership_type(element_type, ctx);

    if (element_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        char message[256];
        char fix[256];
        snprintf(message, sizeof(message),
            "%s does not support movable resource channels yet", name);
        snprintf(fix, sizeof(fix),
            "use blocking '<-' receive and bind the result first");
        semantic_report_channel_transport_policy(
            site, ctx, message,
            "movable resource receive remains restricted on this builtin channel surface",
            fix);
        return TYPE_UNKNOWN;
    }
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
    return wrap_constructed(TYPE_OPTION, element_type);
}

static Type *
type_check_channel_send_builtin(ASTNode *expr, const char *name,
                                bool has_timeout, bool detailed_status,
                                SemanticContext *ctx)
{
    if (!check_call_arity(expr, has_timeout ? 3 : 2, name, ctx))
        return TYPE_UNKNOWN;

    semantic_record_effect(ctx, EFFECT_REMOTE);
    Type *element_type = channel_builtin_element_type(expr, 0, name, ctx);
    Type *value_type = type_check_expression(expr->data.call.arguments[1], ctx);
    OwnershipTypeClass element_ownership =
        semantic_classify_ownership_type(element_type, ctx);
    OwnershipTypeClass value_ownership =
        semantic_classify_ownership_type(value_type, ctx);

    if (has_timeout) {
        require_assignable(type_check_expression(expr->data.call.arguments[2], ctx),
            TYPE_INT, expr->data.call.arguments[2], ctx);
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
            expr->data.call.arguments[1], ctx, message,
            "authority-bearing token state must stay local to the authorized flow at this channel surface",
            "keep the token local to the authorized flow\n"
            "- or send a plain projection/value instead");
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || value_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        char message[256];
        snprintf(message, sizeof(message),
            "%s does not support movable resource sends yet", name);
        semantic_report_channel_transport_policy(
            expr->data.call.arguments[1], ctx, message,
            "movable resource transfer on this builtin channel surface is still restricted",
            "use blocking 'ch <- value' so ownership transfer stays explicit");
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
        || value_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
        if (semantic_validate_channel_transport_ownership(
                expr->data.call.arguments[1], value_type, ctx,
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
                expr->data.call.arguments[1], value_type, ctx,
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
                expr->data.call.arguments[1], value_type, ctx,
                name,
                OWNERSHIP_TYPE_ANCHORED_HANDLE,
                element_ownership, value_ownership,
                "anchored-handle",
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                "anchored handle",
                "bind the anchored handle first in a local variable")) {
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
    }

    require_assignable(value_type, element_type, expr->data.call.arguments[1], ctx);
    return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                           : TYPE_BOOL;
}

static Type *
type_check_channel_recv_builtin(ASTNode *expr, const char *name,
                                bool has_timeout, SemanticContext *ctx)
{
    if (!check_call_arity(expr, has_timeout ? 2 : 1, name, ctx))
        return TYPE_UNKNOWN;

    semantic_record_effect(ctx, EFFECT_REMOTE);
    Type *element_type = channel_builtin_element_type(expr, 0, name, ctx);
    if (has_timeout) {
        require_assignable(type_check_expression(expr->data.call.arguments[1], ctx),
            TYPE_INT, expr->data.call.arguments[1], ctx);
    }
    if (type_is_constructed_named(element_type, "Token")) {
        semantic_error_with_hints(ctx, "PGY_SEM_ANCHORED_HANDLE_COPY", "semantic:anchored_handle:copy_attempt", "use-move-or-retain-binding", expr->data.call.arguments[0],
            "%s cannot yield Token values yet; receive a plain value instead",
            name);
        return TYPE_UNKNOWN;
    }
    return channel_builtin_recv_result(
        element_type, name, expr->data.call.arguments[0], ctx);
}
