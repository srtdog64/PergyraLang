    if (type_is_class_object_type(decl_type, ctx)
        && init != NULL
        && type_is_class_object_type(init_type, ctx)
        && !expr_is_class_constructor_call(init, ctx)
        && (init->type == AST_IDENTIFIER
            || init->type == AST_MEMBER_ACCESS
            || init->type == AST_ARRAY_ACCESS)) {
        semantic_validate_borrowed_escape(
            node, init, ctx, init_type, NULL,
            OWNERSHIP_CONSUMER_NEW_BINDING, NULL, name, NULL,
            false, NULL, NULL);
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
            PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            node,
            "Subjects cannot be copied into a new binding.\n"
            "Reason:\n"
            "- subject values carry identity and anchored state semantics\n"
            "- rebinding an existing subject value as a plain copy would duplicate that identity contract\n"
            "Fix:\n"
            "- construct a fresh subject value instead\n"
            "- or mutate the existing subject through its current binding");
    }

    if (type_is_qubit(decl_type)) {
        const char *borrowed_movable_root =
            init != NULL ? semantic_borrowed_boundary_root_name(init, ctx) : NULL;
        bool borrowed_movable_new_binding =
            init != NULL && borrowed_movable_root != NULL;
        bool valid_qubit_init = false;
        if (borrowed_movable_new_binding) {
            semantic_validate_borrowed_escape(
                node, init, ctx, init_type, borrowed_movable_root,
                OWNERSHIP_CONSUMER_NEW_BINDING, NULL, name, NULL,
                false, NULL, NULL);
            valid_qubit_init = true;
        }
        if (init_type == TYPE_UNKNOWN) {
            valid_qubit_init = true;
        } else if (expr_is_qubit_claim(init)) {
            valid_qubit_init = true;
        } else if (init != NULL && init_type != NULL && type_is_qubit(init_type)) {
            if (init->type == AST_IDENTIFIER) {
                valid_qubit_init = true;
                consume_qubit_value(init, ctx, "moved");
            } else if (expr_is_movable_resource_transfer_source(init)) {
                valid_qubit_init = true;
            }
        }
        if (!valid_qubit_init) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
                PGY_CAUSE_MOVABLE_HANDLE_COPY_ATTEMPT,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                node,
                "QubitSlot is a slot handle (movable) and cannot be plain-copied into a new binding.\n"
                "Reason:\n"
                "- slot handle (movable) values must come from a fresh claim, an explicit move, or a transfer boundary\n"
                "- current initializer does not preserve ownership provenance for QubitSlot\n"
                "Fix:\n"
                "- initialize from ClaimQubit()\n"
                "- or move an existing QubitSlot identifier\n"
                "- or use a transfer boundary such as recv/await");
        }
    }

    if (init != NULL) {
        semantic_validate_borrowed_escape(
            node, init, ctx, init_type, NULL,
            OWNERSHIP_CONSUMER_NEW_BINDING, NULL, name, NULL,
            false, NULL, NULL);
    }
