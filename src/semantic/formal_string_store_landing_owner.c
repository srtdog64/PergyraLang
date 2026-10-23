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
    if (store_landing_value_landable(value, ctx) && ast_node_stable_id(call) != 0)
        store_landing_record_for_routine(ctx, call,
            ctx->current_function_decl,
            store_landing_value_formal_id(value, ctx));
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

static bool store_landing_batch_candidate(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, ASTNode *routine,
    ASTNode *candidate, uint32_t *formal_id_out)
{
    if (formal_id_out == NULL || candidate == NULL
        || candidate->type != AST_CALL
        || ast_call_arg_count(candidate) != 3
        || !ast_call_semantic_callee_is_stdlib(candidate)
        || ast_call_semantic_callee_declared_callable(candidate)
        || ast_call_semantic_callee_decl_id(candidate) != 0
        || ast_call_semantic_callee_value_binding_id(candidate) != 0)
        return false;
    ASTNode *callee = ast_call_callee(candidate);
    ASTNode *receiver = ast_call_argument(candidate, 0);
    ASTNode *value = ast_call_argument(candidate, 2);
    if (callee == NULL || callee->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(callee) != 0
        || ast_identifier_name(callee) == NULL
        || strcmp(ast_identifier_name(callee), "ArraySet") != 0
        || receiver == NULL || receiver->type != AST_IDENTIFIER)
        return false;
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
    if (root == NULL || root->type != AST_IDENTIFIER) return false;
    uint32_t formal_id = ast_identifier_binding_syntax_id(root);
    const Type *formal_path_type = store_landing_published_formal_path_type(
        ctx, published_routine, value, formal_id);
    if (formal_id == 0 || !type_equals(formal_path_type, TYPE_STRING))
        return false;
    *formal_id_out = formal_id;
    return true;
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
    size_t seed_root = SIZE_MAX;
    for (size_t r = 0; r < index->dependency_root_count; r++)
        if (ast_node_stable_id(index->dependency_roots[r]) == seed_id) {
            if (seed_root != SIZE_MAX) return true;
            seed_root = r;
        }
    if (seed_root == SIZE_MAX) return true;
    /* Only a contiguous run is replayed.  The risk-owner receipt was issued
     * at the first failed store and remains live across these shallow stores.
     * Each value must independently resolve to a default/ref formal String
     * path; the formal need not be the seed's because the same live owned
     * Array<String> makes every such unknown-source value require a copy.
     * Crossing another statement would require a fresh program-point
     * ownership state rather than this bounded snapshot. */
    for (size_t r = seed_root + 1; r < index->dependency_root_count; r++) {
        ASTNode *candidate = index->dependency_roots[r];
        uint32_t candidate_formal_id = 0;
        if (!store_landing_batch_candidate(
                ctx, index->claims, routine, candidate,
                &candidate_formal_id))
            break;
        if (!store_landing_record_for_routine(
                ctx, candidate, routine, candidate_formal_id)) return false;
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
        if (routine_id == 0 || formal_id == 0
            || !store_landing_batch_exists(ctx, routine_id, formal_id))
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
