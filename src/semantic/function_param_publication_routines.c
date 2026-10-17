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
bool semantic_param_publication_record_checked_result_relation(SemanticContext *ctx, ASTNode *call,
    const Type *argument_type, const Type *result_type, uint32_t selected_decl, size_t payload_ordinal)
{
    if (!semantic_param_publication_pending(ctx) || call == NULL || call->type != AST_CALL ||
        selected_decl != 0 || payload_ordinal != 0 || ast_call_arg_count(call) == 0 ||
        argument_type == NULL || result_type == NULL || argument_type == TYPE_UNKNOWN || result_type == TYPE_UNKNOWN ||
        argument_type->name == NULL || result_type->name == NULL) return false;
    const char *result_name = ast_call_semantic_value_type_name(call);
    if (result_name == NULL || strcmp(result_name, result_type->name) != 0) return false;
    ASTNode *argument = ast_call_argument(call, 0);
    PublishedNode *row = publication_node(ctx->function_param_publications, call, true);
    if (row == NULL) return false;
    PublishedNode *arg = publication_node(ctx->function_param_publications, argument, false);
    if (arg == NULL || !arg->value_present || arg->value_shape != argument->type) return false;
    char *a = pergyra_strdup(argument_type->name), *r = pergyra_strdup(result_type->name);
    char *av = pergyra_strdup(argument_type->name), *rv = pergyra_strdup(result_type->name);
    if (a == NULL || r == NULL || av == NULL || rv == NULL) { free(a); free(r); free(av); free(rv); return false; }
    free(row->relation_argument_type); free(row->relation_result_type); free(arg->value_type_name); free(row->value_type_name);
    row->relation_argument_type = a; row->relation_result_type = r; arg->value_type_name = av; row->value_type_name = rv;
    row->relation_argument = argument; row->relation_argument_id = ast_node_stable_id(argument);
    row->relation_argument_count = ast_call_arg_count(call);
    row->relation_input_type = argument_type; row->relation_output_type = result_type;
    row->result_relation = row->relation_argument_id != 0;
    return row->result_relation;
}
bool semantic_param_publication_record_routine(SemanticContext *ctx, ASTNode *decl,
    size_t count, Type **types, Type *function_type, bool normal_exit_closed)
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
        if (prior->decl != decl || prior->function_type != function_type ||
            prior->count != count || prior->normal_exit_closed != normal_exit_closed) return false;
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
        normal_exit_closed,
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
    return row != NULL && row->physical_scalar && publication_physical_scalar(row->value_type);
}
bool semantic_param_publication_record_checked_numeric_cast(SemanticContext *ctx, ASTNode *cast)
{
    if (!semantic_param_publication_pending(ctx) || cast == NULL || cast->type != AST_CAST ||
        ast_cast_target_type(cast) == NULL) return false;
    PublishedNode *operand = publication_node(ctx->function_param_publications, ast_cast_operand(cast), false);
    if (operand == NULL || !operand->value_present || !operand->physical_scalar ||
        operand->value_shape != ast_cast_operand(cast)->type) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, cast, true);
    if (row == NULL) return false;
    char *name = pergyra_strdup(ast_cast_target_type(cast));
    if (name == NULL) return false;
    free(row->target_name); row->target_name = name;
    row->target = ast_cast_operand(cast); row->target_id = ast_node_stable_id(row->target);
    row->cast_present = row->target_id != 0;
    return row->cast_present;
}
bool semantic_param_publication_checked_numeric_cast(const SemanticContext *ctx, const ASTNode *cast)
{
    if (cast == NULL || cast->type != AST_CAST ||
        !semantic_param_publication_value_is_physical_scalar(ctx, cast)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, cast, false);
    const char *target = ast_cast_target_type(cast);
    return row != NULL && row->cast_present && row->target == ast_cast_operand(cast) &&
        row->target_id != 0 && row->target_id == ast_node_stable_id(row->target) &&
        row->target_name != NULL && target != NULL && strcmp(row->target_name, target) == 0 &&
        semantic_param_publication_value_is_physical_scalar(ctx, row->target);
}
bool semantic_param_publication_value_has_static_string_backing(const SemanticContext *ctx, const ASTNode *value)
{
    PublishedNode *row = publication_current_value(ctx, value);
    /* A fresh allocation can be copy-safe too; that does not make a ref
     * transfer its cleanup responsibility. Only literals have static storage. */
    return row != NULL && row->string_value && row->value_shape == AST_STRING && row->copy_escape;
}
bool semantic_param_publication_record_call(SemanticContext *ctx, ASTNode *call, ASTNode *site,
    ASTNode *callee, size_t ordinal, const Type *type, const char *borrowed,
    const char *destination, const char *fix)
{
    size_t formal = 0;
    if (!semantic_param_publication_pending(ctx) ||
        !publication_ref_call_tuple(ctx, call, site, callee, ordinal, &formal) ||
        (ctx->current_function_decl != NULL && ast_node_stable_id(ctx->current_function_decl) == 0) ||
        type == NULL || borrowed == NULL) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (!semantic_param_publication_reserve_storage((void **)&store->calls, &store->call_capacity,
            store->call_count, sizeof(*store->calls))) return false;
    PendingRefCall row = {.call = call, .site = site, .callee = callee,
        .caller = ctx->current_function_decl, .ordinal = ordinal, .type = type,
        .borrowed = pergyra_strdup(borrowed), .destination = pergyra_strdup(destination),
        .fix = pergyra_strdup(fix), .call_id = ast_node_stable_id(call),
        .site_id = ast_node_stable_id(site), .callee_id = ast_node_stable_id(callee),
        .caller_id = ast_node_stable_id(ctx->current_function_decl),
        .module = pergyra_strdup(ctx->current_module_path), .call_target = ast_call_callee(call),
        .call_target_id = ast_node_stable_id(ast_call_callee(call)),
        .argument_count = ast_call_arg_count(call)};
    row.formal_ordinal = formal;
    row.formal_id = ast_func_param_stable_id(ast_func_param(callee, formal));
    row.site_shape = site->type;
    row.site_binding_id = ast_identifier_binding_syntax_id(site);
    row.target_shape = row.call_target == NULL ? 0 : row.call_target->type;
    row.target_binding_id = ast_identifier_binding_syntax_id(row.call_target);
    row.call_receiver = row.target_shape == AST_MEMBER_ACCESS ? ast_member_object(row.call_target) : NULL;
    row.call_receiver_id = ast_node_stable_id(row.call_receiver);
    PublishedNode *receiver = publication_node(store, row.call_receiver, false);
    row.call_receiver_type = receiver != NULL && receiver->value_present &&
        receiver->id == row.call_receiver_id && !receiver->meta ? receiver->value_type : NULL;
    const char *target_name = row.target_shape == AST_IDENTIFIER ? ast_identifier_name(row.call_target) :
        row.target_shape == AST_MEMBER_ACCESS ? ast_member_name(row.call_target) : NULL;
    row.target_name = pergyra_strdup(target_name);
    if (row.call_target == NULL || row.call_target_id == 0 ||
        (row.call_target->type != AST_IDENTIFIER && row.call_target->type != AST_MEMBER_ACCESS) ||
        row.formal_id == 0 || target_name == NULL || row.target_name == NULL ||
        (row.call_receiver != NULL && row.call_receiver_id == 0) ||
        row.borrowed == NULL || (destination != NULL && row.destination == NULL) || (fix != NULL && row.fix == NULL) ||
        (ctx->current_module_path != NULL && row.module == NULL)) {
        free(row.borrowed); free(row.destination); free(row.fix); free(row.module); free(row.target_name); return false;
    }
    store->calls[store->call_count++] = row;
    return true;
}
bool semantic_param_publication_ref_call_current(const SemanticContext *ctx, const PendingRefCall *row)
{
    size_t formal = 0;
    if (row == NULL || !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_ref_call_diagnostic_current(ctx, row, &formal) ||
        !publication_ref_call_tuple(ctx, row->call, row->site, row->callee, row->ordinal, &formal) ||
        formal != row->formal_ordinal || row->formal_id != ast_func_param_stable_id(ast_func_param(row->callee, formal)) ||
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
        callee != NULL && formal < callee->count && type_equals(callee->types[formal], TYPE_STRING) &&
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
PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id)
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
bool semantic_param_publication_routine_result_current(const SemanticContext *ctx,
    const ASTNode *decl, const Type *result_type)
{
    if (result_type == NULL || result_type == TYPE_UNKNOWN ||
        !semantic_param_publication_routine_relation_header_ready(ctx, decl)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    return row != NULL && row->normal_exit_closed && row->return_type == result_type && !row->async &&
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
        !semantic_param_publication_routine_result_current(ctx, decl, value->value_type)) return NULL;
    const char *name = ast_call_semantic_value_type_name(call);
    if (name != NULL && (value->value_type->name == NULL || strcmp(name, value->value_type->name) != 0))
        return NULL;
    for (size_t p = 0; p < value->argument_count; p++)
        if (!semantic_param_publication_checked_argument(ctx, call, p)) return NULL;
    *result_type = value->value_type;
    return decl;
}
PublishedNode *publication_checked_intrinsic(const SemanticContext *ctx,
    const ASTNode *call, uint32_t kind, const Type *type, size_t count)
{
    if (!publication_generation_ready(ctx) || (!ctx->function_param_publications->pending &&
        !ctx->function_param_publications->inputs_ready)) return NULL;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (!publication_call_ready(row) || !row->value_present || row->meta || row->value_type != type ||
        row->value_shape != AST_CALL || row->declaration != 0 || row->value_binding != 0 || row->stdlib ||
        !row->kind_present || row->builtin != kind || row->target_shape != AST_IDENTIFIER ||
        row->target_binding != 0 || row->target_id == 0 || row->target_name == NULL ||
        row->nominal_result != NULL || row->argument_count != count) return NULL;
    for (size_t p = 0; p < count; p++) {
        const PublishedCallArgument *arg = &row->arguments[p];
        PublishedNode *value = publication_node(ctx->function_param_publications, arg->node, false);
        if (arg->id == 0 || value == NULL || !value->value_present || value->meta ||
            value->value_type == NULL || value->value_type == TYPE_UNKNOWN || value->value_type != arg->type ||
            value->value_shape != arg->shape || (arg->shape == AST_IDENTIFIER && value->binding_id != arg->binding) ||
            (arg->shape == AST_CALL && !publication_call_ready(value))) return NULL;
    }
    return row;
}
static PublishedNode *publication_allocator_binding(const SemanticContext *ctx, uint32_t id)
{
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (id == 0 || store->node_capacity == 0) return NULL;
    size_t at = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[at].id != 0 && store->nodes[at].id != id) at = (at + 1) & (store->node_capacity - 1);
    return store->nodes[at].id == id ? &store->nodes[at] : NULL;
}
bool semantic_param_publication_record_result_allocator_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *type)
{
    if (!semantic_param_publication_pending(ctx) || declaration == NULL || declaration->type != AST_LET_DECL ||
        type != TYPE_ALLOCATOR || ast_node_stable_id(declaration) == 0) return false;
    ASTNode *init = ast_let_initializer(declaration);
    if (ast_let_is_alias(declaration) || publication_checked_intrinsic(ctx, init,
        BUILTIN_ALLOCATOR_RESULT, TYPE_ALLOCATOR, 0) == NULL) return true;
    const ASTNode *caller = callable_capability_current_declaration(ctx);
    if (caller == NULL || caller->type != AST_FUNC_DECL || ast_node_stable_id(caller) == 0) return true;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, true);
    if (row == NULL) return false;
    if (row->result_allocator_binding) return row->allocator_initializer == init && row->allocator_type == type;
    char *name = pergyra_strdup(ast_let_name(declaration));
    if (name == NULL) return false;
    free(row->target_name); row->target_name = name;
    row->allocator_initializer = init; row->allocator_initializer_id = ast_node_stable_id(init);
    row->allocator_type_node = ast_let_type(declaration);
    row->allocator_type_node_id = ast_node_stable_id(row->allocator_type_node);
    row->allocator_type = type; row->caller = caller; row->caller_id = ast_node_stable_id(caller);
    row->result_allocator_binding = true;
    return true;
}
void semantic_param_publication_invalidate_allocator_binding(SemanticContext *ctx, const ASTNode *value)
{
    if (!publication_generation_ready(ctx)) return;
    while (value != NULL && (value->type == AST_MEMBER_ACCESS || value->type == AST_ARRAY_ACCESS))
        value = value->type == AST_MEMBER_ACCESS ? ast_member_object(value) : ast_array_access_array(value);
    if (value == NULL || value->type != AST_IDENTIFIER) return;
    PublishedNode *row = publication_allocator_binding(ctx, ast_identifier_binding_syntax_id(value));
    if (row != NULL && row->result_allocator_binding) row->allocator_invalidated = true;
}
bool semantic_param_publication_checked_finish_independent(const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *finish = publication_checked_intrinsic(ctx, call, BUILTIN_TEXT_BUILDER_FINISH, TYPE_STRING, 2);
    if (finish == NULL || finish->arguments[0].type != TYPE_TEXT_BUILDER ||
        finish->arguments[1].type != TYPE_ALLOCATOR || finish->arguments[1].shape != AST_IDENTIFIER) return false;
    const PublishedCallArgument *arg = &finish->arguments[1];
    PublishedNode *row = publication_allocator_binding(ctx, arg->binding);
    if (row == NULL || !row->result_allocator_binding || row->allocator_invalidated ||
        row->node == NULL || row->node->type != AST_LET_DECL || row->id != ast_node_stable_id(row->node) ||
        row->id != arg->binding || ast_let_is_alias(row->node) || row->allocator_type != TYPE_ALLOCATOR ||
        row->target_name == NULL || ast_let_name(row->node) == NULL ||
        strcmp(row->target_name, ast_let_name(row->node)) != 0 || ast_identifier_name(arg->node) == NULL ||
        strcmp(row->target_name, ast_identifier_name(arg->node)) != 0 || row->caller == NULL ||
        row->caller != finish->caller || row->caller_id != ast_node_stable_id(row->caller) ||
        row->allocator_initializer != ast_let_initializer(row->node) || row->allocator_initializer_id == 0 ||
        row->allocator_initializer_id != ast_node_stable_id(row->allocator_initializer) ||
        row->allocator_type_node != ast_let_type(row->node) ||
        row->allocator_type_node_id != ast_node_stable_id(row->allocator_type_node) ||
        (row->allocator_type_node != NULL && (row->allocator_type_node_id == 0 ||
         semantic_type_resolution_lookup_metadata_type_ref((SemanticContext *)ctx,
             (ASTNode *)row->allocator_type_node) != TYPE_ALLOCATOR))) return false;
    return publication_checked_intrinsic(ctx, row->allocator_initializer,
        BUILTIN_ALLOCATOR_RESULT, TYPE_ALLOCATOR, 0) != NULL;
}
bool semantic_param_publication_pending(const SemanticContext *ctx)
{ return publication_generation_ready(ctx) && ctx->function_param_publications->pending; }
bool semantic_param_publication_inputs_ready(const SemanticContext *ctx)
{ return publication_generation_ready(ctx) && ctx->function_param_publications->inputs_ready; }
bool semantic_param_publication_checked_call(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    return publication_call_ready(row);
}
bool semantic_param_publication_checked_argument(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    return semantic_param_publication_checked_call(ctx, call) && ordinal < ast_call_arg_count(call);
}
bool semantic_param_publication_call_analysis_unmodelled(
    const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || row->nominal_result != NULL || row->result_relation) return false;
    if (row->kind_present && row->builtin == BUILTIN_TEXT_BUILDER_FINISH &&
        !semantic_param_publication_checked_finish_independent(ctx, call)) return true;
    if (semantic_param_publication_checked_local_owner_constructor(ctx, call)) return false;
    if (row->selected_callable != NULL &&
        semantic_param_publication_direct_callee(ctx, call) != NULL) return false;
    if (row->target_shape == AST_IDENTIFIER && row->kind_present &&
        row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib &&
        row->declaration == 0 && row->value_binding == 0 &&
        row->value_type != TYPE_VOID && !publication_physical_scalar(row->value_type) &&
        !row->origin_ready && !(row->string_value && semantic_indexed_string_independent_result(call, ctx)) &&
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
        size_t formal = 0;
        if (row->call_id != ast_node_stable_id(row->call) || row->site_id != ast_node_stable_id(row->site) ||
            !publication_ref_call_tuple(ctx, row->call, row->site, row->callee, row->ordinal, &formal) ||
            row->formal_ordinal != formal || row->formal_id != ast_func_param_stable_id(ast_func_param(row->callee, formal)) ||
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
