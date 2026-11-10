/* Array payload calls, CFG path state, and element-premise revocation. */
#include "array_storage_element_lifetime_owner.h"
#include "array_storage_deferred_preservation_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "compiler/decl_field_model.h"
#include "function_param_publication_internal.h"
#include "collection_ownership_fact.h"
#include "indexed_string_borrow_owner.h"
#include "diag_codes.h"
#include "type_checker_resolution_metadata_internal.h"
#include "slot_summary.h"
#include "builtin_kind.h"
#include <stdlib.h>
#include <string.h>

bool
semantic_array_storage_payload_call_capture(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *argument, ASTNode *callee_decl,
    size_t ordinal, uint32_t *source_id)
{
    if (source_id == NULL || ctx == NULL || ctx->current_function_decl == NULL
        || binding == NULL || binding->is_consumed || binding->decl_syntax_id == 0
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || callee_decl->is_async_decl || ast_func_body(callee_decl) == NULL) return false;
    uint32_t binding_source = binding->collection_storage_source_syntax_id != 0
        ? binding->collection_storage_source_syntax_id : binding->decl_syntax_id;
    uint32_t current_source = 0;
    size_t formal_ordinal = 0;
    if (!semantic_param_publication_selected_formal_for_actual(ctx,
            call, callee_decl, ordinal, &formal_ordinal)) return false;
    FuncParam *formal = ast_func_param(callee_decl, formal_ordinal);
    if (formal == NULL || formal->type == NULL || binding_source == UINT32_MAX
        || !type_equals(semantic_type_resolution_lookup_resolved_type(ctx, formal->type), TYPE_STRING)
        || !semantic_indexed_string_borrow_source_current(argument, ctx, &current_source)
        || current_source == 0 || current_source != binding_source) return false;
    *source_id = current_source;
    return true;
}

bool
semantic_array_storage_payload_callee_preserves(SemanticContext *ctx,
    ASTNode *argument, ASTNode *callee_decl, size_t ordinal,
    const Type *array_type, uint32_t source_id)
{
    if (ctx == NULL || argument == NULL || callee_decl == NULL
        || callee_decl->type != AST_FUNC_DECL || ordinal >= ast_func_param_count(callee_decl)
        || source_id == 0 || source_id == UINT32_MAX
        || !type_is_constructed_named(array_type, "Array")
        || !type_equals(type_get_constructed_arg(array_type, 0), TYPE_STRING)) return false;
    FuncParam *formal = ast_func_param(callee_decl, ordinal);
    uint32_t current_source = 0;
    const unsigned retaining = SLOT_PARAM_SUMMARY_WRITE | SLOT_PARAM_SUMMARY_RELEASE
        | SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE
        | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    return formal != NULL && formal->type != NULL
        && type_equals(semantic_type_resolution_lookup_resolved_type(ctx, formal->type), TYPE_STRING)
        && semantic_indexed_string_borrow_source_current(argument, ctx, &current_source)
        && current_source == source_id
        && (function_param_flow_summary_for_param(ctx, callee_decl, ordinal) & retaining) == 0;
}

bool
semantic_array_storage_initialize_return_elements(PgyCollectionOwnershipFact *fact,
    const Symbol *binding, const ASTNode *initializer, SemanticContext *ctx)
{
    if (fact == NULL) return false;
    fact->return_elements_owned = false;
    fact->return_elements_source = NULL;
    fact->return_elements_source_id = 0;
    if (binding == NULL || ctx == NULL || initializer == NULL || binding->is_parameter
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)) return false;
    if (initializer->type == AST_IDENTIFIER) {
        uint32_t source_id = ast_identifier_binding_syntax_id(initializer);
        Symbol *source = scope_lookup(ctx->scope, ast_identifier_name(initializer));
        const PgyCollectionOwnershipFact *prior = semantic_collection_ownership_fact_find(ctx,
            fact->function_syntax_id, source_id);
        if (source == NULL || source->decl_syntax_id != source_id || source->is_parameter
            || !type_equals(source->type, binding->type) || prior == NULL
            || prior->disposition != PGY_COLLECTION_DISPOSITION_LIVE) return false;
        fact->return_elements_owned = prior->return_elements_owned;
        fact->return_elements_source = prior->return_elements_source;
        fact->return_elements_source_id = prior->return_elements_source_id;
        return false;
    }
    const ASTNode *dependency = NULL;
    fact->return_elements_owned = semantic_array_storage_return_elements_owned(
        (ASTNode *)initializer, ctx, &dependency);
    if (fact->return_elements_owned && initializer->type == AST_CALL)
        dependency = initializer;
    else if (fact->return_elements_owned && initializer->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(initializer) != 0)
        dependency = initializer;
    fact->return_elements_source = dependency;
    fact->return_elements_source_id = ast_node_stable_id(dependency);
    return fact->return_elements_owned
        && initializer->type == AST_ARRAY_LITERAL
        && ast_array_literal_count(initializer) != 0
        && dependency == initializer
        && fact->return_elements_source_id == ast_node_stable_id(initializer);
}

static bool
array_storage_return_element_dependency_current(const ASTNode *source,
    uint32_t source_id)
{
    if ((source == NULL) != (source_id == 0)) return false;
    return source == NULL
        || ((source->type == AST_CALL || source->type == AST_ARRAY_LITERAL)
            && source_id == ast_node_stable_id(source));
}

bool
semantic_array_storage_capture_return_element_path_state(
    const Symbol *binding, SemanticContext *ctx, bool *present_out,
    bool *owned_out, const ASTNode **source_out, uint32_t *source_id_out)
{
    if (present_out == NULL || owned_out == NULL || source_out == NULL
        || source_id_out == NULL)
        return false;
    *present_out = false;
    *owned_out = false;
    *source_out = NULL;
    *source_id_out = 0;
    if (binding == NULL || ctx == NULL
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0),
            TYPE_STRING))
        return true;
    uint32_t function_id = ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
    if (function_id == 0 || binding->decl_syntax_id == 0)
        return true;
    const PgyCollectionOwnershipFact *fact =
        semantic_collection_ownership_fact_find(ctx, function_id,
            binding->decl_syntax_id);
    if (fact == NULL) return true;
    if (fact->function_syntax_id != function_id
        || fact->binding_syntax_id != binding->decl_syntax_id
        || !array_storage_return_element_dependency_current(
            fact->return_elements_source,
            fact->return_elements_source_id))
        return false;
    *present_out = true;
    *owned_out = fact->return_elements_owned;
    *source_out = fact->return_elements_source;
    *source_id_out = fact->return_elements_source_id;
    return true;
}

bool
semantic_array_storage_restore_return_element_path_state(
    const Symbol *binding, SemanticContext *ctx, bool present, bool owned,
    const ASTNode *source, uint32_t source_id)
{
    if (!present) return true;
    if (binding == NULL || ctx == NULL || binding->decl_syntax_id == 0
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0),
            TYPE_STRING)
        || !array_storage_return_element_dependency_current(source, source_id))
        return false;
    uint32_t function_id = ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
    PgyCollectionOwnershipFact *fact =
        (PgyCollectionOwnershipFact *)semantic_collection_ownership_fact_find(
            ctx, function_id, binding->decl_syntax_id);
    if (function_id == 0 || fact == NULL
        || fact->function_syntax_id != function_id
        || fact->binding_syntax_id != binding->decl_syntax_id)
        return false;
    fact->return_elements_owned = owned;
    fact->return_elements_source = source;
    fact->return_elements_source_id = source_id;
    return true;
}

bool
semantic_array_storage_capture_member_element_path_state(
    const Symbol *binding, SemanticContext *ctx,
    ArrayStorageMemberElementPremiseState *state_out)
{
    if (state_out == NULL)
        return false;
    memset(state_out, 0, sizeof(*state_out));
    if (binding == NULL || ctx == NULL
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0),
            TYPE_STRING))
        return true;
    uint32_t function_id = ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
    const PgyCollectionOwnershipFact *fact =
        semantic_collection_ownership_fact_find(ctx, function_id,
            binding->decl_syntax_id);
    if (fact == NULL || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE)
        return true;
    if (function_id == 0 || binding->decl_syntax_id == 0
        || fact->function_syntax_id != function_id
        || fact->binding_syntax_id != binding->decl_syntax_id
        || fact->member_move_source == NULL
        || fact->member_move_source_id == 0
        || fact->member_move_source_id
            != ast_node_stable_id(fact->member_move_source)
        || fact->source_binding_syntax_id == 0
        || fact->origin_syntax_id == 0
        || fact->source_birth_serial == 0)
        return false;
    *state_out = (ArrayStorageMemberElementPremiseState){
        .present = true,
        .current = fact->member_move_elements_current,
        .source = fact->member_move_source,
        .source_id = fact->member_move_source_id,
        .root_binding_id = fact->source_binding_syntax_id,
        .leaf_field_id = fact->origin_syntax_id,
        .source_birth_serial = fact->source_birth_serial};
    return true;
}

bool
semantic_array_storage_restore_member_element_path_state(
    const Symbol *binding, SemanticContext *ctx,
    const ArrayStorageMemberElementPremiseState *state)
{
    if (state == NULL)
        return false;
    if (!state->present)
        return true;
    if (binding == NULL || ctx == NULL || binding->decl_syntax_id == 0
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0),
            TYPE_STRING)
        || state->source == NULL || state->source_id == 0
        || state->source_id != ast_node_stable_id(state->source)
        || state->root_binding_id == 0 || state->leaf_field_id == 0
        || state->source_birth_serial == 0)
        return false;
    uint32_t function_id = ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
    PgyCollectionOwnershipFact *fact =
        (PgyCollectionOwnershipFact *)semantic_collection_ownership_fact_find(
            ctx, function_id, binding->decl_syntax_id);
    if (function_id == 0 || fact == NULL
        || fact->function_syntax_id != function_id
        || fact->binding_syntax_id != binding->decl_syntax_id
        || fact->origin != PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        || fact->member_move_source != state->source
        || fact->member_move_source_id != state->source_id
        || fact->source_binding_syntax_id != state->root_binding_id
        || fact->origin_syntax_id != state->leaf_field_id
        || fact->source_birth_serial != state->source_birth_serial)
        return false;
    fact->member_move_elements_current = state->current;
    return true;
}

static void
revoke_array_element_premises(ASTNode *value, SemanticContext *ctx,
    bool revoke_member_premise)
{
    if (value == NULL || value->type != AST_IDENTIFIER || ctx == NULL) return;
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
    uint32_t id = ast_identifier_binding_syntax_id(value);
    if (binding == NULL || id == 0 || binding->decl_syntax_id != id
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)) return;
    PgyCollectionOwnershipFact *fact = (PgyCollectionOwnershipFact *)semantic_collection_ownership_fact_find(
        ctx, ast_node_stable_id(ctx->current_function_decl), id);
    if (fact != NULL) {
        fact->return_elements_owned = false;
        if (revoke_member_premise)
            fact->member_move_elements_current = false;
    }
}

void
semantic_array_storage_revoke_return_elements(ASTNode *value,
    SemanticContext *ctx)
{
    revoke_array_element_premises(value, ctx, true);
}

void
semantic_array_storage_revoke_result_elements_only(ASTNode *value,
    SemanticContext *ctx)
{
    revoke_array_element_premises(value, ctx, false);
}

bool
semantic_array_storage_record_return_element_effect(ASTNode *receiver,
    uint32_t kind, bool current, bool member_value_copy_preserves_path,
    SemanticContext *ctx)
{
    /* A terminal drop retires the value; it does not rewrite the premise
     * captured before that consumption. A selected value-copy landing, or an
     * exact candidate the sealed finalizer must select or refuse, is a pending
     * rewrite of this exact push. It may preserve a current moved-member
     * premise while the fixed-point owner replaces the borrowed value, but it
     * cannot restore a premise that an earlier unrelated mutation revoked. */
    if (current && kind != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
        && kind != PGY_COLLECTION_EFFECT_DROP
        && kind != PGY_COLLECTION_EFFECT_OWNED_STRING_SET
        && kind != PGY_COLLECTION_EFFECT_OWNED_STRING_POP)
        revoke_array_element_premises(receiver, ctx,
            !member_value_copy_preserves_path);
    return current;
}

