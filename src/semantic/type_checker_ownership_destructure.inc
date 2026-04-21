static bool
type_check_let_destructure_tail(ASTNode *node, ASTNode *init,
                                SemanticContext *ctx)
{
    Type *init_type = init != NULL ? type_check_expression(init, ctx) : TYPE_UNKNOWN;

    if (type_is_tuple(init_type)) {
        size_t arity = type_tuple_arity(init_type);
        size_t binds = node->data.let_destructure.name_count;
        if (arity != binds) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
                PGY_CAUSE_DESTRUCTURING_ARITY_MISMATCH, PGY_FIX_ALIGN_DESTRUCTURING_ARITY,
                node,
                "Tuple destructuring arity mismatch: binding %llu, tuple arity %llu",
                (unsigned long long) binds, (unsigned long long) arity);
            return false;
        }
        for (size_t i = 0; i < binds; i++) {
            Type *elem = type_tuple_get_element(init_type, i);
            if (init != NULL) {
                semantic_validate_borrowed_escape(
                    node, init, ctx, init_type, NULL,
                    OWNERSHIP_CONSUMER_DESTRUCTURE_TARGET_BINDING, NULL,
                    node->data.let_destructure.names[i], NULL,
                    false, NULL, NULL);
            }
            Symbol *s = symbol_create_variable(
                node->data.let_destructure.names[i],
                elem != NULL ? elem : TYPE_UNKNOWN,
                node->line, node->column);
            scope_declare(ctx->scope, s);
        }
        return true;
    }

    for (size_t i = 0; i < node->data.let_destructure.name_count; i++) {
        Type *elem_type = TYPE_UNKNOWN;
        if (type_is_constructed_named(init_type, "Array")
            || type_is_constructed_named(init_type, "Slice")) {
            elem_type = type_get_constructed_arg(init_type, 0);
        }
        if (init != NULL) {
            semantic_validate_borrowed_escape(
                node, init, ctx, init_type, NULL,
                OWNERSHIP_CONSUMER_DESTRUCTURE_TARGET_BINDING, NULL,
                node->data.let_destructure.names[i], NULL,
                false, NULL, NULL);
        }
        Symbol *s = symbol_create_variable(
            node->data.let_destructure.names[i], elem_type,
            node->line, node->column);
        scope_declare(ctx->scope, s);
    }

    return true;
}
