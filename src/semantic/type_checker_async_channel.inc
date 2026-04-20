Type *
type_check_spawn_expr(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    /* Type-check the spawned function/expression */
    Type *inner = type_check_expression(expr->data.spawn_expr.function, ctx);
    Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
    return type_create_constructed(TYPE_FUTURE, args, 1);
}

bool
semantic_check_channel_send_borrowed_transfer(ASTNode *value_expr,
                                              SemanticContext *ctx,
                                              const char *value_label,
                                              const char *provenance_label,
                                              const char *snapshot_label,
                                              const char *named_binding_fix)
{
    const char *borrowed_root_name;
    Type *value_type;

    if (value_expr == NULL || ctx == NULL) {
        return false;
    }

    value_type = type_check_expression(value_expr, ctx);
    borrowed_root_name = semantic_borrowed_boundary_root_name(value_expr, ctx);

    if (semantic_channel_transfer_requires_named_binding(value_expr,
            borrowed_root_name)) {
        char *source_path = semantic_assignment_target_path(value_expr);
        semantic_report_named_channel_transfer_required(
            value_expr, ctx, "channel send",
            value_label != NULL ? value_label : "boundary value",
            source_path,
            named_binding_fix != NULL ? named_binding_fix
                                      : "bind the value first in a local variable");
        free(source_path);
        return true;
    }

    if (borrowed_root_name != NULL) {
        semantic_validate_borrowed_escape(
            value_expr, value_expr, ctx, value_type, borrowed_root_name,
            OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
            false, NULL, NULL);
        (void)provenance_label;
        (void)snapshot_label;
        return true;
    }

    return false;
}

bool
semantic_check_borrowed_channel_transfer(ASTNode *value_expr,
                                         Type *value_type,
                                         SemanticContext *ctx,
                                         const char *value_label,
                                         const char *named_binding_fix)
{
    const char *borrowed_root_name;

    if (value_expr == NULL || ctx == NULL)
        return false;

    borrowed_root_name = semantic_borrowed_boundary_root_name(value_expr, ctx);
    if (semantic_channel_transfer_requires_named_binding(value_expr,
            borrowed_root_name)) {
        char *source_path = semantic_assignment_target_path(value_expr);
        semantic_report_named_channel_transfer_required(
            value_expr, ctx, "send",
            value_label != NULL ? value_label : "boundary value",
            source_path,
            named_binding_fix != NULL ? named_binding_fix
                                      : "bind the value first in a local variable");
        free(source_path);
        return true;
    }

    if (borrowed_root_name != NULL) {
        semantic_validate_borrowed_escape(
            value_expr, value_expr, ctx, value_type, borrowed_root_name,
            OWNERSHIP_CONSUMER_CHANNEL_SEND, NULL, NULL, NULL,
            false, NULL, NULL);
        return true;
    }

    return false;
}

bool
semantic_validate_channel_transport_ownership(ASTNode *value_expr,
                                              Type *value_type,
                                              SemanticContext *ctx,
                                              const char *transport_name,
                                              OwnershipTypeClass expected_class,
                                              OwnershipTypeClass element_ownership,
                                              OwnershipTypeClass value_ownership,
                                              const char *contract_label,
                                              const char *expected_name,
                                              const char *actual_name,
                                              const char *value_label,
                                              const char *named_binding_fix)
{
    if (element_ownership != expected_class
        && value_ownership != expected_class) {
        return false;
    }

    if (element_ownership != expected_class
        || value_ownership != expected_class) {
        semantic_report_channel_transport_mismatch(
            value_expr,
            ctx,
            transport_name,
            contract_label,
            expected_name,
            actual_name);
        return true;
    }

    if (semantic_check_borrowed_channel_transfer(
            value_expr, value_type, ctx, value_label, named_binding_fix)) {
        return true;
    }

    return false;
}

void
semantic_report_channel_transport_policy(ASTNode *site,
                                         SemanticContext *ctx,
                                         const char *transport_name,
                                         const char *why_text,
                                         const char *fix_text)
{
    semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
        "semantic:channel:transport_rule_violation",
        "keep-handle-local-or-send-inner-value",
        site,
        "%s.\n"
        "Reason:\n"
        "- %s\n"
        "Fix:\n"
        "- %s",
        transport_name != NULL ? transport_name : "Channel transport is not allowed",
        why_text != NULL ? why_text : "channel transport policy is not closed for this value yet",
        fix_text != NULL ? fix_text : "keep the value local or use a supported transport path");
}

void
semantic_report_channel_transport_mismatch(ASTNode *site,
                                           SemanticContext *ctx,
                                           const char *transport_name,
                                           const char *contract_label,
                                           const char *expected_name,
                                           const char *actual_name)
{
    semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
        "semantic:channel:transport_rule_violation",
        "align-channel-element-type",
        site,
        "%s %s mismatch: expected '%s', got '%s'.\n"
        "Reason:\n"
        "- channel element type and sent value must agree on the same %s contract\n"
        "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
        "Fix:\n"
        "- send a value of type '%s'\n"
        "- or change the channel element type to match '%s'",
        transport_name != NULL ? transport_name : "Channel send",
        contract_label != NULL ? contract_label : "value",
        expected_name != NULL ? expected_name : "<expected>",
        actual_name != NULL ? actual_name : "<actual>",
        contract_label != NULL ? contract_label : "value",
        expected_name != NULL ? expected_name : "<expected>",
        actual_name != NULL ? actual_name : "<actual>",
        expected_name != NULL ? expected_name : "<expected>",
        actual_name != NULL ? actual_name : "<actual>");
}

bool
semantic_channel_transfer_requires_named_binding(ASTNode *value_expr,
                                                 const char *borrowed_root_name)
{
    if (value_expr == NULL)
        return false;
    return value_expr->type != AST_IDENTIFIER && borrowed_root_name == NULL;
}

void
semantic_report_named_channel_transfer_required(ASTNode *site,
                                                SemanticContext *ctx,
                                                const char *transport_name,
                                                const char *value_label,
                                                const char *source_path,
                                                const char *bind_fix)
{
    semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
        "semantic:channel:transport_rule_violation",
        "bind-to-named-variable-before-send",
        site,
        "%s %s must transfer from a named variable%s%s%s.\n"
        "Reason:\n"
        "- %s transfer at a channel boundary must point to one concrete source binding\n"
        "- unnamed expressions make moved-here provenance ambiguous\n"
        "Fix:\n"
        "- %s\n"
        "- then send that named variable",
        value_label != NULL ? value_label : "boundary value",
        transport_name != NULL ? transport_name : "send",
        source_path != NULL ? " instead of '" : "",
        source_path != NULL ? source_path : "",
        source_path != NULL ? "'" : "",
        value_label != NULL ? value_label : "boundary value",
        bind_fix != NULL ? bind_fix : "bind the value first in a local variable");
}

static bool
semantic_channel_type_is_token(const Type *type)
{
    return type_is_constructed_named(type, "Token");
}

Type *
type_check_channel_send(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    /* Check channel and value types */
    Type *channel_type = type_check_expression(expr->data.channel_send.channel, ctx);
    Type *value_type = type_check_expression(expr->data.channel_send.value, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "align-channel-element-type",
            expr->data.channel_send.channel,
            "Channel send requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_VOID;
    }

    Type *element_type = channel_type->data.constructed.args[0];
    OwnershipTypeClass element_ownership =
        semantic_classify_ownership_type(element_type, ctx);
    OwnershipTypeClass value_ownership =
        semantic_classify_ownership_type(value_type, ctx);

    if (semantic_channel_type_is_token(element_type)
        || semantic_channel_type_is_token(value_type)) {
        semantic_report_channel_transport_policy(
            expr->data.channel_send.value, ctx,
            "Channels cannot transport Token values yet",
            "token transfer would cross the channel boundary without a closed authority contract\n"
            "- authority-bearing token state must remain local for now",
            "keep the token local to the authorized flow\n"
            "- or send a plain projection/value instead");
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
        || value_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
        if (semantic_validate_channel_transport_ownership(
                expr->data.channel_send.value, value_type, ctx,
                "Channel send",
                OWNERSHIP_TYPE_SUBJECT_IDENTITY,
                element_ownership, value_ownership,
                "subject",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                "subject",
                "bind the subject first in a local variable")) {
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || value_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        if (element_ownership != OWNERSHIP_TYPE_MOVE_ONLY
            || value_ownership != OWNERSHIP_TYPE_MOVE_ONLY) {
            semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
                "semantic:channel:transport_rule_violation",
                "align-channel-element-type",
                expr->data.channel_send.value,
                "Channel send movable-resource mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the movable resource contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type));
            return TYPE_VOID;
        }
        if (semantic_check_channel_send_borrowed_transfer(
                expr->data.channel_send.value, ctx,
                "movable resource",
                "movable-resource provenance",
                "copied/value/projection result",
                "bind the value first by storing the movable resource in a local variable")) {
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE
        || value_ownership == OWNERSHIP_TYPE_ANCHORED_HANDLE) {
        if (semantic_validate_channel_transport_ownership(
                expr->data.channel_send.value, value_type, ctx,
                "Channel send",
                OWNERSHIP_TYPE_ANCHORED_HANDLE,
                element_ownership, value_ownership,
                "anchored-handle",
                resource_handle_display_name(element_type),
                resource_handle_display_name(value_type),
                "anchored handle",
                "bind the anchored handle first in a local variable")) {
            return TYPE_VOID;
        }
        return TYPE_VOID;
    }

    if (element_ownership == OWNERSHIP_TYPE_BORROW_TRACKED
        || value_ownership == OWNERSHIP_TYPE_BORROW_TRACKED) {
        if (semantic_validate_channel_transport_ownership(
                expr->data.channel_send.value, value_type, ctx,
                "Channel send",
                OWNERSHIP_TYPE_BORROW_TRACKED,
                element_ownership, value_ownership,
                "boundary value",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                "boundary value",
                "bind the value first in a local variable")) {
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    require_assignable(value_type, element_type, expr->data.channel_send.value, ctx);
    return TYPE_VOID;
}

Type *
type_check_channel_recv(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    Type *channel_type = type_check_expression(expr->data.channel_recv.channel, ctx);
    if (channel_type == NULL
        || channel_type->kind != TYPE_KIND_CONSTRUCTED
        || !type_equals(channel_type->data.constructed.constructor, TYPE_CHANNEL)
        || channel_type->data.constructed.arg_count != 1) {
        semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "align-channel-element-type",
            expr->data.channel_recv.channel,
            "Channel recv requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_UNKNOWN;
    }

    if (semantic_channel_type_is_token(channel_type->data.constructed.args[0])) {
        semantic_report_channel_transport_policy(
            expr->data.channel_recv.channel, ctx,
            "Channels cannot yield Token values yet",
            "receive would materialize token state outside the closed authority flow\n"
            "- authority-bearing token state remains local-only at channel boundaries under the current authority contract",
            "receive a plain value instead\n"
            "- or keep the token local");
        return TYPE_UNKNOWN;
    }

    return channel_type->data.constructed.args[0];
}
