    /* Check for duplicate in current scope */
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error_with_hints(ctx,
            "PGY_SEM_REDECLARATION",
            "semantic:scope:duplicate_symbol",
            "rename-or-remove-duplicate",
            node,
            "Redeclaration of '%s' in the same scope", name);
        return false;
    }

    /*
     * Detect ClaimSlot / ClaimSecureSlot calls so we can register
     * a SYMBOL_SLOT with proper metadata.
     */
    bool is_slot_decl = false;
    if (init != NULL && init->type == AST_CALL
        && init->data.call.callee->type == AST_IDENTIFIER) {
        const char *callee_name =
            init->data.call.callee->data.identifier.name;
        BuiltinKind bk = builtin_resolve(callee_name);

        if (bk == BUILTIN_CLAIM_SLOT || bk == BUILTIN_CLAIM_SECURE_SLOT) {
            is_slot_decl = true;
            bool is_secure = (bk == BUILTIN_CLAIM_SECURE_SLOT);
            if (is_secure)
                semantic_record_effect(ctx, EFFECT_SECURE);

            /* Resolve inner type from annotation if present.
             * If the annotation is already a Slot type (e.g. Slot<String>),
             * use it directly instead of double-wrapping. */
            Type *slot_type = NULL;
            if (ann != NULL) {
                Type *ann_type = resolve_type_node(ann, ctx);
                if (ann_type->kind == TYPE_KIND_SLOT) {
                    slot_type = ann_type;
                    is_secure = ann_type->data.slot.is_secure;
                } else {
                    slot_type = type_create_slot(ann_type, is_secure);
                }
            } else {
                slot_type = type_create_slot(TYPE_INT, is_secure);
            }
            char token_name_buf[256];
            const char *paired_token = NULL;
            if (is_secure) {
                snprintf(token_name_buf, sizeof(token_name_buf), "%s_token", name);
                paired_token = token_name_buf;
            }
            Symbol *sym = symbol_create_slot(name, slot_type,
                                             is_secure, paired_token,
                                             node->line, node->column);
            scope_declare(ctx->scope, sym);
            if (is_secure) {
                Symbol *tok = symbol_create_token(paired_token, name,
                                                  node->line, node->column);
                if (tok != NULL && slot_type != NULL
                    && slot_type->kind == TYPE_KIND_SLOT) {
                    Type *token_args[1] = { slot_type->data.slot.inner_type };
                    tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
                }
                if (!scope_declare(ctx->scope, tok))
                    symbol_destroy(tok);
            }
            scope_register_slot(ctx->scope, sym);
            return true;
        }
    }
