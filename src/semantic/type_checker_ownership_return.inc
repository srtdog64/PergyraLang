bool
type_check_return_stmt(ASTNode *node, SemanticContext *ctx)
{
    Type *ret_type = TYPE_VOID;
    const char *borrowed_name = NULL;

    if (node->data.return_stmt.value != NULL)
        ret_type = type_check_expression(node->data.return_stmt.value, ctx);

    if (ctx->current_return != NULL)
        require_assignable(ret_type, ctx->current_return, node, ctx);

    if (type_is_anchored_resource_handle(ret_type)) {
        semantic_error_with_hints(ctx, "PGY_SEM_ANCHORED_HANDLE_COPY", "semantic:anchored_handle:copy_attempt", "use-move-or-retain-binding", node,
            "Returning anchored resource handles (Slot/SecureSlot/DeviceSlot) is not supported yet.\n"
            "Reason:\n"
            "- return boundary would let an anchored handle escape its local ownership/runtime anchor\n"
            "- anchored handles are still local-only at return boundaries in the closed subset\n"
            "Fix:\n"
            "- return the inner value instead\n"
            "- or return a future/result token or other boundary-safe projection");
    }

    if (node->data.return_stmt.value != NULL) {
        borrowed_name = semantic_borrowed_boundary_root_name(
            node->data.return_stmt.value, ctx);
    }

    if (node->data.return_stmt.value != NULL
        && borrowed_name != NULL
        && type_is_subject_type(ret_type, ctx)) {
        semantic_report_borrowed_return_escape(
            node, node->data.return_stmt.value, ctx, borrowed_name,
            "subject", "subject provenance",
            "a projection/object/tobject/value result");
    }

    if (node->data.return_stmt.value != NULL
        && borrowed_name != NULL
        && type_is_movable_resource_handle(ret_type)) {
        semantic_report_borrowed_return_escape(
            node, node->data.return_stmt.value, ctx, borrowed_name,
            "movable resource", "movable-resource provenance",
            "a copied/projection/value result");
    }

    if (node->data.return_stmt.value != NULL
        && borrowed_name != NULL
        && type_requires_boundary_borrow_tracking(ret_type, ctx)) {
        semantic_report_borrowed_return_escape(
            node, node->data.return_stmt.value, ctx, borrowed_name,
            "boundary value", "boundary provenance",
            "a copied/value/projection result");
    }

    if (node->data.return_stmt.value != NULL
        && type_is_qubit(ret_type)
        && node->data.return_stmt.value->type == AST_IDENTIFIER) {
        consume_qubit_value(node->data.return_stmt.value, ctx, "returned");
    }

    return !ctx->has_error;
}
