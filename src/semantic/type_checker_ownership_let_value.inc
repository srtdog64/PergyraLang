    if (type_is_anchored_resource_handle(decl_type)) {
        bool is_fresh_claim = expr_is_device_slot_claim(init);
        if (!is_fresh_claim && init_type != NULL && type_is_anchored_resource_handle(init_type)) {
            semantic_error_with_hints(ctx, "PGY_SEM_ANCHORED_HANDLE_COPY", "semantic:anchored_handle:copy_attempt", "use-move-or-retain-binding", node,
                "Anchored resource handles (Slot/SecureSlot/DeviceSlot) cannot be copied into a new binding.\n"
                "Reason:\n"
                "- anchored handles remain tied to one owning runtime anchor\n"
                "- copying them into another binding would duplicate that anchor identity\n"
                "Fix:\n"
                "- keep the handle in one binding\n"
                "- or reacquire it from the owning system");
        }
        semantic_record_effect(ctx, EFFECT_REMOTE);
    }

    Symbol *sym = symbol_create_variable(name, decl_type,
                                         node->line, node->column);

    if (type_is_constructed_named(decl_type, "DeviceSlot"))
        sym->slot_info.state = SLOT_STATE_CLAIMED;

    /* Set qubit semantic state for QubitSlot variables */
    if (type_is_qubit(decl_type)) {
        if (expr_is_qubit_claim(init)) {
            sym->qubit_info.semantic_state = QUBIT_STATE_SUPERPOSITION;
        } else if (init != NULL && init->type == AST_IDENTIFIER) {
            /* Move: copy source qubit's semantic state */
            Symbol *src = lookup_identifier_symbol(init, ctx);
            if (src != NULL)
                sym->qubit_info.semantic_state = src->qubit_info.semantic_state;
            else
                sym->qubit_info.semantic_state = QUBIT_STATE_NONE;
        } else if (init != NULL && init->type == AST_CALL) {
            /* Function returning QubitSlot */
            sym->qubit_info.semantic_state = QUBIT_STATE_SUPERPOSITION;
        } else if (init != NULL
                   && (init->type == AST_CHANNEL_RECV
                       || init->type == AST_AWAIT_EXPR)) {
            /* Transfer from orchestration boundary; precise state is unknown. */
            sym->qubit_info.semantic_state = QUBIT_STATE_NONE;
        }
    }

    if (!is_slot_decl)
        scope_declare(ctx->scope, sym);
