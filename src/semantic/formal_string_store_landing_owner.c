/* A formal's String stored into an `inout` output lands as an owned copy. */
#include "formal_string_store_landing_owner.h"
#include "type_checker_internal.h"
#include "type_checker_call_constructor_internal.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_summary_internal.h"
#include "collection_ownership_fact.h"
#include "loan_copy_landing_owner.h"
#include "../parser/ast_api.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool semantic_formal_string_store_landing_record_risk_owner(
    SemanticContext *ctx, const ASTNode *call, const ASTNode *value,
    uint32_t risk_owner_binding_id)
{
    ASTNode *routine = ctx == NULL ? NULL : ctx->current_function_decl;
    uint32_t routine_id = routine != NULL && routine->type == AST_FUNC_DECL
        ? ast_node_stable_id(routine) : 0;
    uint32_t formal_id = store_landing_value_formal_id(value, ctx);
    if (call == NULL || call->type != AST_CALL || ast_node_stable_id(call) == 0
        || routine_id == 0 || formal_id == 0 || risk_owner_binding_id == 0)
        return true;
    for (size_t i = 0; i < ctx->string_store_landing_risk_owner_count; i++) {
        const FormalStringStoreLandingRiskOwner *row =
            &ctx->string_store_landing_risk_owners[i];
        if (row->routine_id == routine_id && row->formal_id == formal_id
            && row->risk_owner_binding_id == risk_owner_binding_id)
            return true;
    }
    if (ctx->string_store_landing_risk_owner_count
        == ctx->string_store_landing_risk_owner_capacity) {
        size_t capacity = ctx->string_store_landing_risk_owner_capacity == 0
            ? 8 : ctx->string_store_landing_risk_owner_capacity * 2;
        if (capacity > SIZE_MAX / sizeof(*ctx->string_store_landing_risk_owners))
            return false;
        FormalStringStoreLandingRiskOwner *grown = realloc(
            ctx->string_store_landing_risk_owners,
            capacity * sizeof(*grown));
        if (grown == NULL) return false;
        ctx->string_store_landing_risk_owners = grown;
        ctx->string_store_landing_risk_owner_capacity = capacity;
    }
    ctx->string_store_landing_risk_owners[
        ctx->string_store_landing_risk_owner_count++] =
        (FormalStringStoreLandingRiskOwner){routine_id, formal_id,
            risk_owner_binding_id};
    return true;
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

bool semantic_formal_string_store_landing_owned_push(ASTNode *call, SemanticContext *ctx)
{
    return call != NULL && ctx != NULL && ast_call_arg_count(call) == 2 &&
        store_landing_planned(call, ctx) &&
        (store_landing_value_landable(ast_call_argument(call, 1), ctx) ||
         type_equals(store_landing_receiver_element(ast_call_argument(call, 0), ctx), TYPE_STRING)) &&
        semantic_collection_owned_string_push_admissible(ast_call_argument(call, 0), ctx);
}

bool semantic_formal_string_store_landing_owned_set(ASTNode *call,
    SemanticContext *ctx)
{
    return call != NULL && ctx != NULL && ast_call_arg_count(call) == 3
        && store_landing_planned(call, ctx)
        && type_equals(store_landing_receiver_element(
            ast_call_argument(call, 0), ctx), TYPE_STRING)
        && semantic_collection_owned_string_set_required(
            ast_call_argument(call, 0), ctx);
}

bool semantic_formal_string_store_landing(ASTNode *call, size_t receiver_ordinal,
    size_t value_ordinal, SemanticContext *ctx)
{
    size_t count = 0;
    ASTNode **args = call == NULL || ctx == NULL ? NULL : ast_call_arguments(call, &count);
    if (args == NULL || receiver_ordinal >= count || value_ordinal >= count) return true;
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
    if (!store_landing_planned(call, ctx) && !literal &&
        !(output && store_landing_value_landable(value, ctx)))
        return true;
    if (ctx->synthetic_stable_id_next == 0)
        ctx->synthetic_stable_id_next = ast_stable_id_ceiling(ctx->program_root);
    if (ctx->synthetic_stable_id_next == 0 || ctx->synthetic_stable_id_next > UINT32_MAX - 2) return false;
    ASTNode *callee = ast_create_identifier("Clone");
    ASTNode *clone = callee == NULL ? NULL : ast_create_call(callee);
    if (clone == NULL || !ast_call_set_compiler_builtin(clone)) return false;
    ast_add_argument(clone, value);
    if (ast_call_arg_count(clone) != 1) return false;
    callee->line = clone->line = value->line;
    callee->column = clone->column = value->column;
    callee->stable_id = ctx->synthetic_stable_id_next++;
    clone->stable_id = ctx->synthetic_stable_id_next++;
    args[value_ordinal] = clone;
    return true;
}

static bool store_landing_record_for_routine(SemanticContext *ctx,
    const ASTNode *node, const ASTNode *routine, uint32_t formal_id)
{
    uint32_t id = ast_node_stable_id(node);
    if (ctx == NULL || node == NULL || id == 0) return false;
    for (size_t i = 0; i < ctx->string_store_landing_selected_count; i++)
        if (ctx->string_store_landing_selected[i] == id) {
            uint32_t routine_id = routine != NULL && routine->type == AST_FUNC_DECL
                ? ast_node_stable_id(routine) : 0;
            if (ctx->string_store_landing_selected_routines[i] == 0)
                ctx->string_store_landing_selected_routines[i] = routine_id;
            if (ctx->string_store_landing_selected_formals[i] == 0)
                ctx->string_store_landing_selected_formals[i] = formal_id;
            return true;
        }
    for (size_t i = 0; i < ctx->string_store_landing_plan_count; i++)
        if (ctx->string_store_landing_plan[i] == id) return true;
    if (ctx->string_store_landing_selected_count == ctx->string_store_landing_selected_capacity) {
        size_t capacity = ctx->string_store_landing_selected_capacity == 0 ? 16
            : ctx->string_store_landing_selected_capacity * 2;
        if (capacity > SIZE_MAX / sizeof(uint32_t)) return false;
        uint32_t *ids = malloc(capacity * sizeof(*ids));
        uint32_t *routines = malloc(capacity * sizeof(*routines));
        uint32_t *formals = malloc(capacity * sizeof(*formals));
        if (ids == NULL || routines == NULL || formals == NULL) {
            free(ids);
            free(routines);
            free(formals);
            return false;
        }
        if (ctx->string_store_landing_selected_count != 0) {
            memcpy(ids, ctx->string_store_landing_selected,
                ctx->string_store_landing_selected_count * sizeof(*ids));
            memcpy(routines, ctx->string_store_landing_selected_routines,
                ctx->string_store_landing_selected_count * sizeof(*routines));
            memcpy(formals, ctx->string_store_landing_selected_formals,
                ctx->string_store_landing_selected_count * sizeof(*formals));
        }
        free(ctx->string_store_landing_selected);
        free(ctx->string_store_landing_selected_routines);
        free(ctx->string_store_landing_selected_formals);
        ctx->string_store_landing_selected = ids;
        ctx->string_store_landing_selected_routines = routines;
        ctx->string_store_landing_selected_formals = formals;
        ctx->string_store_landing_selected_capacity = capacity;
    }
    size_t at = ctx->string_store_landing_selected_count++;
    ctx->string_store_landing_selected[at] = id;
    ctx->string_store_landing_selected_routines[at] = routine != NULL
        && routine->type == AST_FUNC_DECL ? ast_node_stable_id(routine) : 0;
    ctx->string_store_landing_selected_formals[at] = formal_id;
    if (getenv("PGY_DEBUG_PIPELINE_STAGE") != NULL) {
        const char *routine_name = routine != NULL
            ? ast_declaration_name(routine) : NULL;
        fprintf(stderr,
            "[semantic store landing select] node=%u line=%d col=%d routine=%s routine_id=%u formal_id=%u\n",
            id, node->line, node->column,
            routine_name != NULL ? routine_name : "<none>",
            ctx->string_store_landing_selected_routines[at], formal_id);
    }
    return true;
}

static bool store_landing_record(SemanticContext *ctx, const ASTNode *node)
{
    return store_landing_record_for_routine(ctx, node,
        ctx != NULL ? ctx->current_function_decl : NULL, 0);
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
    return store_landing_record_for_routine(ctx, call,
        ctx->current_function_decl,
        store_landing_value_formal_id(ast_call_argument(call, 2), ctx));
}

void semantic_formal_store_landing_select_held(SemanticContext *ctx, const ASTNode *call, size_t value_ordinal)
{
    if (ctx == NULL || call == NULL || value_ordinal >= ast_call_arg_count(call) || ast_node_stable_id(call) == 0) return;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, ast_call_argument(call, value_ordinal));
    if (!value.ok || value.meta || publication_physical_scalar(value.type) ||
        !semantic_param_publication_clone_independent(ctx, value.type)) return;
    store_landing_record(ctx, call);
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

void semantic_formal_string_store_landing_select_checked(SemanticContext *ctx, ASTNode *call,
    size_t value_ordinal)
{
    ASTNode *value = call == NULL || value_ordinal >= ast_call_arg_count(call) ? NULL
        : ast_call_argument(call, value_ordinal);
    if (value == NULL || ast_node_stable_id(call) == 0) return;

    uint32_t formal_id = store_landing_value_formal_id(value, ctx);
    bool exact_local_string = false;
    if (formal_id == 0 && value->type == AST_IDENTIFIER
        && ast_identifier_name(value) != NULL) {
        Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
        exact_local_string = binding != NULL && !binding->is_parameter
            && !binding->is_host_field && binding->decl_syntax_id != 0
            && binding->decl_syntax_id
                == ast_identifier_binding_syntax_id(value)
            && type_equals(binding->type, TYPE_STRING);
    }
    /* The selected ArraySet is not admitted here.  A later analysis must
     * still prove that the exact receiver is an owned-String Array before it
     * rewrites the call to ArraySetOwnedString.  Recording an exact local
     * String therefore extends the existing copy boundary without treating
     * the local's borrowed descriptor as independently owned. */
    if (formal_id != 0 || exact_local_string)
        (void)store_landing_record_for_routine(ctx, call,
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

static bool store_landing_batch_exists(const SemanticContext *ctx,
    uint32_t routine_id, uint32_t formal_id)
{
    for (size_t i = 0; i < ctx->string_store_landing_risk_owner_count; i++)
        if (ctx->string_store_landing_risk_owners[i].routine_id == routine_id
            && ctx->string_store_landing_risk_owners[i].formal_id == formal_id)
            return true;
    return false;
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
    ASTNode *routine, uint32_t seed_id)
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

bool semantic_formal_string_store_landing_finalize(SemanticContext *ctx)
{
    if (ctx == NULL || !semantic_param_publication_inputs_ready(ctx)
        || ctx->function_param_flow_summaries == NULL) return false;
    size_t seed_count = ctx->string_store_landing_selected_count;
    for (size_t i = 0; i < seed_count; i++) {
        uint32_t routine_id = ctx->string_store_landing_selected_routines[i];
        uint32_t formal_id = ctx->string_store_landing_selected_formals[i];
        if (routine_id == 0
            || (formal_id != 0
                && !store_landing_batch_exists(ctx, routine_id, formal_id)))
            continue;
        PendingRoutine *routine_row = publication_routine(
            ctx->function_param_publications, routine_id);
        ASTNode *routine = routine_row == NULL ? NULL : routine_row->decl;
        if (routine == NULL || routine->type != AST_FUNC_DECL
            || ast_node_stable_id(routine) != routine_id)
            continue;
        if (!store_landing_select_contiguous_formals(ctx, routine,
                ctx->string_store_landing_selected[i])) return false;
    }
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
        return store_landing_record_for_routine(ctx, call,
            ctx->current_function_decl,
            store_landing_value_formal_id(ast_call_argument(call, 1), ctx));

    const ASTNode *receiver = ast_call_argument(call, 0);
    uint32_t binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? semantic_param_publication_binding(ctx, receiver) : 0;
    PublishedNode *selected = publication_node(ctx->function_param_publications,
        call, false);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (binding == 0 || selected == NULL || selected->caller == NULL ||
        !function_param_flow_program_point_index_build(
            ctx->function_param_flow_summaries, (ASTNode *)selected->caller, &index) ||
        index == NULL || !function_param_flow_index_claims_current(ctx, index)) return false;

    size_t selected_call = 0;
    for (size_t i = 0; i < index->claims->count; i++) {
        ASTNode *candidate = index->claims->rows[i].node;
        if (candidate == NULL || candidate->type != AST_CALL ||
            ast_call_arg_count(candidate) != 2) continue;
        ASTNode *callee = ast_call_callee(candidate);
        ASTNode *candidate_receiver = ast_call_argument(candidate, 0);
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
            !store_landing_record(ctx, candidate)) return false;
        if (candidate == call) selected_call++;
    }
    return selected_call == 1;
}

void semantic_formal_string_store_landing_select(SemanticContext *ctx, const ASTNode *call,
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
        !value.ok || value.meta || !type_equals(value.type, TYPE_STRING) || ast_node_stable_id(call) == 0) return;
    if (!semantic_param_publication_checked_array_store(ctx, call, &value_ordinal)) {
        if (row->target_name == NULL || strcmp(row->target_name, "MapSet") != 0 || !row->stdlib ||
            row->argument_count != 3) return;
        value_ordinal = 2;
    }
    if (ordinal != value_ordinal) return;
    uint32_t formal_id = 0;
    for (size_t p = 0; p < ast_func_param_count(routine); p++) {
        FuncParam *param = ast_func_param(routine, p);
        if (param != NULL && ast_func_param_stable_id(param) == ast_identifier_binding_syntax_id(root) &&
            (param->mode == PARAM_MODE_DEFAULT || param->mode == PARAM_MODE_REF)) {
            formal_id = ast_func_param_stable_id(param);
        }
    }
    if (formal_id != 0)
        (void)store_landing_record_for_routine(
            ctx, call, routine, formal_id);
}
