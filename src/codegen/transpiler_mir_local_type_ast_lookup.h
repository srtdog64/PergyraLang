static ASTNode *
transpiler_find_local_type_ast_in_block(TranspilerCtx *ctx,
                                        ASTNode *body,
                                        const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < body->data.block.count; i++) {
            ASTNode *found = transpiler_find_local_type_ast_in_block(
                ctx, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL)
            return body->data.let_decl.type;
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_CALL
            && body->data.let_decl.initializer->data.call.callee != NULL
            && body->data.let_decl.initializer->data.call.callee->type == AST_IDENTIFIER
            && body->data.let_decl.initializer->data.call.callee->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                body->data.let_decl.initializer->data.call.callee->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && decl->data.func_decl.return_type != NULL
                && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
                return decl->data.func_decl.return_type;
            }
        }
        if (body->data.let_decl.initializer != NULL
            && body->data.let_decl.initializer->type == AST_IDENTIFIER
            && body->data.let_decl.initializer->data.identifier.name != NULL) {
            ASTNode *decl = find_function_decl(ctx,
                body->data.let_decl.initializer->data.identifier.name);
            if (decl != NULL
                && decl->type == AST_FUNC_DECL
                && decl->data.func_decl.return_type != NULL
                && decl->data.func_decl.return_type->type == AST_EVENT_HANDLER_TYPE) {
                return decl->data.func_decl.return_type;
            }
        }
        return NULL;
    }
    if (body->type == AST_WITH_STMT)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.with_stmt.body, base_name);
    if (body->type == AST_IF_STMT) {
        ASTNode *found = transpiler_find_local_type_ast_in_block(
            ctx, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.while_loop.body, base_name);
    if (body->type == AST_FOR_LOOP)
        return transpiler_find_local_type_ast_in_block(
            ctx, body->data.for_loop.body, base_name);
    return NULL;
}

static ASTNode *
transpiler_find_local_type_ast(TranspilerCtx *ctx,
                               const ASTNode *func_decl,
                               const char *base_name)
{
    if (func_decl == NULL
        || func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL
        || base_name == NULL) {
        return NULL;
    }
    return transpiler_find_local_type_ast_in_block(
        ctx, func_decl->data.func_decl.body, base_name);
}
