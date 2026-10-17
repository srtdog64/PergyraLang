/* Public storage release admission. Element resources never belong to this
 * operation; unresolved metadata and descriptor aliases refuse release. */
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "builtin_kind.h"
#include "diag_codes.h"
#include "slot_summary.h"
#include "array_storage_deferred_preservation_owner.h"
#include "collection_ownership_fact.h"

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
    Symbol *binding = source != NULL && source->type == AST_IDENTIFIER && ctx != NULL
        ? scope_lookup(ctx->scope, ast_identifier_name(source)) : NULL;
    uint32_t binding_id = binding != NULL ? binding->decl_syntax_id : 0;
    bool array = type_is_constructed_named(type, "Array");
    bool string_array = array
        && type_equals(type_get_constructed_arg(type, 0), TYPE_STRING);
    const PgyCollectionOwnershipFact *owned_strings = string_array && binding_id != 0
        && binding != NULL && !binding->has_exclusive_array_storage && ctx != NULL
        && ctx->current_function_decl != NULL
        ? semantic_collection_ownership_fact_find(ctx,
            ast_node_stable_id(ctx->current_function_decl), binding_id) : NULL;
    bool existing_authority = binding != NULL && binding_id != 0
        && ast_identifier_binding_syntax_id(source) == binding_id
        && (!binding->is_parameter || binding->param_mode == PARAM_MODE_OWN)
        && !binding->is_host_field && !binding->is_consumed
        && !binding->has_escaped_array_storage
        && (binding->has_exclusive_array_storage
            || (string_array
                && owned_strings != NULL
                && owned_strings->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && (owned_strings->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT
                    || owned_strings->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS)));
    bool plain_result = type_equals(result_type, TYPE_VOID)
        || semantic_array_storage_plain_element(result_type, ctx);
    bool exact_named = call != NULL && call->type == AST_CALL
        && callee_decl != NULL && callee_decl->type == AST_FUNC_DECL
        && ast_func_body(callee_decl) != NULL && !callee_decl->is_async_decl
        && ast_node_stable_id(callee_decl) != 0
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee_decl)
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && ast_call_semantic_callee_declared_callable(call);
    /* Only a body summary can preserve an existing storage authority. A
     * Void/scalar return says nothing about a callee retaining its argument. */
    bool exact_user_inout = mode == PARAM_MODE_MUT_REF
        && callee_decl != NULL && ast_node_stable_id(callee_decl) != 0
        && ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee_decl)
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && array
        && semantic_array_storage_plain_element(type_get_constructed_arg(type, 0), ctx);
    bool exact_user_borrow = (mode == PARAM_MODE_REF || mode == PARAM_MODE_DEFAULT)
        && array && plain_result
        && existing_authority && exact_named;
    /* The callee's summary reads call identities that its own body check
     * records; a later declaration has none yet. Pass 2 records the handoff
     * and keeps exclusivity provisional until every body is checked. */
    Symbol *handoff = (exact_user_inout || exact_user_borrow) && binding != NULL
        && semantic_array_storage_deferral_active(ctx) ? binding : NULL;
    bool preserving = (exact_user_inout || exact_user_borrow) && (handoff != NULL
        ? semantic_array_storage_record_pending_call(ctx, handoff, call, callee_decl, ordinal)
        : function_param_flow_preserves_array_storage(ctx, callee_decl, ordinal));
    if (constructor || (mode != PARAM_MODE_OWN && (!preserving || !plain_result)))
        /* A call without current preservation proof may retain the shallow
         * descriptor even when its result is Void. Deep cleanup must see it. */
        array_storage_invalidate_exclusivity(source, type, true, ctx);
}

void
semantic_array_storage_initialize(Symbol *binding, const ASTNode *initializer,
                                  SemanticContext *ctx)
{
    if (binding == NULL || !type_is_constructed_named(binding->type, "Array"))
        return;
    /* A literal allocates fresh backing storage.  General call results remain
     * unproved until return ownership is carried through the active collection
     * ownership rung; callable identity alone is not storage provenance. */
    binding->has_exclusive_array_storage = ctx->current_function_decl != NULL
        && initializer != NULL && initializer->type == AST_ARRAY_LITERAL;
    if (initializer != NULL && initializer->type == AST_IDENTIFIER) {
        Symbol *source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        /* A local move cannot erase a store retained by another descriptor.
         * Capture the prior state before this handoff invalidates the source. */
        binding->has_escaped_array_storage = source == NULL
            || ast_identifier_binding_syntax_id(initializer) != source->decl_syntax_id
            || source->has_escaped_array_storage;
    }
    semantic_array_storage_escape((ASTNode *)initializer, binding->type, ctx);
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

bool
semantic_array_storage_admit_drop(ASTNode *receiver, Type *array_type,
                                 SemanticContext *ctx)
{
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    bool owns = binding != NULL && !binding->is_host_field
        && binding->has_exclusive_array_storage
        && (!binding->is_parameter || binding->param_mode == PARAM_MODE_OWN);
    if (!type_is_constructed_named(array_type, "Array") || !owns) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
        return false;
    }
    if (!semantic_array_storage_plain_element(type_get_constructed_arg(array_type, 0), ctx)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BUILTIN_ARGS_INVALID,
            PGY_CAUSE_BUILTIN_SIGNATURE_MISMATCH, PGY_FIX_MATCH_BUILTIN_SIGNATURE,
            receiver, "ArrayDrop requires plain value elements without independent resource lifetimes; use ArrayDropOwnedStrings for owned strings");
        return false;
    }
    /* Exclusivity that rests on a recorded inout handoff is decided after
     * Pass 2, which refuses this release there if a handoff is unproved. */
    if (binding->has_pending_inout_preservation
        && !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_RELEASE_DROP_STORAGE)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
        return false;
    }
    return consume_array_storage_binding(receiver, ctx);
}
