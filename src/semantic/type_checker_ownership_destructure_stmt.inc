static Type *
ownership_destructure_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_resolve_or_fallback(ctx, type_ref);
}

static bool
type_check_let_destructure_stmt(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *init;

    if (node == NULL || node->type != AST_LET_DESTRUCTURE)
        return true;

    init = node->data.let_destructure.initializer;

    if (init != NULL
        && init->type == AST_CALL
        && init->data.call.callee != NULL
        && init->data.call.callee->type == AST_IDENTIFIER
        && init->data.call.callee->data.identifier.name != NULL
        && init->data.call.generic_args != NULL
        && init->data.call.generic_args->count >= 1) {
        const char *callee_name =
            init->data.call.callee->data.identifier.name;
        bool is_claim_slot =
            (strcmp(callee_name, "ClaimSlot") == 0);
        bool is_claim_secure =
            (strcmp(callee_name, "ClaimSecureSlot") == 0);
        if (is_claim_slot || is_claim_secure) {
            const char *inner_name =
                init->data.call.generic_args->params[0] != NULL
                    ? init->data.call.generic_args->params[0]->name
                    : NULL;
            ASTNode *inner_node =
                init->data.call.generic_args->params[0] != NULL
                    ? init->data.call.generic_args->params[0]->constraint
                    : NULL;
            Type *inner_type = NULL;
            if (inner_node != NULL)
                inner_type = ownership_destructure_resolve_type_ref(
                    inner_node, ctx);
            if (inner_type == NULL && inner_name != NULL) {
                ASTNode synth = {0};
                synth.type = AST_TYPE;
                synth.data.type.name = (char *)inner_name;
                inner_type = ownership_destructure_resolve_type_ref(&synth, ctx);
            }
            if (inner_type == NULL)
                inner_type = TYPE_INT;

            Type *slot_type = type_create_slot(inner_type, is_claim_secure);
            if (is_claim_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);

            if (is_claim_secure && node->data.let_destructure.name_count == 2) {
                const char *slot_name =
                    node->data.let_destructure.names[0];
                const char *token_name =
                    node->data.let_destructure.names[1];
                Symbol *slot_sym = symbol_create_slot(slot_name, slot_type,
                    true, token_name, node->line, node->column);
                scope_declare(ctx->scope, slot_sym);
                Symbol *tok_sym = symbol_create_token(token_name,
                    slot_name, node->line, node->column);
                if (tok_sym != NULL) {
                    Type *token_args[1] = { inner_type };
                    tok_sym->type = type_create_constructed(TYPE_TOKEN,
                        token_args, 1);
                }
                if (!scope_declare(ctx->scope, tok_sym))
                    symbol_destroy(tok_sym);
                scope_register_slot(ctx->scope, slot_sym);
                return true;
            }
            if (is_claim_slot && node->data.let_destructure.name_count == 1) {
                const char *slot_name =
                    node->data.let_destructure.names[0];
                Symbol *slot_sym = symbol_create_slot(slot_name, slot_type,
                    false, NULL, node->line, node->column);
                scope_declare(ctx->scope, slot_sym);
                scope_register_slot(ctx->scope, slot_sym);
                return true;
            }
        }
    }

    return type_check_let_destructure_tail(node, init, ctx);
}
