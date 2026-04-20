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

    if (type_is_anchored_resource_handle(element_type)) {
        semantic_error_with_hints(ctx, "PGY_SEM_ANCHORED_HANDLE_COPY", "semantic:anchored_handle:copy_attempt", "use-move-or-retain-binding", site,
            "%s cannot yield anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; receive a plain value instead",
            name);
        return TYPE_UNKNOWN;
    }
    if (element_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "keep-handle-local-or-send-inner-value",
            site,
            "%s does not support movable resource channels yet; use blocking '<-' receive and bind the result first",
            name);
        return TYPE_UNKNOWN;
    }
    if (type_is_capability_bearing(element_type)) {
        semantic_error_with_hints(ctx, "PGY_SEM_ANCHORED_HANDLE_COPY", "semantic:anchored_handle:copy_attempt", "use-move-or-retain-binding", site,
            "%s cannot yield capability-bearing values (SecureSlot/Token) yet; receive a plain value instead",
            name);
        return TYPE_UNKNOWN;
    }
    return wrap_constructed(TYPE_OPTION, element_type);
}

static bool
builtin_require_named_channel_transfer(ASTNode *value_expr,
                                       const char *builtin_name,
                                       const char *value_label_lower,
                                       const char *bind_fix,
                                       const char *borrowed_root_name,
                                       char *source_path,
                                       SemanticContext *ctx)
{
    char *reason_line = NULL;

    if (value_expr == NULL)
        return false;
    if (value_expr->type == AST_IDENTIFIER || borrowed_root_name != NULL)
        return false;

    if (borrowed_root_name != NULL) {
        const char *rendered_path =
            source_path != NULL ? source_path : "<expression>";
        size_t needed = snprintf(NULL, 0,
            "- '%s' is derived from borrowed source '%s'\n",
            rendered_path, borrowed_root_name);
        reason_line = malloc(needed + 1);
        if (reason_line != NULL) {
            snprintf(reason_line, needed + 1,
                "- '%s' is derived from borrowed source '%s'\n",
                rendered_path, borrowed_root_name);
        }
    }

    semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
        "semantic:channel:transport_rule_violation",
        "bind-to-named-variable-before-send",
        value_expr,
        "%s %s sends must transfer from a named variable instead of '%s'.\n"
        "Reason:\n"
        "- ownership transfer at a channel boundary must point to one concrete source binding\n"
        "- unnamed expressions make moved-here provenance ambiguous\n"
        "%s"
        "Fix:\n"
        "- %s\n"
        "- then send that named variable",
        builtin_name != NULL ? builtin_name : "<builtin>",
        value_label_lower != NULL ? value_label_lower : "value",
        source_path != NULL ? source_path : "<expression>",
        reason_line != NULL ? reason_line : "",
        bind_fix != NULL ? bind_fix : "bind the value first in a local variable");
    free(reason_line);
    return true;
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

    if (type_is_anchored_resource_handle(element_type)
        || type_is_anchored_resource_handle(value_type)) {
        semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "keep-handle-local-or-send-inner-value",
            expr->data.call.arguments[1],
            "%s cannot transport anchored resource handles (Slot/SecureSlot/DeviceSlot) yet; send the inner value or keep the handle local",
            name);
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (type_is_capability_bearing(element_type)
        || type_is_capability_bearing(value_type)) {
        semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "keep-handle-local-or-send-inner-value",
            expr->data.call.arguments[1],
            "%s cannot transport capability-bearing values (SecureSlot/Token) yet; keep capability-bearing state local to the authorized flow",
            name);
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_MOVE_ONLY
        || value_ownership == OWNERSHIP_TYPE_MOVE_ONLY) {
        semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
            "semantic:channel:transport_rule_violation",
            "keep-handle-local-or-send-inner-value",
            expr->data.call.arguments[1],
            "%s does not support movable resource sends yet; use blocking 'ch <- value' so ownership transfer stays explicit",
            name);
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY
        || value_ownership == OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
        char *source_path = builtin_expr_source_path(expr->data.call.arguments[1]);
        const char *borrowed_root_name =
            builtin_borrowed_boundary_root_name(expr->data.call.arguments[1], ctx);
        if (element_ownership != OWNERSHIP_TYPE_SUBJECT_IDENTITY
            || value_ownership != OWNERSHIP_TYPE_SUBJECT_IDENTITY) {
            semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
                "semantic:channel:transport_rule_violation",
                "align-channel-element-type",
                expr->data.call.arguments[1],
                "%s subject mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the same subject boundary contract\n"
                "- ownership transfer cannot be derived when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                name,
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type));
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (expr->data.call.arguments[1] == NULL
            || (expr->data.call.arguments[1]->type != AST_IDENTIFIER
                && borrowed_root_name == NULL)) {
            builtin_require_named_channel_transfer(
                expr->data.call.arguments[1],
                name,
                "subject",
                "bind the subject first in a local variable",
                borrowed_root_name,
                source_path,
                ctx);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (borrowed_root_name != NULL) {
            semantic_validate_borrowed_escape(
                expr->data.call.arguments[1],
                expr->data.call.arguments[1],
                ctx,
                value_type,
                borrowed_root_name,
                OWNERSHIP_CONSUMER_CHANNEL_SEND,
                NULL,
                NULL,
                NULL,
                false,
                NULL,
                NULL);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        free(source_path);
        return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                               : TYPE_BOOL;
    }

    if (element_ownership == OWNERSHIP_TYPE_BORROW_TRACKED
        || value_ownership == OWNERSHIP_TYPE_BORROW_TRACKED) {
        char *source_path = builtin_expr_source_path(expr->data.call.arguments[1]);
        const char *borrowed_root_name =
            builtin_borrowed_boundary_root_name(expr->data.call.arguments[1], ctx);
        if (element_ownership != OWNERSHIP_TYPE_BORROW_TRACKED
            || value_ownership != OWNERSHIP_TYPE_BORROW_TRACKED) {
            semantic_error_with_hints(ctx, "PGY_SEM_CHANNEL_TRANSPORT_INVALID",
                "semantic:channel:transport_rule_violation",
                "align-channel-element-type",
                expr->data.call.arguments[1],
                "%s boundary value mismatch: expected '%s', got '%s'.\n"
                "Reason:\n"
                "- channel element type and sent value must agree on the same boundary value contract\n"
                "- ownership provenance cannot be preserved when the boundary expects '%s' but received '%s'\n"
                "Fix:\n"
                "- send a value of type '%s'\n"
                "- or change the channel element type to match '%s'",
                name,
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type),
                builtin_type_name_or_unknown(element_type),
                builtin_type_name_or_unknown(value_type));
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (expr->data.call.arguments[1] == NULL
            || (expr->data.call.arguments[1]->type != AST_IDENTIFIER
                && borrowed_root_name == NULL)) {
            builtin_require_named_channel_transfer(
                expr->data.call.arguments[1],
                name,
                "boundary value",
                "bind the boundary value first in a local variable",
                borrowed_root_name,
                source_path,
                ctx);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        if (borrowed_root_name != NULL) {
            semantic_validate_borrowed_escape(
                expr->data.call.arguments[1],
                expr->data.call.arguments[1],
                ctx,
                value_type,
                borrowed_root_name,
                OWNERSHIP_CONSUMER_CHANNEL_SEND,
                NULL,
                NULL,
                NULL,
                false,
                NULL,
                NULL);
            free(source_path);
            return detailed_status ? wrap_constructed(TYPE_OPTION, TYPE_BOOL)
                                   : TYPE_BOOL;
        }
        free(source_path);
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
    if (type_is_capability_bearing(element_type)) {
        semantic_error_with_hints(ctx, "PGY_SEM_ANCHORED_HANDLE_COPY", "semantic:anchored_handle:copy_attempt", "use-move-or-retain-binding", expr->data.call.arguments[0],
            "%s cannot yield capability-bearing values (SecureSlot/Token) yet; receive a plain value instead",
            name);
        return TYPE_UNKNOWN;
    }
    return channel_builtin_recv_result(
        element_type, name, expr->data.call.arguments[0], ctx);
}
