/* Recording half of the callable capability equation.
 * Walks declarations, bindings, returns and calls as the checker visits a
 * body and writes the equation rows the solver later seals. */
#include "callable_capability_equations_internal.h"
#include "capability_analyze.h"
#include "type_checker_internal.h"
#include "diag_codes.h"

#include <stdlib.h>
#include <string.h>

void *
callable_capability_allocate_equation(SemanticContext *ctx, size_t bytes)
{
    void *result = pgy_arena_calloc(&ctx->scratch_arena, bytes);
    if (result == NULL) {
        if (ctx->callable_capabilities != NULL)
            ctx->callable_capabilities->failed = true;
        semantic_error(ctx, ctx->current_function_decl, "Could not allocate callable capability facts");
    }
    return result;
}

static CapabilityTarget
capability_symbol_target(Symbol *symbol)
{
    CapabilityTarget value = {0};
    if (symbol == NULL || symbol->type == NULL
        || symbol->type->kind != TYPE_KIND_FUNCTION || symbol->decl_syntax_id == 0)
        return value;
    value.id = symbol->decl_syntax_id;
    value.kind = symbol->kind == SYMBOL_ENUM_CONSTRUCTOR ? CAP_CONSTRUCTOR
        : symbol->kind == SYMBOL_FUNCTION || symbol->kind == SYMBOL_INTENT
        ? CAP_DECL : symbol->is_parameter ? CAP_FORMAL : CAP_BINDING;
    return value;
}

static CapabilityTarget
capability_expression_target(SemanticContext *ctx, ASTNode *expression)
{
    if (expression != NULL && expression->type == AST_IDENTIFIER)
        return capability_symbol_target(scope_lookup(ctx->scope, ast_identifier_name(expression)));
    if (expression != NULL && expression->type == AST_LAMBDA_EXPR)
        return (CapabilityTarget){CAP_DECL, ast_node_stable_id(expression)};
    if (expression != NULL && expression->type == AST_CALL)
        return (CapabilityTarget){CAP_RESULT, ast_node_stable_id(expression)};
    return (CapabilityTarget){0};
}

CallableCapabilityRoutine *
callable_capability_enter(SemanticContext *ctx, ASTNode *decl, Type **params, size_t count)
{
    CallableCapabilityRoutine *previous = ctx->current_callable_capability;
    if (ctx->callable_capabilities == NULL) {
        ctx->callable_capabilities = callable_capability_allocate_equation(ctx,
            sizeof(*ctx->callable_capabilities));
        if (ctx->callable_capabilities == NULL)
            return previous;
    }
    CallableCapabilityRoutine *routine = callable_capability_allocate_equation(ctx, sizeof(*routine));
    if (routine == NULL)
        return previous;
    routine->decl = decl;
    routine->id = ast_node_stable_id(decl);
    routine->declared_mask = decl->type == AST_FUNC_DECL
        ? ast_func_declared_capabilities(decl) : 0;
    routine->formals = callable_capability_allocate_equation(ctx, (count + 1) * sizeof(uint32_t));
    if (routine->formals == NULL)
        return previous;
    for (size_t i = 0; i < count; i++) {
        if (params[i] == NULL || params[i]->kind != TYPE_KIND_FUNCTION)
            continue;
        uint32_t id = 0;
        if (decl->type == AST_LAMBDA_EXPR)
            id = ast_node_stable_id(ast_lambda_param(decl, i));
        else if (decl->type == AST_EVENT_DECL)
            id = ast_node_stable_id(ast_event_param(decl, i));
        else if (decl->type == AST_INTENT_DECL) {
            size_t bindings = ast_intent_decl_binding_count(decl);
            size_t involves = ast_intent_decl_involve_count(decl);
            ASTNode *binding = bindings > 0
                ? ast_intent_decl_bindings(decl, NULL)[i]
                : i < involves ? ast_intent_decl_involves(decl, NULL)[i]
                : ast_intent_decl_values(decl, NULL)[i - involves];
            id = ast_node_stable_id(binding);
        }
        else if (decl->type == AST_FUNC_DECL)
            id = ast_func_param_stable_id(ast_func_param(decl, i));
        routine->formals[routine->count++] = id;
    }
    routine->next = ctx->callable_capabilities->routines;
    ctx->callable_capabilities->routines = routine;
    ctx->callable_capabilities->count++;
    ctx->callable_capabilities->equation_count += count + 1;
    ctx->current_callable_capability = routine;
    return previous;
}

void
callable_capability_program_begin(SemanticContext *ctx, ASTNode *program)
{
    (void)callable_capability_enter(ctx, program, NULL, 0);
}

void
callable_capability_set_effect_contract(SemanticContext *ctx,
    uint32_t declared_effects, bool has_contract)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) return;
    routine->declared_effects = declared_effects;
    routine->has_effect_contract = has_contract;
}

void
callable_capability_leave(SemanticContext *ctx,
    CallableCapabilityRoutine *previous, Type *type, uint32_t direct_mask,
    uint32_t direct_effects)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine != NULL && routine != previous) {
        routine->type = type;
        routine->direct_mask = direct_mask;
        routine->direct_effects = direct_effects;
        type_function_set_capabilities(type, direct_mask | routine->declared_mask);
        /* Existing branch/parallel diagnostics may observe the partial type.
         * It never feeds the fixed point: only direct effects and call edges do. */
        type_function_set_effects(type,
            type_effect_mask_join(ctx->current_function_effects, routine->declared_effects));
        /* Fragment checking has no program seal and publishes no executable.
         * Preserve its direct contract diagnostics through the same policy. */
        if (ctx->program_root == NULL && routine->decl->type == AST_FUNC_DECL)
            type_check_function_effect_contract(routine->decl, ctx,
                routine->declared_effects, routine->has_effect_contract,
                ctx->current_function_effects);
    }
    ctx->current_callable_capability = previous;
}

void
callable_capability_record_binding(SemanticContext *ctx,
                                   Symbol *symbol, ASTNode *value)
{
    if (symbol == NULL || symbol->type == NULL
        || symbol->type->kind != TYPE_KIND_FUNCTION
        || ctx->callable_capabilities == NULL)
        return;
    CapabilityBinding *binding = callable_capability_allocate_equation(ctx, sizeof(*binding));
    if (binding == NULL)
        return;
    binding->id = symbol->decl_syntax_id;
    binding->value = capability_expression_target(ctx, value);
    binding->next = ctx->callable_capabilities->bindings;
    ctx->callable_capabilities->bindings = binding;
    ctx->callable_capabilities->equation_count++;
}

void
callable_capability_record_return(SemanticContext *ctx, ASTNode *value)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) return;
    CapabilityTarget target = capability_expression_target(ctx, value);
    if (!routine->has_result) routine->result = target;
    else if (target.kind != routine->result.kind || target.id != routine->result.id)
        routine->result = (CapabilityTarget){0};
    if (target.kind == CAP_UNKNOWN) {
        routine->result_unknown = true;
    } else {
        CapabilityResultTarget *seen = routine->results;
        while (seen != NULL && (seen->target.kind != target.kind
               || seen->target.id != target.id))
            seen = seen->next;
        if (seen == NULL) {
            CapabilityResultTarget *entry =
                callable_capability_allocate_equation(ctx, sizeof(*entry));
            if (entry == NULL) return;
            entry->target = target;
            entry->next = routine->results;
            routine->results = entry;
        }
    }
    routine->has_result = true;
}

void
callable_capability_invalidate_binding(SemanticContext *ctx, Symbol *symbol)
{
    if (ctx->callable_capabilities == NULL || symbol == NULL)
        return;
    for (CapabilityBinding *b = ctx->callable_capabilities->bindings; b; b = b->next)
        if (b->id == symbol->decl_syntax_id)
            b->value = (CapabilityTarget){0};
}

void
callable_capability_record_call(SemanticContext *ctx, ASTNode *site,
                               Symbol *callee, Type **actual_types)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) {
        /* A standalone expression query has no executable routine boundary. */
        if (callee != NULL)
            semantic_record_capability(ctx, type_function_capabilities(callee->type));
        return;
    }
    CapabilityCall *call = callable_capability_allocate_equation(ctx, sizeof(*call));
    bool event_invoke = site->type == AST_EVENT_INVOKE;
    size_t count = event_invoke ? ast_event_invoke_arg_count(site)
                               : ast_call_arg_count(site);
    if (call == NULL)
        return;
    call->site = site;
    call->callee = capability_symbol_target(callee);
    call->actuals = callable_capability_allocate_equation(ctx, (count + 1) * sizeof(CapabilityTarget));
    if (call->actuals == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        Type *param = type_function_param_type(callee != NULL ? callee->type : NULL, i);
        if (param == NULL || param->kind != TYPE_KIND_FUNCTION)
            continue;
        call->actuals[call->count++] = actual_types == NULL
            || (actual_types[i] != NULL && actual_types[i]->kind == TYPE_KIND_FUNCTION)
            ? capability_expression_target(ctx, event_invoke
                ? ast_event_invoke_argument(site, i) : ast_call_argument(site, i))
            : (CapabilityTarget){0};
    }
    call->next = routine->calls;
    routine->calls = call;
    ctx->callable_capabilities->equation_count++;
}

void
callable_capability_record_method_call(SemanticContext *ctx, ASTNode *site,
                                       Type **params, Type **actual_types)
{
    CallableCapabilityRoutine *routine = ctx->current_callable_capability;
    if (routine == NULL) return;
    CapabilityCall *call = callable_capability_allocate_equation(ctx, sizeof(*call));
    size_t count = ast_call_arg_count(site);
    if (call == NULL) return;
    call->site = site;
    call->callee = (CapabilityTarget){CAP_DECL,
        ast_call_semantic_callee_decl_id(site)};
    call->actuals = callable_capability_allocate_equation(ctx, (count + 1) * sizeof(CapabilityTarget));
    if (call->actuals == NULL) return;
    for (size_t p = 0; p < count; p++)
        if (params[p] != NULL && params[p]->kind == TYPE_KIND_FUNCTION)
            call->actuals[call->count++] = actual_types[p] != NULL
                ? capability_expression_target(ctx, ast_call_argument(site, p))
                : (CapabilityTarget){0};
    call->next = routine->calls;
    routine->calls = call;
    ctx->callable_capabilities->equation_count++;
}

void
callable_capability_record_subscription(SemanticContext *ctx,
                                        ASTNode *event, ASTNode *handler)
{
    if (ctx->callable_capabilities == NULL) return;
    CapabilityTarget target = capability_expression_target(ctx, event);
    CapabilityCall *call = callable_capability_allocate_equation(ctx, sizeof(*call));
    if (call == NULL) return;
    call->site = handler;
    call->event_id = target.kind == CAP_DECL ? target.id : 0;
    call->callee = capability_expression_target(ctx, handler);
    call->next = ctx->callable_capabilities->subscriptions;
    ctx->callable_capabilities->subscriptions = call;
    ctx->callable_capabilities->equation_count++;
}
