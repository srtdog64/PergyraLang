#include "type_checker_internal.h"
#include "type_checker_assignment.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "diag_codes.h"
#include "compiler/decl_field_model.h"

#include <string.h>

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
    if (type_equals(value_type, TYPE_VOID)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_TYPE_MISMATCH,
            PGY_CAUSE_ASSIGNABILITY_CHECK,
            PGY_FIX_ALIGN_OPERAND_TYPE,
            value,
            "Void expression cannot be assigned as a value; split the side effect into a statement before assignment");
        value_type = TYPE_UNKNOWN;
    }

    if (target != NULL && target->type == AST_IDENTIFIER) {
        const char *target_name = ast_identifier_name(target);
        Symbol *target_sym = scope_lookup(ctx->scope, target_name);
        if (target_sym != NULL && target_sym->kind == SYMBOL_SLOT
            && target_sym->type != NULL
            && type_is_owned_slot_handle(target_sym->type)
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

    if (target != NULL && target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = ast_array_access_array(target);
        if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
            const char *array_name = ast_identifier_name(array_node);
            Symbol *array_sym = array_name != NULL
                ? scope_lookup(ctx->scope, array_name)
                : NULL;
            if (array_sym != NULL
                && reject_default_param_collection_mutator_receiver(
                    array_node, array_sym->type, "array index assignment",
                    "array", ctx)) {
                return target_type;
            }
        }
    }

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

    if (target != NULL && target->type == AST_MEMBER_ACCESS) {
        ASTNode *obj_node = ast_member_object(target);
        if (obj_node != NULL && obj_node->type == AST_IDENTIFIER) {
            const char *var_name = ast_identifier_name(obj_node);
            Symbol *sym = scope_lookup(ctx->scope, var_name);
            if (sym != NULL && sym->type != NULL
                && sym->type->kind == TYPE_KIND_CLASS
                && sym->type->name != NULL) {
                ASTNode *decl = semantic_host_decl_for_type(ctx, sym->type);
                if (decl != NULL && decl->type == AST_CLASS_DECL) {
                    NominalDeclKind nk = ast_class_nominal_kind(decl);
                    if (nk == NOMINAL_DECL_OBJECT) {
                        semantic_error_with_hints(ctx,
                            PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
                            PGY_CAUSE_IMMUTABLE_FIELD_WRITE,
                            PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND,
                            expr,
                            "object '%s' fields are read-only after construction.\n"
                            "Reason:\n"
                            "- object is an internal projection contract\n"
                            "- projection state must be refreshed from its source, not mutated directly\n"
                            "Fix:\n"
                            "- update the source subject/value and refresh the object slot\n"
                            "- or construct a new object projection",
                            var_name);
                    } else if (nk == NOMINAL_DECL_TOBJECT) {
                        semantic_error_with_hints(ctx,
                            PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
                            PGY_CAUSE_IMMUTABLE_FIELD_WRITE,
                            PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND,
                            expr,
                            "tobject '%s' fields are immutable.\n"
                            "Reason:\n"
                            "- tobject is a boundary transfer contract\n"
                            "- transfer snapshots must be republished from their source, not mutated in place\n"
                            "Fix:\n"
                            "- update the source subject/value and publish a new tobject\n"
                            "- or construct a new transfer snapshot",
                            var_name);
                    } else {
                        /* struct / subject / class: per-field mutability. A
                         * field declared `let` (is_mutable == false) is an
                         * immutable binding; `let mut` and bare/vessel fields
                         * are assignable. */
                        const char *field_name = ast_member_name(target);
                        if (field_name != NULL) {
                            /* F2 (docs/144) Phase 2: consume the field-shape model. */
                            PgyDeclField *fields = NULL;
                            size_t fc = pgy_class_decl_field_model_build(decl, &fields);
                            for (size_t fi = 0; fi < fc; fi++) {
                                if (fields[fi].name != NULL
                                    && strcmp(fields[fi].name, field_name) == 0) {
                                    if (!fields[fi].is_mutable) {
                                        semantic_error_with_hints(ctx,
                                            PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
                                            PGY_CAUSE_IMMUTABLE_FIELD_WRITE,
                                            PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND,
                                            expr,
                                            "field '%s.%s' is immutable.\n"
                                            "Reason:\n"
                                            "- it is declared with `let` (an immutable field binding)\n"
                                            "Fix:\n"
                                            "- declare it `let mut %s: ...` to allow assignment\n"
                                            "- or set it only at construction",
                                            var_name, field_name, field_name);
                                    }
                                    break;
                                }
                            }
                            pgy_decl_field_model_free(fields, fc);
                        }
                    }
                }
            }
        }
    }

    require_assignable(value_type, target_type, expr, ctx);
    return target_type;
}
