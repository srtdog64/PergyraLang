    /* Slot<T> move semantics: when assigning from another Slot variable,
     * consume (invalidate) the source.  ClaimSlot() creates fresh. */
    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT
        && decl_type->data.slot.access_mode == SLOT_ACCESS_OWNED
        && init != NULL && init->type == AST_IDENTIFIER
        && init_type != NULL && init_type->kind == TYPE_KIND_SLOT
        && init_type->data.slot.access_mode == SLOT_ACCESS_OWNED) {
        /* Move: source Slot becomes invalid after this point */
        const char *src_name = init->data.identifier.name;
        Symbol *src_sym = scope_lookup(ctx->scope, src_name);
        if (src_sym != NULL && src_sym->kind == SYMBOL_SLOT) {
            if (src_sym->slot_info.state == SLOT_STATE_RELEASED) {
                semantic_error_with_hints(ctx, "PGY_SEM_MOVE_FROM_RELEASED", "semantic:move:from_released", "reclaim-or-trace-earlier-move", init,
                    "Cannot move from released slot '%s'.\n"
                    "Reason:\n"
                    "- slot '%s' was already released or invalidated earlier in this scope\n"
                    "- ownership transfer requires a live source handle\n"
                    "Fix:\n"
                    "- reacquire the slot before moving it\n"
                    "- or remove the earlier release/invalidating transfer",
                    src_name,
                    src_name);
            } else {
                src_sym->slot_info.state = SLOT_STATE_RELEASED;
            }
        }
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT
        && decl_type->data.slot.access_mode != SLOT_ACCESS_OWNED) {
        const char *source_slot = NULL;
        if (init != NULL && init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER
            && init->data.call.arg_count >= 1
            && init->data.call.arguments[0] != NULL
            && init->data.call.arguments[0]->type == AST_IDENTIFIER) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (callee_name != NULL
                && (strcmp(callee_name, "ViewRead") == 0
                    || strcmp(callee_name, "ViewWrite") == 0
                    || strcmp(callee_name, "Move") == 0)) {
                source_slot = init->data.call.arguments[0]->data.identifier.name;
            }
        }

        Symbol *sym = symbol_create_view(name, decl_type, source_slot,
            node->line, node->column);
        scope_declare(ctx->scope, sym);
        return !ctx->has_error;
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT) {
        if (decl_type->data.slot.is_secure)
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (slot_transfer_compatible(init_type, decl_type)) {
            if (init != NULL && init->type == AST_IDENTIFIER) {
                Symbol *move_sym = scope_lookup(ctx->scope, init->data.identifier.name);
                if (move_sym != NULL)
                    move_sym->is_consumed = true;
            }
        }
        if (init_type != NULL && type_is_anchored_resource_handle(init_type)) {
            semantic_error_with_hints(ctx, "PGY_SEM_ANCHORED_HANDLE_COPY", "semantic:anchored_handle:copy_attempt", "use-move-or-retain-binding", node,
                "Anchored resource handles (Slot/SecureSlot/DeviceSlot) cannot be copied into a new binding.\n"
                "Reason:\n"
                "- anchored handles are tied to one local ownership/runtime anchor\n"
                "- rebinding them as plain values would duplicate that anchor contract\n"
                "Fix:\n"
                "- use a fresh claim\n"
                "- or initialize from an inner/plain value instead");
        }
        char token_name_buf[256];
        const char *paired_token = NULL;
        if (decl_type->data.slot.is_secure) {
            snprintf(token_name_buf, sizeof(token_name_buf), "%s_token", name);
            paired_token = token_name_buf;
        }
        Symbol *sym = symbol_create_slot(name, decl_type,
            decl_type->data.slot.is_secure, paired_token, node->line, node->column);
        scope_declare(ctx->scope, sym);
        if (decl_type->data.slot.is_secure) {
            Symbol *tok = symbol_create_token(paired_token, name,
                                              node->line, node->column);
            if (tok != NULL) {
                Type *token_args[1] = { decl_type->data.slot.inner_type };
                tok->type = type_create_constructed(TYPE_TOKEN, token_args, 1);
            }
            if (!scope_declare(ctx->scope, tok))
                symbol_destroy(tok);
        }
        scope_register_slot(ctx->scope, sym);
        return !ctx->has_error;
    }
