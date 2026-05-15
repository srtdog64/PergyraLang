/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Let-binding ownership and boundary checks.
 */

#include <stdio.h>
#include <string.h>

#include "diag_codes.h"
#include "type_checker_internal.h"
#include "type_checker_ownership_internal.h"
#include "type_checker_ownership_let_internal.h"
#include "type_checker_ownership_support_internal.h"

static Type *
ownership_let_normalize_type(Type *type)
{
    return type != NULL ? type : TYPE_UNKNOWN;
}

static const char *
ownership_let_call_callee_name(const ASTNode *node)
{
    ASTNode *callee = ast_call_callee(node);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return NULL;
    return callee->data.identifier.name;
}

bool
type_check_let_decl(ASTNode *node, SemanticContext *ctx)
{
    const char *name = node->data.let_decl.name;
    ASTNode *init = node->data.let_decl.initializer;
    ASTNode *ann = node->data.let_decl.type;
    bool handled_slot_claim = false;
    Type *init_type;
    Type *decl_type;

    /* Check for duplicate in current scope */
    if (scope_lookup_current(ctx->scope, name) != NULL) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_REDECLARATION,
            PGY_CAUSE_SCOPE_DUPLICATE_SYMBOL,
            PGY_FIX_RENAME_OR_REMOVE_DUPLICATE,
            node,
            "Redeclaration of '%s' in the same scope", name);
        return false;
    }

    if (!ownership_let_try_claim_slot_decl(node, ctx, name, init, ann,
                                           &handled_slot_claim)) {
        return false;
    }
    if (handled_slot_claim)
        return true;

    /* Normal variable declaration with type inference */
    init_type = (init != NULL)
        ? ownership_let_normalize_type(type_check_expression(init, ctx))
        : TYPE_VOID;
    if (init != NULL)
        mark_world_embedded_zone_arguments(init, ctx);

    /* Type inference: if no annotation, infer from initializer */
    if (ann != NULL) {
        /* Explicit type annotation */
        decl_type = ownership_let_resolve_type_ref(ann, ctx);
        if (decl_type == NULL)
            decl_type = TYPE_UNKNOWN;
        if (ctx->program_root != NULL
            && ast_type_name(ann) != NULL) {
            ASTNode *class_decl = find_type_decl_by_name(ctx->program_root,
                                                         ast_type_name(ann));
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
                ast_type_name(ann) != NULL ? ast_type_name(ann) : "T");
        } else {
            const char *init_callee_name =
                ownership_let_call_callee_name(init);
            if (init->type == AST_CALL
                && init_callee_name != NULL
                && strcmp(init_callee_name, "BoxArray") == 0) {
                init_type = decl_type;
            } else if (init->type == AST_CALL
                       && ast_type_name(ann) != NULL
                       && init_callee_name != NULL
                       && strcmp(init_callee_name,
                                 ast_type_name(ann)) == 0
                       && decl_type != NULL
                       && decl_type->kind == TYPE_KIND_CONSTRUCTED) {
                init_type = decl_type;
            } else if (init->type == AST_ARRAY_LITERAL
                       && ast_array_literal_count(init) == 0
                       && type_is_constructed_named(decl_type, "Array")) {
                init_type = decl_type;
            } else if (init->type == AST_CALL
                       && init_callee_name != NULL
                       && strcmp(init_callee_name,
                                 "ClaimDeviceSlot") == 0
                       && type_is_constructed_named(decl_type, "DeviceSlot")) {
                init_type = decl_type;
            }
            if (!slot_transfer_compatible(init_type, decl_type))
                require_assignable(init_type, decl_type, init, ctx);
        }
    } else if (init != NULL) {
        /* Infer type from initializer */
        decl_type = init_type;

        if (ownership_let_is_unresolved_none_option(init_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_INFER_REQUIRED,
                PGY_CAUSE_INFER_NO_SOURCE,
                PGY_FIX_ADD_ANNOTATION_OR_INITIALIZER,
                init,
                "Cannot infer Option<T> from None without an explicit annotation.\n"
                "Reason:\n"
                "- None carries absence but no payload type\n"
                "- defaulting None to Option<Int> would make backend output depend on a local fallback\n"
                "Fix:\n"
                "- write 'let %s: Option<T> = None()' with a concrete T\n"
                "- or initialize with Some(value) when the payload type should be inferred",
                name != NULL ? name : "value");
            decl_type = TYPE_UNKNOWN;
        }

        if (ownership_let_is_unresolved_empty_array(init_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_INFER_COLLECTION,
                PGY_CAUSE_INFER_COLLECTION_NEEDS_ANNOTATION,
                PGY_FIX_ANNOTATE_COLLECTION_ELEMENT_TYPE,
                init,
                "Cannot infer Array<T> from an empty array literal without an explicit annotation.\n"
                "Reason:\n"
                "- [] has no element value from which T can be inferred\n"
                "- defaulting [] to Array<Int> would make backend output depend on a local fallback\n"
                "Fix:\n"
                "- write 'let %s: Array<T> = []' with a concrete T\n"
                "- or initialize with at least one element when T should be inferred",
                name != NULL ? name : "value");
            decl_type = TYPE_UNKNOWN;
        }

        if (ownership_let_is_unresolved_device_slot(init_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_INFER_REQUIRED,
                PGY_CAUSE_INFER_NO_SOURCE,
                PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                init,
                "Cannot infer DeviceSlot<T> from ClaimDeviceSlot without an explicit annotation.\n"
                "Reason:\n"
                "- ClaimDeviceSlot allocates a device resource but carries no payload value\n"
                "- defaulting DeviceSlot<T> to DeviceSlot<Int> would diverge from backend requirements\n"
                "Fix:\n"
                "- write 'let %s: DeviceSlot<T> = ClaimDeviceSlot()' with a concrete T",
                name != NULL ? name : "device");
            decl_type = TYPE_UNKNOWN;
        }

        if (init->type == AST_CALL
            && init_type == TYPE_UNKNOWN) {
            const char *callee_name = ownership_let_call_callee_name(init);
            if (callee_name != NULL
                && (strcmp(callee_name, "ListNew") == 0
                    || strcmp(callee_name, "SetNew") == 0
                    || strcmp(callee_name, "MapNew") == 0
                    || strcmp(callee_name, "QueueNew") == 0)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_INFER_COLLECTION,
                    PGY_CAUSE_INFER_COLLECTION_NEEDS_ANNOTATION,
                    PGY_FIX_ANNOTATE_COLLECTION_ELEMENT_TYPE,
                    init,
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
        if (init_type != NULL && init_type->kind == TYPE_KIND_GENERIC) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_INFER_GENERIC,
                PGY_CAUSE_INFER_UNBOUND_GENERIC,
                PGY_FIX_ANNOTATE_BINDING_TYPE,
                init,
                "Cannot infer type: generic parameter '%s' is ambiguous. "
                "Please provide a type annotation.", init_type->name);
            decl_type = TYPE_UNKNOWN;
        }
    } else {
        /* No annotation and no initializer */
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_INFER_REQUIRED,
            PGY_CAUSE_INFER_NO_SOURCE,
            PGY_FIX_ADD_ANNOTATION_OR_INITIALIZER,
            node,
            "Cannot infer type: provide a type annotation or initializer");
        decl_type = TYPE_UNKNOWN;
    }

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
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
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
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
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
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_MOVE_FROM_RELEASED,
                    PGY_CAUSE_MOVE_FROM_RELEASED,
                    PGY_FIX_RECLAIM_OR_TRACE_EARLIER_MOVE,
                    init,
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
        bool new_write_view = false;
        bool view_init = false;
        if (init != NULL && init->type == AST_CALL
            && ast_call_arg_count(init) >= 1
            && ast_call_argument(init, 0) != NULL
            && ast_call_argument(init, 0)->type == AST_IDENTIFIER) {
            const char *callee_name = ownership_let_call_callee_name(init);
            if (callee_name != NULL
                && (strcmp(callee_name, "ViewRead") == 0
                    || strcmp(callee_name, "ViewWrite") == 0
                    || strcmp(callee_name, "Move") == 0)) {
                source_slot = ast_call_argument(init, 0)->data.identifier.name;
            }
        }
        view_init = ownership_let_view_init_info(init, &source_slot,
                                                 &new_write_view);
        if (view_init && source_slot != NULL) {
            const char *existing_name = NULL;
            const char *existing_kind = NULL;
            if (ctx->in_parallel) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    node,
                    "%s for slot '%s' cannot be acquired inside a parallel task.\n"
                    "Reason:\n"
                    "- pinned views are scoped capability leases for the stable beta ownership subset\n"
                    "- parallel tasks would make view cleanup and aliasing order depend on task scheduling\n"
                    "Fix:\n"
                    "- acquire the view outside parallel only for sequential work\n"
                    "- or serialize the slot access before/after the parallel block",
                    new_write_view ? "WriteView<T>" : "ReadView<T>",
                    source_slot);
                return false;
            }
            if (ownership_let_find_conflicting_view(ctx->scope,
                                                    source_slot,
                                                    new_write_view,
                                                    &existing_name,
                                                    &existing_kind)) {
                semantic_error_with_hints(ctx,
                    PGY_CODE_SEM_PIN_PARALLEL_CONFLICT,
                    PGY_CAUSE_PIN_PARALLEL_CONFLICT,
                    PGY_FIX_SERIALIZE_PIN_ACCESS,
                    node,
                    "%s for slot '%s' conflicts with existing %s '%s'.\n"
                    "Reason:\n"
                    "- WriteView<T> is exclusive for the stable beta ownership subset\n"
                    "- overlapping read/write views would hide aliasing and cleanup order from CFG dataflow\n"
                    "Fix:\n"
                    "- keep the write view in a smaller scope\n"
                    "- or finish/drop the existing view before acquiring a WriteView<T>",
                    new_write_view ? "WriteView<T>" : "ReadView<T>",
                    source_slot,
                    existing_kind != NULL ? existing_kind : "view",
                    existing_name != NULL ? existing_name : "<view>");
                return false;
            }
        }

        Symbol *sym = symbol_create_view(name, decl_type, source_slot,
            node->line, node->column);
        scope_declare(ctx->scope, sym);
        return !ctx->has_error;
    }

    if (decl_type != NULL && decl_type->kind == TYPE_KIND_SLOT) {
        char token_name_buf[256];
        const char *paired_token = NULL;
        Symbol *sym;

        if (decl_type->data.slot.is_secure)
            semantic_record_effect(ctx, EFFECT_SECURE);
        if (slot_transfer_compatible(init_type, decl_type)) {
            if (init != NULL && init->type == AST_IDENTIFIER) {
                Symbol *move_sym = scope_lookup(ctx->scope,
                    init->data.identifier.name);
                if (move_sym != NULL)
                    move_sym->is_consumed = true;
            }
        }
        if (init_type != NULL && type_is_anchored_resource_handle(init_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
                PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                node,
                "Slot handles (anchored) such as Slot/SecureSlot/DeviceSlot cannot be copied into a new binding.\n"
                "Reason:\n"
                "- slot handles (anchored) are tied to one ownership/runtime anchor and transfer contract\n"
                "- creating a new binding would duplicate that anchor contract across a second boundary-visible handle name\n"
                "Fix:\n"
                "- keep using the current slot handle (anchored) binding directly\n"
                "- or use a fresh claim\n"
                "- or initialize from an inner/plain value\n"
                "- or pass the handle through an explicit ownership boundary");
        }

        if (decl_type->data.slot.is_secure) {
            if (!semantic_format_secure_token_name(
                    token_name_buf, sizeof(token_name_buf), name, node, ctx))
                return !ctx->has_error;
            paired_token = token_name_buf;
        }
        sym = symbol_create_slot(name, decl_type,
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

    if (type_is_anchored_resource_handle(decl_type)) {
        bool is_fresh_claim = expr_is_device_slot_claim(init);
        if (!is_fresh_claim && init_type != NULL
            && type_is_anchored_resource_handle(init_type)) {
            semantic_error_with_hints(ctx,
                PGY_CODE_SEM_ANCHORED_HANDLE_COPY,
                PGY_CAUSE_ANCHORED_HANDLE_COPY_ATTEMPT,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                node,
                "Slot handles (anchored) such as Slot/SecureSlot/DeviceSlot cannot be copied into a new binding.\n"
                "Reason:\n"
                "- slot handles (anchored) remain tied to one owning runtime anchor and transfer contract\n"
                "- copying them into another binding would duplicate that contract across a second boundary-visible handle name\n"
                "Fix:\n"
                "- keep using the current slot handle (anchored) binding directly\n"
                "- or reacquire/transfer it through an explicit ownership boundary");
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

    scope_declare(ctx->scope, sym);

    return !ctx->has_error;
}
