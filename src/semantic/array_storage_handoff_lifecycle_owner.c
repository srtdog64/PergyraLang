/* Array handoff invalidation, binding birth, replacement, and assignment. */
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact_internal.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "indexed_string_named_call_owner.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include "slot_summary.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_defer_release_owner.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "loan_copy_landing_owner.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"
#include "indexed_string_definition_recipe.h"
#include "indexed_string_permanent_element.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
array_storage_invalidate_exclusivity(ASTNode *source, const Type *type,
    bool stored_alias, SemanticContext *ctx)
{
    if (ctx == NULL || source == NULL || source->type != AST_IDENTIFIER
        || !type_is_constructed_named(type, "Array")) return;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(source));
    if (binding != NULL) {
        binding->has_exclusive_array_storage = false;
        if (stored_alias) binding->has_escaped_array_storage = true;
    }
}

void
semantic_array_storage_escape(ASTNode *source, const Type *type,
                              SemanticContext *ctx)
{
    array_storage_invalidate_exclusivity(source, type, true, ctx);
}

void
semantic_array_storage_call_argument(ASTNode *source, const Type *type,
    const Type *result_type, ParamMode mode, bool constructor,
    ASTNode *call, ASTNode *callee_decl, size_t ordinal, SemanticContext *ctx)
{
    (void)result_type;
    if (ctx == NULL) return;
    Symbol *binding = source != NULL && source->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(source)) : NULL;
    uint32_t binding_id = binding != NULL ? binding->decl_syntax_id : 0;
    bool exact_named = call != NULL && call->type == AST_CALL
        && callee_decl != NULL && callee_decl->type == AST_FUNC_DECL
        && ast_func_body(callee_decl) != NULL && !callee_decl->is_async_decl
        && ast_node_stable_id(callee_decl) != 0
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee_decl)
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && ast_call_semantic_callee_declared_callable(call);
    if (type_equals(type, TYPE_STRING) && mode == PARAM_MODE_OWN
        && !constructor && exact_named && semantic_param_publication_pending(ctx)
        && binding != NULL && binding_id != 0 && !binding->is_parameter
        && !binding->is_host_field && !binding->is_consumed
        && ast_identifier_binding_syntax_id(source) == binding_id
        && !semantic_indexed_string_named_local_own_call_record(
            ctx, source, call, callee_decl, ordinal)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            source, "A local String own handoff has no current named-call receipt");
        return;
    }
    if (type_equals(type, TYPE_STRING) &&
        semantic_loan_copy_landing_refusal_tail_current(ctx)) {
        semantic_indexed_string_borrow_call_argument(source, call, callee_decl,
            ordinal, constructor, ctx);
        return;
    }
    if (ctx->has_error) return;
    bool array = type_is_constructed_named(type, "Array");
    bool string_array = array
        && type_equals(type_get_constructed_arg(type, 0), TYPE_STRING);
    bool exact_named_array = array && binding != NULL && binding_id != 0
        && source != NULL && source->type == AST_IDENTIFIER
        && ast_identifier_binding_syntax_id(source) == binding_id
        && type_equals(type, binding->type);
    /* An own formal consumes teardown authority, not just an Array-shaped
     * descriptor. Destructure, iteration, match payload, field/index reads,
     * and ordinary shallow lets all meet here as named bindings. Their
     * syntax-specific birth paths may preserve a descriptor, but only the
     * array-storage owner may grant exclusive cleanup authority. */
    if (mode == PARAM_MODE_OWN && exact_named_array
        && (binding->is_host_field || binding->is_consumed
            || binding->has_escaped_array_storage
            || !binding->has_exclusive_array_storage)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            source,
            "An own Array argument requires one current exclusive cleanup authority; '%s' is borrowed or aliased",
            binding->name != NULL ? binding->name : "value");
        return;
    }
    bool named_descriptor = binding != NULL && binding_id != 0
        && ast_identifier_binding_syntax_id(source) == binding_id
        && type_equals(type, binding->type)
        && !binding->is_host_field && !binding->is_consumed
        && !binding->has_escaped_array_storage;
    /* An own handoff in a defer cleanup takes the Array when the routine exits. */
    if (mode == PARAM_MODE_OWN && named_descriptor && ctx->in_defer_cleanup
        && !semantic_array_storage_record_defer_release(source, string_array, ctx))
        return;
    bool pending_same_binding = binding != NULL && binding_id != 0
        && source != NULL && ast_identifier_binding_syntax_id(source) == binding_id
        && !binding->is_host_field && !binding->is_consumed
        && !binding->has_escaped_array_storage
        && semantic_array_storage_has_pending(binding)
        && semantic_array_storage_deferral_active(ctx);
    /* Replacing an inout aggregate field with fresh storage separates the
     * old descriptor from the caller's field, but it does not by itself
     * prove that an own callee retires that detached storage.  Keep the
     * carrier borrowed unless this exact callee/formal has a current release
     * summary.  The ordinary ownership-boundary checker then reports every
     * unsafe actual; this owner only withdraws the provisional grant. */
    if (mode == PARAM_MODE_OWN && array && binding != NULL
        && binding->array_storage_member_move_source != NULL) {
        size_t owned_formal = 0;
        bool mapped = exact_named
            && (semantic_param_publication_pending(ctx)
                ? semantic_param_publication_selected_formal_for_actual(
                    ctx, call, callee_decl, ordinal, &owned_formal)
                : semantic_param_publication_formal_for_actual(
                    ctx, call, ordinal, &owned_formal));
        bool releases = mapped
            && semantic_collection_owned_parameter_releases_storage(
                ctx, callee_decl, owned_formal);
        /* Publication is sealed only after every routine body has been
         * checked.  An exact selected formal may therefore lack its release
         * summary solely because that body appears later in source order.
         * Keep the exact replacement grant provisional; the owned-element
         * finalizer validates the release summary and fails closed. */
        bool release_check_deferred = mapped
            && semantic_param_publication_pending(ctx);
        if (!releases && !release_check_deferred) {
            binding->has_exclusive_array_storage = false;
            binding->has_escaped_array_storage = true;
        }
    }
    /* Descriptor preservation comes from the callee body, not its result
     * shape. Release authority remains a separate, stricter decision. */
    bool exact_user_inout = mode == PARAM_MODE_MUT_REF
        && callee_decl != NULL && ast_node_stable_id(callee_decl) != 0
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee_decl)
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && array
        && (semantic_array_storage_plain_element(type_get_constructed_arg(type, 0), ctx)
            || string_array);
    bool exact_member_move_borrow = false;
    if ((mode == PARAM_MODE_REF || mode == PARAM_MODE_DEFAULT)
        && string_array && binding != NULL && binding_id != 0
        && exact_named && source != NULL
        && ast_identifier_binding_syntax_id(source) == binding_id
        && type_equals(type, binding->type)) {
        const PgyCollectionOwnershipFact *fact =
            semantic_collection_ownership_fact_find(ctx,
                ast_node_stable_id(ctx->current_function_decl), binding_id);
        size_t source_formal = SIZE_MAX;
        ParamMode source_mode = PARAM_MODE_DEFAULT;
        uint32_t source_fields[PGY_COLLECTION_MEMBER_PATH_LIMIT] = {0};
        size_t source_field_count = 0;
        exact_member_move_borrow =
            collection_member_move_call_entry_parameter_path(
                fact, binding, ctx, &source_formal, &source_mode,
                source_fields, &source_field_count)
            && source_formal != SIZE_MAX && source_field_count != 0;
    }
    if (exact_member_move_borrow
        && !semantic_collection_owned_element_requirement_record_member_read(
            call, callee_decl, ordinal, source, mode, ctx))
        return;
    bool exact_user_borrow = (mode == PARAM_MODE_REF || mode == PARAM_MODE_DEFAULT)
        && array
        && exact_named && named_descriptor;
    /* The callee's summary reads call identities that its own body check
     * records; a later declaration has none yet. Pass 2 records the handoff
     * and keeps exclusivity provisional until every body is checked. */
    bool reserved = false;
    uint32_t source_id = binding != NULL
        ? binding->collection_storage_source_syntax_id : 0;
    if (source_id == 0) source_id = binding_id;
    if (string_array && named_descriptor
        && !semantic_indexed_string_borrow_deferred_release_current(ctx,
            binding_id, source_id, &reserved)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            source, "Array handoff has no current deferred release identity");
        return;
    }
    Symbol *handoff = (exact_user_inout || exact_user_borrow)
        && binding != NULL && semantic_array_storage_deferral_active(ctx) ? binding : NULL;
    size_t formal = 0;
    bool preserving = (exact_user_inout || exact_user_borrow) && (handoff != NULL
        ? semantic_array_storage_record_pending_call(ctx, handoff, call, callee_decl, ordinal)
        : (semantic_param_publication_formal_for_actual(ctx, call, ordinal, &formal) &&
            function_param_flow_preserves_array_storage(ctx, callee_decl, formal)));
    /* The linked consumer follows this handoff; its prefix holds the
     * earlier calls. A defer release is admitted again at each exit. */
    if (handoff != NULL && (pending_same_binding || reserved)
        && (!preserving
            || !semantic_array_storage_record_pending_call_consumer(ctx, handoff, call, ordinal))) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            source, "Array handoff has no current preservation consumer");
        return;
    }
    /* A recorded named handoff preserves only the issued premise until the
     * independent element facet is checked. It never creates an owned one. */
    if ((mode == PARAM_MODE_DEFAULT || mode == PARAM_MODE_MUT_REF)
        && !(string_array && handoff != NULL)) {
        if (mode == PARAM_MODE_DEFAULT && exact_member_move_borrow)
            semantic_array_storage_revoke_result_elements_only(source, ctx);
        else
            semantic_array_storage_revoke_return_elements(source, ctx);
    }
    if (constructor || (mode != PARAM_MODE_OWN && !preserving))
        /* A call without current preservation proof may retain the shallow
         * descriptor even when its result is Void. Deep cleanup must see it. */
        array_storage_invalidate_exclusivity(source, type, true, ctx);
    /* No result-shape fallback follows. The descriptor-preservation owner
     * already separates whole Array storage from element-derived results and
     * fails closed on a retained descriptor. */
}

void
semantic_array_storage_initialize(Symbol *binding, const ASTNode *initializer,
                                  SemanticContext *ctx)
{
    if (binding == NULL || !type_is_constructed_named(binding->type, "Array"))
        return;
    /* A call birth is provisional until the return owner seals its actual
     * sources. A missing receipt never retains the old UNKNOWN permission. */
    binding->has_pending_array_result = false;
    binding->has_exclusive_array_storage = ctx->current_function_decl != NULL
        && initializer != NULL && initializer->type == AST_ARRAY_LITERAL;
    if (initializer != NULL && initializer->type == AST_ARRAY_LITERAL) {
        binding->has_escaped_array_storage = false;
    } else if (initializer != NULL && initializer->type == AST_CALL) {
        binding->has_pending_array_result = true;
        bool recorded = ctx->current_function_decl != NULL
            && semantic_array_storage_record_pending_result(ctx, binding,
                (ASTNode *)initializer);
        binding->has_exclusive_array_storage = recorded;
        binding->has_escaped_array_storage = !recorded;
    } else if (initializer != NULL && initializer->type == AST_IDENTIFIER) {
        Symbol *source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        /* Only a consumed source proves a transfer; a plain let copies the
         * descriptor. Neither path erases a prior escape. */
        binding->has_escaped_array_storage = source == NULL
            || ast_identifier_binding_syntax_id(initializer) != source->decl_syntax_id
            || source->has_escaped_array_storage || !source->is_consumed;
        binding->has_exclusive_array_storage = !binding->has_escaped_array_storage
            && source->has_exclusive_array_storage;
    } else if (initializer != NULL && initializer->type == AST_MEMBER_ACCESS
        && !collection_array_string_type(binding->type)) {
        PgyCollectionMemberPath path;
        if (collection_member_move_path_identity(initializer, ctx, &path)) {
            Symbol *root = scope_lookup(ctx->scope,
                ast_identifier_name(path.root));
            if (root != NULL && root->is_parameter
                && root->param_mode == PARAM_MODE_MUT_REF
                && root->decl_syntax_id == path.root_binding_id) {
                binding->array_storage_member_move_source = initializer;
                binding->array_storage_member_move_source_id =
                    ast_node_stable_id(initializer);
                binding->array_storage_member_move_root_binding_id =
                    path.root_binding_id;
                binding->array_storage_member_move_root_birth_serial =
                    root->birth_serial;
            }
        }
    }
    semantic_array_storage_escape((ASTNode *)initializer, binding->type, ctx);
}

static bool
array_storage_member_move_root_current(const Symbol *binding,
                                       SemanticContext *ctx)
{
    if (binding == NULL || ctx == NULL
        || binding->array_storage_member_move_source == NULL
        || binding->array_storage_member_move_source_id == 0
        || binding->array_storage_member_move_root_binding_id == 0)
        return false;
    PgyCollectionMemberPath path;
    if (!collection_member_move_path_identity(
            binding->array_storage_member_move_source, ctx, &path)
        || path.root_binding_id !=
            binding->array_storage_member_move_root_binding_id)
        return false;
    Symbol *root = scope_lookup(ctx->scope,
        ast_identifier_name(path.root));
    return root != NULL && root->is_parameter
        && root->param_mode == PARAM_MODE_MUT_REF
        && root->decl_syntax_id == path.root_binding_id
        && (binding->array_storage_member_move_root_birth_serial == 0
            || root->birth_serial == 0
            || binding->array_storage_member_move_root_birth_serial
                == root->birth_serial);
}

void
semantic_array_storage_prepare_member_replacement(ASTNode *target,
    ASTNode *value, SemanticContext *ctx)
{
    if (ctx == NULL || target == NULL || target->type != AST_MEMBER_ACCESS
        || value == NULL || value->type != AST_ARRAY_LITERAL
        || ast_array_literal_count(value) != 0)
        return;
    for (Scope *scope = ctx->scope; scope != NULL;
         scope = scope->kind == SCOPE_FUNCTION ? NULL : scope->parent) {
        for (size_t i = 0; i < scope->symbol_count; i++) {
            Symbol *binding = scope->symbols[i];
            if (binding == NULL || binding->is_consumed
                || !type_is_constructed_named(binding->type, "Array")
                || !array_storage_member_move_root_current(binding, ctx)
                || !collection_member_move_path_matches_source(
                    binding->array_storage_member_move_source,
                    binding->array_storage_member_move_source_id,
                    binding->array_storage_member_move_root_binding_id,
                    target, ctx, false))
                continue;
            /* The old backing now has one local carrier and the inout field
             * has a distinct empty backing. Branch joins retain this grant
             * only when every live path performed the exact replacement. */
            binding->has_escaped_array_storage = false;
            binding->has_exclusive_array_storage = true;
        }
    }
}

void
semantic_array_storage_assignment(ASTNode *target, const Type *target_type,
    ASTNode *value, const Type *value_type, SemanticContext *ctx)
{
    Symbol *binding = target != NULL && target->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(target, ctx) : NULL;
    Symbol *source = value != NULL && value->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(value, ctx) : NULL;
    if (binding != NULL && binding == source)
        return;
    if (type_equals(value_type, TYPE_STRING)
        && (binding == NULL || binding->is_host_field
            || (binding->is_parameter && binding->param_mode == PARAM_MODE_MUT_REF)))
        semantic_indexed_string_borrow_call_argument(value, NULL, NULL, 0, false, ctx);
    semantic_array_storage_escape(value, value_type, ctx);
    bool alias = source != NULL || (value != NULL
        && (value->type == AST_MEMBER_ACCESS || value->type == AST_ARRAY_ACCESS));
    array_storage_invalidate_exclusivity(target, target_type, alias, ctx);
    if (binding == NULL || ctx->in_defer_cleanup
        || !type_is_constructed_named(target_type, "Array")
        || scope_lookup_current(ctx->scope, binding->name) != binding)
        return;
    uint32_t builtin = 0;
    bool fresh = value != NULL && ((value->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(value) == 0)
        || (value->type == AST_CALL
            && ast_call_semantic_callee_builtin_kind(value, &builtin)
            && builtin == BUILTIN_CLONE));
    /* Only an exact independent producer at this lexical frontier replaces
     * the escaped storage. No element fact or exclusive grant is invented;
     * outer bindings written in branch/loop/defer scopes stay unproved. */
    if (fresh)
        binding->has_escaped_array_storage = false;
}
