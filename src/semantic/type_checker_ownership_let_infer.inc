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
        decl_type = resolve_type_node(ann, ctx);
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
        if (init != NULL) {
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
                semantic_error_with_hints(ctx, "PGY_SEM_INFER_COLLECTION", "semantic:infer:collection_needs_annotation", "annotate-collection-element-type", init,
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
            semantic_error_with_hints(ctx, "PGY_SEM_INFER_GENERIC", "semantic:infer:unbound_generic", "annotate-binding-type", init,
                "Cannot infer type: generic parameter '%s' is ambiguous. "
                "Please provide a type annotation.", init_type->name);
            decl_type = TYPE_UNKNOWN;
        }
    } else {
        /* No annotation and no initializer */
        semantic_error_with_hints(ctx, "PGY_SEM_INFER_REQUIRED", "semantic:infer:no_source", "add-annotation-or-initializer", node,
            "Cannot infer type: provide a type annotation or initializer");
        decl_type = TYPE_UNKNOWN;
    }
