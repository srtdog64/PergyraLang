#include "collection_ownership_fact_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diag_codes.h"
#include "symbol_table.h"
#include "type_checker.h"
#include "type_checker_internal.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "formal_string_store_landing_owner.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_owned_element_requirement_owner.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_result_inputs.h"
#include "loan_copy_landing_owner.h"
#include "../parser/ast_api.h"

/* An owned String among borrowed elements, or among elements of unknown
 * ownership moved out of a member, leaves no whole-array cleanup policy. */
static bool
owned_string_push_mixes(const PgyCollectionOwnershipFact *fact)
{
    return fact != NULL && (fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS
        || (fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
            && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE));
}

bool
semantic_collection_owned_string_push_admissible(ASTNode *receiver,
                                                 SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    bool provisional =
        semantic_collection_owned_element_member_push_provisional(
            receiver, ctx);
    return !(binding != NULL && binding->has_escaped_array_storage
            && !provisional)
        && (!owned_string_push_mixes(fact) || provisional);
}

typedef enum {
    OWNED_STRING_PARAMETER_PUSH,
    OWNED_STRING_PARAMETER_POP
} OwnedStringParameterMutation;

static bool
owned_string_parameter_mutation_admit(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx,
    bool *parameter_receiver_out, OwnedStringParameterMutation mutation)
{
    ASTNode *routine = ctx != NULL ? ctx->current_function_decl : NULL;
    size_t formal_index = SIZE_MAX;
    ParamMode formal_mode = PARAM_MODE_DEFAULT;
    bool parameter_carrier = false;

    if (parameter_receiver_out != NULL)
        *parameter_receiver_out = false;
    if (receiver == NULL || receiver->type != AST_IDENTIFIER
        || routine == NULL || routine->type != AST_FUNC_DECL)
        return false;
    bool carrier_current =
        semantic_collection_owned_element_parameter_carrier_current(
            receiver, routine, ctx, &formal_index, &formal_mode,
            &parameter_carrier);
    if (parameter_receiver_out != NULL)
        *parameter_receiver_out = parameter_carrier;

    size_t expected_arity = mutation == OWNED_STRING_PARAMETER_POP ? 1u : 2u;
    bool expected_call = mutation == OWNED_STRING_PARAMETER_POP
        ? collection_call_is_unshadowed_builtin(call, "ArrayPop")
        : (collection_call_is_unshadowed_builtin(call, "ArrayPush")
            || collection_call_is_unshadowed_builtin(
                call, "ArrayPushOwnedString"));
    if (call == NULL || call->type != AST_CALL
        || ast_call_arg_count(call) != expected_arity
        || ast_call_argument(call, 0) != receiver
        || !expected_call
        || !carrier_current
        || (formal_mode != PARAM_MODE_OWN
            && formal_mode != PARAM_MODE_MUT_REF
            && formal_mode != PARAM_MODE_DEFAULT))
        return false;
    return formal_index != SIZE_MAX
        && semantic_collection_owned_element_requirement_record_parameter_mutation(
            call, receiver, routine, formal_index, ctx);
}

bool
semantic_collection_owned_string_parameter_push_admit(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx,
    bool *parameter_receiver_out)
{
    return owned_string_parameter_mutation_admit(call, receiver, ctx,
        parameter_receiver_out, OWNED_STRING_PARAMETER_PUSH);
}

bool
semantic_collection_owned_string_parameter_pop_admit(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx,
    bool *parameter_receiver_out)
{
    return owned_string_parameter_mutation_admit(call, receiver, ctx,
        parameter_receiver_out, OWNED_STRING_PARAMETER_POP);
}

bool
semantic_collection_admit_owned_string_push(ASTNode *receiver,
                                             bool prior_exclusive_receipt,
                                             SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    const char *name = binding != NULL && binding->name != NULL
        ? binding->name : "<array>";
    bool provisional =
        semantic_collection_owned_element_member_push_provisional(
            receiver, ctx);

    if (binding != NULL && binding->has_escaped_array_storage
        && !provisional && !prior_exclusive_receipt) {
        if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
            const char *routine = ctx->current_function_decl == NULL ? NULL
                : ast_declaration_name(ctx->current_function_decl);
            fprintf(stderr,
                "pgy: owned-string-push-storage-escaped routine=%s receiver=%s"
                " line=%u column=%u\n",
                routine != NULL ? routine : "<unknown>", name,
                receiver != NULL ? receiver->line : 0,
                receiver != NULL ? receiver->column : 0);
        }
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver, "ArrayPushOwnedString cannot mutate escaped array storage in '%s'",
            name);
        return false;
    }
    if (owned_string_push_mixes(fact) && !provisional) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "ArrayPushOwnedString cannot mix an owned String into '%s' without uniform element ownership.\n"
            "Reason:\n"
            "- the existing elements are borrowed or have no deep-release proof\n"
            "- the resulting array would have no sound whole-array cleanup policy\n"
            "Fix:\n"
            "- Clone the collection into an owned snapshot before the owned push\n"
            "- or use ArrayPush for a borrowed String element",
            name);
        return false;
    }
    if (semantic_array_storage_has_pending(binding)
        && !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_MUTATE_OWNED_STRINGS)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver, "ArrayPushOwnedString requires current preserved array storage in '%s'",
            name);
        return false;
    }
    return true;
}

static bool
owned_string_local_binding_current(const PgyCollectionOwnershipFact *fact,
                                   const Symbol *binding,
                                   const SemanticContext *ctx)
{
    if (fact == NULL || binding == NULL || ctx == NULL
        || fact->function_syntax_id != current_function_syntax_id(ctx))
        return false;
    return fact->binding_syntax_id != 0
        && fact->binding_syntax_id == binding->decl_syntax_id
        && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
        && !binding->is_consumed
        && collection_array_string_type(binding->type);
}

static bool
owned_string_empty_local_origin_current(const PgyCollectionOwnershipFact *fact,
                                        const Symbol *binding,
                                        const SemanticContext *ctx)
{
    return owned_string_local_binding_current(fact, binding, ctx)
        && !binding->is_parameter && !binding->is_host_field
        && fact->origin == PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
        && fact->origin_syntax_id != 0
        && fact->source_binding_syntax_id == 0;
}

static bool
owned_string_local_lifecycle_current(const PgyCollectionOwnershipFact *fact,
                                     const Symbol *binding,
                                     const SemanticContext *ctx)
{
    if (!owned_string_local_binding_current(fact, binding, ctx))
        return false;
    if (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
        || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        return true;

    /* An empty local starts with a vacuous all-elements-owned premise.  Each
     * shallow transition revokes that premise, while an exact owned push/set
     * receipt preserves it.  This is the dynamic transition proof for a
     * locally grown Array<String>; it is deliberately narrower than treating
     * a general return-elements receipt as element-ownership permission. */
    return owned_string_empty_local_origin_current(fact, binding, ctx)
        && fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
        && fact->return_elements_owned
        && fact->return_elements_source == NULL
        && fact->return_elements_source_id == 0;
}

static const ASTNode *
owned_string_empty_local_initializer_for_identity(const ASTNode *receiver,
    const ASTNode *routine, uint32_t binding_id, SemanticContext *ctx)
{
    uint32_t routine_id = routine != NULL && routine->type == AST_FUNC_DECL
        ? ast_node_stable_id(routine) : 0;
    PublishedNode *binding_row = ctx != NULL && binding_id != 0
        ? publication_node_by_id(ctx->function_param_publications, binding_id)
        : NULL;
    const ASTNode *declaration = binding_row != NULL
        ? binding_row->node : NULL;
    const ASTNode *initializer = declaration != NULL
        && declaration->type == AST_LET_DECL
        ? ast_let_initializer(declaration) : NULL;
    const PgyCollectionOwnershipFact *fact = ctx != NULL
        ? semantic_collection_ownership_fact_find(ctx, routine_id, binding_id)
        : NULL;

    /* Declaration rows own stable syntax and type identity, but do not carry a
     * caller.  The collection fact owns the binding-to-routine relation. */
    if (ctx == NULL || routine_id == 0 || binding_id == 0
        || receiver == NULL || receiver->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(receiver) != binding_id
        || binding_row == NULL || binding_row->id != binding_id
        || declaration == NULL || declaration->type != AST_LET_DECL
        || ast_node_stable_id(declaration) != binding_id
        || ast_let_is_alias(declaration)
        || initializer == NULL || initializer->type != AST_ARRAY_LITERAL
        || ast_node_stable_id(initializer) == 0
        || ast_array_literal_count(initializer) != 0
        || fact == NULL || fact->function_syntax_id != routine_id
        || fact->binding_syntax_id != binding_id
        || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
        || fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
        || fact->origin_syntax_id != ast_node_stable_id(initializer)
        || fact->source_binding_syntax_id != 0)
        return NULL;
    return initializer;
}

const ASTNode *
semantic_collection_owned_string_empty_local_initializer_at_call_entry(
    const ASTNode *receiver, const Symbol *binding, const ASTNode *routine,
    SemanticContext *ctx)
{
    if (binding == NULL || binding->decl_syntax_id == 0
        || binding->is_parameter || binding->is_host_field
        || binding->is_consumed || !collection_array_string_type(binding->type))
        return NULL;
    return owned_string_empty_local_initializer_for_identity(receiver, routine,
        binding->decl_syntax_id, ctx);
}

bool
semantic_collection_owned_string_empty_local_origin_for_routine(
    const ASTNode *receiver, const ASTNode *routine, SemanticContext *ctx)
{
    uint32_t binding_id = receiver != NULL
        && receiver->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, receiver) : 0;
    const ASTNode *initializer =
        owned_string_empty_local_initializer_for_identity(
            receiver, routine, binding_id, ctx);
    SemanticPublishedValue value = ctx != NULL && receiver != NULL
        ? semantic_param_publication_value(ctx, receiver)
        : (SemanticPublishedValue){0};
    return initializer != NULL
        && semantic_param_publication_inputs_ready(ctx)
        && value.ok && !value.meta
        && type_is_constructed_named(value.type, "Array")
        && type_equals(type_get_constructed_arg(value.type, 0), TYPE_STRING);
}

bool
semantic_collection_owned_string_set_required(ASTNode *receiver,
                                               SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    return owned_string_local_lifecycle_current(fact, binding, ctx);
}

bool
semantic_collection_admit_owned_string_set(ASTNode *receiver,
                                            SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    const char *name = binding != NULL && binding->name != NULL
        ? binding->name : "<array>";

    if (!owned_string_local_lifecycle_current(fact, binding, ctx)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "Owned String replacement requires one live Array<String> owner for '%s'",
            name);
        return false;
    }
    /* An escaped descriptor can have String element loans outside this
     * function. Those aliases cannot be invalidated when the old element is
     * retired, so replacement fails closed. */
    if (binding->has_escaped_array_storage) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "Owned String replacement cannot retire an element of escaped array storage in '%s'",
            name);
        return false;
    }
    if (semantic_array_storage_has_pending(binding)
        && !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_MUTATE_OWNED_STRINGS)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "Owned String replacement requires current preserved array storage in '%s'",
            name);
        return false;
    }
    return true;
}

bool
semantic_collection_owned_string_pop_required(ASTNode *receiver,
                                               SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    return owned_string_local_lifecycle_current(fact, binding, ctx);
}

bool
semantic_collection_admit_owned_string_pop(ASTNode *receiver,
                                            SemanticContext *ctx)
{
    Symbol *binding = NULL;
    PgyCollectionOwnershipFact *fact =
        receiver_collection_fact(receiver, ctx, &binding);
    const char *name = binding != NULL && binding->name != NULL
        ? binding->name : "<array>";

    if (!owned_string_local_lifecycle_current(fact, binding, ctx)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "Owned String pop requires one live Array<String> owner for '%s'",
            name);
        return false;
    }
    if (binding->has_escaped_array_storage) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "Owned String pop cannot retire an element of escaped array storage in '%s'",
            name);
        return false;
    }
    if (semantic_array_storage_has_pending(binding)
        && !semantic_array_storage_record_pending_drop(ctx, receiver, binding,
            ARRAY_STORAGE_MUTATE_OWNED_STRINGS)) {
        semantic_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            receiver,
            "Owned String pop requires current preserved array storage in '%s'",
            name);
        return false;
    }
    return true;
}

bool
collection_call_is_unshadowed_builtin(const ASTNode *expr,
                                      const char *expected_name)
{
    ASTNode *callee;
    const char *name;

    if (expr == NULL || expr->type != AST_CALL || expected_name == NULL)
        return false;
    callee = ast_call_callee(expr);
    if (callee == NULL || callee->type != AST_IDENTIFIER)
        return false;
    name = ast_identifier_name(callee);
    return name != NULL && strcmp(name, expected_name) == 0
        && ast_identifier_binding_syntax_id(callee) == 0;
}

bool
semantic_collection_record_call_effect(ASTNode *call, ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind, SemanticContext *ctx)
{
    return semantic_collection_record_call_effect_from(call, receiver, kind, 0, ctx);
}

static bool
collection_effect_publish(ASTNode *call,
    PgyCollectionOwnershipEffectKind kind, uint32_t receiver_binding_id,
    uint32_t source_binding_id, SemanticContext *ctx)
{
    uint32_t issued_kind = 0, issued_receiver = 0, issued_source = 0;
    bool current = ast_call_set_semantic_collection_effect(call,
            (uint32_t)kind, receiver_binding_id, source_binding_id)
        || (ast_call_semantic_collection_effect(call, &issued_kind,
                &issued_receiver, &issued_source)
            && issued_kind == (uint32_t)kind
            && issued_receiver == receiver_binding_id
            && issued_source == source_binding_id);
    return current && function_param_flow_collection_effect_projection_publish(
        ctx, call, (uint32_t)kind, receiver_binding_id, source_binding_id);
}

bool
semantic_collection_record_call_effect_from(ASTNode *call, ASTNode *receiver,
    PgyCollectionOwnershipEffectKind kind, uint32_t source_syntax_id, SemanticContext *ctx)
{
    Symbol *binding = NULL;
    uint32_t binding_id;
    bool member_push_provisional;
    PgyCollectionMemberPath direct_member_path;
    size_t direct_member_formal = SIZE_MAX;
    ParamMode direct_member_mode = PARAM_MODE_DEFAULT;
    if (call == NULL || call->type != AST_CALL || receiver == NULL
        || kind <= PGY_COLLECTION_EFFECT_NONE
        || kind > PGY_COLLECTION_EFFECT_OWNED_STRING_POP
        || (source_syntax_id != 0 && kind != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH))
        return false;
    PgyCollectionOwnershipFact *fact = receiver_collection_fact(receiver, ctx, &binding);
    bool selected_value_copy = kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
        && ast_call_arg_count(call) == 2
        && collection_call_is_unshadowed_builtin(call, "ArrayPush")
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && (semantic_formal_string_store_landing_value_copy(call, ctx)
            || semantic_loan_copy_landing_satisfied(
                ctx, ast_call_argument(call, 1)));
    bool selected_member_value_copy = fact != NULL
        && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        && selected_value_copy;
    bool pending_member_value_copy = fact != NULL
        && fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        && kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
        && semantic_formal_string_store_landing_member_value_copy_candidate_recorded(
            ctx, call);
    bool member_value_copy_preserves_path =
        selected_member_value_copy || pending_member_value_copy;
    member_push_provisional = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
        && semantic_collection_owned_element_member_push_provisional(
            receiver, ctx);
    bool direct_parameter_member = member_push_provisional
        && collection_current_parameter_member_path(
            receiver, ctx, &direct_member_formal, &direct_member_mode,
            &direct_member_path);
    bool direct_local_member = member_push_provisional
        && !direct_parameter_member
        && collection_current_local_member_path(
            receiver, ctx, &direct_member_path);
    bool direct_member_receiver =
        direct_parameter_member || direct_local_member;
    if (fact == NULL) {
        if (direct_member_receiver) {
            binding_id = direct_member_path.field_ids[
                direct_member_path.field_count - 1];
            bool current = collection_effect_publish(call, kind, binding_id,
                source_syntax_id, ctx);
            if (!current
                || !semantic_collection_owned_element_requirement_record_member_push(
                    call, receiver, ctx))
                return false;
            return semantic_array_storage_record_return_element_effect(
                receiver, kind, current, false, ctx);
        }
        /* Parameter storage is decided by the interprocedural carriage
         * owner, but the exact store call still needs the same immutable
         * effect receipt as a local receiver.  The carriage consumer then
         * proves whether the formal preserves that effect; it must not
         * reconstruct the store from its spelling. */
        binding_id = receiver_binding_syntax_id(receiver, binding);
        if (binding == NULL || !binding->is_parameter ||
            !collection_array_string_type(binding->type) || binding_id == 0 ||
            binding->decl_syntax_id != binding_id)
            return !ast_call_semantic_collection_effect(
                call, NULL, NULL, NULL);
        bool current = collection_effect_publish(call, kind, binding_id,
            source_syntax_id, ctx);
        return semantic_array_storage_record_return_element_effect(
            receiver, kind, current, false, ctx);
    }
    bool owned_local_drop = kind == PGY_COLLECTION_EFFECT_DROP
        && (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
            || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        && (fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            || fact->origin == PGY_COLLECTION_ORIGIN_CLONE
            || fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL
            || fact->origin == PGY_COLLECTION_ORIGIN_BINDING);
    bool owned_local_set = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_SET
        && (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
            || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        && (fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            || fact->origin == PGY_COLLECTION_ORIGIN_CLONE
            || fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL
            || fact->origin == PGY_COLLECTION_ORIGIN_BINDING);
    bool owned_local_pop = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_POP
        && (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
            || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        && (fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            || fact->origin == PGY_COLLECTION_ORIGIN_CLONE
            || fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL
            || fact->origin == PGY_COLLECTION_ORIGIN_BINDING);
    bool owned_local_push = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
        && (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
            || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        && (fact->origin == PGY_COLLECTION_ORIGIN_MAP_KEYS
            || fact->origin == PGY_COLLECTION_ORIGIN_CLONE
            || fact->origin == PGY_COLLECTION_ORIGIN_OWNED_LITERAL
            || fact->origin == PGY_COLLECTION_ORIGIN_BINDING);
    bool borrowed_shallow =
        (fact->origin == PGY_COLLECTION_ORIGIN_BORROWED_LITERAL
            || fact->origin == PGY_COLLECTION_ORIGIN_BINDING)
        && fact->element_ownership == PGY_STRING_ARRAY_BORROWED_ELEMENTS
        && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
        && kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
        && collection_call_is_unshadowed_builtin(call, "ArrayPush")
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && collection_private_push_recorded(ctx, call, receiver,
            receiver_binding_syntax_id(receiver, binding));
    bool owned_initializer_elements_shallow =
        fact->origin == PGY_COLLECTION_ORIGIN_UNKNOWN
        && fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
        && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
        && fact->return_elements_source != NULL
        && fact->return_elements_source_id != 0
        && fact->return_elements_source_id
            == ast_node_stable_id(fact->return_elements_source)
        && (fact->return_elements_source->type == AST_CALL
            || fact->return_elements_source->type == AST_ARRAY_LITERAL)
        && kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
        && source_syntax_id == 0
        && collection_call_is_unshadowed_builtin(call, "ArrayPush")
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0;
    uint32_t current_owned_source = 0;
    bool owned_initializer_elements_push =
        fact->origin == PGY_COLLECTION_ORIGIN_UNKNOWN
        && fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
        && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
        && fact->return_elements_owned
        && fact->return_elements_source != NULL
        && fact->return_elements_source_id != 0
        && fact->return_elements_source_id
            == ast_node_stable_id(fact->return_elements_source)
        && kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
        && source_syntax_id != 0
        && ast_call_arg_count(call) == 2
        && collection_call_is_unshadowed_builtin(call, "ArrayPush")
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && semantic_collection_owned_string_call_result(
            ast_call_argument(call, 1), ctx, &current_owned_source)
        && current_owned_source == source_syntax_id;
    bool call_result_candidate =
        fact->origin == PGY_COLLECTION_ORIGIN_UNKNOWN
        && fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
        && fact->return_elements_owned
        && fact->return_elements_source != NULL
        && fact->return_elements_source->type == AST_CALL
        && fact->return_elements_source_id != 0
        && fact->return_elements_source_id
            == ast_node_stable_id(fact->return_elements_source)
        && ast_call_semantic_callee_declared_callable(
            fact->return_elements_source)
        && ast_call_semantic_callee_decl_id(
            fact->return_elements_source) != 0
        && ast_call_semantic_callee_value_binding_id(
            fact->return_elements_source) == 0;
    bool call_result_effect = call_result_candidate
        && source_syntax_id == 0
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && ((kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && ast_call_arg_count(call) == 2
                && collection_call_is_unshadowed_builtin(
                    call, "ArrayPushOwnedString"))
            || (kind == PGY_COLLECTION_EFFECT_OWNED_STRING_SET
                && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && ast_call_arg_count(call) == 3
                && collection_call_is_unshadowed_builtin(
                    call, "ArraySetOwnedString"))
            || (kind == PGY_COLLECTION_EFFECT_OWNED_STRING_POP
                && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && ast_call_arg_count(call) == 1
                && collection_call_is_unshadowed_builtin(call, "ArrayPop"))
            || (kind == PGY_COLLECTION_EFFECT_DROP
                && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
                && ast_call_arg_count(call) == 1
                && collection_call_is_unshadowed_builtin(
                    call, "ArrayDropOwnedStrings")));
    if (fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL
        && !owned_local_drop && !owned_local_set && !owned_local_pop
        && !owned_local_push
        && !borrowed_shallow
        && !owned_initializer_elements_shallow
        && !owned_initializer_elements_push && !call_result_effect
        && !member_push_provisional) {
        return semantic_array_storage_record_return_element_effect(receiver,
            kind,
            !ast_call_semantic_collection_effect(call, NULL, NULL, NULL),
            member_value_copy_preserves_path, ctx);
    }
    if (fact->disposition != (owned_local_drop
            ? PGY_COLLECTION_DISPOSITION_RETIRED
            : PGY_COLLECTION_DISPOSITION_LIVE)) return false;
    binding_id = receiver_binding_syntax_id(receiver, binding);
    if (binding == NULL || !collection_array_string_type(binding->type) || binding_id == 0
        || binding->decl_syntax_id != binding_id) return false;
    /* The parser receipt stays write-once; only its exact recheck is valid. */
    bool current = collection_effect_publish(call, kind, binding_id,
        source_syntax_id, ctx);
    if (current && (call_result_effect
            || owned_initializer_elements_push))
        fact->call_result_effect_pending = true;
    if (current && call_result_effect
        && kind == PGY_COLLECTION_EFFECT_DROP)
        fact->disposition = PGY_COLLECTION_DISPOSITION_RETIRED;
    if (member_push_provisional) {
        /* The exact effect receipt is issued before the deferred aggregate
         * requirement consumes it.  This keeps the collection-effect owner
         * authoritative for both lowering shapes instead of asking the
         * requirement owner to infer ownership from the callee name. */
        if (!current
            || !semantic_collection_owned_element_requirement_record_member_push(
                call, receiver, ctx))
            return false;
        fact->return_elements_owned = true;
        fact->return_elements_source = NULL;
        fact->return_elements_source_id = 0;
    }
    return semantic_array_storage_record_return_element_effect(
        receiver, kind, current, member_value_copy_preserves_path, ctx);
}

bool
semantic_collection_member_value_copy_replay_ready(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx)
{
    if (semantic_collection_owned_element_member_value_copy_candidate_current(
            call, receiver, ctx, false))
        return true;

    PublishedNode *call_row = call != NULL
        ? publication_result_current(ctx, call) : NULL;
    uint32_t caller_id = call_row != NULL ? call_row->caller_id : 0;
    uint32_t binding_id = receiver != NULL
        && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    PublishedNode *binding_row = ctx != NULL && binding_id != 0
        ? publication_node_by_id(ctx->function_param_publications, binding_id)
        : NULL;
    ASTNode *declaration = binding_row != NULL
        ? (ASTNode *)binding_row->node : NULL;
    ASTNode *initializer = declaration != NULL
        && declaration->type == AST_LET_DECL
        ? ast_let_initializer(declaration) : NULL;
    ASTNode *value = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 1) : NULL;
    uint32_t builtin = 0;
    const PgyCollectionOwnershipFact *fact = ctx != NULL
        ? semantic_collection_ownership_fact_find(
            ctx, caller_id, binding_id)
        : NULL;
    SemanticPublishedValue receiver_value =
        semantic_param_publication_value(ctx, receiver);

    if (!semantic_formal_string_store_landing_member_value_copy_selected(
            call, ctx)
        || call_row == NULL || call_row->node != call
        || call_row->id != ast_node_stable_id(call)
        || caller_id == 0 || call_row->caller == NULL
        || ast_node_stable_id(call_row->caller) != caller_id
        || !semantic_param_publication_checked_call(ctx, call)
        || !semantic_param_publication_checked_argument(ctx, call, 0)
        || !semantic_param_publication_checked_argument(ctx, call, 1)
        || receiver == NULL || receiver->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(receiver) != binding_id
        || !receiver_value.ok || receiver_value.meta
        || !type_is_constructed_named(receiver_value.type, "Array")
        || !type_equals(type_get_constructed_arg(
                receiver_value.type, 0), TYPE_STRING)
        || !collection_call_is_unshadowed_builtin(
            call, "ArrayPushOwnedString")
        || !ast_call_semantic_callee_builtin_kind(call, &builtin)
        || builtin != BUILTIN_ARRAY_PUSH_OWNED_STRING
        || !semantic_loan_copy_landing_satisfied(ctx, value)
        || fact == NULL || fact->function_syntax_id != caller_id
        || fact->binding_syntax_id != binding_id
        || (fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            && fact->disposition != PGY_COLLECTION_DISPOSITION_RETIRED))
        return false;

    bool member_path_current =
        semantic_collection_owned_element_member_value_copy_candidate_current(
            call, receiver, ctx, true);
    if (member_path_current)
        return true;

    bool independent_clone = binding_row != NULL
        && binding_row->id == binding_id
        && declaration != NULL && declaration->type == AST_LET_DECL
        && ast_node_stable_id(declaration) == binding_id
        && !ast_let_is_alias(declaration)
        && initializer != NULL && initializer->type == AST_CALL
        && fact->origin == PGY_COLLECTION_ORIGIN_CLONE
        && fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
        && fact->origin_syntax_id == ast_node_stable_id(initializer)
        && fact->source_binding_syntax_id == 0
        && fact->member_move_source == NULL
        && fact->member_move_source_id == 0
        && semantic_loan_copy_landing_satisfied(ctx, initializer);
    return independent_clone;
}

bool
semantic_collection_record_member_value_copy_replay_effect(
    ASTNode *call, ASTNode *receiver, SemanticContext *ctx)
{
    PublishedNode *call_row = call != NULL
        ? publication_result_current(ctx, call) : NULL;
    uint32_t binding_id = receiver != NULL
        && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;

    if (!semantic_collection_member_value_copy_replay_ready(
            call, receiver, ctx)
        || call_row == NULL || call_row->caller_id == 0
        || binding_id == 0)
        return false;

    bool current = collection_effect_publish(call,
        PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH, binding_id, 0, ctx);
    return semantic_array_storage_record_return_element_effect(
        receiver, PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH,
        current, false, ctx);
}

static bool
owned_string_mutation_effect_current(
    const ASTNode *call,
    const ASTNode *receiver,
    SemanticContext *ctx,
    PgyCollectionOwnershipEffectKind expected_kind,
    size_t expected_arity,
    const char *expected_name)
{
    Symbol *binding = NULL;
    uint32_t kind = 0, receiver_id = 0, source_id = 0;
    uint32_t expected_receiver, current_source = 0;
    if (call == NULL || call->type != AST_CALL || receiver == NULL ||
        ast_call_arg_count(call) != expected_arity ||
        ast_call_argument(call, 0) != receiver ||
        !ast_call_semantic_collection_effect(call, &kind, &receiver_id,
            &source_id) || kind != (uint32_t)expected_kind)
        return false;
    (void)receiver_collection_fact((ASTNode *)receiver, ctx, &binding);
    expected_receiver = receiver_binding_syntax_id((ASTNode *)receiver,
        binding);
    if (expected_receiver == 0
        && expected_kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH) {
        PgyCollectionMemberPath direct_path;
        ASTNode *direct_function = NULL;
        size_t direct_formal = SIZE_MAX;
        ParamMode direct_mode = PARAM_MODE_DEFAULT;
        bool direct_current = collection_current_parameter_member_path(
            receiver, ctx, &direct_formal, &direct_mode, &direct_path);
        if (!direct_current)
            direct_current = collection_current_local_member_path(
                receiver, ctx, &direct_path);
        if (!direct_current)
            direct_current = collection_published_parameter_member_path(
                receiver, ctx, &direct_function, &direct_formal,
                &direct_mode, &direct_path);
        if (!direct_current)
            direct_current = collection_published_local_member_path(
                receiver, ctx, &direct_function, &direct_path);
        if (direct_current)
            expected_receiver = direct_path.field_ids[
                direct_path.field_count - 1];
    }
    if (expected_receiver == 0 || receiver_id != expected_receiver)
        return false;
    if (source_id == 0)
        return collection_call_is_unshadowed_builtin(call, expected_name);
    return expected_kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
        && collection_call_is_unshadowed_builtin(call, "ArrayPush")
        && semantic_collection_owned_string_call_result(
            ast_call_argument(call, 1), ctx, &current_source)
        && current_source == source_id;
}

bool
semantic_collection_owned_string_push_effect_current(
    const ASTNode *call,
    const ASTNode *receiver,
    SemanticContext *ctx)
{
    return owned_string_mutation_effect_current(call, receiver, ctx,
        PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH, 2,
        "ArrayPushOwnedString");
}

bool
semantic_collection_owned_string_set_effect_current(
    const ASTNode *call,
    const ASTNode *receiver,
    SemanticContext *ctx)
{
    return owned_string_mutation_effect_current(call, receiver, ctx,
        PGY_COLLECTION_EFFECT_OWNED_STRING_SET, 3,
        "ArraySetOwnedString");
}

bool
semantic_collection_owned_string_pop_effect_current(
    const ASTNode *call,
    const ASTNode *receiver,
    SemanticContext *ctx)
{
    return owned_string_mutation_effect_current(call, receiver, ctx,
        PGY_COLLECTION_EFFECT_OWNED_STRING_POP, 1, "ArrayPop");
}

bool
semantic_collection_owned_string_pop_receipt_ready(
    const ASTNode *call,
    const ASTNode *receiver)
{
    uint32_t kind = 0, receiver_id = 0, source_id = 0;
    return call != NULL && call->type == AST_CALL
        && receiver != NULL && receiver->type == AST_IDENTIFIER
        && ast_call_arg_count(call) == 1
        && ast_call_argument(call, 0) == receiver
        && collection_call_is_unshadowed_builtin(call, "ArrayPop")
        && ast_call_semantic_collection_effect(
            call, &kind, &receiver_id, &source_id)
        && kind == PGY_COLLECTION_EFFECT_OWNED_STRING_POP
        && receiver_id != 0
        && receiver_id == ast_identifier_binding_syntax_id(receiver)
        && source_id == 0;
}

static ASTNode *
collection_direct_let_for_binding(ASTNode *body, uint32_t binding_syntax_id)
{
    if (body == NULL || body->type != AST_BLOCK || binding_syntax_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt != NULL && stmt->type == AST_LET_DECL
            && ast_node_stable_id(stmt) == binding_syntax_id)
            return stmt;
    }
    return NULL;
}

typedef struct OwnedStringResultVisit {
    const ASTNode *decl;
    const struct OwnedStringResultVisit *parent;
} OwnedStringResultVisit;

typedef struct OwnedStringExpressionVisit {
    const ASTNode *expression;
    const struct OwnedStringExpressionVisit *parent;
} OwnedStringExpressionVisit;

static bool collection_callable_type_returns_owned_string_at(const ASTNode *call,
    SemanticContext *ctx, uint32_t *producer_id_out, const OwnedStringResultVisit *parent);

static bool
collection_owned_string_inputs_empty(const IndexedStringResultInputs *inputs)
{
    return inputs != NULL && !inputs->prepared && inputs->calls == NULL
        && inputs->dependencies == NULL && inputs->call_count == 0
        && inputs->call_capacity == 0 && inputs->sealed_calls == 0
        && inputs->dependency_count == 0 && inputs->dependency_capacity == 0
        && inputs->sealed_dependencies == 0 && inputs->option == NULL
        && inputs->permanents == NULL && inputs->permanent_count == 0
        && inputs->permanent_capacity == 0 && inputs->sealed_permanents == 0;
}

static bool
collection_owned_string_call_fact_current(
    const ASTNode *call, SemanticContext *ctx, PublishedNode **row_out)
{
    if (row_out != NULL)
        *row_out = NULL;
    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || !publication_generation_ready(ctx)
        || (!ctx->function_param_publications->pending
            && !ctx->function_param_publications->inputs_ready))
        return false;
    PublishedNode *row = publication_node(
        ctx->function_param_publications, call, false);
    if (row == NULL || row->node != call
        || row->id != ast_node_stable_id(call)
        || !publication_call_ready(ctx, row) || !row->value_present || row->meta
        || row->value_shape != AST_CALL || row->value_type != TYPE_STRING
        || !row->string_value)
        return false;
    for (size_t p = 0; p < row->argument_count; p++) {
        const PublishedCallArgument *arg = &row->arguments[p];
        PublishedNode *value = publication_node(
            ctx->function_param_publications, arg->node, false);
        if (value == NULL || value->node != arg->node || value->id != arg->id
            || !value->value_present || value->meta
            || value->value_type == NULL || value->value_type == TYPE_UNKNOWN
            || value->value_type != arg->type || value->value_shape != arg->shape
            || (arg->shape == AST_IDENTIFIER
                && value->binding_id != arg->binding)
            || (arg->shape == AST_CALL && !publication_call_ready(ctx, value)))
            return false;
    }
    if (row_out != NULL)
        *row_out = row;
    return true;
}

static bool collection_owned_string_source_current(const ASTNode *expression,
    SemanticContext *ctx, const OwnedStringResultVisit *parent,
    IndexedStringResultInputs *inputs)
{
    /* String `+` is lowered to StringConcat and owns fresh storage.  Keep the
     * same typed publication proof used by ordinary expression consumers;
     * treating it as an unproved call result would lose the producer exactly
     * when a caller returns the value beside a live Array<String>. */
    if (expression != NULL && expression->type == AST_BINARY)
        return inputs == NULL
            && semantic_indexed_string_independent_result(expression, ctx);
    PublishedNode *row = NULL;
    if (!collection_owned_string_call_fact_current(expression, ctx, &row))
        return false;
    if (inputs != NULL) {
        if (!semantic_indexed_string_option_unwrap_candidate(ctx, expression))
            return false;
        if (!semantic_param_publication_inputs_ready(ctx))
            return ctx->function_param_publications->pending
                && collection_owned_string_inputs_empty(inputs);
        uint32_t source = UINT32_MAX;
        return semantic_indexed_string_result_source_current(
                ctx, expression, &source, inputs)
            && source == 0;
    }
    if (row->kind_present && row->builtin == BUILTIN_TEXT_BUILDER_FINISH)
        return semantic_param_publication_checked_finish_independent(
            ctx, expression);
    if (semantic_indexed_string_independent_result(expression, ctx)) return true;
    uint32_t flow_source = UINT32_MAX;
    if (function_param_flow_string_result_source_current(
            ctx, expression, &flow_source)
        && flow_source == 0)
        return true;
    bool result_lane = false;
    for (size_t a = 0; a < ast_call_arg_count(expression); a++)
        if (semantic_param_publication_result_lane_argument(
                ctx, expression, a))
            result_lane = true;
    if (result_lane
        && function_param_flow_result_lane_string_owned_current(
            ctx, expression))
        return true;
    return collection_callable_type_returns_owned_string_at(
        expression, ctx, NULL, parent);
}

static bool
collection_owned_string_user_call_candidate(
    const ASTNode *call, SemanticContext *ctx)
{
    PublishedNode *value = NULL;
    if (!collection_owned_string_call_fact_current(call, ctx, &value)
        || value == NULL || ast_call_compiler_builtin(call))
        return false;
    ASTNode *callee = ast_call_callee(call);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    uint32_t producer = ast_call_semantic_callee_decl_id(call);
    ASTNode *decl = name == NULL
        ? NULL : semantic_find_callable_decl_by_name(ctx, name);
    Symbol *symbol = name == NULL ? NULL : scope_lookup(ctx->scope, name);
    return producer != 0 && decl != NULL && decl->type == AST_FUNC_DECL
        && ast_func_body(decl) != NULL
        && ast_node_stable_id(decl) == producer && symbol != NULL
        && symbol->kind == SYMBOL_FUNCTION
        && symbol->decl_syntax_id == producer && symbol->type != NULL
        && symbol->type->kind == TYPE_KIND_FUNCTION
        && type_equals(type_function_return_type(symbol->type), TYPE_STRING);
}

static bool
collection_owned_string_receipt_shape_current(
    const ASTNode *decl, SemanticContext *ctx,
    const PublishedOwnedStringResult *receipt)
{
    PublishedNode *row = publication_node(ctx->function_param_publications, decl, false);
    if (row == NULL || row->node != decl || row->id != ast_node_stable_id(decl) ||
        receipt == NULL || row->owned_string_result != receipt
        || row->owned_string_result_cycle_withdrawn ||
        receipt->body == NULL || receipt->body != ast_func_body(decl) ||
        receipt->body_id == 0 || receipt->body_id != ast_node_stable_id(receipt->body) ||
        receipt->source_count == 0 || receipt->source_count != receipt->sealed_source_count ||
        receipt->sources == NULL || receipt->syntax == NULL ||
        !semantic_param_publication_routine_inputs_current(ctx, decl, true) ||
        !function_param_flow_syntax_claims_current(ctx, receipt->syntax)) return false;
    return true;
}

bool
semantic_collection_owned_string_result_cycle_withdrawn_current(
    const ASTNode *expression, SemanticContext *ctx)
{
    const Type *result_type = NULL;
    ASTNode *decl = expression != NULL && expression->type == AST_CALL
        ? semantic_param_publication_declared_result(
            ctx, expression, &result_type) : NULL;
    PublishedNode *row = decl != NULL && ctx != NULL
        && ctx->function_param_publications != NULL
        ? publication_node(ctx->function_param_publications, decl, false)
        : NULL;
    uint32_t producer = expression != NULL && expression->type == AST_CALL
        ? ast_call_semantic_callee_decl_id(expression) : 0;
    return type_equals(result_type, TYPE_STRING)
        && collection_owned_string_user_call_candidate(expression, ctx)
        && producer != 0 && decl != NULL
        && ast_node_stable_id(decl) == producer
        && row != NULL && row->node == decl && row->id == producer
        && row->owned_string_result_examined
        && row->owned_string_result_cycle_withdrawn
        && row->owned_string_result == NULL;
}

static bool
collection_owned_string_result_closed_cycle(
    const ASTNode *decl, SemanticContext *ctx,
    const OwnedStringResultVisit *parent)
{
    for (const OwnedStringResultVisit *visit = parent;
            visit != NULL; visit = visit->parent)
        if (visit->decl == decl)
            return true;
    PublishedNode *row = decl != NULL && ctx != NULL
        && ctx->function_param_publications != NULL
        ? publication_node(ctx->function_param_publications, decl, false)
        : NULL;
    PublishedOwnedStringResult *receipt = row != NULL
        ? row->owned_string_result : NULL;
    if (row == NULL || row->node != decl
        || row->id != ast_node_stable_id(decl)
        || receipt == NULL || receipt->source_count == 0)
        return false;
    OwnedStringResultVisit visit = {decl, parent};
    for (size_t s = 0; s < receipt->source_count; s++) {
        const ASTNode *source_call = receipt->sources[s].value.node;
        const Type *result_type = NULL;
        ASTNode *source_decl = source_call != NULL
            && source_call->type == AST_CALL
            ? semantic_param_publication_declared_result(
                ctx, source_call, &result_type) : NULL;
        PublishedNode *source_row = source_decl != NULL
            ? publication_node(
                ctx->function_param_publications, source_decl, false)
            : NULL;
        if (!type_equals(result_type, TYPE_STRING)
            || source_row == NULL || source_row->node != source_decl
            || source_row->id != ast_node_stable_id(source_decl)
            || source_row->owned_string_result == NULL
            || !collection_owned_string_result_closed_cycle(
                source_decl, ctx, &visit))
            return false;
    }
    return true;
}

bool
semantic_collection_owned_string_result_candidate(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out)
{
    if (producer_syntax_id_out != NULL)
        *producer_syntax_id_out = 0;
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    if (expression == NULL || expression->type != AST_CALL || store == NULL
        || !store->pending || store->inputs_ready) {
        return false;
    }
    if (!collection_owned_string_user_call_candidate(expression, ctx)) {
        return false;
    }
    ASTNode *callee = ast_call_callee(expression);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    uint32_t producer = ast_call_semantic_callee_decl_id(expression);
    ASTNode *decl = name == NULL
        ? NULL : semantic_find_callable_decl_by_name(ctx, name);
    PublishedNode *routine = decl == NULL
        ? NULL : publication_node(store, decl, false);
    PublishedOwnedStringResult *receipt = routine == NULL
        ? NULL : routine->owned_string_result;
    /* A call can precede its callee's body in source order.  The exact call
     * and callee identities are enough to defer the consumer, but never to
     * publish ownership.  After the callee has been examined, a missing
     * receipt returns to the ordinary copy/refusal path. */
    if (routine == NULL || !routine->owned_string_result_examined) {
        if (producer_syntax_id_out != NULL)
            *producer_syntax_id_out = producer;
        return true;
    }
    if (routine->node != decl || routine->id != producer || receipt == NULL
        || (!receipt->static_return_normalization
            && !receipt->source_validation_pending)
        || receipt->source_count == 0
        || receipt->source_count != receipt->sealed_source_count
        || receipt->sources == NULL || receipt->syntax == NULL
        || receipt->body == NULL || receipt->body != ast_func_body(decl)
        || receipt->body_id == 0
        || receipt->body_id != ast_node_stable_id(receipt->body)
        || !semantic_param_publication_routine_inputs_current(ctx, decl, true)
        || !function_param_flow_syntax_claims_current(ctx, receipt->syntax)) {
        return false;
    }
    for (size_t s = 0; s < receipt->source_count; s++) {
        const PublishedCallArgument *source = &receipt->sources[s].value;
        PublishedNode *published = publication_node(store, source->node, false);
        if (source->node == NULL || source->node->type != AST_CALL
            || source->shape != AST_CALL || source->id == 0
            || source->id != ast_node_stable_id(source->node)
            || source->type != TYPE_STRING || published == NULL
            || published->node != source->node || published->id != source->id
            || !publication_call_ready(ctx, published) || !published->value_present
            || published->meta || published->value_type != TYPE_STRING
            || published->value_shape != AST_CALL)
            return false;
    }
    if (producer_syntax_id_out != NULL)
        *producer_syntax_id_out = producer;
    return true;
}

bool
semantic_collection_owned_string_result_flow_demands_prepare(
    SemanticContext *ctx)
{
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    bool debug_phase = getenv("PGY_DEBUG_SEMANTIC_PHASE_TIMING") != NULL;
    if (store == NULL || store->pending || !store->inputs_ready
        || store->finalized || store->flow_summaries_prepared) {
        if (debug_phase)
            fprintf(stderr,
                "[semantic timing] owned_result_flow_demands stop=store_state store=%d pending=%d inputs=%d finalized=%d prepared=%d error=%d\n",
                store != NULL ? 1 : 0,
                store != NULL && store->pending ? 1 : 0,
                store != NULL && store->inputs_ready ? 1 : 0,
                store != NULL && store->finalized ? 1 : 0,
                store != NULL && store->flow_summaries_prepared ? 1 : 0,
                ctx != NULL && ctx->has_error ? 1 : 0);
        return false;
    }
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedOwnedStringResult *receipt = store->nodes[i].owned_string_result;
        if (receipt == NULL || !receipt->source_validation_pending)
            continue;
        for (size_t s = 0; s < receipt->source_count; s++) {
            ASTNode *call = receipt->sources[s].value.node;
            if (!collection_owned_string_user_call_candidate(call, ctx))
                continue;
            ASTNode *callee_expr = ast_call_callee(call);
            const char *name = callee_expr != NULL
                    && callee_expr->type == AST_IDENTIFIER
                ? ast_identifier_name(callee_expr) : NULL;
            uint32_t callee_id = ast_call_semantic_callee_decl_id(call);
            ASTNode *callee = name == NULL
                ? NULL : semantic_find_callable_decl_by_name(ctx, name);
            PublishedNode *callee_row = callee == NULL
                ? NULL : publication_node(store, callee, false);
            if (callee == NULL || callee_id == 0
                || ast_node_stable_id(callee) != callee_id
                || callee_row == NULL || callee_row->node != callee
                || callee_row->id != callee_id) {
                if (debug_phase)
                    fprintf(stderr,
                        "[semantic timing] owned_result_flow_demands stop=callee_identity receipt=%zu source=%zu call=%u callee=%u error=%d\n",
                        i, s, ast_node_stable_id(call), callee_id,
                        ctx->has_error ? 1 : 0);
                return false;
            }
            /* Composition through another exact receipt is settled by the
             * result finalizer. A leaf without that receipt is owned by the
             * existing flow summary, including ordinary identity functions
             * as well as explicit allocator result lanes. Materialize that
             * exact result source now; finalization only consumes its sealed
             * current verdict. */
            if (callee_row->owned_string_result != NULL)
                continue;
            FunctionParamFlowSummaryStore *flow =
                function_param_flow_store_get(&lookup);
            if (flow == NULL || flow->failed) {
                if (debug_phase)
                    fprintf(stderr,
                        "[semantic timing] owned_result_flow_demands stop=flow_store receipt=%zu source=%zu call=%u flow=%d failed=%d error=%d\n",
                        i, s, ast_node_stable_id(call), flow != NULL ? 1 : 0,
                        flow != NULL && flow->failed ? 1 : 0,
                        ctx->has_error ? 1 : 0);
                return false;
            }
            uint32_t ignored_source = UINT32_MAX;
            (void)function_param_flow_string_result_source_prepare(
                ctx, call, &ignored_source);
            if (ctx->has_error || flow->failed) {
                if (debug_phase)
                    fprintf(stderr,
                        "[semantic timing] owned_result_flow_demands stop=source_prepare receipt=%zu source=%zu call=%u ignored=%u failed=%d error=%d\n",
                        i, s, ast_node_stable_id(call), ignored_source,
                        flow->failed ? 1 : 0, ctx->has_error ? 1 : 0);
                return false;
            }
        }
    }
    return true;
}

static bool collection_owned_string_receipt_current(const ASTNode *decl, SemanticContext *ctx,
    const OwnedStringResultVisit *parent)
{
    for (const OwnedStringResultVisit *visit = parent; visit != NULL; visit = visit->parent)
        if (visit->decl == decl) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, decl, false);
    PublishedOwnedStringResult *receipt = row == NULL ? NULL : row->owned_string_result;
    if (!collection_owned_string_receipt_shape_current(decl, ctx, receipt)
        || receipt->static_return_normalization
        || receipt->source_validation_pending
        || receipt->static_return_count != 0)
        return false;
    OwnedStringResultVisit visit = {decl, parent};
    for (size_t p = 0; p < receipt->source_count; p++) {
        PublishedOwnedStringSource *owned_source = &receipt->sources[p];
        const PublishedCallArgument *source = &owned_source->value;
        bool admitted_shape = source->node != NULL &&
            ((source->shape == AST_CALL && source->node->type == AST_CALL) ||
             (source->shape == AST_BINARY && source->node->type == AST_BINARY &&
              owned_source->inputs == NULL));
        if (!admitted_shape || source->id == 0 ||
            source->id != ast_node_stable_id(source->node) ||
            source->type != TYPE_STRING ||
            source->binding != ast_identifier_binding_syntax_id(source->node) ||
            !collection_owned_string_source_current(
                source->node, ctx, &visit, owned_source->inputs)) {
            return false;
        }
    }
    return true;
}

static bool
collection_callable_type_returns_owned_string_at(const ASTNode *call,
    SemanticContext *ctx, uint32_t *producer_id_out, const OwnedStringResultVisit *parent)
{
    if (producer_id_out != NULL) *producer_id_out = 0;
    if (ctx == NULL || !publication_generation_ready(ctx) ||
        (!ctx->function_param_publications->pending && !ctx->function_param_publications->inputs_ready)) return false;
    ASTNode *callee = call == NULL ? NULL : ast_call_callee(call);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER ? ast_identifier_name(callee) : NULL;
    uint32_t producer = call == NULL ? 0 : ast_call_semantic_callee_decl_id(call);
    ASTNode *decl = name == NULL ? NULL : semantic_find_callable_decl_by_name(ctx, name);
    Symbol *symbol = name == NULL ? NULL : scope_lookup(ctx->scope, name);
    PublishedNode *value = publication_node(ctx->function_param_publications, call, false);
    if (producer == 0 || decl == NULL || decl->type != AST_FUNC_DECL || ast_node_stable_id(decl) != producer ||
        symbol == NULL || symbol->kind != SYMBOL_FUNCTION || symbol->decl_syntax_id != producer ||
        symbol->type == NULL || symbol->type->kind != TYPE_KIND_FUNCTION ||
        !type_function_has_body_summary(symbol->type) ||
        !type_equals(type_function_return_type(symbol->type), TYPE_STRING) ||
        (type_function_body_summary(symbol->type) & BODY_SUMMARY_RETURNS_OWNED_STRING) == 0 ||
        value == NULL || value->node != call || value->id != ast_node_stable_id(call) ||
        !publication_call_ready(ctx, value) || !value->value_present || value->meta ||
        value->value_type != TYPE_STRING || !value->string_value || value->value_shape != AST_CALL) {
        return false;
    }
    for (size_t p = 0; p < value->argument_count; p++) {
        const PublishedCallArgument *arg = &value->arguments[p];
        PublishedNode *actual = publication_node(ctx->function_param_publications, arg->node, false);
        if (actual == NULL || actual->node != arg->node || actual->id != arg->id ||
            !actual->value_present || actual->meta || actual->value_type == NULL ||
            actual->value_type == TYPE_UNKNOWN || actual->value_type != arg->type ||
            actual->value_shape != arg->shape || (arg->shape == AST_IDENTIFIER && actual->binding_id != arg->binding) ||
            (arg->shape == AST_CALL && !publication_call_ready(ctx, actual))) {
            return false;
        }
    }
    if (!collection_owned_string_receipt_current(decl, ctx, parent)) {
        return false;
    }
    if (producer_id_out != NULL) *producer_id_out = producer;
    return true;
}

static void collection_owned_string_receipt_destroy(PublishedOwnedStringResult *receipt)
{
    if (receipt == NULL) return;
    for (size_t i = 0; i < receipt->source_count; i++) {
        if (receipt->sources[i].inputs == NULL) continue;
        semantic_indexed_string_result_inputs_destroy(receipt->sources[i].inputs);
        free(receipt->sources[i].inputs);
    }
    function_param_flow_claims_destroy(receipt->syntax);
    free(receipt->sources);
    free(receipt);
}

static bool
collection_owned_string_expression_ready(ASTNode *body,
                                         const ASTNode *expression,
                                         SemanticContext *ctx,
                                         const OwnedStringExpressionVisit *parent,
                                         PublishedOwnedStringResult *receipt)
{
    ASTNode *decl;
    uint32_t binding_id;

    if (body == NULL || expression == NULL)
        return false;
    for (const OwnedStringExpressionVisit *visit = parent;
            visit != NULL; visit = visit->parent)
        if (visit->expression == expression)
            return false;
    OwnedStringExpressionVisit visit = {expression, parent};
    if (expression->type == AST_IDENTIFIER) {
        binding_id = ast_identifier_binding_syntax_id(expression);
        decl = collection_direct_let_for_binding(body, binding_id);
        bool assigned = false, returned = false, declaration_seen = false;
        /* This receipt intentionally admits only a straight-line replacement
         * chain.  Every replacement must itself have an owned-result receipt,
         * so an initial literal is never mistaken for heap-owned storage.
         * Statements before the binding declaration cannot define or replace
         * that binding and are outside this value's program-point lifetime.
         * Control-dependent reaching definitions after the declaration still
         * need their own value fact and remain fail-closed here. */
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            ASTNode *stmt = ast_block_statement(body, i);
            if (stmt == NULL)
                return false;
            if (!declaration_seen) {
                if (stmt != decl)
                    continue;
                declaration_seen = true;
                continue;
            }
            if (stmt->type == AST_ASSIGNMENT) {
                ASTNode *target = ast_assignment_target(stmt);
                if (target != NULL && target->type == AST_IDENTIFIER
                    && ast_identifier_binding_syntax_id(target) == binding_id) {
                    if (returned || !collection_owned_string_expression_ready(
                            body, ast_assignment_value(stmt), ctx,
                            &visit, receipt))
                        return false;
                    assigned = true;
                }
            } else if (stmt->type == AST_RETURN) {
                ASTNode *value = ast_return_value(stmt);
                if (value == NULL || value->type != AST_IDENTIFIER
                    || ast_identifier_binding_syntax_id(value) != binding_id
                    || returned)
                    return false;
                returned = true;
            } else if (stmt->type != AST_LET_DECL
                       && stmt->type != AST_CALL
                       && stmt->type != AST_DEFER_STMT) {
                return false;
            }
        }
        if (decl == NULL || !declaration_seen || !returned)
            return false;
        if (assigned)
            return true;
        return !ast_let_is_mutable(decl)
            && collection_owned_string_expression_ready(
                body, ast_let_initializer(decl), ctx, &visit, receipt);
    }
    if (expression->type != AST_CALL && expression->type != AST_BINARY)
        return false;
    bool binary_concat = expression->type == AST_BINARY;
    bool option_payload = !binary_concat
        && semantic_indexed_string_option_unwrap_candidate(ctx, expression);
    bool source_current = binary_concat
        ? semantic_indexed_string_independent_result(expression, ctx)
        : collection_owned_string_source_current(
            expression, ctx, NULL,
            option_payload ? &(IndexedStringResultInputs){0} : NULL);
    bool source_candidate = !binary_concat && !source_current
        && !option_payload
        && collection_owned_string_user_call_candidate(expression, ctx);
    if (!source_current && !source_candidate)
        return false;
    if (source_candidate)
        receipt->source_validation_pending = true;
    for (size_t p = 0; p < receipt->source_count; p++)
        if (receipt->sources[p].value.node == expression)
            return (receipt->sources[p].inputs != NULL) == option_payload;
    if (receipt->source_count == SIZE_MAX / sizeof(*receipt->sources)) return false;
    PublishedOwnedStringSource *sources = realloc(receipt->sources,
        (receipt->source_count + 1) * sizeof(*sources));
    if (sources == NULL) return false;
    receipt->sources = sources;
    IndexedStringResultInputs *inputs = option_payload
        ? calloc(1, sizeof(*inputs)) : NULL;
    if (option_payload && inputs == NULL) return false;
    sources[receipt->source_count++] = (PublishedOwnedStringSource){
        .value = {expression, ast_node_stable_id(expression), expression->type,
            ast_identifier_binding_syntax_id(expression), TYPE_STRING, 0, 0},
        .inputs = inputs};
    return true;
}

static bool
collection_owned_string_static_returns_select(
    SemanticContext *ctx, const PublishedOwnedStringResult *receipt)
{
    size_t selected = 0;
    if (receipt == NULL || !receipt->static_return_normalization
        || receipt->static_return_count == 0 || receipt->syntax == NULL)
        return false;
    for (size_t i = 0; i < receipt->syntax->count; i++) {
        const FunctionParamFlowIndexClaim *claim = &receipt->syntax->rows[i];
        if (claim->shape != AST_RETURN || claim->node == NULL
            || claim->node->type != AST_RETURN)
            continue;
        ASTNode *value = ast_return_value(claim->node);
        if (value == NULL || value->type != AST_STRING)
            continue;
        if (!semantic_loan_copy_landing_select_string(ctx, value))
            return false;
        selected++;
    }
    return selected == receipt->static_return_count;
}

bool
semantic_collection_owned_string_results_prepare(SemanticContext *ctx)
{
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    if (store == NULL || store->pending || !store->inputs_ready
        || !store->finalized || !store->flow_summaries_prepared)
        return false;
    bool progressed;
    do {
        progressed = false;
        for (size_t i = 0; i < store->node_capacity; i++) {
            PublishedNode *row = &store->nodes[i];
            PublishedOwnedStringResult *receipt = row->owned_string_result;
            if (receipt == NULL || (!receipt->static_return_normalization
                    && !receipt->source_validation_pending))
                continue;
            if (!collection_owned_string_receipt_shape_current(
                    row->node, ctx, receipt)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                    row->node,
                    "An owned String result candidate lost its current syntax receipt");
                return false;
            }
            bool source_unproven = false;
            bool needs_option_inputs = false;
            for (size_t s = 0; s < receipt->source_count; s++)
                if (receipt->sources[s].inputs != NULL)
                    needs_option_inputs = true;
            if (needs_option_inputs) {
                FunctionParamFlowSummaryStore *flow =
                    function_param_flow_store_get(
                        &(SlotFunctionLookup){ctx, ctx->program_root});
                FunctionParamFlowProgramPointIndex *index = NULL;
                if (flow == NULL || flow->failed || flow->solving
                    || flow->validating_current
                    || !function_param_flow_program_point_index_build(
                        flow, (ASTNode *)row->node, &index) || index == NULL
                    || index->function_decl != row->node
                    || index->function_id != row->id)
                    source_unproven = true;
            }
            for (size_t s = 0;
                    !source_unproven && s < receipt->source_count; s++) {
                PublishedOwnedStringSource *source = &receipt->sources[s];
                if (source->inputs == NULL)
                    continue;
                uint32_t source_id = UINT32_MAX;
                if ((!source->inputs->prepared
                        && !semantic_indexed_string_result_source_prepare(
                            ctx, source->value.node, &source_id,
                            source->inputs))
                    || (source->inputs->prepared
                        && !semantic_indexed_string_result_source_current(
                            ctx, source->value.node, &source_id,
                            source->inputs))
                    || source_id != 0)
                    source_unproven = true;
            }
            OwnedStringResultVisit visit = {row->node, NULL};
            for (size_t s = 0;
                    !source_unproven && s < receipt->source_count; s++) {
                PublishedOwnedStringSource *source = &receipt->sources[s];
                bool source_current = collection_owned_string_source_current(
                    source->value.node, ctx, &visit, source->inputs);
                if (!source_current)
                    source_unproven = true;
            }
            if (source_unproven)
                continue;
            if (receipt->static_return_count != 0
                && !collection_owned_string_static_returns_select(
                    ctx, receipt)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                    row->node,
                    "Owned String return normalization lost an exact static branch");
                return false;
            }
            PendingRoutine *routine = publication_routine(store, row->id);
            if (routine == NULL || routine->decl != row->node
                || routine->id != row->id || routine->function_type == NULL
                || routine->return_type != TYPE_STRING
                || !type_equals(
                    type_function_return_type(routine->function_type),
                    TYPE_STRING)) {
                semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                    PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                    row->node,
                    "Owned String result normalization lost its routine identity");
                return false;
            }
            /* Static branches and not-yet-visited user callees are both
             * candidates, not ownership facts.  Only this post-flow pass can
             * retire those conditions and publish the routine summary. */
            receipt->static_return_count = 0;
            receipt->static_return_normalization = false;
            receipt->source_validation_pending = false;
            type_function_set_body_summary(routine->function_type,
                type_function_body_summary(routine->function_type)
                    | BODY_SUMMARY_RETURNS_OWNED_STRING);
            progressed = true;
        }
    } while (progressed);

    /* Preserve the one distinction a deferred caller needs before receipts
     * are retired: a closed candidate cycle has no independently proved base
     * and must remain refused, while a missing/non-owned leaf may still be
     * copied by the ordinary String-store owner. */
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedNode *row = &store->nodes[i];
        PublishedOwnedStringResult *receipt = row->owned_string_result;
        if (receipt != NULL && (receipt->static_return_normalization
                || receipt->source_validation_pending))
            row->owned_string_result_cycle_withdrawn =
                collection_owned_string_result_closed_cycle(
                    row->node, ctx, NULL);
    }
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedNode *row = &store->nodes[i];
        PublishedOwnedStringResult *receipt = row->owned_string_result;
        if (receipt != NULL && (receipt->static_return_normalization
                || receipt->source_validation_pending)) {
            collection_owned_string_receipt_destroy(receipt);
            row->owned_string_result = NULL;
        }
    }
    for (size_t i = 0; i < store->node_capacity; i++) {
        PublishedNode *row = &store->nodes[i];
        if (row->owned_string_result != NULL
            && !collection_owned_string_receipt_current(
                row->node, ctx, NULL)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                row->node,
                "An owned String result summary was withdrawn after flow preparation");
            return false;
        }
    }
    return true;
}

bool
semantic_collection_owned_string_call_result(
    const ASTNode *expression,
    SemanticContext *ctx,
    uint32_t *producer_syntax_id_out)
{
    if (producer_syntax_id_out != NULL)
        *producer_syntax_id_out = 0;
    if (expression == NULL || expression->type != AST_CALL)
        return false;
    if (semantic_indexed_string_independent_result(expression, ctx)) {
        uint32_t source = ast_node_stable_id(expression);
        if (source == 0)
            return false;
        if (producer_syntax_id_out != NULL)
            *producer_syntax_id_out = source;
        return true;
    }
    return collection_callable_type_returns_owned_string_at(
        expression, ctx, producer_syntax_id_out, NULL);
}

typedef struct {
    size_t static_count;
    bool saw_return, saw_owned, count_overflow;
} OwnedStringReturnScan;

static bool
collection_owned_string_static_return_record(OwnedStringReturnScan *scan,
                                             ASTNode *value)
{
    (void)value;
    if (scan->static_count == SIZE_MAX) {
        scan->count_overflow = true;
        return false;
    }
    scan->static_count++;
    return true;
}

static bool
collection_owned_string_returns_ready(ASTNode *node, ASTNode *function_body,
                                      SemanticContext *ctx,
                                      OwnedStringReturnScan *scan,
                                      PublishedOwnedStringResult *receipt)
{
    if (node == NULL)
        return true;
    /* The stable-identity AST is an owned tree. Use the semantic termination
     * contract for the admitted traversal instead of imposing an unrelated
     * nesting limit on this one fact producer. Expression replacement chains
     * have their own exact cycle check above. */
    if (!semantic_termination_contract_tick(ctx, node))
        return false;
    if (node->type == AST_RETURN) {
        ASTNode *value = ast_return_value(node);
        scan->saw_return = true;
        if (value != NULL && value->type == AST_STRING)
            return collection_owned_string_static_return_record(scan, value);
        if (!collection_owned_string_expression_ready(
                function_body, value, ctx, NULL, receipt))
            return false;
        scan->saw_owned = true;
        return true;
    }
    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            ASTNode *stmt = ast_block_statement(node, i);
            if (stmt == NULL || !collection_owned_string_returns_ready(
                    stmt, function_body, ctx, scan, receipt)) {
                return false;
            }
        }
        return true;
    }
    if (node->type == AST_IF_STMT) {
        return collection_owned_string_returns_ready(
                   ast_if_then_branch(node), function_body, ctx,
                   scan, receipt)
            && collection_owned_string_returns_ready(
                   ast_if_else_branch(node), function_body, ctx,
                   scan, receipt);
    }
    if (node->type == AST_WHILE_LOOP) {
        return collection_owned_string_returns_ready(
            ast_while_body(node), function_body, ctx,
            scan, receipt);
    }
    if (node->type == AST_FOR_LOOP) {
        return collection_owned_string_returns_ready(
            ast_for_body(node), function_body, ctx,
            scan, receipt);
    }
    /* Lambda bodies are not enclosing-function returns. Other control owners
     * remain unadmitted until their return facts are explicitly supported. */
    return node->type == AST_LET_DECL || node->type == AST_CALL
        || node->type == AST_ASSIGNMENT || node->type == AST_DEFER_STMT;
}

void
semantic_collection_record_owned_string_result_summary(
    ASTNode *function_decl,
    SemanticContext *ctx)
{
    OwnedStringReturnScan scan = {0};
    bool published = false;
    if (function_decl == NULL || function_decl->type != AST_FUNC_DECL || ctx == NULL ||
        ctx->function_param_publications == NULL) return;
    if (!publication_generation_ready(ctx) || !ctx->tracking_function_effects || !type_equals(ctx->current_return, TYPE_STRING) ||
        !ctx->function_param_publications->pending) return;
    PublishedNode *row = publication_node(
        ctx->function_param_publications, function_decl, true);
    if (row == NULL) {
        semantic_error(ctx, function_decl,
            "Owned String result examination row allocation failed");
        return;
    }
    collection_owned_string_receipt_destroy(row->owned_string_result);
    row->owned_string_result = NULL;
    row->owned_string_result_examined = false;
    row->owned_string_result_cycle_withdrawn = false;
    PublishedOwnedStringResult *receipt = calloc(1, sizeof(*receipt));
    if (receipt == NULL) {
        semantic_error(ctx, function_decl,
            "Owned String result receipt allocation failed");
        row->owned_string_result_examined = true;
        return;
    }
    ASTNode *body = ast_func_body(function_decl);
    bool returns_ready = body != NULL && body->type == AST_BLOCK
        && collection_owned_string_returns_ready(
            body, body, ctx, &scan, receipt);
    if (scan.count_overflow) {
        semantic_error(ctx, function_decl,
            "Owned String return normalization count overflowed");
    } else if (returns_ready && scan.saw_return && scan.saw_owned &&
        receipt->source_count != 0 && ast_node_stable_id(body) != 0 &&
        function_param_flow_syntax_claims_snapshot(ctx, &receipt->syntax, body)) {
        receipt->body = body; receipt->body_id = ast_node_stable_id(body);
        receipt->sealed_source_count = receipt->source_count;
        bool needs_source_proof = false;
        for (size_t s = 0; s < receipt->source_count; s++)
            if (receipt->sources[s].inputs != NULL)
                needs_source_proof = true;
        receipt->static_return_count = scan.static_count;
        receipt->static_return_normalization =
            scan.static_count != 0 || needs_source_proof;
        row = publication_node(ctx->function_param_publications, function_decl, true);
        if (row == NULL) {
            collection_owned_string_receipt_destroy(receipt);
            return;
        }
        row->owned_string_result = receipt;
        /* A mixed static/owned result remains only a candidate until the
         * flow owner proves every non-static source independent. The finalizer
         * then selects exact literal branches for Clone. A borrowed Option
         * payload withdraws this candidate and keeps the caller's conservative
         * copy path; it is never promoted to an owned result. */
        if (!receipt->static_return_normalization
            && !receipt->source_validation_pending)
            semantic_record_body_summary(
                ctx, BODY_SUMMARY_RETURNS_OWNED_STRING);
        published = true;
        receipt = NULL;
    }
    row = publication_node(ctx->function_param_publications, function_decl, false);
    if (row == NULL || row->node != function_decl
        || row->id != ast_node_stable_id(function_decl)) {
        semantic_error(ctx, function_decl,
            "Owned String result examination lost its routine identity");
    } else {
        row->owned_string_result_examined = true;
    }
    if (getenv("PGY_DEBUG_OWNED_STRING_RESULTS") != NULL
        && type_equals(ctx->current_return, TYPE_STRING)) {
        const char *name = ast_declaration_name(function_decl);
        fprintf(stderr,
            "[owned String result] routine=%s id=%u returns_ready=%d saw_return=%d saw_owned=%d sources=%zu static=%zu published=%d errors=%zu\n",
            name != NULL ? name : "<none>",
            ast_node_stable_id(function_decl), returns_ready ? 1 : 0,
            scan.saw_return ? 1 : 0, scan.saw_owned ? 1 : 0,
            receipt != NULL ? receipt->source_count
                : published && row != NULL && row->owned_string_result != NULL
                    ? row->owned_string_result->source_count : 0,
            scan.static_count, published ? 1 : 0,
            ctx->diagnostic_count);
    }
    collection_owned_string_receipt_destroy(receipt);
}
