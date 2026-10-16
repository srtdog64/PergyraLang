/* One all-typed publication boundary. Pending rows own diagnostic strings,
 * borrow compilation-owned AST/Type identity, and never retain scope Symbols. */
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "type_checker_ownership_internal.h"
#include "slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "callable_capability_inference.h"
#include "region_retention_summary.h"
#include "indexed_string_borrow_owner.h"
#include "collection_ownership_fact.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
static bool publication_channel_shape(const PublishedNode *row)
{
    if (row == NULL || !row->channel_binding || row->node == NULL || row->node->type != AST_LET_DECL ||
        row->id == 0 || row->id != ast_node_stable_id(row->node) || ast_let_is_mutable(row->node) ||
        ast_let_is_alias(row->node) || row->target_name == NULL || ast_let_name(row->node) == NULL ||
        strcmp(row->target_name, ast_let_name(row->node)) != 0 ||
        row->channel_type_node == NULL || row->channel_type_node != ast_let_type(row->node) ||
        row->channel_type_id == 0 || row->channel_type_id != ast_node_stable_id(row->channel_type_node) ||
        row->value_shape != row->channel_type_node->type || row->value_type == NULL ||
        row->value_type->kind != TYPE_KIND_CONSTRUCTED || type_constructed_constructor(row->value_type) != TYPE_CHANNEL ||
        type_constructed_arg_count(row->value_type) != 1 || row->channel_element_type == NULL ||
        row->channel_element_type == TYPE_UNKNOWN || type_constructed_arg(row->value_type, 0) != row->channel_element_type ||
        row->value_type_name == NULL || row->value_type->name == NULL ||
        strcmp(row->value_type_name, row->value_type->name) != 0 ||
        row->target == NULL || row->target != ast_let_initializer(row->node) || row->target->type != AST_CALL ||
        row->target_id == 0 || row->target_id != ast_node_stable_id(row->target) ||
        ast_call_arg_count(row->target) != 1 || row->argument_count != 1 || row->arguments == NULL ||
        row->call_receiver == NULL || row->call_receiver != ast_call_callee(row->target) ||
        row->call_receiver->type != AST_IDENTIFIER || row->call_receiver_id == 0 ||
        row->call_receiver_id != ast_node_stable_id(row->call_receiver) ||
        ast_identifier_name(row->call_receiver) == NULL || strcmp(ast_identifier_name(row->call_receiver), "Channel") != 0 ||
        ast_call_semantic_callee_decl_id(row->target) != 0 ||
        ast_call_semantic_callee_value_binding_id(row->target) != 0 || ast_call_semantic_callee_is_stdlib(row->target))
        return false;
    uint32_t kind;
    const PublishedCallArgument *arg = &row->arguments[0];
    return ast_call_semantic_callee_builtin_kind(row->target, &kind) && kind == BUILTIN_NOT_BUILTIN &&
        arg->node != NULL && arg->node == ast_call_argument(row->target, 0) && arg->id != 0 &&
        arg->id == ast_node_stable_id(arg->node) && arg->shape == arg->node->type &&
        arg->binding == ast_identifier_binding_syntax_id(arg->node) && arg->type == TYPE_INT;
}
bool semantic_param_publication_record_channel_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *channel_type)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error || declaration == NULL ||
        declaration->type != AST_LET_DECL || channel_type == NULL || channel_type->name == NULL ||
        type_constructed_constructor(channel_type) != TYPE_CHANNEL ||
        type_constructed_arg_count(channel_type) != 1) return false;
    ASTNode *init = ast_let_initializer(declaration);
    ASTNode *capacity = init != NULL && init->type == AST_CALL && ast_call_arg_count(init) == 1
        ? ast_call_argument(init, 0) : NULL;
    if (init == NULL || capacity == NULL) return false;
    PublishedNode *value = publication_node(ctx->function_param_publications, capacity, false);
    if (value == NULL || !value->value_present || value->value_type != TYPE_INT ||
        !value->physical_scalar || value->value_shape != capacity->type) return false;
    PublishedCallArgument argument = {capacity, ast_node_stable_id(capacity), capacity->type,
        ast_identifier_binding_syntax_id(capacity), TYPE_INT};
    if (!semantic_param_publication_record_value(ctx, init, channel_type, false) ||
        !semantic_param_publication_record_checked_call(ctx, init)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, true);
    if (row == NULL) return false;
    row->channel_binding = false;
    free(row->arguments); free(row->target_name); free(row->value_type_name);
    row->arguments = malloc(sizeof(argument)); row->target_name = pergyra_strdup(ast_let_name(declaration));
    row->value_type_name = pergyra_strdup(channel_type->name);
    if (row->arguments == NULL || row->target_name == NULL || row->value_type_name == NULL) return false;
    *row->arguments = argument; row->argument_count = 1;
    row->value_type = channel_type; row->channel_element_type = type_constructed_arg(channel_type, 0);
    row->channel_type_node = ast_let_type(declaration); row->channel_type_id = ast_node_stable_id(row->channel_type_node);
    row->value_shape = row->channel_type_node == NULL ? AST_LET_DECL : row->channel_type_node->type;
    row->target = init; row->target_id = ast_node_stable_id(init);
    row->call_receiver = ast_call_callee(init); row->call_receiver_id = ast_node_stable_id(row->call_receiver);
    row->channel_binding = true;
    if (!publication_channel_shape(row)) return false;
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, declaration)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, declaration, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax);
    row->deferred_syntax = syntax;
    return true;
}
bool semantic_param_publication_channel_binding(const SemanticContext *ctx, const ASTNode *declaration)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, false);
    if (!publication_channel_shape(row) || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax) ||
        semantic_type_resolution_lookup_metadata_type_ref((SemanticContext *)ctx, (ASTNode *)row->channel_type_node) != row->value_type)
        return false;
    PublishedNode *init = publication_current_value(ctx, row->target);
    PublishedNode *capacity = publication_current_value(ctx, row->arguments[0].node);
    return init != NULL && init->value_type == row->value_type && capacity != NULL &&
        capacity->value_type == TYPE_INT && capacity->physical_scalar;
}

static bool publication_result_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool backing, bool *result_depends, bool *external_retains)
{
    if (result_depends == NULL || external_retains == NULL) return false;
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || ordinal >= row->argument_count) return false;
    if (row->nominal_result != NULL) {
        *result_depends = true; *external_retains = false; return true;
    }
    PublishedNode *arg = publication_current_value(ctx, ast_call_argument(call, 0));
    const char *result = ast_call_semantic_value_type_name(call);
    if (arg == NULL || !row->result_relation || !row->kind_present ||
        row->declaration != 0 || row->value_binding != 0 ||
        row->argument_count != row->relation_argument_count || row->relation_argument != arg->node || row->relation_argument_id != arg->id ||
        row->relation_argument_type == NULL || row->relation_result_type == NULL || arg->value_type_name == NULL ||
        row->value_type_name == NULL || result == NULL || strcmp(row->relation_argument_type, arg->value_type_name) != 0 ||
        strcmp(row->relation_result_type, row->value_type_name) != 0 || strcmp(result, row->relation_result_type) != 0 ||
        (arg->node->type == AST_CALL && (ast_call_semantic_value_type_name(arg->node) == NULL ||
         strcmp(arg->value_type_name, ast_call_semantic_value_type_name(arg->node)) != 0))) return false;
    if (row->builtin == BUILTIN_READ) {
        const Type *receiver = row->relation_input_type;
        if (row->stdlib || row->target == NULL || row->target->type != AST_IDENTIFIER ||
            ast_identifier_name(row->target) == NULL || strcmp(ast_identifier_name(row->target), "Read") != 0 ||
            receiver == NULL || arg->value_type != receiver || receiver->kind != TYPE_KIND_SLOT ||
            type_is_write_view(receiver) || type_is_move_token(receiver) ||
            row->relation_output_type != row->value_type ||
            type_slot_inner_type(receiver) != row->relation_output_type ||
            row->argument_count != (type_slot_is_secure(receiver) ? 2u : 1u)) return false;
    } else if (row->builtin == BUILTIN_VIEW_READ) {
        if (row->stdlib || row->target == NULL || row->target->type != AST_IDENTIFIER ||
            ast_identifier_name(row->target) == NULL || strcmp(ast_identifier_name(row->target), "ViewRead") != 0 ||
            row->argument_count != 1 || arg->value_type != row->relation_input_type ||
            row->relation_input_type == NULL || row->relation_input_type->kind != TYPE_KIND_SLOT ||
            type_slot_access_mode(row->relation_input_type) != SLOT_ACCESS_OWNED ||
            row->value_type != row->relation_output_type || !type_is_read_view(row->value_type) ||
            type_slot_inner_type(row->value_type) != type_slot_inner_type(row->relation_input_type)) return false;
    } else if (row->builtin != BUILTIN_NOT_BUILTIN || !row->stdlib) return false;
    *result_depends = ordinal == 0 && (row->builtin != BUILTIN_READ || backing);
    *external_retains = false;
    return true;
}
bool semantic_param_publication_checked_result_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains)
{
    return publication_result_relation(ctx, call, ordinal, false, result_depends, external_retains);
}
bool semantic_param_publication_checked_backing_relation(const SemanticContext *ctx, const ASTNode *call,
    size_t ordinal, bool *result_depends, bool *external_retains)
{
    return publication_result_relation(ctx, call, ordinal, true, result_depends, external_retains);
}
static PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id);
bool semantic_param_publication_record_routine(SemanticContext *ctx, ASTNode *decl,
    size_t count, Type **types, Type *function_type)
{
    if (!semantic_param_publication_pending(ctx) || decl == NULL || ast_node_stable_id(decl) == 0 ||
        decl->type != AST_FUNC_DECL || function_type == NULL || count != ast_func_param_count(decl) ||
        count != type_function_param_count(function_type) || (count != 0 && types == NULL)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *node = publication_node(store, decl, true);
    if (node == NULL) return false;
    for (size_t p = 0; p < count; p++)
        if (types[p] == NULL || !type_equals(types[p], type_function_param_type(function_type, p)) ||
            ast_func_param(decl, p) == NULL || ast_func_param_stable_id(ast_func_param(decl, p)) == 0 ||
            ast_func_param(decl, p)->mode != type_function_param_mode(function_type, p)) return false;
    if (node->routine_index != 0) {
        if (node->routine_index > store->routine_count) return false;
        PendingRoutine *prior = &store->routines[node->routine_index - 1];
        if (prior->decl != decl || prior->function_type != function_type || prior->count != count) return false;
        if (prior->body != ast_func_body(decl) || prior->body_id != ast_node_stable_id(prior->body)) return false;
        for (size_t p = 0; p < count; p++) if (prior->types[p] != types[p] ||
            prior->formals[p].id != ast_func_param_stable_id(ast_func_param(decl, p)) ||
            prior->formals[p].mode != ast_func_param(decl, p)->mode) return false;
        return true;
    }
    if (count > SIZE_MAX / sizeof(*types) || count > SIZE_MAX / sizeof(PublishedFormal) ||
        !semantic_param_publication_reserve_storage((void **)&store->routines,
            &store->routine_capacity, store->routine_count, sizeof(*store->routines))) return false;
    Type **copy = count == 0 ? NULL : malloc(count * sizeof(*copy));
    PublishedFormal *formals = count == 0 ? NULL : malloc(count * sizeof(*formals));
    if (count != 0 && (copy == NULL || formals == NULL)) { free(copy); free(formals); return false; }
    if (count != 0) memcpy(copy, types, count * sizeof(*copy));
    for (size_t p = 0; p < count; p++) formals[p] = (PublishedFormal){
        ast_func_param_stable_id(ast_func_param(decl, p)), ast_func_param(decl, p)->mode,
        ast_func_param(decl, p)->type, ast_node_stable_id(ast_func_param(decl, p)->type),
        types[p]->kind, type_is_anchored_resource_handle(types[p])};
    char *module = pergyra_strdup(ctx->current_module_path);
    if (ctx->current_module_path != NULL && module == NULL) { free(copy); free(formals); return false; }
    store->routines[store->routine_count++] = (PendingRoutine){decl, function_type, copy, count,
        ast_node_stable_id(decl), module, formals, ast_func_body(decl), ast_node_stable_id(ast_func_body(decl)),
        type_function_return_type(function_type), ast_func_return_type(decl),
        ast_node_stable_id(ast_func_return_type(decl)), decl->is_async_decl};
    node->routine_index = store->routine_count;
    return true;
}
bool semantic_param_publication_routine_inputs_current(const SemanticContext *ctx, const ASTNode *decl, bool require_body)
{
    if (!publication_generation_ready(ctx)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    if (row == NULL || decl == NULL || decl->type != AST_FUNC_DECL || row->decl != decl ||
        row->id != ast_node_stable_id(decl) || row->count != ast_func_param_count(decl) ||
        row->count != type_function_param_count(row->function_type) || row->body != ast_func_body(decl) ||
        row->body_id != ast_node_stable_id(row->body) ||
        (require_body && (row->body == NULL || row->body_id == 0)) ||
        !type_function_has_body_summary(row->function_type)) return false;
    for (size_t p = 0; p < row->count; p++) {
        FuncParam *param = ast_func_param(decl, p);
        if (param == NULL || row->formals[p].id != ast_func_param_stable_id(param) ||
            row->formals[p].type_node != param->type ||
            row->formals[p].type_node_id != ast_node_stable_id(param->type) ||
            row->formals[p].mode != param->mode || param->mode != type_function_param_mode(row->function_type, p) ||
            !type_equals(row->types[p], type_function_param_type(row->function_type, p))) return false;
    }
    return true;
}
bool semantic_param_publication_parameter_is_anchored_handle(
    const SemanticContext *ctx, const ASTNode *decl, size_t ordinal, bool *anchored)
{
    if (!semantic_param_publication_inputs_ready(ctx) || anchored == NULL) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    if (!semantic_param_publication_routine_inputs_current(ctx, decl, true) || ordinal >= row->count) return false;
    const PublishedFormal *formal = &row->formals[ordinal];
    if (formal->id == 0 || (formal->type_node != NULL && formal->type_node_id == 0) ||
        row->types[ordinal] == NULL || row->types[ordinal] == TYPE_UNKNOWN ||
        row->types[ordinal] != type_function_param_type(row->function_type, ordinal) ||
        formal->kind != row->types[ordinal]->kind || formal->anchored != type_is_anchored_resource_handle(row->types[ordinal])) return false;
    *anchored = formal->anchored;
    return true;
}
bool semantic_param_publication_return_backing_required(
    const SemanticContext *ctx, const ASTNode *decl, size_t ordinal, bool *required)
{
    if (required == NULL || !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_routine_inputs_current(ctx, decl, true)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    if (row == NULL || ordinal >= row->count || row->types == NULL || row->formals == NULL ||
        row->types[ordinal] == NULL || row->types[ordinal] == TYPE_UNKNOWN ||
        row->async != decl->is_async_decl || row->return_type == NULL || row->return_type == TYPE_UNKNOWN ||
        row->return_type != type_function_return_type(row->function_type) ||
        (row->return_node != NULL && row->return_node_id == 0) ||
        row->return_node != ast_func_return_type(decl) ||
        row->return_node_id != ast_node_stable_id(row->return_node)) return false;
    bool anchored;
    if (!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, ordinal, &anchored)) return false;
    *required = !publication_physical_scalar(row->types[ordinal]) &&
        !(row->return_type == TYPE_VOID || publication_physical_scalar(row->return_type));
    return true;
}
bool publication_routine_header_current(
    const SemanticContext *ctx, const ASTNode *decl)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    if (!semantic_param_publication_routine_inputs_current(ctx, decl, true) || row->async != decl->is_async_decl ||
        row->return_type == NULL || row->return_type == TYPE_UNKNOWN ||
        row->return_type != type_function_return_type(row->function_type) ||
        row->return_node != ast_func_return_type(decl) ||
        row->return_node_id != ast_node_stable_id(row->return_node)) return false;
    for (size_t p = 0; p < row->count; p++) {
        bool anchored;
        if (!semantic_param_publication_parameter_is_anchored_handle(ctx, decl, p, &anchored)) return false;
    }
    return true;
}
bool semantic_param_publication_routine_relation_header_ready(const SemanticContext *ctx, const ASTNode *decl)
{
    return publication_routine_header_current(ctx, decl) && !decl->is_async_decl;
}
bool semantic_param_publication_routine_no_output_header_ready(const SemanticContext *ctx, const ASTNode *decl)
{
    if (!semantic_param_publication_routine_relation_header_ready(ctx, decl)) return false;
    for (size_t p = 0; p < ast_func_param_count(decl); p++)
        if (ast_func_param(decl, p)->mode != PARAM_MODE_REF && ast_func_param(decl, p)->mode != PARAM_MODE_DEFAULT) return false;
    return true;
}
bool semantic_param_publication_routine_return_dependency_is_scalar(
    const SemanticContext *ctx, const ASTNode *decl)
{
    if (!semantic_param_publication_routine_no_output_header_ready(ctx, decl)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    return row->return_type == TYPE_VOID || publication_physical_scalar(row->return_type);
}
bool semantic_param_publication_value_is_physical_scalar(const SemanticContext *ctx, const ASTNode *value)
{
    PublishedNode *row = publication_current_value(ctx, value);
    return row != NULL && row->physical_scalar;
}
bool semantic_param_publication_value_has_static_string_backing(const SemanticContext *ctx, const ASTNode *value)
{
    PublishedNode *row = publication_current_value(ctx, value);
    /* A fresh allocation can be copy-safe too; that does not make a ref
     * transfer its cleanup responsibility. Only literals have static storage. */
    return row != NULL && row->string_value && row->value_shape == AST_STRING && row->copy_escape;
}
static bool publication_ref_call_tuple(const ASTNode *call, const ASTNode *site,
    const ASTNode *callee, size_t ordinal)
{
    return call != NULL && call->type == AST_CALL && site != NULL && callee != NULL &&
        callee->type == AST_FUNC_DECL && ast_node_stable_id(call) != 0 &&
        ast_node_stable_id(site) != 0 && ast_node_stable_id(callee) != 0 &&
        ordinal < ast_func_param_count(callee) && ordinal < ast_call_arg_count(call) &&
        ast_call_argument(call, ordinal) == site &&
        ast_call_semantic_callee_decl_id(call) == ast_node_stable_id(callee) &&
        ast_call_semantic_callee_value_binding_id(call) == 0 && ast_func_param(callee, ordinal) != NULL &&
        ast_func_param(callee, ordinal)->mode == PARAM_MODE_REF;
}
bool semantic_param_publication_record_call(SemanticContext *ctx, ASTNode *call, ASTNode *site,
    ASTNode *callee, size_t ordinal, const Type *type, const char *borrowed,
    const char *destination, const char *fix)
{
    if (!semantic_param_publication_pending(ctx) || !publication_ref_call_tuple(call, site, callee, ordinal) ||
        (ctx->current_function_decl != NULL && ast_node_stable_id(ctx->current_function_decl) == 0) ||
        type == NULL || borrowed == NULL) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (!semantic_param_publication_reserve_storage((void **)&store->calls, &store->call_capacity,
            store->call_count, sizeof(*store->calls))) return false;
    PendingRefCall row = {call, site, callee, ctx->current_function_decl, ordinal, type,
        pergyra_strdup(borrowed), pergyra_strdup(destination), pergyra_strdup(fix),
        ast_node_stable_id(call), ast_node_stable_id(site), ast_node_stable_id(callee),
        ctx->current_function_decl == NULL ? 0 : ast_node_stable_id(ctx->current_function_decl),
        pergyra_strdup(ctx->current_module_path), ast_call_callee(call),
        ast_node_stable_id(ast_call_callee(call)), ast_call_arg_count(call)};
    if (row.call_target == NULL || row.call_target_id == 0 ||
        (row.call_target->type != AST_IDENTIFIER && row.call_target->type != AST_MEMBER_ACCESS) ||
        row.borrowed == NULL || (destination != NULL && row.destination == NULL) || (fix != NULL && row.fix == NULL) ||
        (ctx->current_module_path != NULL && row.module == NULL)) {
        free(row.borrowed); free(row.destination); free(row.fix); free(row.module); return false;
    }
    store->calls[store->call_count++] = row;
    return true;
}
bool semantic_param_publication_ref_call_current(const SemanticContext *ctx, const PendingRefCall *row)
{
    if (row == NULL || !semantic_param_publication_inputs_ready(ctx) ||
        !publication_ref_call_tuple(row->call, row->site, row->callee, row->ordinal) ||
        row->call_id != ast_node_stable_id(row->call) || row->site_id != ast_node_stable_id(row->site) ||
        row->callee_id != ast_node_stable_id(row->callee) || row->caller == NULL ||
        row->caller_id != ast_node_stable_id(row->caller) || row->call_target != ast_call_callee(row->call) ||
        row->call_target_id != ast_node_stable_id(row->call_target) || row->argument_count != ast_call_arg_count(row->call) ||
        !type_equals(row->type, TYPE_STRING) ||
        !semantic_param_publication_routine_inputs_current(ctx, row->caller, true) ||
        semantic_param_publication_callee(ctx, row->call, row->ordinal) != row->callee) return false;
    PublishedNode *actual = publication_current_value(ctx, row->site);
    PublishedNode *result = publication_current_value(ctx, row->call);
    const PendingRoutine *callee = publication_routine(ctx->function_param_publications, row->callee_id);
    return actual != NULL && actual->string_value && result != NULL && result->string_value &&
        callee != NULL && row->ordinal < callee->count && type_equals(callee->types[row->ordinal], TYPE_STRING) &&
        type_equals(callee->return_type, TYPE_STRING);
}
bool semantic_param_publication_record_body_call(SemanticContext *ctx, ASTNode *call)
{
    if (ctx == NULL || !ctx->tracking_function_effects)
        return true; /* Top-level calls have no routine body dependency. */
    const ASTNode *current = callable_capability_current_declaration(ctx);
    if (call == NULL || call->type != AST_CALL) return false;
    const ASTNode *callee = ast_call_callee(call);
    ASTNode *decl = callee != NULL && callee->type == AST_IDENTIFIER
        ? semantic_find_callable_decl_by_name(ctx, ast_identifier_name(callee)) : NULL;
    if ((current != NULL && current->type != AST_FUNC_DECL) ||
        ast_call_semantic_callee_value_binding_id(call) != 0 ||
        (decl != NULL && decl->type != AST_FUNC_DECL)) {
        semantic_record_body_summary(ctx, BODY_SUMMARY_MAY_ESCAPE_REF);
        return true;
    }
    if (ctx->current_function_decl == NULL) return true;
    if (current != ctx->current_function_decl || ast_node_stable_id(current) == 0) return false;
    if (!semantic_param_publication_pending(ctx) || ast_node_stable_id(call) == 0 ||
        ast_call_semantic_callee_decl_id(call) == 0) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (!semantic_param_publication_reserve_storage((void **)&store->body_calls, &store->body_call_capacity,
            store->body_call_count, sizeof(*store->body_calls))) return false;
    store->body_calls[store->body_call_count++] = (PendingBodyCall){call, ctx->current_function_decl,
        ast_node_stable_id(call), ast_node_stable_id(ctx->current_function_decl),
        ast_call_semantic_callee_decl_id(call), 0, 0};
    return true;
}
static PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id)
{
    if (store == NULL || id == 0 || store->node_capacity == 0) return NULL;
    size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
        slot = (slot + 1) & (store->node_capacity - 1);
    PublishedNode *node = &store->nodes[slot];
    if (node->id != id || node->routine_index == 0 || node->routine_index > store->routine_count) return NULL;
    PendingRoutine *row = &store->routines[node->routine_index - 1];
    return row->id == id && row->decl == node->node && ast_node_stable_id(row->decl) == id ? row : NULL;
}
ASTNode *semantic_param_publication_direct_callee(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return NULL;
    PublishedNode *node = publication_node(ctx->function_param_publications, call, false);
    /* Member receivers need an admitted self/argument ordinal mapping. */
    if (node == NULL || node->declaration == 0 || node->value_binding != 0 ||
        node->target == NULL || node->target->type != AST_IDENTIFIER) return NULL;
    PendingRoutine *row = publication_routine(ctx->function_param_publications, node->declaration);
    if (row == NULL || !semantic_param_publication_routine_inputs_current(ctx, row->decl, true)) return NULL;
    return row->decl;
}
ASTNode *semantic_param_publication_callee(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    if (!semantic_param_publication_checked_argument(ctx, call, ordinal)) return NULL;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    return decl != NULL && ordinal < ast_func_param_count(decl) ? decl : NULL;
}
bool semantic_param_publication_routine_result_current(const SemanticContext *ctx,
    const ASTNode *decl, const Type *result_type)
{
    if (result_type == NULL || result_type == TYPE_UNKNOWN ||
        !semantic_param_publication_routine_relation_header_ready(ctx, decl)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    return row != NULL && row->return_type == result_type && !row->async &&
        (row->return_node == NULL || row->return_node_id != 0);
}
ASTNode *semantic_param_publication_declared_result(const SemanticContext *ctx,
    const ASTNode *call, const Type **result_type)
{
    if (result_type == NULL) return NULL;
    *result_type = NULL;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    PublishedNode *value = publication_result_current(ctx, call);
    if (decl == NULL || value == NULL || value->nominal_result != NULL ||
        value->stdlib || !value->kind_present || value->builtin != BUILTIN_NOT_BUILTIN ||
        value->argument_count != ast_func_param_count(decl) ||
        !semantic_param_publication_routine_result_current(ctx, decl, value->value_type)) return NULL;
    const char *name = ast_call_semantic_value_type_name(call);
    if (name != NULL && (value->value_type->name == NULL || strcmp(name, value->value_type->name) != 0))
        return NULL;
    for (size_t p = 0; p < value->argument_count; p++)
        if (!semantic_param_publication_checked_argument(ctx, call, p)) return NULL;
    *result_type = value->value_type;
    return decl;
}
bool semantic_param_publication_call_analysis_unmodelled(
    const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *row = publication_current_value(ctx, call);
    if (row == NULL || call->type != AST_CALL || !publication_call_ready(row) ||
        row->value_type == NULL || row->value_type == TYPE_UNKNOWN ||
        row->nominal_result != NULL || row->result_relation) return false;
    for (size_t i = 0; i < row->argument_count; i++) {
        const PublishedCallArgument *arg = &row->arguments[i];
        PublishedNode *value = publication_current_value(ctx, arg->node);
        if (value == NULL || arg->type == NULL || arg->type == TYPE_UNKNOWN ||
            arg->type != value->value_type) return false;
    }
    if (row->target_shape == AST_IDENTIFIER && row->kind_present &&
        row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib &&
        row->declaration == 0 && row->value_binding == 0 &&
        row->value_type != TYPE_VOID && !publication_physical_scalar(row->value_type) &&
        !row->origin_ready && !(row->string_value && semantic_indexed_string_independent_result(call)) &&
        !semantic_typed_map_query_fact_issued(ctx, call) &&
        !semantic_typed_map_query_result_independent(ctx, call)) return true;
    if (row->target_shape == AST_MEMBER_ACCESS) {
        const ASTNode *receiver = ast_member_object(row->target);
        PublishedNode *value = publication_current_value(ctx, receiver);
        return receiver != NULL && receiver == row->call_receiver && row->call_receiver_id != 0 &&
            row->call_receiver_id == ast_node_stable_id(receiver) && value != NULL &&
            value->value_type != NULL && value->value_type != TYPE_UNKNOWN;
    }
    if (row->value_binding != 0) {
        PublishedNode *target = publication_current_value(ctx, row->target);
        return row->target_shape == AST_IDENTIFIER && row->target_binding == row->value_binding &&
            target != NULL && target->value_type != NULL && target->value_type->kind == TYPE_KIND_FUNCTION;
    }
    /* Absence of a routine is not constructor evidence: only the typed
     * producer's exact nominal target receipt distinguishes this case. */
    if (row->unmodelled_constructor == NULL || row->unmodelled_constructor_id == 0 ||
        row->unmodelled_constructor_id != ast_node_stable_id(row->unmodelled_constructor) ||
        row->unmodelled_constructor->type != AST_CLASS_DECL || ast_class_is_struct(row->unmodelled_constructor) ||
        row->unmodelled_constructor_type != row->value_type || row->value_type->kind != TYPE_KIND_CLASS ||
        row->value_type->nominal_flavor != TYPE_NOMINAL_CLASS ||
        row->target_shape != AST_IDENTIFIER || row->target_name == NULL ||
        ast_class_name(row->unmodelled_constructor) == NULL ||
        strcmp(row->target_name, ast_class_name(row->unmodelled_constructor)) != 0 ||
        (row->declaration != 0 && row->declaration != row->unmodelled_constructor_id) ||
        publication_routine(ctx->function_param_publications, row->unmodelled_constructor_id) != NULL) return false;
    return true;
}
bool semantic_param_publication_admitted_view(const SemanticContext *ctx, SemanticParamPublicationView *view)
{
    if (view == NULL || !semantic_param_publication_inputs_ready(ctx)) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    *view = (SemanticParamPublicationView){store->program, store->program_id, store->routines,
        store->routine_count, store->calls, store->call_count, store->body_calls, store->body_call_count,
        store->read_loans, store->read_loan_count};
    return true;
}
bool semantic_param_publication_view_current(const SemanticContext *ctx, const SemanticParamPublicationView *view)
{
    if (view == NULL || !semantic_param_publication_inputs_ready(ctx)) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    return view->program == store->program && view->program_id == store->program_id &&
        view->routines == store->routines && view->routine_count == store->routine_count &&
        view->calls == store->calls && view->call_count == store->call_count &&
        view->read_loans == store->read_loans && view->read_loan_count == store->read_loan_count &&
        view->body_calls == store->body_calls && view->body_call_count == store->body_call_count;
}
bool semantic_param_publication_finalize(SemanticContext *ctx)
{
    if (!semantic_param_publication_pending(ctx) || ctx->function_param_flow_summaries != NULL) return false;
    if (!semantic_typed_map_query_facts_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    for (size_t n = 0; n < store->node_capacity; n++) {
        PublishedNode *row = &store->nodes[n];
        if (row->id == 0) continue;
        if (row->id != ast_node_stable_id(row->node) ||
            (row->call_present && !publication_call_ready(row))) return false;
    }
    for (size_t r = 0; r < store->routine_count; r++) {
        PendingRoutine *row = &store->routines[r];
        if (!semantic_param_publication_routine_inputs_current(ctx, row->decl, false)) return false;
    }
    for (size_t c = 0; c < store->call_count; c++) {
        PendingRefCall *row = &store->calls[c];
        if (row->call_id != ast_node_stable_id(row->call) || row->site_id != ast_node_stable_id(row->site) ||
            !publication_ref_call_tuple(row->call, row->site, row->callee, row->ordinal) ||
            row->call_target != ast_call_callee(row->call) ||
            row->call_target_id != ast_node_stable_id(row->call_target) ||
            row->argument_count != ast_call_arg_count(row->call) ||
            row->callee_id != ast_node_stable_id(row->callee) ||
            publication_routine(store, row->callee_id) == NULL ||
            (row->caller != NULL && row->caller_id != ast_node_stable_id(row->caller))) return false;
    }
    for (size_t b = 0; b < store->body_call_count; b++) {
        PendingBodyCall *row = &store->body_calls[b];
        PendingRoutine *caller = publication_routine(store, row->caller_id);
        PendingRoutine *callee = publication_routine(store, row->callee_id);
        if (row->call_id != ast_node_stable_id(row->call) || row->call->type != AST_CALL ||
            row->caller_id != ast_node_stable_id(row->caller) ||
            ast_call_semantic_callee_decl_id(row->call) != row->callee_id ||
            ast_call_semantic_callee_value_binding_id(row->call) != 0 ||
            caller == NULL || caller->decl != row->caller || callee == NULL) return false;
        row->caller_index = (size_t)(caller - store->routines);
        row->callee_index = (size_t)(callee - store->routines);
    }
    store->pending = false; store->inputs_ready = true;
    SemanticParamPublicationView view;
    if (!semantic_param_publication_admitted_view(ctx, &view)) return false;
    if (!semantic_consume_param_publication(ctx, &view)) return false;
    store->finalized = !ctx->has_error;
    return !ctx->has_error;
}
