/* Role-admitted implementation edges for the callable capability equation owner. */
#include "callable_capability_equations_internal.h"
#include "type_checker_internal.h"

void
callable_capability_record_abstract_method(SemanticContext *ctx,
    ASTNode *method, Type **params, size_t count, Type *result)
{
    CallableCapabilityRoutine *previous = callable_capability_enter(ctx, method, params, count);
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL || routine == previous) return;
    routine->abstract_dispatch = true;
    routine->declared_effects = declared_effects_from_function_node(method, ctx,
        &routine->has_effect_contract);
    Type *signature = type_create_function(params, count, result);
    if (signature == NULL) semantic_error(ctx, method, "Could not allocate abstract callable capability signature");
    callable_capability_leave(ctx, previous, signature, 0, 0);
}

void
callable_capability_record_implementation(SemanticContext *ctx,
    ASTNode *signature, ASTNode *implementation)
{
    if (ctx->callable_capabilities == NULL) return;
    CapabilityDispatch *edge = callable_capability_allocate_equation(ctx, sizeof(*edge));
    if (edge == NULL) return;
    edge->signature_id = ast_node_stable_id(signature);
    edge->implementation_id = ast_node_stable_id(implementation);
    edge->site = implementation;
    edge->next = ctx->callable_capabilities->dispatches;
    ctx->callable_capabilities->dispatches = edge;
    ctx->callable_capabilities->equation_count++;
}

bool
callable_capability_seal_dispatch(SemanticContext *ctx)
{
    struct CallableCapabilityStore *store = ctx->callable_capabilities;
    /* A dynamic ability call admits every implementation recorded by its role
     * owner. Propagate their real equations; never assume an abstract body is
     * pure, or choose the currently bound role by scanning source order. */
    for (CapabilityDispatch *edge = store->dispatches; edge; edge = edge->next) {
        CallableCapabilityRoutine *signature = callable_capability_find_routine(store, edge->signature_id);
        CallableCapabilityRoutine *implementation = callable_capability_find_routine(store, edge->implementation_id);
        if (signature == NULL || implementation == NULL || signature->count != implementation->count) {
            semantic_error(ctx, edge->site, "Ability implementation capability signature identity is missing or incompatible");
            return false;
        }
        CapabilityCall *call = callable_capability_allocate_equation(ctx, sizeof(*call));
        if (call == NULL) return false;
        call->site = edge->site;
        call->callee = (CapabilityTarget){CAP_DECL, implementation->id};
        call->count = signature->count;
        call->actuals = callable_capability_allocate_equation(ctx, (call->count + 1) * sizeof(CapabilityTarget));
        if (call->actuals == NULL) return false;
        for (size_t p = 0; p < call->count; p++)
            call->actuals[p] = (CapabilityTarget){CAP_FORMAL, signature->formals[p]};
        call->next = signature->calls;
        signature->calls = call;
    }
    store->dispatches = NULL;
    return true;
}
