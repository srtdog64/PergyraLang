/* One all-typed publication boundary. Pending rows own diagnostic strings,
 * borrow compilation-owned AST/Type identity, and never retain scope Symbols. */
#include "function_param_publication_internal.h"
#include "../typed_map_query_fact_owner.h"
#include "../type_checker/type_checker_internal.h"
#include "../type_checker/type_checker_resolution_internal.h"
#include "../type_checker/type_checker_ownership_internal.h"
#include "../slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "../callable_capability_inference.h"
#include "../region_retention_summary.h"
#include "../indexed_string_borrow_owner.h"
#include "../collection_ownership_fact.h"
#include "../../common/string_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
        ast_node_stable_id(ast_func_return_type(decl)), decl->is_async_decl,
        false};
    node->routine_index = store->routine_count;
    return true;
}
static bool publication_routine_inputs_replay(const PendingRoutine *row,
    const ASTNode *decl, bool require_body)
{
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

bool semantic_param_publication_routine_inputs_current(const SemanticContext *ctx,
    const ASTNode *decl, bool require_body)
{
    if (!publication_generation_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    const PendingRoutine *row = publication_routine(store,
        ast_node_stable_id(decl));
    if (row == NULL) return false;
    if (store->inputs_ready) {
        if (!row->inputs_current_at_seal) return false;
        if (!store->verify_currency_memo)
            return !require_body || (row->body != NULL && row->body_id != 0);
    }
    return publication_routine_inputs_replay(row, decl, require_body);
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
bool semantic_param_publication_routine_return_storage_free(const SemanticContext *ctx, const ASTNode *decl)
{
    if (!semantic_param_publication_routine_relation_header_ready(ctx, decl)) return false;
    const PendingRoutine *row = publication_routine(ctx->function_param_publications, ast_node_stable_id(decl));
    return row->return_type == TYPE_VOID || publication_physical_scalar(row->return_type);
}
bool semantic_param_publication_owned_sink_formal_current(
    const SemanticContext *ctx, const ASTNode *decl, size_t ordinal)
{
    if (!semantic_param_publication_routine_return_storage_free(ctx, decl))
        return false;
    const PendingRoutine *row = publication_routine(
        ctx->function_param_publications, ast_node_stable_id(decl));
    if (row == NULL || ordinal >= row->count || row->formals == NULL ||
        row->types == NULL || row->formals[ordinal].mode != PARAM_MODE_OWN)
        return false;
    for (size_t p = 0; p < row->count; p++) {
        if (p == ordinal || row->formals[p].mode == PARAM_MODE_OWN ||
            row->formals[p].mode == PARAM_MODE_REF)
            continue;
        if (row->formals[p].mode != PARAM_MODE_DEFAULT ||
            row->types[p] == NULL || row->types[p] == TYPE_UNKNOWN ||
            !publication_physical_scalar(row->types[p]))
            return false;
    }
    return true;
}
bool semantic_param_publication_routine_return_dependency_is_scalar(
    const SemanticContext *ctx, const ASTNode *decl)
{
    return semantic_param_publication_routine_no_output_header_ready(ctx, decl) &&
        semantic_param_publication_routine_return_storage_free(ctx, decl);
}
bool semantic_param_publication_value_is_physical_scalar(const SemanticContext *ctx, const ASTNode *value)
{
    PublishedNode *row = publication_current_value(ctx, value);
    return row != NULL && row->physical_scalar && publication_physical_scalar(row->value_type);
}

bool semantic_param_publication_iteration_binding_shares_backing(
    const SemanticContext *ctx, const ASTNode *loop, bool *shares)
{
    if (ctx == NULL || loop == NULL || loop->type != AST_FOR_LOOP ||
        shares == NULL || ast_node_stable_id(loop) == 0 ||
        ast_for_variable(loop) == NULL)
        return false;
    ASTNode *iterable = ast_for_iterable(loop);
    if (iterable == NULL) {
        *shares = false;
        return ast_for_range_start(loop) != NULL &&
            ast_for_range_end(loop) != NULL;
    }
    SemanticPublishedValue value = semantic_param_publication_value(ctx,
        iterable);
    const Type *element = value.ok && !value.meta
        ? type_get_constructed_arg(value.type, 0) : NULL;
    if (element == NULL)
        return false;
    *shares = !type_equals(element, TYPE_STRING) &&
        semantic_classify_ownership_type(element, ctx) !=
            OWNERSHIP_TYPE_COPY_ONLY;
    return true;
}
bool semantic_param_publication_physical_array_element_value(
    const SemanticContext *ctx, const ASTNode *value)
{
    if (value == NULL || value->type != AST_ARRAY_ACCESS ||
        !semantic_param_publication_value_is_physical_scalar(ctx, value)) return false;
    ASTNode *array = ast_array_access_array(value);
    ASTNode *index = ast_array_access_index(value);
    if (array == NULL || index == NULL || ast_node_stable_id(array) == 0 ||
        ast_node_stable_id(index) == 0) return false;
    SemanticPublishedValue element_value = semantic_param_publication_value(ctx, value);
    SemanticPublishedValue receiver = semantic_param_publication_value(ctx, array);
    SemanticPublishedValue offset = semantic_param_publication_value(ctx, index);
    const Type *element = receiver.ok && !receiver.meta &&
        type_is_constructed_named(receiver.type, "Array")
        ? type_get_constructed_arg(receiver.type, 0) : NULL;
    return element_value.ok && !element_value.meta && element != NULL &&
        publication_physical_scalar(element) &&
        type_equals(element, element_value.type) && offset.ok && !offset.meta &&
        type_equals(offset.type, TYPE_INT) &&
        semantic_param_publication_value_is_physical_scalar(ctx, index);
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
static void
publication_pending_borrowed_call_retire(PendingRefCall *row)
{
    if (row == NULL) return;
    free(row->borrowed);
    free(row->destination);
    free(row->fix);
    free(row->module);
    free(row->target_name);
    *row = (PendingRefCall){0};
}

static bool
publication_pending_borrowed_call_build(SemanticContext *ctx,
    ASTNode *call, ASTNode *site, ASTNode *callee, size_t ordinal,
    ParamMode mode, const Type *type, const char *borrowed,
    const char *destination, const char *fix, PendingRefCall *row)
{
    size_t formal = 0;
    bool tuple_current = mode == PARAM_MODE_REF
        ? publication_ref_call_tuple(
            ctx, call, site, callee, ordinal, &formal)
        : mode == PARAM_MODE_DEFAULT
            && publication_default_borrow_call_tuple(
                ctx, call, site, callee, ordinal, &formal);
    if (row == NULL || !semantic_param_publication_pending(ctx) ||
        !tuple_current ||
        (ctx->current_function_decl != NULL && ast_node_stable_id(ctx->current_function_decl) == 0) ||
        type == NULL || borrowed == NULL) return false;
    *row = (PendingRefCall){.call = call, .site = site, .callee = callee,
        .caller = ctx->current_function_decl, .ordinal = ordinal, .type = type,
        .borrowed = pergyra_strdup(borrowed), .destination = pergyra_strdup(destination),
        .fix = pergyra_strdup(fix), .call_id = ast_node_stable_id(call),
        .site_id = ast_node_stable_id(site), .callee_id = ast_node_stable_id(callee),
        .caller_id = ast_node_stable_id(ctx->current_function_decl),
        .module = pergyra_strdup(ctx->current_module_path), .call_target = ast_call_callee(call),
        .call_target_id = ast_node_stable_id(ast_call_callee(call)),
        .argument_count = ast_call_arg_count(call)};
    row->formal_ordinal = formal;
    row->formal_id = ast_func_param_stable_id(ast_func_param(callee, formal));
    row->site_shape = site->type;
    row->site_binding_id = ast_identifier_binding_syntax_id(site);
    row->target_shape = row->call_target == NULL ? 0 : row->call_target->type;
    row->target_binding_id = ast_identifier_binding_syntax_id(row->call_target);
    row->call_receiver = row->target_shape == AST_MEMBER_ACCESS
        ? ast_member_object(row->call_target) : NULL;
    row->call_receiver_id = ast_node_stable_id(row->call_receiver);
    PublishedNode *receiver = publication_node(
        ctx->function_param_publications, row->call_receiver, false);
    row->call_receiver_type = receiver != NULL && receiver->value_present &&
        receiver->id == row->call_receiver_id && !receiver->meta
        ? receiver->value_type
        : publication_node(ctx->function_param_publications, call, false) != NULL
            ? publication_node(ctx->function_param_publications, call, false)->selected_receiver_type
            : NULL;
    const char *target_name = row->target_shape == AST_IDENTIFIER
        ? ast_identifier_name(row->call_target)
        : row->target_shape == AST_MEMBER_ACCESS
            ? ast_member_name(row->call_target) : NULL;
    row->target_name = pergyra_strdup(target_name);
    if (row->call_target == NULL || row->call_target_id == 0 ||
        (row->call_target->type != AST_IDENTIFIER && row->call_target->type != AST_MEMBER_ACCESS) ||
        row->formal_id == 0 || target_name == NULL || row->target_name == NULL ||
        (row->call_receiver != NULL && row->call_receiver_id == 0) ||
        row->borrowed == NULL ||
        (destination != NULL && row->destination == NULL) ||
        (fix != NULL && row->fix == NULL) ||
        (ctx->current_module_path != NULL && row->module == NULL)) {
        publication_pending_borrowed_call_retire(row);
        return false;
    }
    return true;
}

bool semantic_param_publication_record_call(SemanticContext *ctx,
    ASTNode *call, ASTNode *site, ASTNode *callee, size_t ordinal,
    const Type *type, const char *borrowed, const char *destination,
    const char *fix)
{
    PendingRefCall row = {0};
    if (!publication_pending_borrowed_call_build(ctx, call, site, callee,
            ordinal, PARAM_MODE_REF, type, borrowed, destination, fix, &row))
        return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (!semantic_param_publication_reserve_storage((void **)&store->calls,
            &store->call_capacity, store->call_count,
            sizeof(*store->calls))) {
        publication_pending_borrowed_call_retire(&row);
        return false;
    }
    store->calls[store->call_count++] = row;
    return true;
}

bool semantic_param_publication_record_default_borrow_call(
    SemanticContext *ctx, ASTNode *call, ASTNode *site, ASTNode *callee,
    size_t ordinal, const Type *type, const char *borrowed,
    const char *destination, const char *fix)
{
    PendingBorrowedDefaultCall row = {0};
    if (!publication_pending_borrowed_call_build(ctx, call, site, callee,
            ordinal, PARAM_MODE_DEFAULT, type, borrowed, destination, fix,
            &row))
        return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    if (!semantic_param_publication_reserve_storage(
            (void **)&store->default_borrow_calls,
            &store->default_borrow_call_capacity,
            store->default_borrow_call_count,
            sizeof(*store->default_borrow_calls))) {
        publication_pending_borrowed_call_retire(&row);
        return false;
    }
    store->default_borrow_calls[store->default_borrow_call_count++] = row;
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
        !semantic_param_publication_routine_inputs_current(ctx, row->caller, true) ||
        semantic_param_publication_callee(ctx, row->call, row->ordinal) != row->callee) return false;
    PublishedNode *actual = publication_current_value(ctx, row->site);
    PublishedNode *result = publication_current_value(ctx, row->call);
    const PendingRoutine *callee = publication_routine(ctx->function_param_publications, row->callee_id);
    if (actual == NULL || result == NULL || callee == NULL || formal >= callee->count ||
        !type_equals(result->value_type, callee->return_type)) return false;
    if (type_equals(row->type, TYPE_STRING))
        return actual->string_value && type_equals(callee->types[formal], TYPE_STRING);
    /* A borrowed aggregate is lent by its formal's type; the loan walk then
     * proves the result as a `ref String`'s. */
    return actual->category == (uint32_t)OWNERSHIP_TYPE_BORROW_TRACKED &&
        type_equals(actual->value_type, row->type) && type_equals(callee->types[formal], row->type);
}

bool semantic_param_publication_default_borrow_call_current(
    const SemanticContext *ctx, const PendingBorrowedDefaultCall *row,
    size_t *formal_out)
{
    size_t formal = 0;
    if (formal_out == NULL || row == NULL ||
        !semantic_param_publication_default_borrow_call_diagnostic_current(
            ctx, row, &formal)) {
        return false;
    }
    PublishedNode *actual = publication_current_value(ctx, row->site);
    const PendingRoutine *callee = publication_routine(
        ctx->function_param_publications, row->callee_id);
    if (actual == NULL || callee == NULL || formal >= callee->count ||
        actual->category != (uint32_t)OWNERSHIP_TYPE_BORROW_TRACKED ||
        !type_equals(actual->value_type, row->type) ||
        !type_equals(callee->types[formal], row->type)) {
        return false;
    }
    *formal_out = formal;
    return true;
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
    for (size_t i = 0; i < store->body_call_count; i++) {
        const PendingBodyCall *prior = &store->body_calls[i];
        if (prior->call != call || prior->caller != ctx->current_function_decl)
            continue;
        /* A repeated semantic visit does not mint a second edge.  The exact
         * syntax identity may be published only once per caller generation. */
        return prior->call_id == ast_node_stable_id(call) &&
            prior->caller_id == ast_node_stable_id(ctx->current_function_decl) &&
            prior->callee_id == ast_call_semantic_callee_decl_id(call) &&
            prior->caller_index == 0 && prior->callee_index == 0;
    }
    if (!semantic_param_publication_reserve_storage((void **)&store->body_calls, &store->body_call_capacity,
            store->body_call_count, sizeof(*store->body_calls))) return false;
    store->body_calls[store->body_call_count++] = (PendingBodyCall){call, ctx->current_function_decl,
        ast_node_stable_id(call), ast_node_stable_id(ctx->current_function_decl),
        ast_call_semantic_callee_decl_id(call), 0, 0};
    return true;
}

static bool publication_body_call_row_current(
    const SemanticContext *ctx, const PendingBodyCall *row)
{
    if (row == NULL || !semantic_param_publication_inputs_ready(ctx))
        return false;
    const FunctionParamPublicationStore *store =
        ctx->function_param_publications;
    if (row->call == NULL || row->call->type != AST_CALL ||
        row->caller == NULL || row->caller->type != AST_FUNC_DECL ||
        row->call_id == 0 || row->call_id != ast_node_stable_id(row->call) ||
        row->caller_id == 0 ||
        row->caller_id != ast_node_stable_id(row->caller) ||
        row->callee_id == 0 ||
        ast_call_semantic_callee_decl_id(row->call) != row->callee_id ||
        ast_call_semantic_callee_value_binding_id(row->call) != 0 ||
        row->caller_index >= store->routine_count ||
        row->callee_index >= store->routine_count)
        return false;
    const PendingRoutine *caller = &store->routines[row->caller_index];
    const PendingRoutine *callee = &store->routines[row->callee_index];
    return caller->decl == row->caller && caller->id == row->caller_id &&
        callee->decl != NULL && callee->id == row->callee_id &&
        ast_node_stable_id(callee->decl) == row->callee_id &&
        semantic_param_publication_routine_inputs_current(
            ctx, caller->decl, true) &&
        semantic_param_publication_routine_inputs_current(
            ctx, callee->decl, false);
}

bool semantic_param_publication_body_call_at_current(
    const SemanticContext *ctx, size_t index, const PendingBodyCall **row)
{
    if (row == NULL || !semantic_param_publication_inputs_ready(ctx))
        return false;
    const FunctionParamPublicationStore *store =
        ctx->function_param_publications;
    if (index >= store->body_call_count ||
        !publication_body_call_row_current(ctx, &store->body_calls[index]))
        return false;
    *row = &store->body_calls[index];
    return true;
}

bool semantic_param_publication_body_call_current(
    const SemanticContext *ctx, const PendingBodyCall *row)
{
    if (row == NULL || !semantic_param_publication_inputs_ready(ctx))
        return false;
    const FunctionParamPublicationStore *store =
        ctx->function_param_publications;
    for (size_t i = 0; i < store->body_call_count; i++) {
        const PendingBodyCall *owned = NULL;
        if (&store->body_calls[i] == row)
            return semantic_param_publication_body_call_at_current(
                ctx, i, &owned) && owned == row;
    }
    return false;
}

PendingRoutine *publication_routine(FunctionParamPublicationStore *store, uint32_t id)
{
    PublishedNode *node = publication_node_by_id(store, id);
    if (node == NULL || node->routine_index == 0 ||
        node->routine_index > store->routine_count) return NULL;
    PendingRoutine *row = &store->routines[node->routine_index - 1];
    return row->id == id && row->decl == node->node && ast_node_stable_id(row->decl) == id ? row : NULL;
}

bool semantic_param_publication_body_actual_current(const SemanticContext *ctx,
    const ASTNode *caller, const ASTNode *call, size_t actual,
    bool *body_call, ASTNode **callee, size_t *formal)
{
    if (body_call == NULL || callee == NULL || formal == NULL) return false;
    *body_call = false;
    *callee = NULL;
    *formal = 0;
    if (!semantic_param_publication_inputs_ready(ctx) || caller == NULL ||
        caller->type != AST_FUNC_DECL || call == NULL || call->type != AST_CALL ||
        actual >= ast_call_arg_count(call)) return false;

    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *published = publication_node(store, call, false);
    if (published == NULL || !publication_call_ready(ctx, published))
        return false;
    if (published->caller != caller
        || published->caller_id != ast_node_stable_id(caller)) {
        bool caller_contains_lambda = false;
        bool lambda_contains_call = false;
        uint32_t nested_id = ast_node_stable_id(published->caller);
        uint32_t call_id = ast_node_stable_id(call);

        /* Lambdas are detached execution scopes inside the named routine,
         * but they are not PendingRoutine rows.  Calls without a selected
         * program body (builtins and other dedicated boundaries) therefore
         * remain outside this body-edge owner.  Prove both containment
         * relations by stable identity; a user routine with a body still
         * fails closed below because no lambda body edge was published. */
        if (published->caller == NULL
            || published->caller->type != AST_LAMBDA_EXPR
            || nested_id == 0 || call_id == 0
            || !ast_subtree_intersects_stable_ids(
                ast_func_body(caller), &nested_id, 1,
                &caller_contains_lambda)
            || !caller_contains_lambda
            || !ast_subtree_intersects_stable_ids(
                published->caller, &call_id, 1,
                &lambda_contains_call)
            || !lambda_contains_call)
            return false;
    }

    const PendingBodyCall *edge = NULL;
    size_t body_call_index = published->body_call_index_plus_one == 0
        ? SIZE_MAX : published->body_call_index_plus_one - 1;
    if (body_call_index != SIZE_MAX &&
        (!semantic_param_publication_body_call_at_current(
             ctx, body_call_index, &edge) ||
         edge->call != call || edge->caller != caller))
        return false;
    if (store->verify_currency_memo) {
        const PendingBodyCall *replayed = NULL;
        size_t replayed_index = SIZE_MAX;
        for (size_t i = 0; i < store->body_call_count; i++) {
            const PendingBodyCall *candidate = &store->body_calls[i];
            if (candidate->call != call || candidate->caller != caller)
                continue;
            if (replayed != NULL)
                return false;
            replayed = candidate;
            replayed_index = i;
        }
        if (replayed != edge || replayed_index != body_call_index)
            return false;
    }
    if (edge == NULL) {
        /* A body-bearing selected routine must have been published as one
         * exact body edge. Builtins, value calls and bodyless declarations
         * stay outside this owner and keep their dedicated consumers. */
        if (published->declared_callable && published->declaration != 0 &&
            published->value_binding == 0 && published->selected_callable != NULL &&
            ast_func_body(published->selected_callable) != NULL)
            return false;
        return true;
    }

    PendingRoutine *callee_row = &store->routines[edge->callee_index];
    if (callee_row->body == NULL) return true;

    if (!published->declared_callable ||
        published->declaration != edge->callee_id ||
        published->selected_callable != callee_row->decl ||
        published->selected_callable_id != edge->callee_id ||
        published->argument_count != ast_call_arg_count(call)) return false;
    const PublishedCallArgument *argument = &published->arguments[actual];
    if (argument->node != ast_call_argument(call, actual) ||
        argument->id == 0 || argument->id != ast_node_stable_id(argument->node) ||
        argument->formal_ordinal >= callee_row->count ||
        argument->formal_id == 0 ||
        argument->formal_id != callee_row->formals[argument->formal_ordinal].id ||
        argument->formal_id != ast_func_param_stable_id(
            ast_func_param(callee_row->decl, argument->formal_ordinal)))
        return false;

    *body_call = true;
    *callee = callee_row->decl;
    *formal = argument->formal_ordinal;
    return true;
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
    if (!publication_call_ready(ctx, row) || !row->value_present || row->meta || row->value_type != type ||
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
            (arg->shape == AST_CALL && !publication_call_ready(ctx, value))) return NULL;
    }
    return row;
}
bool semantic_param_publication_checked_readfile_independent(
    const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *row = publication_checked_intrinsic(ctx, call,
        BUILTIN_READ_FILE, TYPE_STRING, 1);
    if (row == NULL || row->declared_callable || row->selected_callable != NULL ||
        row->selected_callable_id != 0 || row->result_relation ||
        strcmp(row->target_name, "ReadFile") != 0 ||
        row->arguments[0].type != TYPE_STRING) return false;
    const char *result_name = ast_call_semantic_value_type_name(call);
    const ASTNode *argument = row->arguments[0].node;
    const char *argument_name = argument->type == AST_CALL
        ? ast_call_semantic_value_type_name(argument) : NULL;
    return (result_name == NULL || strcmp(result_name, TYPE_STRING->name) == 0) &&
        (argument_name == NULL || strcmp(argument_name, TYPE_STRING->name) == 0);
}
static PublishedNode *publication_allocator_binding(const SemanticContext *ctx, uint32_t id)
{
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    return publication_node_by_id(store, id);
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
/* The call's Allocator argument at `ordinal` names a local this routine
 * made with AllocatorResult() and never rebound or handed to a call that
 * could rebind it: a result lane, whose finished text owns heap storage. */
static bool publication_result_lane_argument(const SemanticContext *ctx, const PublishedNode *call_row,
    size_t ordinal)
{
    if (call_row == NULL || ordinal >= call_row->argument_count ||
        call_row->arguments[ordinal].type != TYPE_ALLOCATOR ||
        call_row->arguments[ordinal].shape != AST_IDENTIFIER) return false;
    const PublishedCallArgument *arg = &call_row->arguments[ordinal];
    PublishedNode *row = publication_allocator_binding(ctx, arg->binding);
    if (row == NULL || !row->result_allocator_binding || row->allocator_invalidated ||
        row->node == NULL || row->node->type != AST_LET_DECL || row->id != ast_node_stable_id(row->node) ||
        row->id != arg->binding || ast_let_is_alias(row->node) || row->allocator_type != TYPE_ALLOCATOR ||
        row->target_name == NULL || ast_let_name(row->node) == NULL ||
        strcmp(row->target_name, ast_let_name(row->node)) != 0 || ast_identifier_name(arg->node) == NULL ||
        strcmp(row->target_name, ast_identifier_name(arg->node)) != 0 || row->caller == NULL ||
        row->caller != call_row->caller || row->caller_id != ast_node_stable_id(row->caller) ||
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
bool semantic_param_publication_checked_finish_independent(const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *finish = publication_checked_intrinsic(ctx, call, BUILTIN_TEXT_BUILDER_FINISH, TYPE_STRING, 2);
    return finish != NULL && finish->arguments[0].type == TYPE_TEXT_BUILDER &&
        publication_result_lane_argument(ctx, finish, 1);
}
bool semantic_param_publication_result_lane_argument(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    return semantic_param_publication_checked_call(ctx, call) &&
        publication_result_lane_argument(ctx, publication_result_current(ctx, call), ordinal);
}
bool semantic_param_publication_pending(const SemanticContext *ctx)
{ return publication_generation_ready(ctx) && ctx->function_param_publications->pending; }
bool semantic_param_publication_inputs_ready(const SemanticContext *ctx)
{ return publication_generation_ready(ctx) && ctx->function_param_publications->inputs_ready; }
bool semantic_param_publication_checked_call(const SemanticContext *ctx, const ASTNode *call)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    return publication_call_ready(ctx, row);
}
bool semantic_param_publication_checked_argument(const SemanticContext *ctx, const ASTNode *call, size_t ordinal)
{
    return semantic_param_publication_checked_call(ctx, call) && ordinal < ast_call_arg_count(call);
}
bool semantic_param_publication_checked_none(const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *row = publication_result_current(ctx, call);
    return row != NULL && row->nominal_result == NULL && !row->result_relation &&
        row->target_shape == AST_IDENTIFIER && row->kind_present && row->stdlib &&
        row->builtin == BUILTIN_NOT_BUILTIN && row->declaration == 0 && row->value_binding == 0 &&
        row->argument_count == 0 && ast_call_arg_count(call) == 0 && row->target_name != NULL &&
        strcmp(row->target_name, "None") == 0 && type_is_constructed_named(row->value_type, "Option");
}
bool semantic_param_publication_call_analysis_unmodelled(
    const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || row->nominal_result != NULL || row->result_relation) return false;
    if (row->kind_present && row->builtin == BUILTIN_TEXT_BUILDER_FINISH) {
        bool depends = false, retains = false;
        return !semantic_param_publication_checked_result_relation(ctx, call, 1, &depends, &retains);
    }
    if (semantic_param_publication_checked_none(ctx, call)) return false;
    if (semantic_param_publication_checked_local_owner_constructor(ctx, call)) return false;
    if (row->selected_callable != NULL &&
        semantic_param_publication_direct_callee(ctx, call) != NULL) return false;
    if (row->target_shape == AST_IDENTIFIER && row->kind_present &&
        row->builtin == BUILTIN_NOT_BUILTIN && row->stdlib &&
        row->declaration == 0 && row->value_binding == 0 &&
        row->value_type != TYPE_VOID && !publication_physical_scalar(row->value_type) &&
        !row->origin_ready && !(row->string_value && semantic_indexed_string_independent_result(call, ctx)) &&
        !semantic_typed_map_query_fact_issued(ctx, call) &&
        !semantic_typed_map_query_result_independent(ctx, call)) {
        /* A checked relation for every argument models the call. */
        bool related = row->argument_count != 0;
        for (size_t i = 0; related && i < row->argument_count; i++) {
            bool depends = false, retains = false;
            related = semantic_param_publication_checked_backing_relation(ctx, call, i, &depends, &retains);
        }
        return !related;
    }
    if (semantic_param_publication_slice_view_receiver(ctx, call) != NULL) return false;
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
        store->routine_count, store->calls, store->call_count,
        store->default_borrow_calls, store->default_borrow_call_count,
        store->body_calls, store->body_call_count,
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
        view->default_borrow_calls == store->default_borrow_calls &&
        view->default_borrow_call_count == store->default_borrow_call_count &&
        view->read_loans == store->read_loans && view->read_loan_count == store->read_loan_count &&
        view->body_calls == store->body_calls && view->body_call_count == store->body_call_count;
}
bool semantic_param_publication_seal_inputs(SemanticContext *ctx)
{
    if (!semantic_param_publication_pending(ctx) || ctx->function_param_flow_summaries != NULL) return false;
    if (!semantic_typed_map_query_facts_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    store->verify_currency_memo =
        getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") != NULL;
    for (size_t n = 0; n < store->node_capacity; n++) {
        PublishedNode *row = &store->nodes[n];
        if (row->id == 0) continue;
        if (row->id != ast_node_stable_id(row->node) ||
            (row->call_present && !publication_call_ready(ctx, row))) return false;
        if (row->call_present) row->call_current_at_seal = true;
    }
    for (size_t r = 0; r < store->routine_count; r++) {
        PendingRoutine *row = &store->routines[r];
        if (!semantic_param_publication_routine_inputs_current(ctx, row->decl, false)) return false;
        row->inputs_current_at_seal = true;
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
    for (size_t c = 0; c < store->default_borrow_call_count; c++) {
        PendingBorrowedDefaultCall *row = &store->default_borrow_calls[c];
        size_t formal = 0;
        if (row->call_id != ast_node_stable_id(row->call) ||
            row->site_id != ast_node_stable_id(row->site) ||
            !publication_default_borrow_call_tuple(ctx, row->call, row->site,
                row->callee, row->ordinal, &formal) ||
            row->formal_ordinal != formal ||
            row->formal_id != ast_func_param_stable_id(
                ast_func_param(row->callee, formal)) ||
            row->call_target != ast_call_callee(row->call) ||
            row->call_target_id != ast_node_stable_id(row->call_target) ||
            row->argument_count != ast_call_arg_count(row->call) ||
            row->callee_id != ast_node_stable_id(row->callee) ||
            publication_routine(store, row->callee_id) == NULL ||
            (row->caller != NULL &&
             row->caller_id != ast_node_stable_id(row->caller))) {
            return false;
        }
    }
    for (size_t b = 0; b < store->body_call_count; b++) {
        PendingBodyCall *row = &store->body_calls[b];
        PendingRoutine *caller = publication_routine(store, row->caller_id);
        PendingRoutine *callee = publication_routine(store, row->callee_id);
        PublishedNode *published = publication_node(store, row->call, false);
        if (row->call_id != ast_node_stable_id(row->call) || row->call->type != AST_CALL ||
            row->caller_id != ast_node_stable_id(row->caller) ||
            ast_call_semantic_callee_decl_id(row->call) != row->callee_id ||
            ast_call_semantic_callee_value_binding_id(row->call) != 0 ||
            caller == NULL || caller->decl != row->caller || callee == NULL ||
            published == NULL || !published->call_present ||
            published->node != row->call || published->id != row->call_id ||
            published->body_call_index_plus_one != 0) {
            return false;
        }
        row->caller_index = (size_t)(caller - store->routines);
        row->callee_index = (size_t)(callee - store->routines);
        published->body_call_index_plus_one = b + 1;
    }
    /* The seal above replayed the program generation and every published
     * input. The AST topology is immutable for the rest of this store's
     * lifetime; verification mode still replays the generation on each use. */
    store->program_current_at_seal = true;
    store->pending = false; store->inputs_ready = true;
    SemanticParamPublicationView view;
    return semantic_param_publication_admitted_view(ctx, &view);
}

bool publication_array_result_borrowed_formal(Type *function_type,
    const Type *formal_type, ParamMode mode, SemanticContext *ctx)
{
    const Type *result_type = function_type == NULL
        ? NULL : type_function_return_type(function_type);
    return type_is_constructed_named(result_type, "Array") &&
        (mode == PARAM_MODE_DEFAULT || mode == PARAM_MODE_MUT_REF) &&
        formal_type != NULL && formal_type != TYPE_UNKNOWN &&
        !publication_physical_scalar(formal_type) &&
        semantic_classify_ownership_type(formal_type, ctx) ==
            OWNERSHIP_TYPE_BORROW_TRACKED;
}

static bool
publication_flow_summary_demand(SemanticContext *ctx, ASTNode *decl,
    size_t formal, bool timing, bool *demanded, size_t *demand_count)
{
    if (ctx == NULL || decl == NULL || demanded == NULL ||
        demand_count == NULL) {
        return false;
    }
    if (!*demanded && !function_param_flow_publication_demand_begin(ctx, decl))
        return false;
    *demanded = true;
    (*demand_count)++;
    clock_t started = timing ? clock() : 0;
    size_t entries_before = ctx->function_param_flow_summaries != NULL
        ? ctx->function_param_flow_summaries->count : 0;
    size_t evaluations_before = ctx->function_param_flow_summaries != NULL
        ? ctx->function_param_flow_summaries->body_evaluations : 0;
    (void)function_param_flow_summary_for_param(ctx, decl, formal);
    if (timing) {
        double elapsed = (double)(clock() - started) /
            (double)CLOCKS_PER_SEC;
        FunctionParamFlowSummaryStore *flow =
            ctx->function_param_flow_summaries;
        if (elapsed >= 0.05)
            fprintf(stderr,
                "[semantic timing] flow_demand routine=%s formal=%zu"
                " elapsed=%.3f new_entries=%zu evaluations=%zu\n",
                ast_declaration_name(decl) != NULL
                    ? ast_declaration_name(decl) : "<unknown>",
                formal, elapsed,
                flow != NULL && flow->count >= entries_before
                    ? flow->count - entries_before : 0,
                flow != NULL && flow->body_evaluations >= evaluations_before
                    ? flow->body_evaluations - evaluations_before : 0);
    }
    return ctx->function_param_flow_summaries != NULL &&
        !ctx->function_param_flow_summaries->failed;
}

bool semantic_param_publication_prepare_flow_summaries(SemanticContext *ctx)
{
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    SemanticParamPublicationView view;
    if (store == NULL || store->pending || !store->inputs_ready ||
        store->finalized ||
        !semantic_param_publication_admitted_view(ctx, &view))
        return false;
    if (store->flow_summaries_prepared) return true;

    /* Publication order is the one preparation schedule for this sealed
     * generation. In particular, a landing query must not open a large
     * recursive closure early merely because it encountered one local. */
    bool demanded = false;
    bool timing = getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL;
    size_t demand_count = 0;
    clock_t demand_loop_started = timing ? clock() : 0;
    for (size_t r = 0; r < view.routine_count; r++) {
        const PendingRoutine *row = &view.routines[r];
        if (ast_func_body(row->decl) == NULL) continue;
        for (size_t p = 0; p < row->count; p++) {
            FuncParam *param = ast_func_param(row->decl, p);
            if (param == NULL || (param->mode != PARAM_MODE_REF &&
                !publication_array_result_borrowed_formal(row->function_type,
                    row->types[p], param->mode, ctx)))
                continue;
            if (!publication_flow_summary_demand(ctx, row->decl, p, timing,
                    &demanded, &demand_count))
                return false;
        }
    }
    for (size_t c = 0; c < view.default_borrow_call_count; c++) {
        const PendingBorrowedDefaultCall *row =
            &view.default_borrow_calls[c];
        size_t formal = 0;
        if (!semantic_param_publication_default_borrow_call_current(
                ctx, row, &formal) ||
            !publication_flow_summary_demand(ctx, row->callee, formal, timing,
                &demanded, &demand_count)) {
            return false;
        }
    }
    if (timing) {
        FunctionParamFlowSummaryStore *flow =
            ctx->function_param_flow_summaries;
        fprintf(stderr,
            "[semantic timing] flow_demand_loop=%.3f demands=%zu"
            " entries=%zu evaluations=%zu cache_hits=%zu recursion_hits=%zu"
            " fixed_point_passes=%zu work_units=%zu frame_visits=%zu"
            " indexed_statements=%zu indexed_points=%zu\n",
            (double)(clock() - demand_loop_started) /
                (double)CLOCKS_PER_SEC,
            demand_count, flow != NULL ? flow->count : 0,
            flow != NULL ? flow->body_evaluations : 0,
            flow != NULL ? flow->cache_hits : 0,
            flow != NULL ? flow->recursion_hits : 0,
            flow != NULL ? flow->fixed_point_passes : 0,
            flow != NULL ? flow->work_units : 0,
            flow != NULL ? flow->closure_frame_node_visits : 0,
            flow != NULL ? flow->indexed_statement_visits : 0,
            flow != NULL ? flow->indexed_program_points : 0);
    }
    clock_t result_demands_started = timing ? clock() : 0;
    bool result_demands_ready =
        semantic_collection_owned_string_result_flow_demands_prepare(ctx);
    bool publication_demand_ready = !demanded ||
        function_param_flow_publication_demand_end(ctx);
    if (!result_demands_ready || !publication_demand_ready)
        return false;
    if (timing)
        fprintf(stderr,
            "[semantic timing] owned_result_flow_demands=%.3f\n",
            (double)(clock() - result_demands_started) /
                (double)CLOCKS_PER_SEC);
    if (demanded && ctx->function_param_flow_summaries == NULL) return false;
    store->flow_summaries_prepared = true;
    return true;
}

bool semantic_param_publication_finalize(SemanticContext *ctx)
{
    FunctionParamPublicationStore *store = ctx != NULL
        ? ctx->function_param_publications : NULL;
    SemanticParamPublicationView view;
    if (store == NULL || store->pending || !store->inputs_ready
        || store->finalized
        || !semantic_param_publication_admitted_view(ctx, &view))
        return false;
    if (!semantic_param_publication_prepare_flow_summaries(ctx)) return false;
    if (!semantic_consume_param_publication(ctx, &view)) return false;
    store->finalized = !ctx->has_error;
    return !ctx->has_error;
}
