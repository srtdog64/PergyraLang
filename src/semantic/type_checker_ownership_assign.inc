bool
semantic_check_assignment_borrow_rebind(ASTNode *expr,
                                        SemanticContext *ctx,
                                        Type *target_type,
                                        Type *value_type)
{
    if (type_is_resource_handle(target_type) || type_is_resource_handle(value_type)) {
        if (semantic_validate_borrowed_escape(
                expr, expr->data.assignment.value, ctx, value_type, NULL,
                OWNERSHIP_CONSUMER_ASSIGNMENT_REBIND,
                expr->data.assignment.target, NULL, NULL,
                false, NULL, NULL)) {
            return true;
        }
        semantic_error_with_hints(ctx, "PGY_SEM_IMMUTABLE_FIELD_WRITE", "semantic:immutable_field:write", "reconstruct-or-change-host-kind", expr,
            "Resource handle assignment is not allowed.\n"
            "Reason:\n"
            "- anchored handles (Slot/SecureSlot/DeviceSlot) cannot be copied or rebound with '='\n"
            "- assignment rebind would create a second boundary-visible handle binding outside the original anchor\n"
            "- movable handles must transfer ownership through a new binding\n"
            "Fix:\n"
            "- use Read/Write for Slot<T>\n"
            "- keep using the current anchored handle binding directly\n"
            "- or move the anchored handle through an explicit ownership boundary\n"
            "- move a QubitSlot into a new binding\n"
            "- or Claim... to create a fresh handle");
        return true;
    }

    if (semantic_validate_borrowed_escape(
            expr, expr->data.assignment.value, ctx, value_type, NULL,
            OWNERSHIP_CONSUMER_ASSIGNMENT_REBIND,
            expr->data.assignment.target, NULL, NULL,
            false, NULL, NULL)) {
        return true;
    }

    return false;
}
