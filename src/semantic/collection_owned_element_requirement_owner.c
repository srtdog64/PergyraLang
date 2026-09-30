/* Exact owned String-element entry obligations for the native bootstrap.
 * Typed operations record seeds, forwarding edges and caller snapshots once.
 * Finalization after Pass 2 makes source order irrelevant without rescanning
 * callee bodies or turning unknown provenance into ownership permission. */
#include "collection_owned_element_requirement_owner.h"

#include <stdint.h>
#include <stdlib.h>

#include "collection_ownership_fact.h"
#include "diag_codes.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

typedef struct
{
    uint32_t function_id;
    uint32_t parameter_id;
    bool required;
} OwnedElementParameter;

typedef struct
{
    size_t source;
    size_t target;
} OwnedElementForwardingEdge;

typedef struct
{
    ASTNode *argument;
    size_t target;
    PgyStringArrayOwnership element_ownership;
} OwnedElementActualSnapshot;

struct CollectionOwnedElementRequirementStore
{
    ASTNode *program;
    OwnedElementParameter *parameters;
    size_t parameter_count;
    size_t parameter_capacity;
    OwnedElementForwardingEdge *edges;
    size_t edge_count;
    size_t edge_capacity;
    OwnedElementActualSnapshot *actuals;
    size_t actual_count;
    size_t actual_capacity;
};

static bool
requirement_failure(SemanticContext *ctx, ASTNode *site, const char *detail)
{
    if (ctx != NULL)
        semantic_error(ctx, site,
            "Owned String-element entry requirements failed closed: %s",
            detail);
    return false;
}

static bool
is_string_array(const Type *type)
{
    Type *inner;
    if (!type_is_constructed_named(type, "Array"))
        return false;
    inner = type_get_constructed_arg(type, 0);
    return inner != NULL && type_equals(inner, TYPE_STRING);
}

static void *
requirement_rows_reserve(void *rows, size_t *capacity,
                         size_t needed, size_t row_size)
{
    size_t next;
    void *grown;
    if (needed <= *capacity)
        return rows;
    next = *capacity == 0 ? 8 : *capacity;
    while (next < needed) {
        if (next > SIZE_MAX / 2)
            return NULL;
        next *= 2;
    }
    if (next > SIZE_MAX / row_size)
        return NULL;
    grown = realloc(rows, next * row_size);
    if (grown == NULL)
        return NULL;
    *capacity = next;
    return grown;
}

static bool
requirement_parameter_row(CollectionOwnedElementRequirementStore *store,
                           uint32_t function_id, uint32_t parameter_id,
                           size_t *row_out)
{
    OwnedElementParameter *grown;
    if (function_id == 0 || parameter_id == 0)
        return false;
    for (size_t i = 0; i < store->parameter_count; i++) {
        if (store->parameters[i].function_id == function_id
            && store->parameters[i].parameter_id == parameter_id) {
            *row_out = i;
            return true;
        }
    }
    if (store->parameter_count == SIZE_MAX)
        return false;
    grown = requirement_rows_reserve(store->parameters,
        &store->parameter_capacity, store->parameter_count + 1,
        sizeof(*store->parameters));
    if (grown == NULL)
        return false;
    store->parameters = grown;
    *row_out = store->parameter_count++;
    store->parameters[*row_out] = (OwnedElementParameter){
        function_id, parameter_id, false};
    return true;
}

static FuncParam *
own_formal_for_binding(ASTNode *function, uint32_t binding_id)
{
    if (function == NULL || function->type != AST_FUNC_DECL
        || binding_id == 0)
        return NULL;
    for (size_t i = 0; i < ast_func_param_count(function); i++) {
        FuncParam *parameter = ast_func_param(function, i);
        if (parameter != NULL && parameter->mode == PARAM_MODE_OWN
            && ast_func_param_stable_id(parameter) == binding_id)
            return parameter;
    }
    return NULL;
}

void
semantic_collection_owned_element_requirements_destroy(
    CollectionOwnedElementRequirementStore *store)
{
    if (store == NULL)
        return;
    free(store->parameters);
    free(store->edges);
    free(store->actuals);
    free(store);
}

bool
semantic_collection_owned_element_requirements_begin(
    ASTNode *program, SemanticContext *ctx)
{
    if (ctx == NULL || program == NULL || program->type != AST_PROGRAM
        || ast_node_stable_id(program) == 0)
        return requirement_failure(ctx, program, "missing program identity");
    semantic_collection_owned_element_requirements_destroy(
        ctx->collection_owned_element_requirements);
    ctx->collection_owned_element_requirements = calloc(1,
        sizeof(*ctx->collection_owned_element_requirements));
    if (ctx->collection_owned_element_requirements == NULL)
        return requirement_failure(ctx, program, "store allocation failed");
    ctx->collection_owned_element_requirements->program = program;
    return true;
}

bool
semantic_collection_owned_element_requirement_record_deep_drop(
    ASTNode *call, ASTNode *receiver, const Type *array_type,
    SemanticContext *ctx)
{
    Symbol *binding;
    uint32_t binding_id;
    size_t row;
    CollectionOwnedElementRequirementStore *store;

    /* Only the resolved stdlib deep-drop branch calls this entrypoint. A
     * same-spelling source function never reaches this semantic operation. */
    if (ctx == NULL || receiver == NULL
        || receiver->type != AST_IDENTIFIER || !is_string_array(array_type))
        return requirement_failure(ctx, call, "missing typed deep-drop receiver");
    binding = scope_lookup(ctx->scope, ast_identifier_name(receiver));
    if (binding == NULL)
        return requirement_failure(ctx, call, "missing deep-drop receiver binding");
    if (!binding->is_parameter
        || binding->param_mode != PARAM_MODE_OWN)
        return true;
    binding_id = ast_identifier_binding_syntax_id(receiver);
    store = ctx->collection_owned_element_requirements;
    if (store == NULL || store->program != ctx->program_root
        || call == NULL || call->type != AST_CALL
        || ast_node_stable_id(call) == 0
        || ast_call_semantic_callee_decl_id(call) != 0
        || ast_call_semantic_callee_declared_callable(call)
        || ast_call_semantic_callee_value_binding_id(call) != 0
        || binding_id == 0 || binding_id != binding->decl_syntax_id
        || own_formal_for_binding(ctx->current_function_decl, binding_id)
            == NULL)
        return requirement_failure(ctx, call, "missing deep-drop formal identity");
    if (!requirement_parameter_row(store,
            ast_node_stable_id(ctx->current_function_decl), binding_id, &row))
        return requirement_failure(ctx, call, "seed allocation or identity failed");
    store->parameters[row].required = true;
    return true;
}

bool
semantic_collection_owned_element_requirement_record_argument(
    ASTNode *call, ASTNode *callee_decl, size_t argument_index,
    ASTNode *argument, const Type *argument_type, ParamMode parameter_mode,
    SemanticContext *ctx)
{
    CollectionOwnedElementRequirementStore *store;
    FuncParam *target_parameter;
    uint32_t function_id;
    uint32_t caller_id;
    uint32_t binding_id = 0;
    size_t target;
    PgyStringArrayOwnership ownership = PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN;

    if (parameter_mode != PARAM_MODE_OWN || !is_string_array(argument_type))
        return true;
    store = ctx != NULL ? ctx->collection_owned_element_requirements : NULL;
    function_id = ast_node_stable_id(callee_decl);
    caller_id = ctx != NULL ? ast_node_stable_id(ctx->current_function_decl) : 0;
    if (store == NULL || store->program != ctx->program_root
        || call == NULL || call->type != AST_CALL || argument == NULL
        || ast_node_stable_id(call) == 0 || caller_id == 0
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || function_id == 0
        || ast_call_semantic_callee_decl_id(call) != function_id
        || !ast_call_semantic_callee_declared_callable(call)
        || argument_index >= ast_func_param_count(callee_decl))
        return requirement_failure(ctx, call, "missing owned-call declaration identity");
    target_parameter = ast_func_param(callee_decl, argument_index);
    if (target_parameter == NULL || target_parameter->mode != PARAM_MODE_OWN
        || ast_func_param_stable_id(target_parameter) == 0)
        return requirement_failure(ctx, call, "missing owned-call formal identity");
    /* Parameter placeholders may still be UNKNOWN for forward declarations.
     * Only an exact typed deep-drop seed will activate this target later. */
    if (!requirement_parameter_row(store, function_id,
            ast_func_param_stable_id(target_parameter), &target))
        return requirement_failure(ctx, call, "target allocation failed");
    if (argument->type == AST_IDENTIFIER) {
        Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(argument));
        const PgyCollectionOwnershipFact *fact;
        binding_id = ast_identifier_binding_syntax_id(argument);
        if (binding == NULL || binding_id == 0
            || binding_id != binding->decl_syntax_id)
            return requirement_failure(ctx, call, "missing actual binding identity");
        fact = semantic_collection_ownership_fact_find(ctx, caller_id, binding_id);
        if (fact != NULL) {
            ownership = fact->element_ownership;
        } else if (!binding->is_parameter) {
            return requirement_failure(ctx, call, "missing local collection fact");
        }
        if (binding->is_parameter && binding->param_mode == PARAM_MODE_OWN) {
            size_t source;
            OwnedElementForwardingEdge *grown;
            if (own_formal_for_binding(ctx->current_function_decl, binding_id)
                    == NULL
                || !requirement_parameter_row(store, caller_id, binding_id, &source))
                return requirement_failure(ctx, call, "forwarding source identity failed");
            if (store->edge_count == SIZE_MAX)
                return requirement_failure(ctx, call, "forwarding edge allocation failed");
            grown = requirement_rows_reserve(store->edges,
                &store->edge_capacity, store->edge_count + 1,
                sizeof(*store->edges));
            if (grown == NULL)
                return requirement_failure(ctx, call, "forwarding edge allocation failed");
            store->edges = grown;
            store->edges[store->edge_count++] =
                (OwnedElementForwardingEdge){source, target};
        }
    }
    if (store->actual_count == SIZE_MAX)
        return requirement_failure(ctx, call, "actual snapshot allocation failed");
    OwnedElementActualSnapshot *grown = requirement_rows_reserve(store->actuals,
        &store->actual_capacity, store->actual_count + 1,
        sizeof(*store->actuals));
    if (grown == NULL)
        return requirement_failure(ctx, call, "actual snapshot allocation failed");
    store->actuals = grown;
    store->actuals[store->actual_count++] = (OwnedElementActualSnapshot){
        argument, target, ownership};
    return true;
}

bool
semantic_collection_owned_element_requirements_finalize(SemanticContext *ctx)
{
    CollectionOwnedElementRequirementStore *store = ctx != NULL
        ? ctx->collection_owned_element_requirements : NULL;
    size_t remaining;
    bool changed = true;
    if (store == NULL || store->program != ctx->program_root)
        return requirement_failure(ctx, ctx != NULL ? ctx->program_root : NULL,
            "missing analyzed-program store");
    if (store->edge_count == SIZE_MAX)
        return requirement_failure(ctx, store->program, "forwarding bound overflow");
    remaining = store->edge_count + 1;
    while (changed && remaining > 0) {
        changed = false;
        for (size_t i = 0; i < store->edge_count; i++) {
            OwnedElementForwardingEdge edge = store->edges[i];
            if (store->parameters[edge.target].required
                && !store->parameters[edge.source].required) {
                store->parameters[edge.source].required = true;
                changed = true;
            }
        }
        remaining--;
    }
    if (changed)
        return requirement_failure(ctx, store->program, "forwarding did not converge");
    for (size_t i = 0; i < store->actual_count; i++) {
        OwnedElementActualSnapshot actual = store->actuals[i];
        OwnedElementParameter target = store->parameters[actual.target];
        /* UNKNOWN stays bootstrap debt. This ratchet grants no permission:
         * it only falsifies a known borrowed actual at a required boundary. */
        if (!target.required || actual.element_ownership !=
                PGY_STRING_ARRAY_BORROWED_ELEMENTS)
            continue;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            actual.argument,
            "Borrowed String elements cannot cross own parameter syntax %u of callable syntax %u.\n"
            "Reason:\n"
            "- the exact callee or its own-formal forwarding path deep-releases String elements\n"
            "- storage transfer does not transfer a borrowed element lifetime\n"
            "Fix:\n"
            "- Clone into a named owned snapshot before this call\n"
            "- or produce uniformly owned elements with ArrayPushOwnedString",
            target.parameter_id, target.function_id);
        return false;
    }
    return true;
}
