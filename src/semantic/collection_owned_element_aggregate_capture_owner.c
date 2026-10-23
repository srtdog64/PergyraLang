/* Captures default-aggregate call and return facts while their typed
 * identities are current.  The member-requirement owner later validates the
 * complete definition/return graph for the exact required field path. */
#include "collection_owned_element_requirement_internal.h"

#include "callable_capability_inference.h"
#include "default_aggregate_move_rule.h"
#include "type_checker_internal.h"
#include "../parser/ast_api.h"

bool
collection_owned_record_default_aggregate_actual(
    ASTNode *call, ASTNode *callee_decl,
    size_t argument_index, size_t formal_index,
    ASTNode *argument, const Type *argument_type,
    ParamMode parameter_mode, SemanticContext *ctx)
{
    if (ctx == NULL || call == NULL || call->type != AST_CALL
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || argument == NULL || argument_type == NULL
        || parameter_mode == PARAM_MODE_REF
        || !semantic_default_aggregate_value(ctx, argument_type)
        || type_is_constructed_named(argument_type, "Array"))
        return true;
    CollectionOwnedElementRequirementStore *store =
        ctx->collection_owned_element_requirements;
    ASTNode *caller = (ASTNode *)callable_capability_current_declaration(ctx);
    bool caller_is_intent = caller != NULL
        && caller->type == AST_INTENT_DECL;
    bool caller_is_routine = caller != NULL
        && caller->type == AST_FUNC_DECL;
    FuncParam *formal = formal_index < ast_func_param_count(callee_decl)
        ? ast_func_param(callee_decl, formal_index) : NULL;
    if (store == NULL || store->program != ctx->program_root
        || (!caller_is_routine && !caller_is_intent)
        || (caller_is_routine
            && (ctx->current_function_decl != caller
                || !ctx->tracking_function_effects))
        || (caller_is_intent
            && (ctx->current_function_decl != NULL
                || !ctx->tracking_function_effects))
        || argument_index >= ast_call_arg_count(call)
        || ast_call_argument(call, argument_index) != argument
        || formal == NULL || formal->mode != parameter_mode
        || ast_node_stable_id(call) == 0
        || ast_node_stable_id(callee_decl) == 0
        || ast_node_stable_id(caller) == 0
        || ast_node_stable_id(argument) == 0
        || ast_func_param_stable_id(formal) == 0)
        return collection_owned_requirement_failure(ctx, call,
            "missing default aggregate call identity");
    for (size_t i = 0; i < store->aggregate_actual_count; i++) {
        OwnedElementAggregateActual *row = &store->aggregate_actuals[i];
        if (row->call == call && row->formal_ordinal == formal_index)
            return row->callee == callee_decl && row->caller == caller
                && row->argument == argument
                && row->argument_ordinal == argument_index
                && row->formal == formal
                && row->formal_mode == parameter_mode
                && row->caller_is_intent == caller_is_intent
                && type_equals(row->argument_type, argument_type);
    }
    OwnedElementAggregateActual *grown =
        collection_owned_requirement_rows_reserve(
            store->aggregate_actuals, &store->aggregate_actual_capacity,
            store->aggregate_actual_count + 1,
            sizeof(*store->aggregate_actuals));
    if (grown == NULL)
        return collection_owned_requirement_failure(ctx, call,
            "default aggregate call allocation failed");
    store->aggregate_actuals = grown;
    store->aggregate_actuals[store->aggregate_actual_count++] =
        (OwnedElementAggregateActual){
            .call = call, .callee = callee_decl, .caller = caller,
            .argument = argument, .formal = formal,
            .call_id = ast_node_stable_id(call),
            .callee_id = ast_node_stable_id(callee_decl),
            .caller_id = ast_node_stable_id(caller),
            .argument_id = ast_node_stable_id(argument),
            .formal_id = ast_func_param_stable_id(formal),
            .argument_ordinal = argument_index,
            .formal_ordinal = formal_index,
            .formal_mode = parameter_mode,
            .caller_is_intent = caller_is_intent,
            .argument_type = argument_type};
    return true;
}

bool
semantic_collection_owned_element_requirement_record_return(
    ASTNode *statement, ASTNode *value, const Type *value_type,
    SemanticContext *ctx)
{
    if (ctx == NULL || value == NULL || value_type == NULL
        || value_type == TYPE_UNKNOWN
        || !semantic_default_aggregate_value(ctx, value_type)
        || type_is_constructed_named(value_type, "Array"))
        return true;
    CollectionOwnedElementRequirementStore *store =
        ctx->collection_owned_element_requirements;
    ASTNode *function = ctx->current_function_decl;
    uint32_t value_binding_id = 0;
    if (value->type == AST_IDENTIFIER) {
        Symbol *binding = scope_lookup(ctx->scope, ast_identifier_name(value));
        uint32_t annotated = ast_identifier_binding_syntax_id(value);
        if ((binding == NULL && annotated != 0)
            || (binding != NULL && (binding->decl_syntax_id == 0
                || (annotated != 0
                    && annotated != binding->decl_syntax_id))))
            return collection_owned_requirement_failure(ctx, value,
                "default aggregate return binding identity is unavailable");
        if (binding != NULL)
            value_binding_id = binding->decl_syntax_id;
    }
    if (store == NULL || store->program != ctx->program_root
        || statement == NULL || statement->type != AST_RETURN
        || ast_return_value(statement) != value
        || function == NULL || function->type != AST_FUNC_DECL
        || ast_node_stable_id(statement) == 0
        || ast_node_stable_id(value) == 0
        || ast_node_stable_id(function) == 0)
        return collection_owned_requirement_failure(ctx, statement,
            "missing default aggregate return identity");
    for (size_t i = 0; i < store->aggregate_return_count; i++) {
        OwnedElementAggregateReturn *row = &store->aggregate_returns[i];
        if (row->statement == statement)
            return row->value == value && row->function == function
                && row->value_binding_id == value_binding_id
                && type_equals(row->value_type, value_type);
    }
    OwnedElementAggregateReturn *grown =
        collection_owned_requirement_rows_reserve(
            store->aggregate_returns, &store->aggregate_return_capacity,
            store->aggregate_return_count + 1,
            sizeof(*store->aggregate_returns));
    if (grown == NULL)
        return collection_owned_requirement_failure(ctx, statement,
            "default aggregate return allocation failed");
    store->aggregate_returns = grown;
    store->aggregate_returns[store->aggregate_return_count++] =
        (OwnedElementAggregateReturn){
            .statement = statement, .value = value, .function = function,
            .statement_id = ast_node_stable_id(statement),
            .value_id = ast_node_stable_id(value),
            .value_binding_id = value_binding_id,
            .function_id = ast_node_stable_id(function),
            .value_type = value_type};
    return true;
}
