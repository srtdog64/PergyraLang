Type *
type_check_assignment(ASTNode *expr, SemanticContext *ctx)
{
    reject_if_embedded_world_zone_mutation(ctx, expr,
        expr->data.assignment.target, "assignment");
    Type *value_type  = type_check_expression(expr->data.assignment.value,  ctx);
    Type *target_type = type_check_expression(expr->data.assignment.target, ctx);

    if (type_is_slot_handle(target_type)
        && target_type->data.slot.inner_type != NULL
        && !type_is_resource_handle(value_type)
        && type_is_assignable(value_type, target_type->data.slot.inner_type)) {
        return target_type;
    }

    if (semantic_check_assignment_borrow_rebind(
            expr, ctx, target_type, value_type)) {
        return target_type;
    }

    if (type_is_class_object_type(target_type, ctx)
        || type_is_class_object_type(value_type, ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
            PGY_CAUSE_SUBJECT_REBIND_FORBIDDEN,
            PGY_FIX_MUTATE_FIELD_OR_USE_METHOD,
            expr,
            "Subject assignment is not allowed; subjects are identity-bearing active hosts. Mutate fields or methods on the existing subject instead of rebinding it with '='");
        return target_type;
    }

    /* Reject field assignment on object/tobject; projection sync uses a
     * separate refresh/publish path and is not affected by this check. */
    if (expr->data.assignment.target != NULL
        && expr->data.assignment.target->type == AST_MEMBER_ACCESS) {
        ASTNode *obj_node = expr->data.assignment.target->data.member.object;
        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER) {
            const char *var_name = obj_node->data.identifier.name;
            Symbol *sym = scope_lookup(ctx->scope, var_name);
            if (sym != NULL && sym->type != NULL
                && sym->type->kind == TYPE_KIND_CLASS
                && sym->type->name != NULL) {
                ASTNode *decl = find_type_decl_by_name(ctx->program_root,
                                                        sym->type->name);
                if (decl != NULL && decl->type == AST_CLASS_DECL) {
                    NominalDeclKind nk = decl->data.class_decl.nominal_kind;
                    if (nk == NOMINAL_DECL_OBJECT) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE, PGY_CAUSE_IMMUTABLE_FIELD_WRITE, PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, expr,
                            "object '%s' fields are read-only after construction.\n"
                            "Reason:\n"
                            "- object is an internal projection contract\n"
                            "- projection state must be refreshed from its source, not mutated directly\n"
                            "Fix:\n"
                            "- update the source subject/value and refresh the object slot\n"
                            "- or construct a new object projection",
                            var_name);
                    } else if (nk == NOMINAL_DECL_TOBJECT) {
                        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE, PGY_CAUSE_IMMUTABLE_FIELD_WRITE, PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, expr,
                            "tobject '%s' fields are immutable.\n"
                            "Reason:\n"
                            "- tobject is a boundary transfer contract\n"
                            "- transfer snapshots must be republished from their source, not mutated in place\n"
                            "Fix:\n"
                            "- update the source subject/value and publish a new tobject\n"
                            "- or construct a new transfer snapshot",
                            var_name);
                    }
                }
            }
        }
    }

    require_assignable(value_type, target_type, expr, ctx);
    return target_type;
}
