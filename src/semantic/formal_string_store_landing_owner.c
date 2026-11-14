/* A formal's String stored into an `inout` output lands as an owned copy. */
#include "formal_string_store_landing_owner.h"
#include "diag_codes.h"
#include "type_checker/type_checker_internal.h"
#include "type_checker/type_checker_resolution_metadata_internal.h"
#include "type_checker/type_checker_call_constructor_internal.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "collection_owned_element_requirement_owner.h"
#include "collection_ownership_fact.h"
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_definition_recipe.h"
#include "loan_copy_landing_owner.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    OWNED_STRING_ARRAY_PUSH_CANDIDATE_RETURNED_ARRAY = 0,
    OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
} OwnedStringArrayPushCandidateKind;

struct OwnedStringArrayPushCandidate {
    OwnedStringArrayPushCandidateKind kind;
    const ASTNode *call;
    uint32_t call_id;
    const ASTNode *routine;
    uint32_t routine_id;
    const ASTNode *receiver;
    uint32_t receiver_id;
    uint32_t receiver_binding_id;
    const ASTNode *initializer;
    uint32_t initializer_id;
    bool blocked_by_uncopyable_aggregate_reuse;
};

static bool store_landing_owned_array_plan_current(
    SemanticContext *ctx, const ASTNode *call);
static bool store_landing_owned_array_candidate_recorded(
    const SemanticContext *ctx, const ASTNode *call);
static bool store_landing_record_for_routine(SemanticContext *ctx,
    const ASTNode *node, const ASTNode *routine, uint32_t formal_id);
static bool store_landing_record_for_routine_kind(SemanticContext *ctx,
    const ASTNode *node, const ASTNode *routine, uint32_t formal_id,
    PgyStringStoreLandingKind kind);
static const ASTNode *store_landing_unassigned_definition_birth(
    SemanticContext *ctx, const ASTNode *value);

/* The formal a path is rooted at, by its scope binding: the value argument
 * is not checked yet, so its identifiers carry no binding identity. */
static FuncParam *store_landing_root_formal(const ASTNode *node, SemanticContext *ctx)
{
    while (node != NULL && (node->type == AST_MEMBER_ACCESS || node->type == AST_ARRAY_ACCESS))
        node = node->type == AST_MEMBER_ACCESS ? ast_member_object(node) : ast_array_access_array(node);
    ASTNode *decl = ctx->current_function_decl;
    if (node == NULL || node->type != AST_IDENTIFIER || ast_identifier_name(node) == NULL ||
        decl == NULL || decl->type != AST_FUNC_DECL) return NULL;
    Symbol *symbol = scope_lookup(ctx->scope, ast_identifier_name(node));
    if (symbol == NULL || !symbol->is_parameter || symbol->decl_syntax_id == 0) return NULL;
    for (size_t p = 0; p < ast_func_param_count(decl); p++) {
        FuncParam *param = ast_func_param(decl, p);
        if (param != NULL && ast_func_param_stable_id(param) == symbol->decl_syntax_id) return param;
    }
    return NULL;
}

/* The declared type of a path of a formal: a field of a nominal value or an
 * Array element, each step from its declaration. NULL when any step is not
 * one of those. */
static const Type *store_landing_path_type(const ASTNode *node, SemanticContext *ctx)
{
    if (node == NULL) return NULL;
    if (node->type == AST_IDENTIFIER) {
        Symbol *symbol = ast_identifier_name(node) == NULL ? NULL : scope_lookup(ctx->scope, ast_identifier_name(node));
        return symbol == NULL ? NULL : symbol->type;
    }
    if (node->type == AST_ARRAY_ACCESS) {
        const Type *array = store_landing_path_type(ast_array_access_array(node), ctx);
        return type_is_constructed_named(array, "Array") ? type_get_constructed_arg(array, 0) : NULL;
    }
    if (node->type != AST_MEMBER_ACCESS || ast_member_name(node) == NULL) return NULL;
    const Type *object = store_landing_path_type(ast_member_object(node), ctx);
    ASTNode *decl = object == NULL || object->kind != TYPE_KIND_CLASS ? NULL
        : semantic_host_decl_for_type(ctx, object);
    if (decl == NULL || decl->type != AST_CLASS_DECL) return NULL;
    for (size_t i = 0;; i++) {
        const char *name = NULL;
        Type *field = constructor_decl_field_resolved_type_at(decl, i, &name, ctx);
        if (name == NULL) return NULL;
        if (strcmp(name, ast_member_name(node)) == 0) return field;
    }
}

/* What a landing copies, while the store call is checked: a String path of
 * a by-value or `ref` formal. */
static bool store_landing_value_landable(const ASTNode *value, SemanticContext *ctx)
{
    FuncParam *source = store_landing_root_formal(value, ctx);
    return source != NULL && (source->mode == PARAM_MODE_DEFAULT || source->mode == PARAM_MODE_REF) &&
        type_equals(store_landing_path_type(value, ctx), TYPE_STRING);
}

static uint32_t store_landing_value_formal_id(const ASTNode *value,
    SemanticContext *ctx)
{
    FuncParam *formal = store_landing_value_landable(value, ctx)
        ? store_landing_root_formal(value, ctx) : NULL;
    return formal != NULL ? ast_func_param_stable_id(formal) : 0;
}

/* A checked local path may carry a borrowed String through a projection
 * struct.  Its exact root binding and declared path type let the existing
 * owned-Array store clone that String without treating the projection as an
 * independent owner. */
static bool store_landing_value_exact_local_string_path(
    const ASTNode *value, SemanticContext *ctx)
{
    const ASTNode *root = value;
    while (root != NULL && (root->type == AST_MEMBER_ACCESS ||
        root->type == AST_ARRAY_ACCESS)) {
        root = root->type == AST_MEMBER_ACCESS
            ? ast_member_object(root) : ast_array_access_array(root);
    }
    if (root == NULL || root->type != AST_IDENTIFIER ||
        ast_identifier_name(root) == NULL ||
        !type_equals(store_landing_path_type(value, ctx), TYPE_STRING)) {
        return false;
    }
    Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(root));
    return binding != NULL && !binding->is_parameter &&
        !binding->is_host_field && binding->decl_syntax_id != 0 &&
        binding->decl_syntax_id == ast_identifier_binding_syntax_id(root);
}

bool semantic_formal_string_store_landing_current_routine_seeded(
    const SemanticContext *ctx)
{
    uint32_t routine_id = ctx != NULL && ctx->current_function_decl != NULL
        && ctx->current_function_decl->type == AST_FUNC_DECL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
    if (routine_id == 0) return false;
    for (size_t i = 0; i < ctx->string_store_landing_selected_count; i++)
        if (ctx->string_store_landing_selected_routines[i] == routine_id)
            return true;
    return false;
}

/* The element type of a store receiver that is an Array path, from its
 * declarations: a planned push of a String the value does not name as a
 * formal path is still a String push, though the value is not checked yet. */
static const Type *store_landing_receiver_element(const ASTNode *receiver, SemanticContext *ctx)
{
    const Type *array = store_landing_path_type(receiver, ctx);
    return type_is_constructed_named(array, "Array") ? type_get_constructed_arg(array, 0) : NULL;
}

static bool store_landing_planned(const ASTNode *call, const SemanticContext *ctx)
{
    for (size_t i = 0; i < ctx->string_store_landing_plan_count; i++)
        if (ctx->string_store_landing_plan[i] == ast_node_stable_id(call)) return true;
    return false;
}

static bool store_landing_planned_kind(const ASTNode *call,
    const SemanticContext *ctx, PgyStringStoreLandingKind kind)
{
    uint32_t id = call != NULL ? ast_node_stable_id(call) : 0;
    for (size_t i = 0; ctx != NULL && id != 0 &&
         i < ctx->string_store_landing_plan_count; i++) {
        if (ctx->string_store_landing_plan[i] != id)
            continue;
        /* Legacy direct phase units provide only VALUE_COPY identities.
         * Production fixed-point replay always carries the typed kind. */
        return ctx->string_store_landing_plan_kinds == NULL
            ? kind == PGY_STRING_STORE_LANDING_VALUE_COPY
            : ctx->string_store_landing_plan_kinds[i] == kind;
    }
    return false;
}

static bool store_landing_selected_kind(const SemanticContext *ctx,
    const ASTNode *call, PgyStringStoreLandingKind kind)
{
    uint32_t id = call != NULL ? ast_node_stable_id(call) : 0;
    for (size_t i = 0; ctx != NULL && id != 0
         && i < ctx->string_store_landing_selected_count; i++)
        if (ctx->string_store_landing_selected[i] == id)
            return ctx->string_store_landing_selected_nodes[i] == call
                && ctx->string_store_landing_selected_kinds[i] == kind;
    return false;
}

static PgyStringStoreLandingKind
store_landing_kind_for_call(const ASTNode *call, SemanticContext *ctx)
{
    if (call != NULL && ctx != NULL && ast_call_arg_count(call) == 3
        && collection_call_is_unshadowed_builtin(call, "ArraySet")) {
        const ASTNode *receiver = ast_call_argument(call, 0);
        const ASTNode *payload = ast_call_argument(call, 2);
        uint32_t binding = receiver != NULL
            && receiver->type == AST_IDENTIFIER
            ? semantic_param_publication_binding(ctx, receiver) : 0;
        PublishedNode *binding_row = binding != 0
            ? publication_node_by_id(
                ctx->function_param_publications, binding) : NULL;
        const ASTNode *declaration = binding_row != NULL
            ? binding_row->node : NULL;
        const ASTNode *initializer = declaration != NULL
            && declaration->type == AST_LET_DECL
            ? ast_let_initializer(declaration) : NULL;
        SemanticPublishedValue receiver_value =
            semantic_param_publication_value(ctx, receiver);
        SemanticPublishedValue payload_value =
            semantic_param_publication_value(ctx, payload);
        /* The selected Clone wraps this unchanged local's whole initializer
         * on replay.  That exact receipt makes the next receiver a uniformly
         * owned Array<String>, so the same fixed-point round can select the
         * owned replacement instead of first publishing a stale shallow
         * VALUE_COPY and refining it one analysis later. */
        if (declaration != NULL && declaration->type == AST_LET_DECL
            && !ast_let_is_alias(declaration)
            && store_landing_unassigned_definition_birth(ctx, receiver)
                == initializer
            && ast_node_stable_id(declaration) == binding
            && initializer != NULL
            && ast_let_initializer(declaration) == initializer
            && semantic_loan_copy_landing_selected(ctx, initializer)
            && receiver_value.ok && !receiver_value.meta
            && type_is_constructed_named(receiver_value.type, "Array")
            && type_equals(type_get_constructed_arg(
                receiver_value.type, 0), TYPE_STRING)
            && payload_value.ok && !payload_value.meta
            && type_equals(payload_value.type, TYPE_STRING))
            return PGY_STRING_STORE_LANDING_OWNED_SET;
    }
    if (call != NULL && ctx != NULL && ast_call_arg_count(call) == 2
        && collection_call_is_unshadowed_builtin(call, "ArrayPush")) {
        PublishedNode *row = publication_result_current(ctx, call);
        const ASTNode *routine = row != NULL ? row->caller : NULL;
        const ASTNode *receiver = ast_call_argument(call, 0);
        uint32_t receiver_id = receiver != NULL
            && receiver->type == AST_IDENTIFIER
            ? semantic_param_publication_binding(ctx, receiver) : 0;
        bool parameter_receiver = false;
        for (size_t i = 0; routine != NULL
             && routine->type == AST_FUNC_DECL
             && receiver_id != 0 && i < ast_func_param_count(routine); i++) {
            FuncParam *formal = ast_func_param(routine, i);
            const Type *formal_type = formal != NULL
                && formal->type != NULL
                ? semantic_type_resolution_lookup_resolved_type(
                    ctx, formal->type) : NULL;
            if (formal != NULL
                && ast_func_param_stable_id(formal) == receiver_id
                && (formal->mode == PARAM_MODE_DEFAULT
                    || formal->mode == PARAM_MODE_OWN
                    || formal->mode == PARAM_MODE_MUT_REF)
                && type_is_constructed_named(formal_type, "Array")
                && type_equals(type_get_constructed_arg(formal_type, 0),
                    TYPE_STRING)) {
                parameter_receiver = true;
                break;
            }
        }
        /* Only an exact empty local has the vacuous uniform-ownership
         * premise needed to combine value copying with the owned store in
         * one replay. A parameter carrier also selects the typed operation,
         * but replay still has to produce the caller-wide element-ownership
         * requirement before admission. A populated or otherwise unproved
         * local keeps the ordinary store and lands a value copy instead.
         * Later typed selectors may still refine that exact VALUE_COPY plan
         * when their receiver-lifecycle owner proves it. */
        if (parameter_receiver
            || semantic_collection_owned_string_empty_local_origin_for_routine(
                receiver, routine, ctx))
            return PGY_STRING_STORE_LANDING_OWNED_PUSH;
    }
    return PGY_STRING_STORE_LANDING_VALUE_COPY;
}

static const ASTNode *
store_landing_unassigned_definition_birth(SemanticContext *ctx,
    const ASTNode *value)
{
    if (ctx == NULL || value == NULL || value->type != AST_IDENTIFIER)
        return NULL;
    uint32_t binding = semantic_param_publication_binding(ctx, value);
    if (binding == 0 && value->type == AST_IDENTIFIER)
        binding = ast_identifier_binding_syntax_id(value);
    PublishedNode *binding_row = binding == 0 ? NULL
        : publication_node_by_id(ctx->function_param_publications, binding);
    const ASTNode *declaration = binding_row != NULL
        ? binding_row->node : NULL;
    const ASTNode *initializer = declaration != NULL
        && declaration->type == AST_LET_DECL
        ? ast_let_initializer(declaration) : NULL;
    const IndexedStringDefinitionRecipe *recipe = binding_row != NULL
        ? binding_row->indexed_string_definition : NULL;
    if (binding == 0 || declaration == NULL
        || ast_node_stable_id(declaration) != binding
        || initializer == NULL || ast_node_stable_id(initializer) == 0)
        return NULL;
    /* An immutable non-alias local cannot acquire a later definition. Its
     * declaration already owns the exact birth even before the String
     * definition recipe is sealed at the end of the routine. */
    if (!ast_let_is_alias(declaration)
        && !ast_let_is_mutable(declaration))
        return initializer;
    /* Array storage identity joins every descriptor replacement. An unchanged
     * birth can therefore be selected for copying even when `var` permits
     * element writes. This only chooses a copy site: owned-set replay still
     * requires the collection owner's current element-ownership proof. */
    PublishedNode *value_row = publication_node(
        ctx->function_param_publications, value, false);
    PublishedNode *birth_row = publication_node(
        ctx->function_param_publications, initializer, false);
    if (!ast_let_is_alias(declaration)
        && value_row != NULL && value_row->node == value
        && value_row->id == ast_node_stable_id(value)
        && value_row->value_present && !value_row->meta
        && value_row->value_shape == AST_IDENTIFIER
        && type_is_constructed_named(value_row->value_type, "Array")
        && value_row->binding_id == binding
        && birth_row != NULL && birth_row->node == initializer
        && birth_row->id == ast_node_stable_id(initializer)
        && birth_row->value_present && !birth_row->meta
        && value_row->caller != NULL
        && value_row->caller_id == ast_node_stable_id(value_row->caller)
        && value_row->caller == birth_row->caller
        && value_row->caller_id == birth_row->caller_id
        && value_row->indexed_array_source_present
        && value_row->indexed_array_source_id != 0
        && value_row->indexed_array_source_id != UINT32_MAX) {
        uint32_t birth_source = binding;
        if (initializer->type == AST_IDENTIFIER) {
            if (!birth_row->indexed_array_source_present)
                return NULL;
            birth_source = birth_row->indexed_array_source_id;
        }
        if (value_row->indexed_array_source_id == birth_source)
            return initializer;
        return NULL;
    }
    if (recipe == NULL || recipe->binding_id != binding
        || recipe->declaration != declaration
        || recipe->declaration_id != binding
        || recipe->birth.value != initializer
        || recipe->birth.value_id != ast_node_stable_id(initializer)
        || recipe->assignment_count != 0
        || recipe->sealed_assignments != 0)
        return NULL;
    return initializer;
}

static bool
store_landing_definition_birth_independent(SemanticContext *ctx,
    const ASTNode *value)
{
    const ASTNode *initializer =
        store_landing_unassigned_definition_birth(ctx, value);
    const ASTNode *callee = initializer != NULL
        ? ast_call_callee(initializer) : NULL;
    bool landed_clone = initializer != NULL
        && initializer->type == AST_CALL
        && ast_call_compiler_builtin(initializer)
        && ast_call_arg_count(initializer) == 1
        && callee != NULL && callee->type == AST_IDENTIFIER
        && ast_identifier_name(callee) != NULL
        && strcmp(ast_identifier_name(callee), "Clone") == 0;
    return landed_clone
        || (initializer != NULL
            && semantic_loan_copy_landing_satisfied(ctx, initializer));
}

bool semantic_formal_string_store_landing_owned_push(ASTNode *call, SemanticContext *ctx)
{
    return call != NULL && ctx != NULL && ast_call_arg_count(call) == 2 &&
        store_landing_planned_kind(call, ctx,
            PGY_STRING_STORE_LANDING_OWNED_PUSH) &&
        (store_landing_value_landable(ast_call_argument(call, 1), ctx) ||
         type_equals(store_landing_receiver_element(ast_call_argument(call, 0), ctx), TYPE_STRING));
}

bool semantic_formal_string_store_landing_selected_owned_push(
    const SemanticContext *ctx, const ASTNode *call)
{
    return store_landing_selected_kind(ctx, call,
        PGY_STRING_STORE_LANDING_OWNED_PUSH);
}

bool semantic_formal_string_store_landing_value_copy(const ASTNode *call,
    SemanticContext *ctx)
{
    return call != NULL && ctx != NULL && ast_call_arg_count(call) == 2
        && (store_landing_planned_kind(call, ctx,
                PGY_STRING_STORE_LANDING_VALUE_COPY)
            || store_landing_planned_kind(call, ctx,
                PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY));
}

bool semantic_formal_string_store_landing_owned_array_plan_current(
    SemanticContext *ctx, const ASTNode *call)
{
    return store_landing_owned_array_plan_current(ctx, call);
}

bool semantic_formal_string_store_landing_owned_set(ASTNode *call,
    SemanticContext *ctx)
{
    if (call == NULL || ctx == NULL || ast_call_arg_count(call) != 3
        || !store_landing_planned_kind(call, ctx,
            PGY_STRING_STORE_LANDING_OWNED_SET)
        || !type_equals(store_landing_receiver_element(
            ast_call_argument(call, 0), ctx), TYPE_STRING))
        return false;
    ASTNode *receiver = ast_call_argument(call, 0);
    if (semantic_collection_owned_string_set_required(receiver, ctx))
        return true;
    /* The owned-set plan may have been issued while a provisional Clone made
     * a local field projection independent. If the outer fixed point later
     * retires that Clone, reselect only the exact definition birth on which
     * this store depends. Keeping every earlier carrier copy would preserve
     * safety but deep-copy long functional-update chains. */
    const ASTNode *birth = store_landing_unassigned_definition_birth(
        ctx, receiver);
    const Type *receiver_type = store_landing_path_type(receiver, ctx);
    if (birth != NULL && receiver_type != NULL)
        (void)semantic_loan_copy_landing_select_typed_carrier(
            ctx, birth, receiver_type);
    return false;
}

static bool
store_landing_inout_receiver_formal(const ASTNode *receiver,
    ASTNode *routine, size_t *formal_index_out)
{
    uint32_t binding_id = receiver != NULL
        && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    if (routine == NULL || routine->type != AST_FUNC_DECL
        || binding_id == 0 || formal_index_out == NULL)
        return false;
    for (size_t i = 0; i < ast_func_param_count(routine); i++) {
        FuncParam *formal = ast_func_param(routine, i);
        if (formal != NULL && formal->mode == PARAM_MODE_MUT_REF
            && ast_func_param_stable_id(formal) == binding_id) {
            *formal_index_out = i;
            return true;
        }
    }
    return false;
}

bool
semantic_formal_string_store_landing_inout_set_receiver_current(
    const ASTNode *call, SemanticContext *ctx)
{
    const ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 3
        ? ast_call_argument(call, 0) : NULL;
    size_t formal_index = 0;
    return ctx != NULL
        && store_landing_inout_receiver_formal(receiver,
            ctx->current_function_decl, &formal_index);
}

bool
semantic_formal_string_store_landing_inout_owned_set_for_formal(
    ASTNode *call, SemanticContext *ctx, ASTNode *routine,
    size_t formal_index)
{
    ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 3
        ? ast_call_argument(call, 0) : NULL;
    bool source_set = collection_call_is_unshadowed_builtin(call, "ArraySet");
    bool lowered_set = call != NULL && ast_call_compiler_builtin(call)
        && collection_call_is_unshadowed_builtin(call, "ArraySetOwnedString");
    FuncParam *formal = routine != NULL && routine->type == AST_FUNC_DECL
        && formal_index < ast_func_param_count(routine)
        ? ast_func_param(routine, formal_index) : NULL;
    uint32_t receiver_id = receiver != NULL
        && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    uint32_t formal_id = formal != NULL
        ? ast_func_param_stable_id(formal) : 0;
    uint32_t effect_kind = 0, effect_receiver = 0, effect_source = 0;
    bool planned = call != NULL && ctx != NULL
        && store_landing_planned_kind(call, ctx,
            PGY_STRING_STORE_LANDING_OWNED_SET);
    SemanticPublishedValue published_receiver =
        semantic_param_publication_value(ctx, receiver);
    const Type *formal_type = formal != NULL && formal->type != NULL
        ? semantic_type_resolution_lookup_resolved_type(ctx, formal->type)
        : NULL;
    bool formal_string_array = type_is_constructed_named(formal_type,
            "Array")
        && type_equals(type_get_constructed_arg(formal_type, 0),
            TYPE_STRING);
    bool element_string = (published_receiver.ok && !published_receiver.meta
            && type_is_constructed_named(published_receiver.type, "Array")
            && type_equals(type_get_constructed_arg(
                published_receiver.type, 0), TYPE_STRING))
        || (planned && formal_string_array);
    bool effect_ok = call != NULL && ctx != NULL
        && ast_call_semantic_collection_effect(call, &effect_kind,
            &effect_receiver, &effect_source);
    if (call == NULL || ctx == NULL || !planned
        || (!source_set && !lowered_set)
        || !element_string
        || formal == NULL || formal->mode != PARAM_MODE_MUT_REF
        || formal_id == 0 || receiver_id != formal_id)
        return false;
    /* On reload the typed landing plan is available before this call's
     * collection effect is republished. The previous sealed generation
     * issued OWNED_SET only from the exact shallow effect below; replay may
     * therefore validate the source identity first and publish the owned
     * effect later in the same type-check. Every non-replay visit still
     * requires the current effect. */
    if (effect_ok) {
        if (effect_receiver != formal_id || effect_source != 0
            || (source_set
                && effect_kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION)
            || (lowered_set
                && effect_kind != PGY_COLLECTION_EFFECT_OWNED_STRING_SET))
            return false;
    } else if (!source_set || !planned) {
        return false;
    }
    return semantic_collection_owned_element_requirement_record_parameter_mutation(
        call, receiver, routine, formal_index, ctx);
}

bool
semantic_formal_string_store_landing_inout_owned_set_current(
    ASTNode *call, SemanticContext *ctx)
{
    ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 3
        ? ast_call_argument(call, 0) : NULL;
    ASTNode *routine = ctx != NULL ? ctx->current_function_decl : NULL;
    size_t formal_index = 0;
    return store_landing_inout_receiver_formal(
            receiver, routine, &formal_index)
        && semantic_formal_string_store_landing_inout_owned_set_for_formal(
            call, ctx, routine, formal_index);
}

bool
semantic_formal_string_store_landing_select_inout_owned_set(
    SemanticContext *ctx, const ASTNode *call, ASTNode *routine,
    size_t formal_index)
{
    const ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 3
        ? ast_call_argument(call, 0) : NULL;
    const ASTNode *value = call != NULL && ast_call_arg_count(call) == 3
        ? ast_call_argument(call, 2) : NULL;
    FuncParam *formal = routine != NULL && routine->type == AST_FUNC_DECL
        && formal_index < ast_func_param_count(routine)
        ? ast_func_param(routine, formal_index) : NULL;
    SemanticPublishedValue receiver_value =
        semantic_param_publication_value(ctx, receiver);
    SemanticPublishedValue stored_value =
        semantic_param_publication_value(ctx, value);
    uint32_t effect_kind = 0, effect_receiver = 0, effect_source = 0;
    size_t stored_ordinal = SIZE_MAX;
    uint32_t formal_id = formal != NULL
        ? ast_func_param_stable_id(formal) : 0;

    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || ast_node_stable_id(call) == 0 || receiver == NULL
        || formal == NULL || formal->mode != PARAM_MODE_MUT_REF
        || formal_id == 0 || receiver->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(receiver) != formal_id
        || !collection_call_is_unshadowed_builtin(call, "ArraySet")
        || !semantic_param_publication_checked_array_store(
            ctx, call, &stored_ordinal) || stored_ordinal != 2
        || !receiver_value.ok || receiver_value.meta
        || !type_is_constructed_named(receiver_value.type, "Array")
        || !type_equals(type_get_constructed_arg(
            receiver_value.type, 0), TYPE_STRING)
        || !stored_value.ok || stored_value.meta
        || !type_equals(stored_value.type, TYPE_STRING)
        || !ast_call_semantic_collection_effect(call, &effect_kind,
            &effect_receiver, &effect_source)
        || effect_kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
        || effect_receiver != formal_id || effect_source != 0)
        return false;
    if (!semantic_collection_owned_element_requirement_record_parameter_mutation(
            (ASTNode *)call, (ASTNode *)receiver, routine,
            formal_index, ctx))
        return false;
    /* This is an exact receiver-lifecycle rewrite, not a value-formal copy
     * batch.  Keep routine/formal expansion out of the generic String-store
     * batch and publish only this call identity for replay. */
    return store_landing_record_for_routine_kind(ctx, call, NULL, 0,
        PGY_STRING_STORE_LANDING_OWNED_SET);
}

bool semantic_formal_string_store_landing(ASTNode *call, size_t receiver_ordinal,
    size_t value_ordinal, SemanticContext *ctx)
{
    size_t count = 0;
    ASTNode **args = call == NULL || ctx == NULL ? NULL : ast_call_arguments(call, &count);
    if (args == NULL || receiver_ordinal >= count || value_ordinal >= count) return true;
    /* Owned operations are receiver-lifecycle plans. If their typed replay
     * failed, never reinterpret the same identity as a value-only Clone. */
    if (store_landing_planned_kind(call, ctx,
            PGY_STRING_STORE_LANDING_OWNED_PUSH)
        || store_landing_planned_kind(call, ctx,
            PGY_STRING_STORE_LANDING_OWNED_SET))
        return false;
    ASTNode *value = args[value_ordinal];
    FuncParam *receiver = store_landing_root_formal(args[receiver_ordinal], ctx);
    /* Into an `inout` output a formal's String always leaves, and so does a
     * literal stored as an Array<String> element: the caller's Array would
     * hold static text it cannot own. Into anything else only an earlier
     * analysis's flow facts can say a value leaves, and they plan only
     * values Clone copies whole. */
    bool output = receiver != NULL && receiver->mode == PARAM_MODE_MUT_REF;
    bool literal = output && value != NULL && value->type == AST_STRING &&
        type_equals(store_landing_receiver_element(args[receiver_ordinal], ctx), TYPE_STRING);
    bool planned = store_landing_planned(call, ctx);
    if (!planned && !literal &&
        !(output && store_landing_value_landable(value, ctx)))
        return true;
    /* A named value stored into an `inout` Array<String> can be used again
     * elsewhere in the routine.  Do not choose its copy site while this call
     * is first being checked: the sealed whole-routine analysis owns whether
     * one producer copy can satisfy every use or this exact store must become
     * an owned push.  A selected VALUE_COPY plan re-enters below and literals
     * remain an exact local decision. */
    if (!planned && output && !literal
        && value != NULL && value->type == AST_IDENTIFIER
        && args[receiver_ordinal]->type == AST_IDENTIFIER)
        return true;

    /* A recheck consumes the already-built copy. This only recognizes our
     * rewrite; the ordinary Clone type/ownership checks still admit it. A
     * source call merely named Clone does not carry this marker. */
    ASTNode *existing_callee = ast_call_callee(value);
    if (ast_call_compiler_builtin(value) && ast_call_arg_count(value) == 1 &&
        existing_callee != NULL && existing_callee->type == AST_IDENTIFIER &&
        ast_identifier_name(existing_callee) != NULL &&
        strcmp(ast_identifier_name(existing_callee), "Clone") == 0)
        return true;

    uint32_t next_id = ctx->synthetic_stable_id_next != 0
        ? ctx->synthetic_stable_id_next : ast_stable_id_ceiling(ctx->program_root);
    if (next_id == 0 || next_id > UINT32_MAX - 2) return false;
    ASTNode *callee = ast_create_identifier("Clone");
    ASTNode *clone = callee == NULL ? NULL : ast_create_call(callee);
    if (clone == NULL) {
        ast_destroy(callee);
        return false;
    }
    /* Reserve everything while value still belongs solely to the original
     * call. Failure must not adopt or destroy that value, or consume IDs. */
    ast_add_argument(clone, NULL);
    clone->origin_path = value->origin_path != NULL
        ? pergyra_strdup(value->origin_path) : NULL;
    if (ast_identifier_name(callee) == NULL || ast_call_arg_count(clone) != 1 ||
        !ast_call_set_compiler_builtin(clone) ||
        (value->origin_path != NULL && clone->origin_path == NULL)) {
        ast_destroy(clone);
        return false;
    }
    callee->line = clone->line = value->line;
    callee->column = clone->column = value->column;
    callee->stable_id = next_id++;
    clone->stable_id = next_id++;
    clone->data.call.arguments[0] = value;
    args[value_ordinal] = clone;
    ctx->synthetic_stable_id_next = next_id;
    return true;
}

static bool store_landing_record_for_routine_kind(SemanticContext *ctx,
    const ASTNode *node, const ASTNode *routine, uint32_t formal_id,
    PgyStringStoreLandingKind kind)
{
    uint32_t id = ast_node_stable_id(node);
    if (ctx == NULL || node == NULL || id == 0) return false;
    for (size_t i = 0; i < ctx->string_store_landing_selected_count; i++)
        if (ctx->string_store_landing_selected[i] == id) {
            if (ctx->string_store_landing_selected_nodes[i] != node)
                return false;
            PgyStringStoreLandingKind selected_kind =
                ctx->string_store_landing_selected_kinds[i];
            bool refine_value_copy =
                selected_kind == PGY_STRING_STORE_LANDING_VALUE_COPY
                && (kind == PGY_STRING_STORE_LANDING_OWNED_PUSH
                    || kind == PGY_STRING_STORE_LANDING_OWNED_SET
                    || kind == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY);
            bool prefer_formal_owned_push =
                (selected_kind == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY
                    && kind == PGY_STRING_STORE_LANDING_OWNED_PUSH)
                || (selected_kind == PGY_STRING_STORE_LANDING_OWNED_PUSH
                    && kind == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY);
            if (refine_value_copy
                || (selected_kind == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY
                    && kind == PGY_STRING_STORE_LANDING_OWNED_PUSH))
                ctx->string_store_landing_selected_kinds[i] = kind;
            else if (selected_kind != kind && !prefer_formal_owned_push)
                return false;
            uint32_t routine_id = routine != NULL
                && routine->type == AST_FUNC_DECL
                ? ast_node_stable_id(routine) : 0;
            if (ctx->string_store_landing_selected_routines[i] == 0)
                ctx->string_store_landing_selected_routines[i] = routine_id;
            if (ctx->string_store_landing_selected_formals[i] == 0)
                ctx->string_store_landing_selected_formals[i] = formal_id;
            return true;
        }
    for (size_t i = 0; i < ctx->string_store_landing_plan_count; i++)
        if (ctx->string_store_landing_plan[i] == id) {
            PgyStringStoreLandingKind planned_kind =
                ctx->string_store_landing_plan_kinds == NULL
                ? PGY_STRING_STORE_LANDING_VALUE_COPY
                : ctx->string_store_landing_plan_kinds[i];
            if (planned_kind == kind)
                return true;
            /* The receiver lifecycle can become known one analysis after the
             * borrowed value selected a copy. Both owned store operations
             * include that copy and therefore soundly refine VALUE_COPY for
             * this exact call identity. */
            if (planned_kind == PGY_STRING_STORE_LANDING_VALUE_COPY &&
                (kind == PGY_STRING_STORE_LANDING_OWNED_PUSH ||
                 kind == PGY_STRING_STORE_LANDING_OWNED_SET ||
                 kind == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY))
                break;
            /* A sealed aggregate-member fallback may later discover the
             * existing formal owned-push proof.  The formal proof is the
             * stronger authority.  Conversely, a later fallback observation
             * cannot demote an already planned formal owned push. */
            if (planned_kind == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY
                && kind == PGY_STRING_STORE_LANDING_OWNED_PUSH)
                break;
            if (planned_kind == PGY_STRING_STORE_LANDING_OWNED_PUSH
                && kind == PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY)
                return true;
            return false;
        }
    if (ctx->string_store_landing_selected_count == ctx->string_store_landing_selected_capacity) {
        size_t capacity = ctx->string_store_landing_selected_capacity == 0 ? 16
            : ctx->string_store_landing_selected_capacity * 2;
        if (capacity > SIZE_MAX / sizeof(uint32_t)) return false;
        uint32_t *ids = malloc(capacity * sizeof(*ids));
        PgyStringStoreLandingKind *kinds =
            malloc(capacity * sizeof(*kinds));
        const ASTNode **nodes = malloc(capacity * sizeof(*nodes));
        uint32_t *routines = malloc(capacity * sizeof(*routines));
        uint32_t *formals = malloc(capacity * sizeof(*formals));
        if (ids == NULL || kinds == NULL || nodes == NULL ||
            routines == NULL || formals == NULL) {
            free(ids);
            free(kinds);
            free(nodes);
            free(routines);
            free(formals);
            return false;
        }
        if (ctx->string_store_landing_selected_count != 0) {
            memcpy(ids, ctx->string_store_landing_selected,
                ctx->string_store_landing_selected_count * sizeof(*ids));
            memcpy(kinds, ctx->string_store_landing_selected_kinds,
                ctx->string_store_landing_selected_count * sizeof(*kinds));
            memcpy(nodes, ctx->string_store_landing_selected_nodes,
                ctx->string_store_landing_selected_count * sizeof(*nodes));
            memcpy(routines, ctx->string_store_landing_selected_routines,
                ctx->string_store_landing_selected_count * sizeof(*routines));
            memcpy(formals, ctx->string_store_landing_selected_formals,
                ctx->string_store_landing_selected_count * sizeof(*formals));
        }
        free(ctx->string_store_landing_selected);
        free(ctx->string_store_landing_selected_kinds);
        free(ctx->string_store_landing_selected_nodes);
        free(ctx->string_store_landing_selected_routines);
        free(ctx->string_store_landing_selected_formals);
        ctx->string_store_landing_selected = ids;
        ctx->string_store_landing_selected_kinds = kinds;
        ctx->string_store_landing_selected_nodes = nodes;
        ctx->string_store_landing_selected_routines = routines;
        ctx->string_store_landing_selected_formals = formals;
        ctx->string_store_landing_selected_capacity = capacity;
    }
    size_t at = ctx->string_store_landing_selected_count++;
    ctx->string_store_landing_selected[at] = id;
    ctx->string_store_landing_selected_kinds[at] = kind;
    ctx->string_store_landing_selected_nodes[at] = node;
    ctx->string_store_landing_selected_routines[at] = routine != NULL
        && routine->type == AST_FUNC_DECL ? ast_node_stable_id(routine) : 0;
    ctx->string_store_landing_selected_formals[at] = formal_id;
    if (getenv("PGY_DEBUG_LANDING_STORES") != NULL ||
        getenv("PGY_DEBUG_PIPELINE_STAGE") != NULL) {
        const char *routine_name = routine != NULL
            ? ast_declaration_name(routine) : NULL;
        fprintf(stderr,
            "[semantic store landing select] node=%u kind=%u path=%s line=%d col=%d routine=%s routine_id=%u formal_id=%u\n",
            id, (unsigned)kind,
            node->origin_path != NULL ? node->origin_path : "<none>",
            node->line, node->column,
            routine_name != NULL ? routine_name : "<none>",
            ctx->string_store_landing_selected_routines[at], formal_id);
    }
    return true;
}

static bool store_landing_record_for_routine(SemanticContext *ctx,
    const ASTNode *node, const ASTNode *routine, uint32_t formal_id)
{
    return store_landing_record_for_routine_kind(ctx, node, routine,
        formal_id, store_landing_kind_for_call(node, ctx));
}

static bool store_landing_candidate_append(SemanticContext *ctx,
    OwnedStringArrayPushCandidateKind kind, const ASTNode *call,
    const ASTNode *routine, const ASTNode *receiver,
    uint32_t receiver_binding_id, const ASTNode *initializer)
{
    uint32_t call_id = call != NULL ? ast_node_stable_id(call) : 0;
    uint32_t routine_id = routine != NULL
        ? ast_node_stable_id(routine) : 0;
    uint32_t receiver_id = receiver != NULL
        ? ast_node_stable_id(receiver) : 0;
    if (ctx == NULL || call == NULL || call_id == 0
        || routine == NULL || routine->type != AST_FUNC_DECL
        || routine_id == 0 || receiver == NULL || receiver_id == 0)
        return false;
    for (size_t i = 0;
         i < ctx->owned_string_array_push_candidate_count; i++) {
        const OwnedStringArrayPushCandidate *candidate =
            &ctx->owned_string_array_push_candidates[i];
        if (candidate->call_id != call_id) continue;
        return candidate->kind == kind && candidate->call == call
            && candidate->routine == routine
            && candidate->routine_id == routine_id
            && candidate->receiver == receiver
            && candidate->receiver_id == receiver_id;
    }
    if (ctx->owned_string_array_push_candidate_count
        == ctx->owned_string_array_push_candidate_capacity) {
        size_t capacity =
            ctx->owned_string_array_push_candidate_capacity == 0 ? 8
            : ctx->owned_string_array_push_candidate_capacity * 2;
        if (capacity < ctx->owned_string_array_push_candidate_count
            || capacity > SIZE_MAX / sizeof(OwnedStringArrayPushCandidate))
            return false;
        OwnedStringArrayPushCandidate *grown = realloc(
            ctx->owned_string_array_push_candidates,
            capacity * sizeof(*grown));
        if (grown == NULL) return false;
        ctx->owned_string_array_push_candidates = grown;
        ctx->owned_string_array_push_candidate_capacity = capacity;
    }
    ctx->owned_string_array_push_candidates[
        ctx->owned_string_array_push_candidate_count++] =
        (OwnedStringArrayPushCandidate){
            .kind = kind,
            .call = call,
            .call_id = call_id,
            .routine = routine,
            .routine_id = routine_id,
            .receiver = receiver,
            .receiver_id = receiver_id,
            .receiver_binding_id = receiver_binding_id,
            .initializer = initializer,
            .initializer_id = initializer != NULL
                ? ast_node_stable_id(initializer) : 0,
        };
    return true;
}

bool semantic_formal_string_store_landing_record_owned_array_candidate(
    SemanticContext *ctx, const ASTNode *call)
{
    if (ctx == NULL || !semantic_param_publication_pending(ctx)
        || call == NULL || call->type != AST_CALL
        || ast_call_arg_count(call) != 2
        || !collection_call_is_unshadowed_builtin(call, "ArrayPush")
        || ast_call_semantic_callee_decl_id(call) != 0
        || ast_call_semantic_callee_value_binding_id(call) != 0)
        return false;
    const ASTNode *receiver = ast_call_argument(call, 0);
    ASTNode *source = ast_call_argument(call, 1);
    ASTNode *caller = ctx->current_function_decl;
    uint32_t caller_id = caller != NULL && caller->type == AST_FUNC_DECL
        ? ast_node_stable_id(caller) : 0;
    uint32_t binding_id = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    uint32_t source_id = UINT32_MAX;
    const PgyCollectionOwnershipFact *fact =
        semantic_collection_ownership_fact_find(ctx, caller_id, binding_id);
    PublishedNode *binding_row = publication_node_by_id(
        ctx->function_param_publications, binding_id);
    const ASTNode *binding_decl = binding_row == NULL ? NULL : binding_row->node;
    const ASTNode *initializer = binding_decl != NULL
        && binding_decl->type == AST_LET_DECL
        ? ast_let_initializer(binding_decl) : NULL;
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(receiver)) : NULL;
    if (caller_id == 0 || binding_id == 0 || source == NULL
        || source->type != AST_CALL
        || ast_call_semantic_callee_decl_id(source) == 0
        || !semantic_indexed_string_borrow_source_current(
            source, ctx, &source_id) || source_id != 0
        || fact == NULL
        || fact->function_syntax_id != caller_id
        || fact->binding_syntax_id != binding_id
        || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
        || fact->origin != PGY_COLLECTION_ORIGIN_UNKNOWN
        || fact->element_ownership != PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
        || initializer == NULL
        || (initializer->type != AST_CALL
            && initializer->type != AST_ARRAY_LITERAL)
        || (initializer->type != AST_ARRAY_LITERAL
            && (fact->return_elements_source == NULL
                || fact->return_elements_source != initializer
                || fact->return_elements_source_id == 0
                || fact->return_elements_source_id
                    != ast_node_stable_id(initializer)))
        || (initializer->type == AST_ARRAY_LITERAL
            && (binding == NULL || binding->decl_syntax_id != binding_id
                || binding->is_parameter || binding->is_host_field
                || binding->is_consumed
                || !binding->has_exclusive_array_storage
                || binding->has_escaped_array_storage
                || binding->has_pending_inout_preservation
                || binding->has_pending_array_result))
        || ast_let_is_alias(binding_decl))
        return false;
    uint32_t call_id = ast_node_stable_id(call);
    if (call_id == 0) return false;
    bool follows_same_returned_array = false;
    for (size_t i = 0;
         i < ctx->owned_string_array_push_candidate_count; i++) {
        const OwnedStringArrayPushCandidate *candidate =
            &ctx->owned_string_array_push_candidates[i];
        if (candidate->kind !=
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_RETURNED_ARRAY)
            continue;
        if (candidate->call_id == call_id)
            return candidate->call == call;
        if (candidate->routine_id == caller_id
            && candidate->receiver_binding_id == binding_id
            && candidate->initializer == initializer
            && candidate->initializer_id == ast_node_stable_id(initializer))
            follows_same_returned_array = true;
    }
    /* The first plain push consumes the provisional all-elements-owned bit.
     * Later independent pushes into the same returned Array are still exact
     * candidates: each is validated after sealing and each becomes its own
     * owned store. A local Array literal is also recorded when its preliminary
     * bit is false so the sealed initializer proof can issue the fail-closed
     * diagnostic; recording a candidate grants no ownership permission. This
     * does not restore whole-array ownership after a shallow store. */
    if (!fact->return_elements_owned && !follows_same_returned_array
        && initializer->type != AST_ARRAY_LITERAL)
        return false;
    return store_landing_candidate_append(ctx,
        OWNED_STRING_ARRAY_PUSH_CANDIDATE_RETURNED_ARRAY, call, caller,
        receiver, binding_id, initializer);
}

bool semantic_formal_string_store_landing_record_member_value_copy_candidate(
    SemanticContext *ctx, const ASTNode *call)
{
    const ASTNode *routine = ctx != NULL ? ctx->current_function_decl : NULL;
    ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 0) : NULL;
    ASTNode *value = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 1) : NULL;
    uint32_t binding_id = receiver != NULL
        && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    bool replay = semantic_formal_string_store_landing_member_value_copy_selected(
        call, ctx);
    if (ctx == NULL || !semantic_param_publication_pending(ctx)
        || call == NULL || call->type != AST_CALL
        || ast_call_arg_count(call) != 2
        || !collection_call_is_unshadowed_builtin(call, "ArrayPush")
        || ast_call_semantic_callee_decl_id(call) != 0
        || ast_call_semantic_callee_value_binding_id(call) != 0
        || routine == NULL || routine->type != AST_FUNC_DECL
        || receiver == NULL || receiver->type != AST_IDENTIFIER
        || value == NULL
        || (!replay
            && !semantic_collection_owned_element_member_push_provisional(
                receiver, ctx))
        || (!replay
            && semantic_formal_string_store_landing_value_copy(call, ctx)))
        return false;
    return store_landing_candidate_append(ctx,
        OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY, call, routine,
        receiver, binding_id, NULL);
}

bool semantic_formal_string_store_landing_member_value_copy_candidate_recorded(
    const SemanticContext *ctx, const ASTNode *call)
{
    uint32_t call_id = call != NULL ? ast_node_stable_id(call) : 0;
    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || call_id == 0)
        return false;
    for (size_t i = 0;
         i < ctx->owned_string_array_push_candidate_count; i++) {
        const OwnedStringArrayPushCandidate *candidate =
            &ctx->owned_string_array_push_candidates[i];
        if (candidate->kind !=
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            || candidate->call_id != call_id)
            continue;
        return candidate->call == call;
    }
    return false;
}

void semantic_formal_string_store_landing_block_member_value_copy_candidates(
    SemanticContext *ctx, const ASTNode *routine)
{
    uint32_t routine_id = routine != NULL
        ? ast_node_stable_id(routine) : 0;
    if (ctx == NULL || routine == NULL || routine->type != AST_FUNC_DECL
        || routine_id == 0)
        return;
    for (size_t i = 0;
         i < ctx->owned_string_array_push_candidate_count; i++) {
        OwnedStringArrayPushCandidate *candidate =
            &ctx->owned_string_array_push_candidates[i];
        if (candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            && candidate->routine_id == routine_id)
            candidate->blocked_by_uncopyable_aggregate_reuse = true;
    }
}

bool semantic_formal_string_store_landing_member_value_copy_selected(
    const ASTNode *call, SemanticContext *ctx)
{
    return call != NULL && ctx != NULL && call->type == AST_CALL
        && ast_call_arg_count(call) == 2
        && store_landing_planned_kind(call, ctx,
            PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY);
}

static bool store_landing_owned_array_initializer_current(
    SemanticContext *ctx, const ASTNode *initializer)
{
    if (initializer == NULL) return false;
    if (initializer->type == AST_CALL)
        return semantic_array_storage_owned_string_result_current(ctx, initializer);
    if (initializer->type != AST_ARRAY_LITERAL) return false;
    const ASTNode *dependency = NULL;
    return semantic_array_storage_return_elements_owned(
            (ASTNode *)initializer, ctx, &dependency)
        && dependency == NULL;
}

static bool store_landing_owned_array_literal_normalization_select(
    SemanticContext *ctx, const ASTNode *initializer)
{
    if (ctx == NULL || initializer == NULL
        || initializer->type != AST_ARRAY_LITERAL
        || ast_array_literal_count(initializer) == 0)
        return false;

    size_t static_count = 0;
    for (size_t i = 0; i < ast_array_literal_count(initializer); i++) {
        ASTNode *element = ast_array_literal_element(initializer, i);
        SemanticPublishedValue value = semantic_param_publication_value(
            ctx, element);
        uint32_t producer = 0;
        if (element == NULL || !value.ok || value.meta
            || !type_equals(value.type, TYPE_STRING))
            return false;
        if (element->type == AST_CALL
            && semantic_collection_owned_string_call_result(
                element, ctx, &producer)
            && producer != 0)
            continue;
        /* A source literal has permanent bytes but no cleanup authority.
         * Normalize only that exact, independently copyable node. A named or
         * otherwise borrowed element remains a fail-closed mixed literal. */
        if (element->type != AST_STRING)
            return false;
        static_count++;
    }
    if (static_count == 0)
        return false;
    for (size_t i = 0; i < ast_array_literal_count(initializer); i++) {
        ASTNode *element = ast_array_literal_element(initializer, i);
        if (element->type == AST_STRING
            && !semantic_loan_copy_landing_select_string(ctx, element))
            return false;
    }
    return true;
}

/* A plan id is issued only after the prior sealed analysis observed this
 * exact local literal while its storage was still exclusive at the push.
 * Later code may legitimately move that Array and set the Symbol's aggregate
 * escape bit. Recheck the stable identities and current all-elements-owned
 * premise instead of applying that later state retroactively. */
static bool store_landing_owned_array_plan_current(
    SemanticContext *ctx, const ASTNode *call)
{
    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || !store_landing_planned(call, ctx)
        || ast_call_arg_count(call) != 2)
        return false;
    const ASTNode *receiver = ast_call_argument(call, 0);
    const ASTNode *source = ast_call_argument(call, 1);
    uint32_t binding_id = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(receiver)) : NULL;
    uint32_t routine_id = ctx->current_function_decl != NULL
        ? ast_node_stable_id(ctx->current_function_decl) : 0;
    const PgyCollectionOwnershipFact *fact =
        semantic_collection_ownership_fact_find(ctx, routine_id, binding_id);
    PublishedNode *binding_row = publication_node_by_id(
        ctx->function_param_publications, binding_id);
    const ASTNode *binding_decl = binding_row == NULL ? NULL : binding_row->node;
    const ASTNode *initializer = binding_decl != NULL
        && binding_decl->type == AST_LET_DECL
        ? ast_let_initializer(binding_decl) : NULL;
    bool current = binding_id != 0 && routine_id != 0
        && binding != NULL && binding->decl_syntax_id == binding_id
        && !binding->is_parameter && !binding->is_host_field
        && !binding->is_consumed
        && binding_decl != NULL && ast_node_stable_id(binding_decl) == binding_id
        && !ast_let_is_alias(binding_decl)
        && initializer != NULL
        && (initializer->type == AST_CALL
            || initializer->type == AST_ARRAY_LITERAL)
        && source != NULL && source->type == AST_CALL
        && fact != NULL && fact->function_syntax_id == routine_id
        && fact->binding_syntax_id == binding_id
        && fact->disposition == PGY_COLLECTION_DISPOSITION_LIVE
        && fact->origin == PGY_COLLECTION_ORIGIN_UNKNOWN
        && fact->element_ownership == PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
        && fact->return_elements_owned
        && fact->return_elements_source == initializer
        && fact->return_elements_source_id != 0
        && fact->return_elements_source_id == ast_node_stable_id(initializer);
    return current;
}

static bool store_landing_owned_array_candidate_current(
    SemanticContext *ctx, const OwnedStringArrayPushCandidate *candidate,
    const ASTNode **routine_out, bool *normalizing_out)
{
    if (routine_out == NULL || normalizing_out == NULL) return false;
    *routine_out = NULL;
    *normalizing_out = false;
    const ASTNode *call = candidate == NULL ? NULL : candidate->call;
    PublishedNode *row = call == NULL ? NULL
        : publication_result_current(ctx, call);
    const ASTNode *routine = row == NULL ? NULL : row->caller;
    const ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 0) : NULL;
    ASTNode *source = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 1) : NULL;
    uint32_t routine_id = routine == NULL ? 0 : ast_node_stable_id(routine);
    uint32_t binding_id = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    PublishedNode *binding_row = publication_node_by_id(
        ctx->function_param_publications, binding_id);
    const ASTNode *binding_decl = binding_row == NULL ? NULL : binding_row->node;
    const ASTNode *initializer = binding_decl != NULL
        && binding_decl->type == AST_LET_DECL
        ? ast_let_initializer(binding_decl) : NULL;
    const PgyCollectionOwnershipFact *fact =
        semantic_collection_ownership_fact_find(ctx, routine_id, binding_id);
    uint32_t producer_id = 0, string_source = UINT32_MAX;
    if (candidate == NULL
        || candidate->kind !=
            OWNED_STRING_ARRAY_PUSH_CANDIDATE_RETURNED_ARRAY
        || call == NULL || call->type != AST_CALL
        || candidate->call_id == 0
        || candidate->call_id != ast_node_stable_id(call)
        || candidate->routine_id == 0
        || candidate->routine_id != routine_id
        || candidate->routine != routine
        || candidate->receiver != receiver
        || candidate->receiver_id == 0
        || candidate->receiver_id != ast_node_stable_id(receiver)
        || candidate->receiver_binding_id == 0
        || candidate->receiver_binding_id != binding_id
        || candidate->initializer == NULL
        || candidate->initializer != initializer
        || candidate->initializer_id == 0
        || candidate->initializer_id != ast_node_stable_id(initializer)
        || row == NULL || row->node != call || row->id != candidate->call_id
        || routine == NULL || routine->type != AST_FUNC_DECL
        || row->caller_id != routine_id || routine_id == 0
        || !semantic_param_publication_checked_call(ctx, call)
        || !semantic_param_publication_checked_argument(ctx, call, 0)
        || !semantic_param_publication_checked_argument(ctx, call, 1)
        || !collection_call_is_unshadowed_builtin(call, "ArrayPush")
        || receiver == NULL || binding_id == 0
        || source == NULL || source->type != AST_CALL
        || binding_decl == NULL || ast_node_stable_id(binding_decl) != binding_id
        || ast_let_is_alias(binding_decl)
        || initializer == NULL
        || (initializer->type != AST_CALL
            && initializer->type != AST_ARRAY_LITERAL)
        || fact == NULL
        || fact->function_syntax_id != routine_id
        || fact->binding_syntax_id != binding_id
        || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
        || fact->origin != PGY_COLLECTION_ORIGIN_UNKNOWN
        || fact->element_ownership != PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN
        || (!semantic_collection_owned_string_push_effect_current(
                call, receiver, ctx)
            && !store_landing_owned_array_candidate_recorded(ctx, call))
        || !semantic_collection_owned_string_call_result(
            source, ctx, &producer_id) || producer_id == 0
        || !semantic_indexed_string_borrow_source_current(
            source, ctx, &string_source) || string_source != 0)
        return false;
    if (!store_landing_owned_array_initializer_current(ctx, initializer)) {
        if (!store_landing_owned_array_literal_normalization_select(
                ctx, initializer))
            return false;
        *normalizing_out = true;
        return false;
    }
    if (!fact->return_elements_owned
        || fact->return_elements_source != initializer
        || fact->return_elements_source_id == 0
        || fact->return_elements_source_id != ast_node_stable_id(initializer)
        || !semantic_formal_string_store_landing_select_owned(
            ctx, (ASTNode *)call))
        return false;
    *routine_out = routine;
    return true;
}

static bool store_landing_member_value_copy_candidate_current(
    SemanticContext *ctx, const OwnedStringArrayPushCandidate *candidate,
    const ASTNode **routine_out)
{
    if (routine_out == NULL) return false;
    *routine_out = NULL;
    const ASTNode *call = candidate != NULL ? candidate->call : NULL;
    PublishedNode *row = call != NULL
        ? publication_result_current(ctx, call) : NULL;
    const ASTNode *routine = row != NULL ? row->caller : NULL;
    const ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 0) : NULL;
    if (candidate == NULL
        || candidate->kind !=
            OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
        || candidate->blocked_by_uncopyable_aggregate_reuse
        || call == NULL || call->type != AST_CALL
        || candidate->call_id == 0
        || candidate->call_id != ast_node_stable_id(call)
        || candidate->routine == NULL
        || candidate->routine != routine
        || candidate->routine_id == 0
        || candidate->routine_id != ast_node_stable_id(routine)
        || candidate->receiver == NULL
        || candidate->receiver != receiver
        || candidate->receiver_id == 0
        || candidate->receiver_id != ast_node_stable_id(receiver)
        || row == NULL || row->node != call || row->id != candidate->call_id
        || row->caller_id != candidate->routine_id
        || !semantic_collection_member_value_copy_replay_ready(
            (ASTNode *)call, (ASTNode *)receiver, ctx))
        return false;
    *routine_out = routine;
    return true;
}

static bool store_landing_owned_array_candidate_recorded(
    const SemanticContext *ctx, const ASTNode *call)
{
    uint32_t call_id = call != NULL ? ast_node_stable_id(call) : 0;
    if (ctx == NULL || call == NULL || call_id == 0) return false;
    for (size_t i = 0;
         i < ctx->owned_string_array_push_candidate_count; i++) {
        OwnedStringArrayPushCandidate *candidate =
            &ctx->owned_string_array_push_candidates[i];
        if (candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_RETURNED_ARRAY
            && candidate->call_id == call_id)
            return candidate->call == call;
    }
    return false;
}

bool semantic_formal_string_store_landing_select_owned_set(
    SemanticContext *ctx, const ASTNode *call)
{
    if (call == NULL || ctx == NULL || ast_call_arg_count(call) != 3
        || ast_node_stable_id(call) == 0
        || !type_equals(store_landing_receiver_element(
            ast_call_argument(call, 0), ctx), TYPE_STRING)
        || !semantic_collection_owned_string_set_required(
            ast_call_argument(call, 0), ctx)) return false;
    return store_landing_record_for_routine_kind(ctx, call,
        ctx->current_function_decl,
        store_landing_value_formal_id(ast_call_argument(call, 2), ctx),
        PGY_STRING_STORE_LANDING_OWNED_SET);
}

bool semantic_formal_store_landing_select_held(SemanticContext *ctx,
    const ASTNode *call, size_t value_ordinal)
{
    if (ctx == NULL || call == NULL || value_ordinal >= ast_call_arg_count(call) ||
        ast_node_stable_id(call) == 0) return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, ast_call_argument(call, value_ordinal));
    if (!value.ok) return false;
    if (value.meta || publication_physical_scalar(value.type) ||
        !semantic_param_publication_clone_independent(ctx, value.type)) return true;
    ASTNode *stored = (ASTNode *)ast_call_argument(call, value_ordinal);
    if (store_landing_kind_for_call(call, ctx)
            == PGY_STRING_STORE_LANDING_OWNED_PUSH
        && store_landing_definition_birth_independent(ctx, stored))
        return true;
    const ASTNode *routine = ctx->current_function_decl;
    if (routine == NULL) {
        PublishedNode *call_row = publication_result_current(ctx, call);
        if (call_row != NULL && call_row->caller != NULL
            && call_row->caller->type == AST_FUNC_DECL
            && call_row->caller_id != 0
            && call_row->caller_id
                == ast_node_stable_id(call_row->caller))
            routine = call_row->caller;
    }
    return store_landing_record_for_routine_kind(ctx, call,
        routine, 0,
        store_landing_kind_for_call(call, ctx));
}

bool semantic_formal_store_landing_selects_value(const SemanticContext *ctx, const ASTNode *call,
    const ASTNode *value)
{
    /* Every store this owner selects (ArrayPush, ArraySet, MapSet) takes
     * its value last. */
    uint32_t id = call != NULL && call->type == AST_CALL && ast_call_arg_count(call) != 0 &&
        ast_call_argument(call, ast_call_arg_count(call) - 1) == value ? ast_node_stable_id(call) : 0;
    for (size_t i = 0; ctx != NULL && id != 0 && i < ctx->string_store_landing_selected_count; i++)
        if (ctx->string_store_landing_selected[i] == id) return true;
    return false;
}

bool semantic_formal_store_landing_value_selected(
    const SemanticContext *ctx, const ASTNode *value)
{
    for (size_t i = 0; ctx != NULL && value != NULL
        && i < ctx->string_store_landing_selected_count; i++) {
        const ASTNode *call = ctx->string_store_landing_selected_nodes[i];
        if (call == NULL || call->type != AST_CALL
            || ast_node_stable_id(call) == 0
            || ast_node_stable_id(call)
                != ctx->string_store_landing_selected[i]
            || ast_call_arg_count(call) == 0)
            continue;
        if (ast_call_argument(call,
                ast_call_arg_count(call) - 1) == value)
            return true;
    }
    return false;
}

bool semantic_formal_store_landing_select_assigned(SemanticContext *ctx, const ASTNode *routine,
    uint32_t input, const ASTNode *target, const ASTNode *value)
{
    while (target != NULL && (target->type == AST_MEMBER_ACCESS || target->type == AST_ARRAY_ACCESS))
        target = target->type == AST_MEMBER_ACCESS ? ast_member_object(target) : ast_array_access_array(target);
    uint32_t root = target != NULL && target->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, target) : 0;
    /* A local assigned back, as `out.names = names` after pushes into
     * `names`, got the formal's backing from a store into it: that store is
     * where a copy stands, and copying the local would end the write-back. */
    uint32_t local = value != NULL && value->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, value) : 0;
    bool output = false;
    for (size_t p = 0; root != 0 && p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param == NULL) continue;
        output |= param->mode == PARAM_MODE_MUT_REF && ast_func_param_stable_id(param) == root && root != input;
        if (ast_func_param_stable_id(param) == local) local = 0;
    }
    return !output || local != 0 || semantic_loan_copy_landing_select_carrier(ctx, value);
}

bool semantic_formal_string_store_landing_select_checked(SemanticContext *ctx, ASTNode *call,
    size_t value_ordinal)
{
    ASTNode *value = call == NULL || value_ordinal >= ast_call_arg_count(call) ? NULL
        : ast_call_argument(call, value_ordinal);
    if (ctx == NULL || value == NULL || ast_node_stable_id(call) == 0)
        return false;

    uint32_t formal_id = store_landing_value_formal_id(value, ctx);
    bool exact_local_string = formal_id == 0 &&
        store_landing_value_exact_local_string_path(value, ctx);
    /* The selected ArraySet is not admitted here.  A later analysis must
     * still prove that the exact receiver is an owned-String Array before it
     * rewrites the call to ArraySetOwnedString.  Recording an exact local
     * String therefore extends the existing copy boundary without treating
     * the local's borrowed descriptor as independently owned. */
    if (formal_id == 0 && !exact_local_string)
        return true;
    return store_landing_record_for_routine(ctx, call,
        ctx->current_function_decl, formal_id);
}

static const FunctionParamFlowIndexClaim *store_landing_claim(
    const FunctionParamFlowIndexClaims *claims, const ASTNode *node)
{
    const FunctionParamFlowIndexClaim *found = NULL;
    if (claims == NULL || node == NULL) return NULL;
    for (size_t i = 0; i < claims->count; i++) {
        if (claims->rows[i].node != node) continue;
        if (found != NULL) return NULL;
        found = &claims->rows[i];
    }
    return found;
}

static const Type *store_landing_published_formal_path_type(
    SemanticContext *ctx, const PendingRoutine *routine,
    const ASTNode *value, uint32_t formal_id)
{
    if (routine == NULL || value == NULL) return NULL;
    if (value->type == AST_IDENTIFIER) {
        if (ast_identifier_binding_syntax_id(value) != formal_id)
            return NULL;
        for (size_t p = 0; p < routine->count; p++) {
            FuncParam *formal = ast_func_param(routine->decl, p);
            if (formal != NULL && ast_func_param_stable_id(formal) == formal_id
                && routine->formals[p].id == formal_id
                && (formal->mode == PARAM_MODE_DEFAULT
                    || formal->mode == PARAM_MODE_REF))
                return routine->types[p];
        }
        return NULL;
    }
    if (value->type == AST_ARRAY_ACCESS) {
        const Type *array = store_landing_published_formal_path_type(
            ctx, routine, ast_array_access_array(value), formal_id);
        return type_is_constructed_named(array, "Array")
            ? type_get_constructed_arg(array, 0) : NULL;
    }
    if (value->type != AST_MEMBER_ACCESS
        || ast_member_name(value) == NULL) return NULL;
    const Type *object = store_landing_published_formal_path_type(
        ctx, routine, ast_member_object(value), formal_id);
    ASTNode *decl = object == NULL || object->kind != TYPE_KIND_CLASS
        ? NULL : semantic_host_decl_for_type(ctx, object);
    if (decl == NULL || decl->type != AST_CLASS_DECL) return NULL;
    for (size_t i = 0;; i++) {
        const char *name = NULL;
        Type *field = constructor_decl_field_resolved_type_at(
            decl, i, &name, ctx);
        if (name == NULL) return NULL;
        if (strcmp(name, ast_member_name(value)) == 0) return field;
    }
}

static bool store_landing_receiver_lineage_replayable(
    const PgyCollectionOwnershipFact *fact, uint32_t routine_id,
    uint32_t binding_id)
{
    if (fact == NULL || routine_id == 0 || binding_id == 0
        || fact->function_syntax_id != routine_id
        || fact->binding_syntax_id != binding_id)
        return false;
    if (fact->element_ownership == PGY_STRING_ARRAY_OWNED_ELEMENTS
        || fact->element_ownership == PGY_STRING_ARRAY_MAP_KEYS_SNAPSHOT)
        return true;
    /* A moved field's end-of-routine row may already be retired or released
     * even though the indexed call site precedes that transition. Its
     * complete stored source identity is enough to schedule a replay, never
     * to admit the mutation: the next analysis reconstructs the live
     * symbol/birth and re-runs the ordinary owned-push checks. */
    return fact->origin == PGY_COLLECTION_ORIGIN_MEMBER_MOVE
        && fact->source_binding_syntax_id != 0
        && fact->source_binding_syntax_id != binding_id
        && fact->origin_syntax_id != 0
        && fact->source_birth_serial != 0
        && fact->member_move_source != NULL
        && fact->member_move_source_id != 0
        && ast_node_stable_id(fact->member_move_source)
            == fact->member_move_source_id;
}

static bool store_landing_batch_candidate(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, ASTNode *routine,
    ASTNode *candidate, uint32_t *formal_id_out)
{
    size_t value_ordinal = 0;
    if (formal_id_out == NULL || candidate == NULL
        || candidate->type != AST_CALL
        || !ast_call_semantic_callee_is_stdlib(candidate)
        || ast_call_semantic_callee_declared_callable(candidate)
        || ast_call_semantic_callee_decl_id(candidate) != 0
        || ast_call_semantic_callee_value_binding_id(candidate) != 0)
        return false;
    ASTNode *callee = ast_call_callee(candidate);
    ASTNode *receiver = ast_call_argument(candidate, 0);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(callee) != 0
        || ast_identifier_name(callee) == NULL
        || receiver == NULL || receiver->type != AST_IDENTIFIER)
        return false;
    if (strcmp(ast_identifier_name(callee), "ArrayPush") == 0
        && ast_call_arg_count(candidate) == 2)
        value_ordinal = 1;
    else if (strcmp(ast_identifier_name(callee), "ArraySet") == 0
        && ast_call_arg_count(candidate) == 3)
        value_ordinal = 2;
    else
        return false;
    ASTNode *value = ast_call_argument(candidate, value_ordinal);
    uint32_t routine_id = ast_node_stable_id(routine);

    const FunctionParamFlowIndexClaim *call_claim =
        store_landing_claim(claims, candidate);
    const FunctionParamFlowIndexClaim *receiver_claim =
        store_landing_claim(claims, receiver);
    const FunctionParamFlowIndexClaim *value_claim =
        store_landing_claim(claims, value);
    const PendingRoutine *published_routine = publication_routine(
        ctx->function_param_publications, routine_id);
    bool reachable = false;
    bool current_execution = semantic_param_publication_execution(
        ctx, candidate, &reachable);
    const ASTNode *root = value;
    while (root != NULL && (root->type == AST_MEMBER_ACCESS
            || root->type == AST_ARRAY_ACCESS))
        root = root->type == AST_MEMBER_ACCESS
            ? ast_member_object(root) : ast_array_access_array(root);
    uint32_t formal_id = root != NULL && root->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(root) : 0;
    const Type *formal_path_type = formal_id != 0
        ? store_landing_published_formal_path_type(
            ctx, published_routine, value, formal_id)
        : NULL;
    if (call_claim == NULL || receiver_claim == NULL || value_claim == NULL
        || !call_claim->semantic_promoted
        || !call_claim->snapshot_semantic_complete
        || !call_claim->execution_present || !call_claim->reachable
        || !current_execution
        || !reachable || !receiver_claim->value_ready
        || receiver_claim->value_meta
        || !type_is_constructed_named(receiver_claim->value_type, "Array")
        || !type_equals(type_get_constructed_arg(receiver_claim->value_type, 0),
            TYPE_STRING)
        || !value_claim->snapshot_complete)
        return false;
    if (formal_id != 0 && type_equals(formal_path_type, TYPE_STRING)) {
        *formal_id_out = formal_id;
        return true;
    }
    /* A local/literal String has no formal id. It can still be selected
     * without guessing when the collection owner already records that this
     * exact receiver is a live owned-String snapshot: ordinary mutation is
     * forbidden there, and the planned owned store is the existing repair. */
    const PgyCollectionOwnershipFact *receiver_fact =
        semantic_collection_ownership_fact_find(ctx, routine_id,
            receiver_claim->binding);
    /* The earlier refusal may leave later value expressions untyped.  The
     * exact Array<String> receiver fixes the builtin's expected value type;
     * a ready value must already agree, while an unready value is only
     * scheduled.  Replay type-checks it and the ownership admission again. */
    bool value_expected_string = !value_claim->value_ready
        || (!value_claim->value_meta
            && type_equals(value_claim->value_type, TYPE_STRING));
    if (!value_expected_string
        || !store_landing_receiver_lineage_replayable(
            receiver_fact, routine_id, receiver_claim->binding))
        return false;
    *formal_id_out = 0;
    return true;
}

bool semantic_formal_string_store_landing_select_return_dependency(
    SemanticContext *ctx, ASTNode *routine, uint32_t formal_id,
    bool *selected_out)
{
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (selected_out == NULL) return false;
    *selected_out = false;
    if (ctx == NULL || routine == NULL || routine->type != AST_FUNC_DECL
        || formal_id == 0 || ctx->function_param_flow_summaries == NULL
        || !semantic_param_publication_inputs_ready(ctx)
        || !function_param_flow_program_point_index_build(
            ctx->function_param_flow_summaries, routine, &index)
        || index == NULL)
        return false;

    for (size_t i = 0; i < index->claims->count; i++) {
        ASTNode *candidate = index->claims->rows[i].node;
        uint32_t candidate_formal = 0;
        if (!store_landing_batch_candidate(ctx, index->claims, routine,
                candidate, &candidate_formal)
            || candidate_formal != formal_id)
            continue;
        if (!store_landing_record_for_routine(
                ctx, candidate, routine, formal_id))
            return false;
        *selected_out = true;
    }
    return true;
}

static bool store_landing_scalar_store_transparent(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, ASTNode *candidate)
{
    if (ctx == NULL || claims == NULL || candidate == NULL
        || candidate->type != AST_CALL
        || !ast_call_semantic_callee_is_stdlib(candidate)
        || ast_call_semantic_callee_declared_callable(candidate)
        || ast_call_semantic_callee_decl_id(candidate) != 0
        || ast_call_semantic_callee_value_binding_id(candidate) != 0)
        return false;
    ASTNode *callee = ast_call_callee(candidate);
    ASTNode *receiver = ast_call_argument(candidate, 0);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    if (name == NULL || ast_identifier_binding_syntax_id(callee) != 0
        || receiver == NULL || receiver->type != AST_IDENTIFIER
        || !((strcmp(name, "ArrayPush") == 0
                && ast_call_arg_count(candidate) == 2)
            || (strcmp(name, "ArraySet") == 0
                && ast_call_arg_count(candidate) == 3)))
        return false;
    const FunctionParamFlowIndexClaim *call_claim =
        store_landing_claim(claims, candidate);
    const FunctionParamFlowIndexClaim *receiver_claim =
        store_landing_claim(claims, receiver);
    bool reachable = false;
    bool current_execution = semantic_param_publication_execution(
        ctx, candidate, &reachable);
    const Type *element = receiver_claim != NULL
        && receiver_claim->value_ready && !receiver_claim->value_meta
        && type_is_constructed_named(receiver_claim->value_type, "Array")
        ? type_get_constructed_arg(receiver_claim->value_type, 0) : NULL;
    return call_claim != NULL && receiver_claim != NULL
        && call_claim->semantic_promoted
        && call_claim->snapshot_semantic_complete
        && call_claim->execution_present && call_claim->reachable
        && current_execution && reachable
        && publication_physical_scalar(element);
}

static bool store_landing_pure_replay_expression(const ASTNode *node,
    size_t depth)
{
    if (node == NULL || depth > 64) return false;
    if (node->type == AST_IDENTIFIER || node->type == AST_NUMBER
        || node->type == AST_STRING || node->type == AST_BOOLEAN)
        return true;
    if (node->type != AST_CALL
        || !ast_call_semantic_callee_is_stdlib(node)
        || ast_call_semantic_callee_declared_callable(node)
        || ast_call_semantic_callee_decl_id(node) != 0
        || ast_call_semantic_callee_value_binding_id(node) != 0)
        return false;
    const ASTNode *callee = ast_call_callee(node);
    const char *name = callee != NULL && callee->type == AST_IDENTIFIER
        ? ast_identifier_name(callee) : NULL;
    bool pure = name != NULL && ast_identifier_binding_syntax_id(callee) == 0
        && (strcmp(name, "ArrayLength") == 0
            || strcmp(name, "Concat") == 0
            || strcmp(name, "StringConcat") == 0
            || strcmp(name, "ToString") == 0
            || strcmp(name, "ToInt") == 0);
    if (!pure) return false;
    for (size_t i = 0; i < ast_call_arg_count(node); i++)
        if (!store_landing_pure_replay_expression(
                ast_call_argument(node, i), depth + 1))
            return false;
    return true;
}

static bool store_landing_pure_let_transparent(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, ASTNode *candidate)
{
    if (ctx == NULL || claims == NULL || candidate == NULL
        || candidate->type != AST_LET_DECL)
        return false;
    const FunctionParamFlowIndexClaim *claim =
        store_landing_claim(claims, candidate);
    bool reachable = false;
    bool current_execution = semantic_param_publication_execution(
        ctx, candidate, &reachable);
    return claim != NULL && claim->semantic_promoted
        && claim->snapshot_semantic_complete
        && claim->execution_present && claim->reachable
        && current_execution && reachable
        && store_landing_pure_replay_expression(
            ast_let_initializer(candidate), 0);
}

static const FunctionParamFlowIndexClaim *store_landing_parent_claim(
    const FunctionParamFlowIndexClaims *claims, const ASTNode *child,
    size_t *child_ordinal_out)
{
    const FunctionParamFlowIndexClaim *found = NULL;
    size_t found_child = SIZE_MAX;
    if (claims == NULL || child == NULL || child_ordinal_out == NULL)
        return NULL;
    for (size_t r = 0; r < claims->count; r++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[r];
        for (size_t c = 0; c < row->child_count; c++) {
            if (row->children[c] != child) continue;
            if (found != NULL) return NULL;
            found = row;
            found_child = c;
        }
    }
    if (found == NULL || found_child == SIZE_MAX) return NULL;
    *child_ordinal_out = found_child;
    return found;
}

static bool store_landing_select_contiguous_formals(SemanticContext *ctx,
    ASTNode *routine, uint32_t seed_id, uint32_t seed_formal_id)
{
    FunctionParamFlowSummaryStore *flow = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    /* The flow index is demand-owned.  A routine that no earlier consumer
     * queried legitimately has no row yet; ask its owner to publish the
     * current snapshot instead of silently falling back to one store per
     * whole-program analysis. */
    size_t at = function_param_flow_program_point_index_find(
        flow, ast_node_stable_id(routine));
    if (at == SIZE_MAX) {
        if (!function_param_flow_program_point_index_build(flow, routine, &index)
            || index == NULL) return false;
    } else {
        index = &flow->program_point_indexes[at];
    }
    if (index->function_decl != routine
        || !function_param_flow_index_roots_current(index)
        || !function_param_flow_syntax_claims_current(NULL, index->claims))
        return true;
    const FunctionParamFlowIndexClaim *seed = NULL;
    for (size_t c = 0; c < index->claims->count; c++)
        if (index->claims->rows[c].id == seed_id) {
            if (seed != NULL) return true;
            seed = &index->claims->rows[c];
        }
    if (seed == NULL || seed->node == NULL) return true;
    uint32_t current_seed_formal_id = 0;
    if (!store_landing_batch_candidate(ctx, index->claims, routine,
            seed->node, &current_seed_formal_id)
        || current_seed_formal_id != seed_formal_id)
        return true;
    const ASTNode *child = seed->node;
    const FunctionParamFlowIndexClaim *block = NULL;
    size_t child_ordinal = SIZE_MAX;
    for (size_t depth = 0; depth < index->claims->count; depth++) {
        const FunctionParamFlowIndexClaim *parent =
            store_landing_parent_claim(index->claims, child, &child_ordinal);
        if (parent == NULL) return true;
        if (parent->shape == AST_BLOCK) {
            block = parent;
            break;
        }
        child = parent->node;
    }
    if (block == NULL || child_ordinal >= block->child_count) return true;
    /* Only a straight-line run in the seed's nearest block is replayed.
     * Exact scalar Array stores and side-effect-free scalar/String lets are
     * transparent; a branch, assignment, user call, or other statement ends
     * the snapshot. Every scheduled store is fully checked on replay. */
    for (size_t c = child_ordinal + 1; c < block->child_count; c++) {
        ASTNode *candidate = block->children[c];
        uint32_t candidate_formal_id = 0;
        if (store_landing_batch_candidate(
                ctx, index->claims, routine, candidate,
                &candidate_formal_id)) {
            if (!store_landing_record_for_routine(
                    ctx, candidate, routine, candidate_formal_id))
                return false;
            continue;
        }
        if (store_landing_scalar_store_transparent(
                ctx, index->claims, candidate))
            continue;
        if (store_landing_pure_let_transparent(
                ctx, index->claims, candidate))
            continue;
        break;
    }
    return true;
}

static const ASTNode *store_landing_compiler_clone_input(
    const ASTNode *value)
{
    uint32_t builtin = 0;
    return value != NULL && value->type == AST_CALL
        && ast_call_compiler_builtin(value)
        && ast_call_arg_count(value) == 1
        && ast_call_semantic_callee_builtin_kind(value, &builtin)
        && builtin == BUILTIN_CLONE
        && ast_call_semantic_callee_is_stdlib(value)
        && ast_call_semantic_callee_decl_id(value) == 0
        && ast_call_semantic_callee_value_binding_id(value) == 0
        ? ast_call_argument(value, 0) : NULL;
}

/* A direct producer or an earlier fixed-point round can make one push owned
 * before its peers.  If the same fresh local Array still has shallow String
 * pushes, select every one of those shallow calls in the same landing batch.
 * An independent result keeps its exact direct-transfer receipt instead of
 * being copied a second time. */
static bool store_landing_select_mixed_local_receiver_pushes(
    SemanticContext *ctx)
{
    FunctionParamPublicationStore *publication = ctx != NULL
        ? ctx->function_param_publications : NULL;
    if (publication == NULL || !semantic_param_publication_inputs_ready(ctx))
        return false;
    if (publication->node_capacity == 0)
        return true;
    if (publication->node_capacity > SIZE_MAX / sizeof(ASTNode *))
        return false;
    ASTNode **owned_seed = calloc(publication->node_capacity,
        sizeof(*owned_seed));
    ASTNode **shallow_seeds = calloc(publication->node_capacity,
        sizeof(*shallow_seeds));
    uint32_t *shallow_bindings = calloc(publication->node_capacity,
        sizeof(*shallow_bindings));
    unsigned char *typed_owned_seed = calloc(publication->node_capacity,
        sizeof(*typed_owned_seed));
    if (owned_seed == NULL || shallow_seeds == NULL
        || shallow_bindings == NULL || typed_owned_seed == NULL) {
        free(owned_seed);
        free(shallow_seeds);
        free(shallow_bindings);
        free(typed_owned_seed);
        return false;
    }
    size_t shallow_count = 0;
    for (size_t i = 0; i < publication->node_capacity; i++) {
        PublishedNode *seed_row = &publication->nodes[i];
        ASTNode *seed = (ASTNode *)seed_row->node;
        bool general_push = collection_call_is_unshadowed_builtin(
            seed, "ArrayPush");
        bool owned_push = collection_call_is_unshadowed_builtin(
            seed, "ArrayPushOwnedString");
        bool general_set = collection_call_is_unshadowed_builtin(
            seed, "ArraySet");
        if (seed_row->id == 0 || seed == NULL || seed->type != AST_CALL
            || seed_row != publication_result_current(ctx, seed)
            || seed_row->caller == NULL || seed_row->caller_id == 0
            || !seed_row->execution_present || !seed_row->reachable
            || (!general_push && !owned_push && !general_set)
            || ((general_push || owned_push)
                && ast_call_arg_count(seed) != 2)
            || (general_set && ast_call_arg_count(seed) != 3)
            || !semantic_param_publication_checked_call(ctx, seed)
            || !semantic_param_publication_checked_argument(ctx, seed, 0)
            || !semantic_param_publication_checked_argument(ctx, seed,
                general_set ? 2 : 1))
            continue;
        ASTNode *receiver = ast_call_argument(seed, 0);
        uint32_t binding = receiver != NULL
            && receiver->type == AST_IDENTIFIER
            ? semantic_param_publication_binding(ctx, receiver) : 0;
        uint32_t kind = 0, effect_receiver = 0, source = 0;
        PublishedNode *binding_row = binding != 0
            ? publication_node_by_id(publication, binding) : NULL;
        if (binding == 0 || binding >= publication->node_capacity
            || binding_row == NULL
            || binding_row->node == NULL
            || binding_row->node->type != AST_LET_DECL
            || !ast_call_semantic_collection_effect(seed, &kind,
                &effect_receiver, &source)
            || effect_receiver != binding)
            continue;
        bool copied_empty_local =
            kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
            && source == 0
            && general_push
            && store_landing_planned_kind(seed, ctx,
                PGY_STRING_STORE_LANDING_VALUE_COPY)
            && semantic_collection_owned_string_empty_local_origin_for_routine(
                receiver, seed_row->caller, ctx);
        const ASTNode *set_value = general_set
            ? ast_call_argument(seed, 2) : NULL;
        const ASTNode *set_value_path =
            store_landing_compiler_clone_input(set_value);
        if (set_value_path == NULL)
            set_value_path = set_value;
        bool selected_empty_local_set = general_set
            && kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
            && source == 0
            && set_value_path != NULL
            && set_value_path->type == AST_MEMBER_ACCESS
            && (store_landing_planned_kind(seed, ctx,
                    PGY_STRING_STORE_LANDING_VALUE_COPY)
                || store_landing_selected_kind(ctx, seed,
                    PGY_STRING_STORE_LANDING_VALUE_COPY))
            && semantic_collection_owned_string_empty_local_origin_for_routine(
                receiver, seed_row->caller, ctx);
        bool current_owned_push =
            kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
            && source == 0
            && owned_push
            && semantic_collection_owned_string_push_effect_current(
                seed, receiver, ctx);
        bool planned_owned_push =
            store_landing_planned_kind(seed, ctx,
                PGY_STRING_STORE_LANDING_OWNED_PUSH)
            || store_landing_selected_kind(ctx, seed,
                PGY_STRING_STORE_LANDING_OWNED_PUSH);
        if (kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
            && source == 0 && general_push) {
            if (shallow_count >= publication->node_capacity) {
                free(owned_seed);
                free(shallow_seeds);
                free(shallow_bindings);
                free(typed_owned_seed);
                return false;
            }
            shallow_seeds[shallow_count] = seed;
            shallow_bindings[shallow_count++] = binding;
        }
        if (owned_seed[binding] == NULL
            && (current_owned_push || copied_empty_local
                || planned_owned_push
                || selected_empty_local_set
                || (kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                    && source != 0 && general_push)))
            owned_seed[binding] = seed;
        if (current_owned_push)
            typed_owned_seed[binding] = 1;
    }
    bool ok = true;
    for (size_t i = 0; i < shallow_count; i++) {
        uint32_t binding = shallow_bindings[i];
        if (binding == 0 || binding >= publication->node_capacity
            || owned_seed[binding] == NULL)
            continue;
        ASTNode *value = shallow_seeds[i] != NULL
            && ast_call_arg_count(shallow_seeds[i]) == 2
            ? ast_call_argument(shallow_seeds[i], 1) : NULL;
        bool selected = typed_owned_seed[binding] != 0
                || (value != NULL && value->type == AST_STRING)
            ? semantic_formal_string_store_landing_select_owned(
                ctx, shallow_seeds[i])
            : semantic_formal_string_store_landing_select_value_copy(
                ctx, shallow_seeds[i]);
        if (!selected) {
            ok = false;
            break;
        }
    }
    free(owned_seed);
    free(shallow_seeds);
    free(shallow_bindings);
    free(typed_owned_seed);
    if (!ok)
        return false;
    return true;
}

static bool
store_landing_definition_birth_copy_selected(SemanticContext *ctx,
    const ASTNode *value)
{
    const ASTNode *initializer =
        store_landing_unassigned_definition_birth(ctx, value);
    bool selected = false;
    if (initializer == NULL
        || !ast_subtree_intersects_stable_ids(initializer,
            ctx->loan_copy_landing_selected,
            ctx->loan_copy_landing_selected_count, &selected))
        return false;
    return selected;
}

static bool
store_landing_select_projected_owned_sets(SemanticContext *ctx)
{
    FunctionParamPublicationStore *publication = ctx != NULL
        ? ctx->function_param_publications : NULL;
    if (publication == NULL)
        return false;
    for (size_t i = 0; i < publication->node_capacity; i++) {
        PublishedNode *row = &publication->nodes[i];
        ASTNode *call = (ASTNode *)row->node;
        if (row->id == 0 || call == NULL || call->type != AST_CALL
            || row->id != ast_node_stable_id(call)
            || row != publication_result_current(ctx, call)
            || ast_call_arg_count(call) != 3
            || !collection_call_is_unshadowed_builtin(call, "ArraySet")
            || !semantic_param_publication_checked_call(ctx, call)
            || !semantic_param_publication_checked_argument(ctx, call, 0)
            || !semantic_param_publication_checked_argument(ctx, call, 2))
            continue;
        const ASTNode *receiver = ast_call_argument(call, 0);
        uint32_t binding = receiver != NULL
            && receiver->type == AST_IDENTIFIER
            ? semantic_param_publication_binding(ctx, receiver) : 0;
        PublishedNode *binding_row = binding != 0
            ? publication_node_by_id(publication, binding) : NULL;
        const ASTNode *declaration = binding_row != NULL
            ? binding_row->node : NULL;
        const ASTNode *initializer = declaration != NULL
            && declaration->type == AST_LET_DECL
            ? ast_let_initializer(declaration) : NULL;
        if (declaration == NULL || ast_node_stable_id(declaration) != binding
            || initializer == NULL
            || !semantic_loan_copy_landing_selected(ctx, initializer)
            || !semantic_loan_copy_landing_carrier_copies(ctx, initializer)
            || store_landing_kind_for_call(call, ctx)
                != PGY_STRING_STORE_LANDING_OWNED_SET)
            continue;
        /* The next parse wraps this exact receiver initializer in a whole-
         * value Clone.  The typed publication owns both the binding and the
         * checked ArraySet, so project only that selected edge and keep the
         * actual owned-set proof on replay. */
        if (!semantic_formal_store_landing_select_held(ctx, call, 2))
            return false;
    }
    return true;
}

bool semantic_formal_string_store_landing_finalize(SemanticContext *ctx)
{
    if (ctx == NULL || !semantic_param_publication_inputs_ready(ctx))
        return false;
    size_t seed_count = ctx->string_store_landing_selected_count;
    /* Formal-seed expansion consumes the flow index. An owned local literal
     * candidate does not: its exact binding, initializer, source call and
     * all-elements-owned premise are already published by their owners. */
    if (seed_count != 0 && ctx->function_param_flow_summaries == NULL)
        return false;
    for (size_t i = 0; i < seed_count; i++) {
        uint32_t routine_id = ctx->string_store_landing_selected_routines[i];
        uint32_t formal_id = ctx->string_store_landing_selected_formals[i];
        PendingRoutine *routine_row = publication_routine(
            ctx->function_param_publications, routine_id);
        ASTNode *routine = routine_row == NULL ? NULL : routine_row->decl;
        if (routine_id == 0)
            continue;
        if (routine == NULL || routine->type != AST_FUNC_DECL
            || ast_node_stable_id(routine) != routine_id)
            continue;
        if (!store_landing_select_contiguous_formals(ctx, routine,
                ctx->string_store_landing_selected[i], formal_id)) return false;
    }
    size_t candidate_count = ctx->owned_string_array_push_candidate_count;
    const ASTNode **candidate_routines = candidate_count == 0 ? NULL
        : calloc(candidate_count, sizeof(*candidate_routines));
    bool *candidate_current = candidate_count == 0 ? NULL
        : calloc(candidate_count, sizeof(*candidate_current));
    bool *candidate_normalizing = candidate_count == 0 ? NULL
        : calloc(candidate_count, sizeof(*candidate_normalizing));
    if (candidate_count != 0
        && (candidate_routines == NULL || candidate_current == NULL
            || candidate_normalizing == NULL)) {
        free(candidate_routines);
        free(candidate_current);
        free(candidate_normalizing);
        return false;
    }
    /* Validate every candidate against the same sealed generation before
     * publishing any diagnostic.  A rejected literal must not make a later,
     * unrelated returned-Array proof appear stale merely because the context
     * now contains an earlier error. */
    for (size_t i = 0; i < candidate_count; i++) {
        const OwnedStringArrayPushCandidate *candidate =
            &ctx->owned_string_array_push_candidates[i];
        if (candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            && candidate->blocked_by_uncopyable_aggregate_reuse)
            continue;
        if (candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            && store_landing_selected_kind(ctx, candidate->call,
                PGY_STRING_STORE_LANDING_OWNED_PUSH))
            continue;
        candidate_current[i] = candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            ? store_landing_member_value_copy_candidate_current(
                ctx, candidate, &candidate_routines[i])
            : store_landing_owned_array_candidate_current(
                ctx, candidate, &candidate_routines[i],
                &candidate_normalizing[i]);
    }
    for (size_t i = 0; i < candidate_count; i++) {
        const OwnedStringArrayPushCandidate *candidate =
            &ctx->owned_string_array_push_candidates[i];
        if (candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            && candidate->blocked_by_uncopyable_aggregate_reuse)
            continue;
        if (candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            && store_landing_selected_kind(ctx, candidate->call,
                PGY_STRING_STORE_LANDING_OWNED_PUSH))
            continue;
        if (candidate_normalizing[i])
            continue;
        if (!candidate_current[i]) {
            ASTNode *source = candidate->call != NULL
                && ast_call_arg_count(candidate->call) == 2
                ? ast_call_argument(candidate->call, 1) : (ASTNode *)candidate->call;
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                source,
                candidate->kind ==
                    OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
                    ? "An aggregate member String copy candidate has no current owned-element path proof"
                    : "An independent String push has no current owned-Array element proof");
            continue;
        }
        if (candidate->kind ==
                OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
            && semantic_formal_string_store_landing_member_value_copy_selected(
                candidate->call, ctx)
            && !semantic_collection_record_member_value_copy_replay_effect(
                (ASTNode *)candidate->call,
                (ASTNode *)candidate->receiver, ctx)) {
            semantic_error(ctx, candidate->call,
                "Aggregate member String copy replay lost its exact ownership effect");
            continue;
        }
        if (!store_landing_record_for_routine_kind(
                ctx, candidate->call, candidate_routines[i], 0,
                candidate->kind ==
                    OWNED_STRING_ARRAY_PUSH_CANDIDATE_MEMBER_VALUE_COPY
                    ? PGY_STRING_STORE_LANDING_MEMBER_VALUE_COPY
                    : PGY_STRING_STORE_LANDING_OWNED_PUSH))
        {
            free(candidate_routines);
            free(candidate_current);
            free(candidate_normalizing);
            return false;
        }
    }
    free(candidate_routines);
    free(candidate_current);
    free(candidate_normalizing);
    if (!store_landing_select_mixed_local_receiver_pushes(ctx))
        return false;
    if (!store_landing_select_projected_owned_sets(ctx))
        return false;
    /* A copy selected inside a stored expression changes that expression's
     * backing before the next analysis. Do not publish the enclosing store's
     * stale pre-copy verdict. Independent stores remain batched; a store that
     * is still unsafe after the inner copy is selected again on replay. */
    size_t kept = 0;
    for (size_t i = 0; i < ctx->string_store_landing_selected_count; i++) {
        const ASTNode *call = ctx->string_store_landing_selected_nodes[i];
        bool nested_copy = false;
        if (call == NULL || call->type != AST_CALL || ast_call_arg_count(call) == 0
            || ast_node_stable_id(call) != ctx->string_store_landing_selected[i]
            || !ast_subtree_intersects_stable_ids(
                ast_call_argument(call, ast_call_arg_count(call) - 1),
                ctx->loan_copy_landing_selected,
                ctx->loan_copy_landing_selected_count, &nested_copy))
            return false;
        bool producer_copy =
            ctx->string_store_landing_selected_kinds[i]
                == PGY_STRING_STORE_LANDING_OWNED_PUSH
            && ast_call_arg_count(call) == 2
            && store_landing_definition_birth_copy_selected(
                ctx, ast_call_argument(call, 1));
        PgyStringStoreLandingKind selected_kind =
            ctx->string_store_landing_selected_kinds[i];
        bool projected_owned_store =
            (nested_copy || producer_copy)
            && selected_kind != PGY_STRING_STORE_LANDING_VALUE_COPY
            && store_landing_kind_for_call(call, ctx) == selected_kind;
        if ((nested_copy || producer_copy) && !projected_owned_store)
            continue;
        if (kept != i) {
            ctx->string_store_landing_selected[kept] =
                ctx->string_store_landing_selected[i];
            ctx->string_store_landing_selected_kinds[kept] =
                ctx->string_store_landing_selected_kinds[i];
            ctx->string_store_landing_selected_nodes[kept] = call;
            ctx->string_store_landing_selected_routines[kept] =
                ctx->string_store_landing_selected_routines[i];
            ctx->string_store_landing_selected_formals[kept] =
                ctx->string_store_landing_selected_formals[i];
        }
        kept++;
    }
    ctx->string_store_landing_selected_count = kept;
    return true;
}

bool semantic_formal_string_store_landing_select_owned(SemanticContext *ctx, const ASTNode *call)
{
    const Type *element = call != NULL && ast_call_arg_count(call) == 2
        ? store_landing_receiver_element(ast_call_argument(call, 0), ctx)
        : NULL;
    if (!type_equals(element, TYPE_STRING) && call != NULL
        && ast_call_arg_count(call) == 2) {
        /* Summary finalization runs after the routine scope has gone away.
         * Its immutable publication is then the type owner; the type-check
         * path above remains the owner while the call itself is checked. */
        SemanticPublishedValue receiver = semantic_param_publication_value(
            ctx, ast_call_argument(call, 0));
        if (receiver.ok && !receiver.meta
            && type_is_constructed_named(receiver.type, "Array"))
            element = type_get_constructed_arg(receiver.type, 0);
    }
    if (call == NULL || ast_call_arg_count(call) != 2 ||
        ast_node_stable_id(call) == 0 || !type_equals(element, TYPE_STRING)) return false;
    if (!semantic_param_publication_inputs_ready(ctx))
        return store_landing_record_for_routine_kind(ctx, call,
            ctx->current_function_decl,
            store_landing_value_formal_id(ast_call_argument(call, 1), ctx),
            PGY_STRING_STORE_LANDING_OWNED_PUSH);

    const ASTNode *receiver = ast_call_argument(call, 0);
    uint32_t binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, receiver) : 0;
    PublishedNode *selected = publication_node(ctx->function_param_publications,
        call, false);
    FunctionParamPublicationStore *publication =
        ctx->function_param_publications;
    if (binding == 0 || publication == NULL || selected == NULL
        || selected->caller == NULL || selected->caller_id == 0)
        return false;

    size_t selected_call = 0;
    for (size_t i = 0; i < publication->node_capacity; i++) {
        PublishedNode *candidate_row = &publication->nodes[i];
        ASTNode *candidate = (ASTNode *)candidate_row->node;
        if (candidate_row->id == 0 || candidate == NULL)
            continue;
        if (candidate_row->caller != selected->caller
            || candidate_row->caller_id != selected->caller_id
            || !candidate_row->execution_present
            || !candidate_row->reachable)
            continue;
        if (candidate == NULL || candidate->type != AST_CALL)
            continue;
        ASTNode *candidate_receiver = ast_call_arg_count(candidate) != 0
            ? ast_call_argument(candidate, 0) : NULL;
        if (candidate_receiver == NULL
            || candidate_receiver->type != AST_IDENTIFIER
            || semantic_param_publication_binding(
                ctx, candidate_receiver) != binding)
            continue;
        if (candidate_row != publication_result_current(ctx, candidate)
            || !semantic_param_publication_checked_call(ctx, candidate))
            return false;
        size_t stored_ordinal = SIZE_MAX;
        bool array_store = semantic_param_publication_checked_array_store(
            ctx, candidate, &stored_ordinal);
        uint32_t effect_kind = 0, effect_receiver = 0, effect_source = 0;
        bool effect = ast_call_semantic_collection_effect(candidate,
            &effect_kind, &effect_receiver, &effect_source);
        if (!effect) {
            if (!array_store) continue;
            ASTNode *pending_value = array_store && stored_ordinal == 1
                ? ast_call_argument(candidate, 1) : NULL;
            uint32_t pending_producer = 0;
            uint32_t pending_source = UINT32_MAX;
            if (stored_ordinal != 1
                || !store_landing_owned_array_candidate_recorded(
                    ctx, candidate)
                || pending_value == NULL || pending_value->type != AST_CALL
                || !semantic_collection_owned_string_call_result(
                    pending_value, ctx, &pending_producer)
                || pending_producer == 0
                || !semantic_indexed_string_borrow_source_current(
                    pending_value, ctx, &pending_source)
                || pending_source != 0)
                return false;
            if (candidate == call) selected_call++;
            continue;
        }
        if (effect_receiver != binding
            || !semantic_param_publication_checked_argument(
                ctx, candidate, 0))
            return false;
        if (effect_kind == PGY_COLLECTION_EFFECT_OWNED_STRING_SET) {
            if (ast_call_arg_count(candidate) != 3
                || !semantic_param_publication_checked_argument(
                    ctx, candidate, 2)
                || !semantic_collection_owned_string_set_effect_current(
                    candidate, ast_call_argument(candidate, 0), ctx))
                return false;
            continue;
        }
        const ASTNode *stored_value = ast_call_arg_count(candidate) == 3
            ? ast_call_argument(candidate, 2) : NULL;
        const ASTNode *stored_path =
            store_landing_compiler_clone_input(stored_value);
        if (stored_path == NULL)
            stored_path = stored_value;
        bool selected_shallow_set =
            effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
            && effect_source == 0
            && ast_call_arg_count(candidate) == 3
            && collection_call_is_unshadowed_builtin(candidate, "ArraySet")
            && semantic_param_publication_checked_argument(ctx, candidate, 2)
            && stored_path != NULL
            && stored_path->type == AST_MEMBER_ACCESS
            && (store_landing_planned_kind(candidate, ctx,
                    PGY_STRING_STORE_LANDING_VALUE_COPY)
                || store_landing_selected_kind(ctx, candidate,
                    PGY_STRING_STORE_LANDING_VALUE_COPY));
        if (selected_shallow_set) {
            SemanticPublishedValue stored = semantic_param_publication_value(
                ctx, ast_call_argument(candidate, 2));
            if (!stored.ok || stored.meta
                || !type_equals(stored.type, TYPE_STRING)
                || !store_landing_record_for_routine_kind(ctx, candidate,
                    selected->caller, 0,
                    PGY_STRING_STORE_LANDING_OWNED_SET))
                return false;
            continue;
        }
        if (ast_call_arg_count(candidate) != 2) return false;
        if (!semantic_param_publication_checked_argument(ctx, candidate, 1))
            return false;
        ASTNode *callee = ast_call_callee(candidate);
        ASTNode *candidate_value = ast_call_argument(candidate, 1);
        if (callee == NULL || callee->type != AST_IDENTIFIER ||
            ast_identifier_binding_syntax_id(callee) != 0 ||
            ast_identifier_name(callee) == NULL ||
            strcmp(ast_identifier_name(callee), "ArrayPush") != 0 ||
            candidate_receiver == NULL || candidate_receiver->type != AST_IDENTIFIER ||
            semantic_param_publication_binding(ctx, candidate_receiver) != binding)
            continue;
        SemanticPublishedValue value = semantic_param_publication_value(ctx,
            candidate_value);
        if (!value.ok || value.meta || !type_equals(value.type, TYPE_STRING) ||
            ast_node_stable_id(candidate) == 0 ||
            effect_receiver != binding) return false;
        /* The collection-effect owner has already sealed this general
         * ArrayPush as an owned transfer from the exact String producer.  The
         * receiver's provisional all-elements-owned fact remains a separate
         * obligation: this landing turns that exact store into the explicit
         * owned-store route consumed by cleanup synthesis. */
        uint32_t independent_source = 0;
        if (store_landing_definition_birth_independent(
                ctx, candidate_value)) {
            if (candidate == call)
                selected_call++;
            continue;
        }
        bool independent =
            semantic_collection_owned_string_push_effect_current(
                candidate, candidate_receiver, ctx)
            && semantic_collection_owned_string_call_result(
                candidate_value, ctx, &independent_source)
            && independent_source != 0;
        if (independent) {
            /* The value already owns its String storage, so no Clone landing
             * is needed.  The Array still needs an owned-store plan: either
             * the exact local-Array candidate finalizer owns that proof, or
             * the receiver's current ownership fact admits the transfer. */
            if (candidate == call) {
                if (store_landing_owned_array_candidate_recorded(ctx, call)) {
                    selected_call++;
                } else if ((semantic_collection_owned_string_push_admissible(
                                candidate_receiver, ctx)
                            || (store_landing_planned_kind(candidate, ctx,
                                    PGY_STRING_STORE_LANDING_VALUE_COPY)
                                && semantic_collection_owned_string_empty_local_origin_for_routine(
                                    candidate_receiver, selected->caller, ctx)))
                    && store_landing_record_for_routine_kind(ctx, candidate,
                        selected->caller, 0,
                        PGY_STRING_STORE_LANDING_OWNED_PUSH)) {
                    selected_call++;
                }
            }
            continue;
        }
        /* Summary finalization runs after `current_function_decl` is
         * retired.  The publication row is the typed owner of this call's
         * caller, so preserve that identity in the landing receipt instead
         * of recording an ownerless store that a later round cannot batch. */
        if (effect_kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION ||
            effect_source != 0 || !store_landing_record_for_routine_kind(
                ctx, candidate, selected->caller,
                store_landing_value_formal_id(candidate_value, ctx),
                PGY_STRING_STORE_LANDING_OWNED_PUSH))
            return false;
        if (candidate == call) selected_call++;
    }
    return selected_call == 1;
}

bool
semantic_formal_string_store_landing_select_deferred_result_copy(
    SemanticContext *ctx, const ASTNode *call)
{
    const ASTNode *receiver = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 0) : NULL;
    const ASTNode *value = call != NULL && ast_call_arg_count(call) == 2
        ? ast_call_argument(call, 1) : NULL;
    const Type *element = receiver != NULL
        ? store_landing_receiver_element((ASTNode *)receiver, ctx) : NULL;
    if (!type_equals(element, TYPE_STRING) && receiver != NULL) {
        SemanticPublishedValue published_receiver =
            semantic_param_publication_value(ctx, receiver);
        if (published_receiver.ok && !published_receiver.meta
            && type_is_constructed_named(
                published_receiver.type, "Array"))
            element = type_get_constructed_arg(
                published_receiver.type, 0);
    }
    FunctionParamPublicationStore *publication = ctx != NULL
        ? ctx->function_param_publications : NULL;
    PublishedNode *selected = publication != NULL && call != NULL
        ? publication_node(publication, call, false) : NULL;
    uint32_t binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, receiver) : 0;
    uint32_t effect_kind = 0, effect_receiver = 0, effect_source = 0;
    ASTNode *callee = call != NULL ? ast_call_callee(call) : NULL;
    SemanticPublishedValue published_value =
        semantic_param_publication_value(ctx, value);
    uint32_t current_producer = 0;
    bool current_owned = ctx != NULL && value != NULL
        && semantic_collection_owned_string_call_result(
            value, ctx, &current_producer);
    bool cycle_withdrawn = ctx != NULL && value != NULL &&
        semantic_collection_owned_string_result_cycle_withdrawn_current(
            value, ctx);
    bool effect_present = call != NULL
        && ast_call_semantic_collection_effect(
            call, &effect_kind, &effect_receiver, &effect_source);
    bool common_ready = !(ctx == NULL || call == NULL
        || call->type != AST_CALL
        || ast_call_arg_count(call) != 2 || ast_node_stable_id(call) == 0
        || !type_equals(element, TYPE_STRING)
        || receiver == NULL || receiver->type != AST_IDENTIFIER
        || binding == 0 || value == NULL || value->type != AST_CALL
        || selected == NULL || selected->caller == NULL
        || selected->caller_id == 0
        || selected->caller_id != ast_node_stable_id(selected->caller)
        || !semantic_param_publication_checked_call(ctx, call)
        || !semantic_param_publication_checked_argument(ctx, call, 0)
        || !semantic_param_publication_checked_argument(ctx, call, 1)
        || !published_value.ok || published_value.meta
        || !type_equals(published_value.type, TYPE_STRING)
        || callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(callee) != 0
        || ast_identifier_name(callee) == NULL
        || strcmp(ast_identifier_name(callee), "ArrayPush") != 0);
    if (!common_ready
        || (effect_present
            && (effect_kind != PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH
                || effect_receiver != binding || effect_source == 0))
        || current_owned
        || current_producer != 0
        || cycle_withdrawn) {
        return false;
    }
    /* An absent effect is deliberate for a populated or mixed receiver: its
     * whole-array owner cannot promise uniform element ownership.  The exact
     * pending result receipt still proves where a value copy can stand.  Ask
     * the existing store-kind owner for the plan instead of manufacturing an
     * owned receiver effect or duplicating that policy here. */
    bool recorded = store_landing_record_for_routine(ctx, call,
        selected->caller, store_landing_value_formal_id(value, ctx));
    return recorded;
}

bool semantic_formal_string_store_landing_select_empty_local_receiver(
    SemanticContext *ctx, const ASTNode *receiver,
    const ArrayStorageReturnElementReceipt *receipt)
{
    FunctionParamPublicationStore *publication = ctx != NULL
        ? ctx->function_param_publications : NULL;
    PublishedNode *receiver_row = receiver != NULL && publication != NULL
        ? publication_node(publication, receiver, false) : NULL;
    const ASTNode *routine = receiver_row != NULL
        ? receiver_row->caller : NULL;
    uint32_t binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, receiver) : 0;
    ASTNode *seed = NULL;
    uint32_t seed_id = UINT32_MAX;

    if (ctx == NULL || receiver == NULL
        || receiver->type != AST_IDENTIFIER || binding == 0
        || publication == NULL
        || !semantic_param_publication_inputs_ready(ctx)
        || receiver_row == NULL || receiver_row->node != receiver
        || receiver_row->id == 0
        || receiver_row->id != ast_node_stable_id(receiver)
        || !receiver_row->value_present || receiver_row->meta
        || routine == NULL || routine->type != AST_FUNC_DECL
        || receiver_row->caller_id == 0
        || receiver_row->caller_id != ast_node_stable_id(routine)
        || receipt == NULL || receipt->function_id == 0
        || receipt->function_id != receiver_row->caller_id
        || receipt->binding_id == 0 || receipt->binding_id != binding
        || receipt->empty_local_initializer == NULL
        || receipt->empty_local_initializer_id == 0
        || receipt->empty_local_initializer_id
            != ast_node_stable_id(receipt->empty_local_initializer))
        return false;
    PublishedNode *binding_row = publication_node_by_id(publication, binding);
    const ASTNode *declaration = binding_row != NULL
        ? binding_row->node : NULL;
    const ASTNode *initializer = declaration != NULL
        && declaration->type == AST_LET_DECL
        ? ast_let_initializer(declaration) : NULL;
    if (binding_row == NULL || binding_row->id != binding
        || declaration == NULL || ast_node_stable_id(declaration) != binding
        || ast_let_is_alias(declaration)
        || initializer != receipt->empty_local_initializer
        || initializer->type != AST_ARRAY_LITERAL
        || ast_array_literal_count(initializer) != 0)
        return false;

    for (size_t i = 0; i < publication->node_capacity; i++) {
        PublishedNode *candidate_row = &publication->nodes[i];
        ASTNode *candidate = (ASTNode *)candidate_row->node;
        if (candidate_row->id == 0 || candidate == NULL
            || candidate->type != AST_CALL
            || candidate_row != publication_result_current(ctx, candidate)
            || candidate_row->caller != routine
            || candidate_row->caller_id != receiver_row->caller_id
            || !candidate_row->execution_present || !candidate_row->reachable
            || !collection_call_is_unshadowed_builtin(candidate, "ArrayPush")
            || ast_call_arg_count(candidate) != 2
            || !semantic_param_publication_checked_call(ctx, candidate)
            || !semantic_param_publication_checked_argument(ctx, candidate, 0)
            || !semantic_param_publication_checked_argument(ctx, candidate, 1))
            continue;
        ASTNode *candidate_receiver = ast_call_argument(candidate, 0);
        uint32_t kind = 0, effect_receiver = 0, effect_source = 0;
        if (candidate_receiver == NULL
            || candidate_receiver->type != AST_IDENTIFIER
            || semantic_param_publication_binding(
                ctx, candidate_receiver) != binding
            || !ast_call_semantic_collection_effect(candidate, &kind,
                &effect_receiver, &effect_source)
            || kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION
            || effect_receiver != binding || effect_source != 0)
            continue;
        uint32_t candidate_id = ast_node_stable_id(candidate);
        if (candidate_id != 0 && candidate_id < seed_id) {
            seed = candidate;
            seed_id = candidate_id;
        }
    }
    return seed != NULL
        && semantic_formal_string_store_landing_select_owned(ctx, seed);
}

bool semantic_formal_string_store_landing_select_value_copy(
    SemanticContext *ctx, const ASTNode *call)
{
    if (ctx == NULL)
        return false;
    const Type *element = call != NULL && ast_call_arg_count(call) == 2
        ? store_landing_receiver_element(ast_call_argument(call, 0), ctx)
        : NULL;
    if (!type_equals(element, TYPE_STRING) && call != NULL
        && ast_call_arg_count(call) == 2) {
        SemanticPublishedValue receiver = semantic_param_publication_value(
            ctx, ast_call_argument(call, 0));
        if (receiver.ok && !receiver.meta
            && type_is_constructed_named(receiver.type, "Array"))
            element = type_get_constructed_arg(receiver.type, 0);
    }
    if (call == NULL || ast_call_arg_count(call) != 2
        || ast_node_stable_id(call) == 0
        || !type_equals(element, TYPE_STRING))
        return false;
    return store_landing_record_for_routine_kind(ctx, call,
        ctx->current_function_decl,
        store_landing_value_formal_id(ast_call_argument(call, 1), ctx),
        PGY_STRING_STORE_LANDING_VALUE_COPY);
}

bool semantic_formal_string_store_landing_select(SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, const ASTNode *root, const ASTNode *routine)
{
    /* Only a String a store builtin keeps, rooted at a by-value or `ref`
     * formal of the routine whose summary records the escape. */
    size_t value_ordinal = 0;
    if (root == NULL && call != NULL && ordinal < ast_call_arg_count(call)) {
        root = ast_call_argument(call, ordinal);
        while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
        if (root != NULL && root->type != AST_IDENTIFIER) root = NULL;
    }
    PublishedNode *row = publication_result_current(ctx, call);
    SemanticPublishedValue value = semantic_param_publication_value(ctx, ast_call_argument(call, ordinal));
    if (row == NULL || root == NULL || routine == NULL || routine->type != AST_FUNC_DECL ||
        !value.ok || value.meta || !type_equals(value.type, TYPE_STRING) || ast_node_stable_id(call) == 0)
        return true;
    if (!semantic_param_publication_checked_array_store(ctx, call, &value_ordinal)) {
        if (row->target_name == NULL || strcmp(row->target_name, "MapSet") != 0 || !row->stdlib ||
            row->argument_count != 3) return true;
        value_ordinal = 2;
    }
    if (ordinal != value_ordinal) return true;
    uint32_t formal_id = 0;
    for (size_t p = 0; p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL && ast_func_param_stable_id(param) == ast_identifier_binding_syntax_id(root) &&
            (param->mode == PARAM_MODE_DEFAULT || param->mode == PARAM_MODE_REF)) {
            formal_id = ast_func_param_stable_id(param);
        }
    }
    if (formal_id != 0) {
        return store_landing_record_for_routine(
            ctx, call, routine, formal_id);
    }
    return true;
}
