#ifndef PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H
#define PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H

Type *
type_check_assignment(ASTNode *expr, SemanticContext *ctx)
{
    Type *value_type;
    Type *target_type;
    ASTNode *target = ast_assignment_target(expr);
    ASTNode *value = ast_assignment_value(expr);

    reject_if_embedded_world_zone_mutation(ctx, expr, target, "assignment");
    value_type = type_check_expression(value, ctx);
    if (value_type == NULL)
        value_type = TYPE_UNKNOWN;

    if (target != NULL && target->type == AST_IDENTIFIER) {
        const char *target_name = ast_identifier_name(target);
        Symbol *target_sym = scope_lookup(ctx->scope, target_name);
        if (target_sym != NULL && target_sym->kind == SYMBOL_SLOT
            && target_sym->type != NULL && type_is_owned_slot_handle(target_sym->type)
            && type_slot_inner_type(target_sym->type) != NULL
            && !type_is_resource_handle(value_type)
            && type_is_assignable(value_type,
                type_slot_inner_type(target_sym->type))) {
            const char *active_view_name = NULL;
            const char *active_view_kind = NULL;
            if (semantic_find_active_slot_view_for_source(ctx->scope,
                    target_sym->name, &active_view_name, &active_view_kind,
                    NULL)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    target,
                    "Cannot assign to slot '%s' while %s '%s' is live.\n"
                    "Reason:\n"
                    "- slot assignment sugar lowers to a slot write\n"
                    "- owner writes during a live view would bypass the view's aliasing contract\n"
                    "Fix:\n"
                    "- write through the active view when it is a WriteView<T>\n"
                    "- or end the pin/view scope before assigning to '%s'",
                    target_sym->name,
                    active_view_kind != NULL ? active_view_kind : "view",
                    active_view_name != NULL ? active_view_name : "<view>",
                    target_sym->name);
                return target_sym->type;
            }
        }
    }

    target_type = type_check_expression(target, ctx);
    if (target_type == NULL)
        target_type = TYPE_UNKNOWN;

    if (type_is_slot_handle(target_type)
        && type_slot_inner_type(target_type) != NULL
        && !type_is_resource_handle(value_type)
        && type_is_assignable(value_type, type_slot_inner_type(target_type))) {
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
    if (target != NULL && target->type == AST_MEMBER_ACCESS) {
        ASTNode *obj_node = ast_member_object(target);
        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER) {
            const char *var_name = ast_identifier_name(obj_node);
            Symbol *sym = scope_lookup(ctx->scope, var_name);
            if (sym != NULL && sym->type != NULL
                && sym->type->kind == TYPE_KIND_CLASS
                && sym->type->name != NULL) {
                ASTNode *decl = find_type_decl_by_name(ctx->program_root,
                                                        sym->type->name);
                if (decl != NULL && decl->type == AST_CLASS_DECL) {
                    NominalDeclKind nk = ast_class_nominal_kind(decl);
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
#endif /* PGY_SRC_SEMANTIC_TYPE_CHECKER_ASSIGNMENT_H */
