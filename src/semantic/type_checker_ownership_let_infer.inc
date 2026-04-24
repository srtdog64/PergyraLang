    /* Normal variable declaration with type inference */
    Type *init_type = (init != NULL)
                      ? type_check_expression(init, ctx)
                      : TYPE_VOID;
    if (init != NULL)
        mark_world_embedded_zone_arguments(init, ctx);

    Type *decl_type;

    /* Type inference: if no annotation, infer from initializer */
    if (ann != NULL) {
        /* Explicit type annotation */
        decl_type = ownership_let_resolve_type_ref(ann, ctx);
        if (ctx->program_root != NULL
            && ann->type == AST_TYPE
            && ann->data.type.name != NULL) {
            ASTNode *class_decl = find_type_decl_by_name(ctx->program_root,
                                                         ann->data.type.name);
            if (class_decl != NULL && class_decl->type == AST_CLASS_DECL) {
                validate_class_where_clause_specialization_ast(class_decl,
                                                               ann,
                                                               ann,
                                                               ctx);
            }
        }
        if (init == NULL) {
            /* Function-body lets must carry an initializer.  The C backend
             * emits a scalar zero fallback and the LLVM backend emits no
             * store at all, so silently allowing `let x: T;` here diverges
             * the backends at first read.  Class/subject fields have their
             * own parser path (ClassField), so this check only fires for
             * function-body locals. */
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_UNINIT_LOCAL,
                PGY_CAUSE_UNINIT_LOCAL,
                PGY_FIX_INITIALIZE_AT_BINDING,
                node,
                "Local binding '%s' has a type annotation but no initializer.\n"
                "Reason:\n"
                "- function-body lets must be initialized at the binding site\n"
                "- the backends diverge on uninitialized reads\n"
                "Fix:\n"
                "- provide an initializer directly: 'let %s: %s = ...'\n"
                "- or use a conditional expression as the initializer",
                name != NULL ? name : "<binding>",
                name != NULL ? name : "<binding>",
                (ann->type == AST_TYPE && ann->data.type.name != NULL)
                    ? ann->data.type.name : "T");
        } else {
            if (init->type == AST_CALL
                && init->data.call.callee->type == AST_IDENTIFIER
                && strcmp(init->data.call.callee->data.identifier.name,
                          "BoxArray") == 0) {
                init_type = decl_type;
            } else if (init->type == AST_CALL
                       && init->data.call.callee != NULL
                       && init->data.call.callee->type == AST_IDENTIFIER
                       && ann->type == AST_TYPE
                       && ann->data.type.name != NULL
                       && strcmp(init->data.call.callee->data.identifier.name,
                                 ann->data.type.name) == 0
                       && decl_type != NULL
                       && decl_type->kind == TYPE_KIND_CONSTRUCTED) {
                init_type = decl_type;
            }
            if (!slot_transfer_compatible(init_type, decl_type))
                require_assignable(init_type, decl_type, init, ctx);
        }
    } else if (init != NULL) {
        /* Infer type from initializer */
        decl_type = init_type;

        if (init->type == AST_CALL
            && init->data.call.callee != NULL
            && init->data.call.callee->type == AST_IDENTIFIER
            && init_type == TYPE_UNKNOWN) {
            const char *callee_name = init->data.call.callee->data.identifier.name;
            if (callee_name != NULL
                && (strcmp(callee_name, "ListNew") == 0
                    || strcmp(callee_name, "SetNew") == 0
                    || strcmp(callee_name, "MapNew") == 0
                    || strcmp(callee_name, "QueueNew") == 0)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_COLLECTION, PGY_CAUSE_INFER_COLLECTION_NEEDS_ANNOTATION, PGY_FIX_ANNOTATE_COLLECTION_ELEMENT_TYPE, init,
                    "Cannot infer collection type from '%s()' without an explicit annotation; write 'let value: %s<...> = %s()'",
                    callee_name,
                    strcmp(callee_name, "MapNew") == 0 ? "HashMap" :
                    (strcmp(callee_name, "QueueNew") == 0 ? "Queue" :
                    (strcmp(callee_name, "SetNew") == 0 ? "Set" : "List")),
                    callee_name);
                decl_type = TYPE_UNKNOWN;
            }
        }

        /* For generic types like Box<T>, Array<T>, Result<T,E>,
           ensure the inferred type is concrete */
        if (init_type->kind == TYPE_KIND_GENERIC) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_GENERIC, PGY_CAUSE_INFER_UNBOUND_GENERIC, PGY_FIX_ANNOTATE_BINDING_TYPE, init,
                "Cannot infer type: generic parameter '%s' is ambiguous. "
                "Please provide a type annotation.", init_type->name);
            decl_type = TYPE_UNKNOWN;
        }
    } else {
        /* No annotation and no initializer */
        semantic_error_with_hints(ctx, PGY_CODE_SEM_INFER_REQUIRED, PGY_CAUSE_INFER_NO_SOURCE, PGY_FIX_ADD_ANNOTATION_OR_INITIALIZER, node,
            "Cannot infer type: provide a type annotation or initializer");
        decl_type = TYPE_UNKNOWN;
    }
