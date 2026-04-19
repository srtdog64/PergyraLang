Type *
type_check_spawn_expr(ASTNode *expr, SemanticContext *ctx)
{
    semantic_record_effect(ctx, EFFECT_REMOTE);
    /* Type-check the spawned function/expression */
    Type *inner = type_check_expression(expr->data.spawn_expr.function, ctx);
    Type *args[1] = { inner != NULL ? inner : TYPE_UNKNOWN };
    return type_create_constructed(TYPE_FUTURE, args, 1);
}

static bool
semantic_check_channel_send_borrowed_transfer(ASTNode *value_expr,
                                              SemanticContext *ctx,
                                              const char *value_label,
                                              const char *provenance_label,
                                              const char *snapshot_label,
                                              const char *named_binding_fix)
{
    const char *borrowed_root_name;

    if (value_expr == NULL || ctx == NULL) {
        return false;
    }

    borrowed_root_name = semantic_borrowed_boundary_root_name(value_expr, ctx);

    if (value_expr->type != AST_IDENTIFIER
        && borrowed_root_name == NULL) {
        char *source_path = semantic_assignment_target_path(value_expr);
        semantic_error(ctx, value_expr,
            "%s channel sends must transfer from a named variable%s%s%s.\n"
            "Reason:\n"
            "- ownership transfer at a channel boundary must point to one concrete source binding\n"
            "- unnamed expressions make moved-here provenance ambiguous\n"
            "Fix:\n"
            "- %s\n"
            "- then send that named variable",
            value_label != NULL ? value_label : "Boundary value",
            source_path != NULL ? " instead of '" : "",
            source_path != NULL ? source_path : "",
            source_path != NULL ? "'" : "",
            named_binding_fix != NULL ? named_binding_fix
                                      : "bind the value first in a local variable");
        free(source_path);
        return true;
    }

    if (borrowed_root_name != NULL) {
        char *source_path = semantic_assignment_target_path(value_expr);
        semantic_error_with_hints(ctx, "PGY_SEM_BORROW_ESCAPE",
            "semantic:borrow_escape", "change-ref-to-own-or-stop-escape",
            value_expr,
            "Borrowed ref %s '%s' cannot escape through channel send from '%s'.\n"
            "Reason:\n"
            "- consumer path is function '%s'\n"
            "- '%s' entered this function as a borrowed 'ref' %s\n"
            "- '%s' is derived from that borrowed %s\n"
            "- channel send would transfer that borrow beyond the current call boundary\n"
            "Fix:\n"
            "- send a %s instead\n"
            "- or change the current parameter to 'own' if transfer is intended",
            value_label != NULL ? value_label : "boundary value",
            borrowed_root_name,
            source_path != NULL ? source_path : borrowed_root_name,
            semantic_current_consumer_name(ctx),
            borrowed_root_name,
            value_label != NULL ? value_label : "boundary value",
            source_path != NULL ? source_path : borrowed_root_name,
            provenance_label != NULL ? provenance_label : "boundary provenance",
            snapshot_label != NULL ? snapshot_label
                                   : "copied/value/projection result");
        free(source_path);
        return true;
    }

    return false;
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
        semantic_error(ctx, expr->data.channel_send.channel,
            "Channel send requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_VOID;
    }

    Type *element_type = channel_type->data.constructed.args[0];

    if (type_is_anchored_resource_handle(element_type)
        || type_is_anchored_resource_handle(value_type)) {
        semantic_error(ctx, expr->data.channel_send.value,
            "Channels cannot transport anchored resource handles (Slot/SecureSlot/DeviceSlot) yet.\n"
            "Reason:\n"
            "- channel send would move an anchored handle outside its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at channel boundaries in the closed subset\n"
            "Fix:\n"
            "- send the inner value instead\n"
            "- or keep the anchored handle local");
        return TYPE_VOID;
    }

    if (type_is_capability_bearing(element_type)
        || type_is_capability_bearing(value_type)) {
        semantic_error(ctx, expr->data.channel_send.value,
            "Channels cannot transport capability-bearing values (SecureSlot/Token) yet.\n"
            "Reason:\n"
            "- capability-bearing values would cross the channel boundary without a closed authority contract\n"
            "- authorized flow must keep capability-bearing state local for now\n"
            "Fix:\n"
            "- keep capability-bearing state local to the authorized flow\n"
            "- or send a plain projection/value instead");
        return TYPE_VOID;
    }

    if (type_is_subject_type(element_type, ctx)
        || type_is_subject_type(value_type, ctx)) {
        if (!type_is_subject_type(element_type, ctx)
            || !type_is_subject_type(value_type, ctx)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Channel send subject mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the subject boundary contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type));
            return TYPE_VOID;
        }
        if (semantic_check_channel_send_borrowed_transfer(
                expr->data.channel_send.value, ctx,
                "subject",
                "subject provenance",
                "projection/object/tobject/value result",
                "bind the subject first in a local variable")) {
            return TYPE_VOID;
        }
        consume_qubit_value(expr->data.channel_send.value, ctx, "sent through channel");
        return TYPE_VOID;
    }

    if (type_is_movable_resource_handle(element_type)
        || type_is_movable_resource_handle(value_type)) {
        if (!type_is_movable_resource_handle(element_type)
            || !type_is_movable_resource_handle(value_type)) {
            semantic_error(ctx, expr->data.channel_send.value,
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

    if (type_requires_boundary_borrow_tracking(element_type, ctx)
        || type_requires_boundary_borrow_tracking(value_type, ctx)) {
        if (!type_requires_boundary_borrow_tracking(element_type, ctx)
            || !type_requires_boundary_borrow_tracking(value_type, ctx)
            || type_is_subject_type(element_type, ctx)
            || type_is_subject_type(value_type, ctx)
            || type_is_movable_resource_handle(element_type)
            || type_is_movable_resource_handle(value_type)) {
            semantic_error(ctx, expr->data.channel_send.value,
                "Channel send boundary value mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the same boundary value contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type),
                type_name_or_unknown(element_type),
                type_name_or_unknown(value_type));
            return TYPE_VOID;
        }
        if (semantic_check_channel_send_borrowed_transfer(
                expr->data.channel_send.value, ctx,
                "boundary value",
                "boundary provenance",
                "copied/value/projection result",
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
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channel recv requires Channel<T>, got '%s'",
            channel_type != NULL ? channel_type->name : "<null>");
        return TYPE_UNKNOWN;
    }

    if (type_is_anchored_resource_handle(channel_type->data.constructed.args[0])) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channels cannot yield anchored resource handles (Slot/SecureSlot/DeviceSlot) yet.\n"
            "Reason:\n"
            "- receive would materialize an anchored handle beyond its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at channel boundaries in the closed subset\n"
            "Fix:\n"
            "- receive a plain value instead\n"
            "- or keep the anchored handle local");
        return TYPE_UNKNOWN;
    }

    if (type_is_capability_bearing(channel_type->data.constructed.args[0])) {
        semantic_error(ctx, expr->data.channel_recv.channel,
            "Channels cannot yield capability-bearing values (SecureSlot/Token) yet.\n"
            "Reason:\n"
            "- receive would materialize capability-bearing state outside the closed authority flow\n"
            "- capability-bearing values are still local-only at channel boundaries in the closed subset\n"
            "Fix:\n"
            "- receive a plain value instead\n"
            "- or keep capability-bearing state local");
        return TYPE_UNKNOWN;
    }

    return channel_type->data.constructed.args[0];
}

