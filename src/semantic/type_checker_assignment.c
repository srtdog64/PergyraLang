#include "type_checker_internal.h"
#include "string_window_extent.h"
#include "callable_capability_inference.h"
#include "type_checker_assignment.h"
#include "type_checker_builtins_internal.h"
#include "collection_ownership_fact.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "indexed_string_borrow_owner.h"
#include "function_param_publication_owner.h"
#include "type_checker_ownership_consumers_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_let_internal.h"
#include "type_checker_resolution_internal.h"
#include "diag_codes.h"
#include "compiler/decl_field_model.h"

#include <string.h>
#include <stdlib.h>

typedef struct {
    Type *type;
    Symbol *root;
    bool projected_slice;
} AssignmentFieldPath;

/* Walk the lvalue once, carrying the resolved type and root borrow mode.
 * Reading a nested member must not erase an outer immutable boundary. Types
 * come from declaration metadata; do not type-check/evaluate receivers again. */
static AssignmentFieldPath
assignment_field_write_path(ASTNode *node, SemanticContext *ctx)
{
    AssignmentFieldPath path = {TYPE_UNKNOWN, NULL, false};
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
        if (type_is_constructed_named(path.type, "Slice")
                && ast_array_access_array(node)->type != AST_IDENTIFIER)
            path.projected_slice = true;
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
                "Fix:\n- declare it `var %s: ...` or set it only at construction",
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

bool
semantic_admit_binding_replacement(ASTNode *target, SemanticContext *ctx)
{
    if (target == NULL || target->type != AST_IDENTIFIER)
        return true;
    Symbol *binding = lookup_identifier_symbol(target, ctx);
    if (binding == NULL || binding->kind != SYMBOL_VARIABLE
        || type_is_slot_handle(binding->type))
        return true;
    if (binding->is_parameter) {
        if (binding->param_mode != PARAM_MODE_REF)
            return true;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
            PGY_CAUSE_IMMUTABLE_FIELD_WRITE, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            target, "Cannot replace read-only ref parameter '%s'; use an inout boundary for caller-visible mutation",
            binding->name);
        return false;
    }
    if (binding->is_mut_binding)
        return true;
    if (binding->is_host_field) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_FIELD_WRITE,
            PGY_CAUSE_IMMUTABLE_FIELD_WRITE, PGY_FIX_RECONSTRUCT_OR_CHANGE_HOST_KIND,
            target, "Cannot replace immutable field '%s'; use a mutable field on a writable host",
            binding->name);
        return false;
    }
    semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_BINDING_WRITE,
        PGY_CAUSE_IMMUTABLE_BINDING_WRITE, PGY_FIX_USE_MUTABLE_BINDING,
        target, "Cannot reassign immutable binding '%s' declared at %u:%u.\n"
        "Reason:\n- `let` fixes the binding after initialization\n"
        "Fix:\n- declare it with `var` or `let mut` when rebinding is intended",
        binding->name, binding->decl_line, binding->decl_col);
    return false;
}

bool
semantic_admit_value_mutation(ASTNode *place, SemanticContext *ctx)
{
    ASTNode *root = place;
    while (root != NULL && (root->type == AST_MEMBER_ACCESS
                            || root->type == AST_ARRAY_ACCESS)) {
        root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root)
                                             : ast_array_access_array(root);
    }
    if (root == NULL || root->type != AST_IDENTIFIER)
        return true; /* Addressability is checked by the place owner. */
    Symbol *binding = lookup_identifier_symbol(root, ctx);
    /* An identity host's state is governed by its field/action authority;
     * an immutable handle does not freeze the host itself. Value aggregates
     * and collections, in contrast, inherit the root binding's permission;
     * a loop or match binding takes the permission of the owner it names. */
    if (binding != NULL && binding->is_storage_alias_binding) {
        if (!binding->storage_alias_owner_writable) {
            const Symbol *owner = binding->storage_alias_owner;
            semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_BINDING_WRITE,
                PGY_CAUSE_IMMUTABLE_BINDING_WRITE, PGY_FIX_USE_MUTABLE_BINDING,
                root, "Cannot change storage through '%s': it names a value held by immutable binding '%s' declared at %u:%u.\n"
                "Reason:\n- a loop or match binding names its iterable's or subject's storage\n"
                "Fix:\n- declare that owner with `var` or `let mut`",
                binding->name, owner != NULL ? owner->name : "<unknown>",
                owner != NULL ? owner->decl_line : 0u,
                owner != NULL ? owner->decl_col : 0u);
            return false;
        }
    } else if (binding == NULL || !expr_type_is_nominal_host_type(binding->type, ctx)) {
        if (!semantic_admit_binding_replacement(root, ctx))
            return false;
    }
    size_t before = ctx->diagnostic_count;
    bool projected_slice = false;
    if (place != root)
        projected_slice = assignment_field_write_path(place, ctx).projected_slice;
    if (ctx->diagnostic_count != before)
        return false;
    if (projected_slice) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_REDUCE_SCOPE_OR_RETRY, place,
            "A projected Slice has no proved writable backing place; use a view of a writable owner or SliceCopy before mutation");
        return false;
    }
    if (binding != NULL && type_is_constructed_named(binding->type, "Slice")) {
        /* The existing view origin owns the backing place. A mutable view
         * binding cannot grant write access to an immutable backing value. */
        if (ctx->in_parallel && binding->slice_split_info.has_fact && place == root) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, place,
                "A parallel split Slice descriptor cannot be replaced through an inout boundary");
            return false;
        }
        if (binding->is_parameter) {
            if (binding->param_mode == PARAM_MODE_DEFAULT) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
                    PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
                    place, "Slice mutation cannot target default value parameter '%s'; use an inout boundary for caller-visible mutation",
                    binding->name);
                return false;
            }
            return true; /* A ref boundary was refused above. */
        }
        bool proved = binding->slice_borrow_sources != NULL;
        for (SliceBorrowSource *source = binding->slice_borrow_sources;
                source != NULL; source = source->next) {
            if (source->readonly) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_IMMUTABLE_BINDING_WRITE,
                    PGY_CAUSE_IMMUTABLE_BINDING_WRITE, PGY_FIX_USE_MUTABLE_BINDING, place,
                    "A copied Slice cannot grant write permission from an immutable view");
                return false;
            }
            if (source->place == NULL || source->root == NULL) { proved = false; continue; }
            if (!semantic_admit_value_mutation(source->place, ctx)) return false;
        }
        if (!proved) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_REDUCE_SCOPE_OR_RETRY, place,
                "Slice write requires a proved writable backing place; use a view of a writable owner or SliceCopy before mutation");
            return false;
        }
    }
    return true;
}

/* Whether the storage `owner` names may change: a `var` local, a non-`ref`
 * parameter, an identity host or handle, a writable loop/match binding, or a
 * value no binding holds (a call result the loop or match alone owns). */
static bool
storage_alias_owner_writable(ASTNode *owner, SemanticContext *ctx,
                             Symbol **owner_binding)
{
    ASTNode *root = owner;
    while (root != NULL && (root->type == AST_MEMBER_ACCESS
                            || root->type == AST_ARRAY_ACCESS)) {
        root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root)
                                             : ast_array_access_array(root);
    }
    if (root == NULL || root->type != AST_IDENTIFIER)
        return true;
    Symbol *binding = lookup_identifier_symbol(root, ctx);
    if (binding == NULL || binding->kind != SYMBOL_VARIABLE)
        return true;
    *owner_binding = binding->is_storage_alias_binding
        && binding->storage_alias_owner != NULL
        ? binding->storage_alias_owner : binding;
    if (binding->is_storage_alias_binding)
        return binding->storage_alias_owner_writable;
    if (binding->is_parameter)
        return binding->param_mode != PARAM_MODE_REF;
    if (type_is_slot_handle(binding->type)
        || expr_type_is_nominal_host_type(binding->type, ctx))
        return true;
    return binding->is_mut_binding;
}

void
semantic_mark_storage_alias_binding(Symbol *binding, ASTNode *owner,
                                    SemanticContext *ctx)
{
    if (binding == NULL || owner == NULL)
        return;
    binding->is_storage_alias_binding = true;
    binding->storage_alias_owner = NULL;
    binding->storage_alias_owner_writable =
        storage_alias_owner_writable(owner, ctx, &binding->storage_alias_owner);
}

Type *
type_check_assignment(ASTNode *expr, SemanticContext *ctx)
{
    size_t diagnostic_base = ctx->diagnostic_count;
    Type *value_type;
    Type *target_type = NULL;
    ASTNode *target = ast_assignment_target(expr);
    ASTNode *value = ast_assignment_value(expr);
    AstAssignmentOperation operation = ast_assignment_operation(expr);
    if (operation != AST_ASSIGN_SET) {
        target_type = type_check_expression(target, ctx);
        if (target_type != NULL && target_type->kind == TYPE_KIND_FUNCTION
            && (operation == AST_ASSIGN_ADD || operation == AST_ASSIGN_SUBTRACT)) {
            /* Target typing owns the arithmetic/event distinction. The event
             * owner retains callback escape and handler-signature admission. */
            free(expr->data.assignment.semantic_binding_type_name);
            memset(&expr->data, 0, sizeof(expr->data));
            expr->type = operation == AST_ASSIGN_ADD
                ? AST_EVENT_SUBSCRIBE : AST_EVENT_UNSUBSCRIBE;
            expr->data.event_op.event = target;
            expr->data.event_op.handler = value;
            (void)type_check_event_subscription(expr, ctx,
                operation == AST_ASSIGN_ADD ? "subscription" : "unsubscription");
            return TYPE_VOID;
        }
    }
    /* Empty replacement closes an exact inout-member extraction before the
     * lvalue read. Without this ordering the moved-member guard would reject
     * the very assignment that separates the two descriptors. */
    semantic_array_storage_prepare_member_replacement(target, value, ctx);
    semantic_string_window_extent_record_assignment(ctx, target);
    Symbol *value_binding = target != NULL && target->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(target, ctx) : NULL;
    (void)semantic_admit_binding_replacement(target, ctx);
    bool tracked_value_place = value_binding != NULL
        && semantic_classify_ownership_type(value_binding->type, ctx)
            == OWNERSHIP_TYPE_BORROW_TRACKED;
    /* An empty collection literal takes the target's type as its storage
     * site, so the target is typed first; the literal has no operands. */
    bool target_typed_first = operation != AST_ASSIGN_SET
        || semantic_expr_is_empty_collection_literal(value);

    reject_if_embedded_world_zone_mutation(ctx, expr, target, "assignment");
    if (target_typed_first && target_type == NULL)
        target_type = tracked_value_place ? value_binding->type
            : type_check_expression(target, ctx);
    Type *released_member_restore_type =
        semantic_collection_released_member_restore_value_type(
            target, value, ctx);
    value_type = released_member_restore_type != NULL
        ? released_member_restore_type
        : type_check_expression_at_typed_site(value, target_type, ctx);
    semantic_bound_party_note_value_use(ctx, value);
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

    /* A direct field-to-local move may be closed only by assigning that exact
     * local back to the exact source field.  Record the transition before the
     * target read so member-use validation sees the restored state. */
    (void)semantic_collection_restore_moved_member(target, value, ctx);

    if (target != NULL && target->type == AST_IDENTIFIER) {
        Symbol *target_sym = lookup_identifier_symbol(target, ctx);
        if (target_sym != NULL && type_is_constructed_named(target_sym->type, "Slice")
                && ctx->in_parallel && target_sym->slice_split_info.has_fact) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_SERIALIZE_OUTSIDE_PARALLEL, target,
                "A parallel split Slice descriptor cannot be reassigned");
            return target_sym->type;
        }
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
        if (target_sym != NULL &&
            type_is_constructed_named(target_sym->type, "Array")) {
            Symbol *slice_sym = semantic_find_active_slice_borrow_for_array(
                ctx->scope, target_sym);
            if (slice_sym != NULL) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_REDUCE_SCOPE_OR_RETRY,
                    target,
                    "Cannot replace Array storage for '%s' while Slice '%s' is live.\n"
                    "Reason:\n- Slice<T> is a borrowed write-through view into the Array backing storage\n"
                    "- rebinding the Array would invalidate that view\n"
                    "Fix:\n- rebind before creating the Slice, or end the Slice's lexical scope first",
                    target_sym->name, slice_sym->name);
                return target_sym->type;
            }
        }
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

    if (!target_typed_first)
        target_type = tracked_value_place ? value_binding->type
            : type_check_expression(target, ctx);
    if (target_type == NULL)
        target_type = TYPE_UNKNOWN;
    if (operation != AST_ASSIGN_SET)
        (void)semantic_check_numeric_operands(expr,
            ast_assignment_binary_operator(operation), target_type, value_type, ctx);

    if (target != NULL && target->type == AST_ARRAY_ACCESS) {
        ASTNode *array_node = ast_array_access_array(target);
        if (array_node != NULL && array_node->type == AST_IDENTIFIER) {
            const char *array_name = ast_identifier_name(array_node);
            Symbol *array_sym = array_name != NULL
                ? scope_lookup(ctx->scope, array_name)
                : NULL;
            if (array_sym != NULL
                && reject_invalid_array_mutator_receiver(
                    array_node, array_sym->type, "array index assignment",
                    ctx)) {
                return target_type;
            }
            if (array_sym != NULL && type_is_constructed_named(array_sym->type, "Array")) {
                if (!semantic_array_storage_admit_mutation(array_node, ctx)
                    || semantic_collection_reject_unsafe_owned_string_mutation(
                        array_node, "array index assignment", ctx)) return target_type;
                if (type_equals(type_get_constructed_arg(array_sym->type, 0), TYPE_STRING))
                    semantic_array_storage_revoke_return_elements(array_node, ctx);
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
        if (!ctx->has_error && type_equals(type_slot_inner_type(target_type), TYPE_STRING))
            semantic_indexed_string_borrow_call_argument(
                value, NULL, NULL, 0, false, ctx);
        callable_capability_record_retained_value(ctx, expr, value, value_type);
        return target_type;
    }

    if (semantic_collection_reject_unsafe_string_array_assignment(
            target, value, target_type, value_type, ctx)) {
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
        (void)semantic_admit_value_mutation(target, ctx);
    }

    require_assignable(value_type, target_type, expr, ctx);
    bool callable_carrier_store = target != NULL && (target->type == AST_MEMBER_ACCESS || target->type == AST_ARRAY_ACCESS);
    if (target != NULL && target->type == AST_IDENTIFIER) {
        Symbol *target_sym = lookup_identifier_symbol(target, ctx);
        callable_carrier_store |= target_sym != NULL && (target_sym->is_host_field ||
            (target_sym->is_parameter && target_sym->param_mode == PARAM_MODE_MUT_REF));
    }
    if (callable_carrier_store) callable_capability_record_retained_value(ctx, expr, value, value_type);
    if (ctx->diagnostic_count == diagnostic_base
        && target != NULL && target->type == AST_IDENTIFIER) {
        Symbol *target_sym = lookup_identifier_symbol(target, ctx);
        /* The target is a write place, not a read of its old generation.
         * Restore liveness only after the RHS and all assignment contracts
         * admit the new value; this grants no array storage exclusivity. */
        if (tracked_value_place && target_sym != NULL)
            target_sym->is_consumed = false;
        if (target_sym != NULL && target_sym->type == TYPE_ALLOCATOR &&
            semantic_param_publication_pending(ctx))
            semantic_param_publication_invalidate_allocator_binding(ctx, target);
        semantic_indexed_string_borrow_record_assignment(
            target_sym, value, expr, ctx);
        if (target_sym != NULL && type_is_constructed_named(target_sym->type, "Slice")) {
            ownership_slice_record_source(target_sym, value, ctx);
            target_sym->slice_split_info.has_fact = false;
        }
        if (!ctx->has_error && semantic_param_publication_pending(ctx) &&
            target_sym != NULL && (!target_sym->is_parameter || target_sym->param_mode == PARAM_MODE_DEFAULT ||
                target_sym->param_mode == PARAM_MODE_OWN) &&
            !target_sym->is_host_field &&
            !type_is_slot_handle(target_type) && !type_is_resource_handle(target_type) &&
            !semantic_param_publication_record_assignment_replacement(ctx, expr,
                target_sym->decl_syntax_id, target_type))
            semantic_error(ctx, expr, "Checked local assignment replacement publication failed");
    }
    return target_type;
}
