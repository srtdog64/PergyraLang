#include "type_checker_internal.h"
#include "callable_capability_inference.h"
#include "type_checker_assignment.h"
#include "type_checker_builtins_internal.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_resolution_internal.h"
#include "diag_codes.h"
#include "compiler/decl_field_model.h"

#include <string.h>

typedef struct {
    Type *type;
    Symbol *root;
} AssignmentFieldPath;

/* Walk the lvalue once, carrying the resolved type and root borrow mode.
 * Reading a nested member must not erase an outer immutable boundary. Types
 * come from declaration metadata; do not type-check/evaluate receivers again. */
static AssignmentFieldPath
assignment_field_write_path(ASTNode *node, SemanticContext *ctx)
{
    AssignmentFieldPath path = {TYPE_UNKNOWN, NULL};
    if (node == NULL)
        return path;
    if (node->type == AST_IDENTIFIER) {
        path.root = scope_lookup(ctx->scope, ast_identifier_name(node));
        if (path.root != NULL && path.root->type != NULL)
            path.type = path.root->type;
        return path;
    }
    if (node->type == AST_ARRAY_ACCESS) {
        path = assignment_field_write_path(ast_array_access_array(node), ctx);
        if ((type_is_constructed_named(path.type, "Array")
                || type_is_constructed_named(path.type, "Slice"))
            && type_constructed_arg_count(path.type) == 1)
            path.type = type_constructed_arg(path.type, 0);
        else
            path.type = TYPE_UNKNOWN;
        return path;
    }
    if (node->type != AST_MEMBER_ACCESS)
        return path;
    path = assignment_field_write_path(ast_member_object(node), ctx);
    if (path.root == NULL || path.type == TYPE_UNKNOWN)
        return path;
    const char *root_name = path.root->name;
    if (path.root->is_parameter && path.root->param_mode == PARAM_MODE_REF) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
            PGY_CAUSE_IMMUTABLE_FIELD_WRITE, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            node, "Cannot write through read-only ref parameter '%s'.\n"
            "Reason:\n- ref is a non-owning read-only borrow\n"
            "Fix:\n- caller-visible mutation requires an inout boundary",
            root_name);
        path.type = TYPE_UNKNOWN;
        return path;
    }
    ASTNode *decl = semantic_host_decl_for_type(ctx, path.type);
    path.type = TYPE_UNKNOWN;
    if (decl == NULL || decl->type != AST_CLASS_DECL)
        return path;
    NominalDeclKind kind = ast_class_nominal_kind(decl);
    const char *field_name = ast_member_name(node);
    PgyDeclField *fields = NULL;
    size_t count = pgy_class_decl_field_model_build(decl, &fields);
    for (size_t i = 0; i < count; i++) {
        if (fields[i].name == NULL || field_name == NULL
            || strcmp(fields[i].name, field_name) != 0)
            continue;
        if (kind == NOMINAL_DECL_OBJECT) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
                PGY_CAUSE_IMMUTABLE_FIELD_WRITE,
                PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, node,
                "object '%s' fields are read-only after construction.\n"
                "Fix:\n- update the source and refresh or construct a new projection",
                root_name);
        } else if (kind == NOMINAL_DECL_TOBJECT) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
                PGY_CAUSE_IMMUTABLE_FIELD_WRITE,
                PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, node,
                "tobject '%s' fields are immutable.\n"
                "Fix:\n- update the source and publish a new transfer snapshot",
                root_name);
        } else if (!fields[i].is_mutable) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
                PGY_CAUSE_IMMUTABLE_FIELD_WRITE,
                PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND, node,
                "field '%s.%s' is immutable.\n"
                "Reason:\n- it is declared with `let` (an immutable field binding)\n"
                "Fix:\n- declare it `let mut %s: ...` or set it only at construction",
                root_name, field_name, field_name);
        } else {
            Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(
                ctx, fields[i].type_ast);
            if (resolved != NULL)
                path.type = resolved;
        }
        break;
    }
    pgy_decl_field_model_free(fields, count);
    return path;
}

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
        Symbol *target_sym = lookup_identifier_symbol(target, ctx);
        if (target_sym != NULL && target_sym->type != NULL &&
            target_sym->type != TYPE_UNKNOWN && target_sym->type->name != NULL &&
            !ast_assignment_set_semantic_binding_type_name_copy(expr,
                target_sym->type->name)) {
            semantic_error(ctx, expr,
                "Out of memory while recording assignment binding type");
        }
        if (target_sym != NULL && target_sym->type != NULL
            && target_sym->type->kind == TYPE_KIND_FUNCTION)
            callable_capability_invalidate_binding(ctx, target_sym);
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
                && reject_non_inout_param_collection_mutator_receiver(
                    array_node, array_sym->type, "array index assignment",
                    "array", ctx)) {
                return target_type;
            }
            /* Slice<T> is a borrowed view: writing through a default value
             * parameter would mutate the caller's backing array invisibly.
             * Same contract as the collection mutator receivers -- caller-
             * visible mutation must be spelled 'inout'. */
            if (array_sym != NULL && array_sym->is_parameter
                && array_sym->param_mode == PARAM_MODE_DEFAULT
                && type_is_constructed_named(array_sym->type, "Slice")) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                    PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH,
                    PGY_FIX_MATCH_BUILTIN_SIGNATURE, array_node,
                    "Slice index assignment cannot target default value parameter '%s'.\n"
                    "Reason:\n"
                    "- a Slice<T> is a borrowed view; writing through it mutates the caller's backing array\n"
                    "- caller-visible mutation must be explicit at the function boundary\n"
                    "Fix:\n"
                    "- spell the parameter as 'inout %s: %s' when caller mutation is intended\n"
                    "- or SliceCopy(%s) into an owned Array and mutate that locally",
                    array_name != NULL ? array_name : "<receiver>",
                    array_name != NULL ? array_name : "<receiver>",
                    array_sym->type != NULL && array_sym->type->name != NULL
                        ? array_sym->type->name : "Slice<T>",
                    array_name != NULL ? array_name : "<receiver>");
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

    if (target != NULL && (target->type == AST_MEMBER_ACCESS
            || target->type == AST_ARRAY_ACCESS)) {
        (void)assignment_field_write_path(target, ctx);
    }

    require_assignable(value_type, target_type, expr, ctx);
    return target_type;
}
