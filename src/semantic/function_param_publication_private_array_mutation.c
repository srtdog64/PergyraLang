/* Private Array mutation publication: the ArrayPush, ArraySet, ArrayPop and
 * ArraySort calls on a local Array literal that nothing else holds, recorded
 * with the syntax they were proved on, and the checked Array stores. */
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_internal.h"
#include "type_checker_resolution_metadata_internal.h"
#include "type_checker_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool publication_record_private_array_push(SemanticContext *ctx, ASTNode *call,
    PublishedNode *row)
{
    /* Private store is a dependency premise, not element cleanup authority. */
    bool push = row->target_name != NULL &&
        strcmp(row->target_name, "ArrayPush") == 0 &&
        row->argument_count == 2;
    bool set = row->target_name != NULL &&
        strcmp(row->target_name, "ArraySet") == 0 &&
        row->argument_count == 3;
    size_t stored = set ? 2 : 1;
    if (ctx->has_error || (!push && !set) || !row->stdlib || !row->kind_present ||
        row->builtin != BUILTIN_NOT_BUILTIN || row->declaration != 0 || row->value_binding != 0 ||
        row->target_shape != AST_IDENTIFIER || row->target_binding != 0 ||
        row->target_name == NULL) return true;
    ASTNode *receiver = ast_call_argument(call, 0);
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    const Type *element = binding == NULL ? NULL : type_get_constructed_arg(binding->type, 0);
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    uint32_t id = binding == NULL ? 0 : binding->decl_syntax_id;
    PublishedNode *local_row = publication_node_by_id(store, id);
    const ASTNode *local = local_row != NULL ? local_row->node : NULL;
    const ASTNode *initializer = local != NULL && local->type == AST_LET_DECL ? ast_let_initializer(local) : NULL;
    PublishedNode *initial_value = publication_node(store, initializer, false);
    PublishedNode *receiver_value = publication_node(store, receiver, false);
    PublishedNode *index_value = set
        ? publication_node(store, ast_call_argument(call, 1), false) : NULL;
    PublishedNode *payload_value = publication_node(store,
        ast_call_argument(call, stored), false);
    bool private_receiver = binding != NULL && !binding->is_parameter &&
        !binding->is_host_field &&
        (!set || !publication_physical_scalar(element)) &&
        binding->has_exclusive_array_storage && !binding->has_escaped_array_storage &&
        !(binding->has_pending_inout_preservation || binding->has_pending_array_result) &&
        id == ast_identifier_binding_syntax_id(receiver) && row->deferred_syntax == NULL &&
        row->caller != NULL && row->value_present && row->value_type == TYPE_VOID &&
        type_is_constructed_named(binding->type, "Array") &&
        type_equals(row->arguments[0].type, binding->type) &&
        local != NULL && !ast_let_is_alias(local) && initializer != NULL &&
        ast_let_type(local) != NULL && ast_let_type(local)->type == AST_TYPE &&
        ast_node_stable_id(ast_let_type(local)) != 0 &&
        semantic_type_resolution_lookup_resolved_type(ctx, ast_let_type(local)) == binding->type &&
        initializer->type == AST_ARRAY_LITERAL && initial_value != NULL &&
        initial_value->value_present && !initial_value->meta &&
        type_equals(initial_value->value_type, binding->type) &&
        receiver_value != NULL && receiver_value->value_present && !receiver_value->meta &&
        type_equals(receiver_value->value_type, binding->type) &&
        payload_value != NULL && payload_value->value_present && !payload_value->meta &&
        type_equals(payload_value->value_type, element) &&
        (!set || (index_value != NULL && index_value->value_present &&
            !index_value->meta && index_value->physical_scalar &&
            type_equals(index_value->value_type, TYPE_INT) &&
            type_equals(row->arguments[1].type, TYPE_INT)));
    if (private_receiver) {
        FunctionParamFlowIndexClaims *syntax = NULL;
        if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, (ASTNode *)local) ||
            !function_param_flow_syntax_claims_snapshot(ctx, &syntax, call)) {
            function_param_flow_claims_destroy(syntax);
            return false;
        }
        row = publication_node(store, call, false);
        if (row == NULL) {
            function_param_flow_claims_destroy(syntax);
            return false;
        }
        row->deferred_syntax = syntax;
        row->private_binding = local; row->private_binding_id = id;
        row->private_binding_type_node = ast_let_type(local);
        row->private_binding_type_id = ast_node_stable_id(row->private_binding_type_node);
        row->private_initializer = initializer; row->private_initializer_id = ast_node_stable_id(initializer);
        row->private_exclusive = true; row->private_escaped = false; row->private_pending = false;
        row->private_array_store_receiver = true;
        row->private_array_push = push &&
            (type_equals(element, TYPE_STRING) || publication_physical_scalar(element)) &&
            type_equals(initial_value->string_array_type,
                type_equals(element, TYPE_STRING) ? binding->type : NULL) &&
            type_equals(receiver_value->string_array_type,
                type_equals(element, TYPE_STRING) ? binding->type : NULL);
    }
    return true;
}

bool semantic_param_publication_private_array_store_receiver(
    const SemanticContext *ctx, const ASTNode *call, const ASTNode *owner)
{
    PublishedNode *row = publication_result_current(ctx, call);
    bool push = row != NULL && row->target_name != NULL &&
        strcmp(row->target_name, "ArrayPush") == 0 &&
        row->argument_count == 2;
    bool set = row != NULL && row->target_name != NULL &&
        strcmp(row->target_name, "ArraySet") == 0 &&
        row->argument_count == 3;
    size_t stored = set ? 2 : 1;
    if (row == NULL || !row->private_array_store_receiver ||
        row->caller == NULL ||
        (owner != NULL && row->caller != owner) ||
        row->caller_id != ast_node_stable_id(row->caller) ||
        !row->private_exclusive || row->private_escaped ||
        row->private_pending || (!push && !set) ||
        row->value_type != TYPE_VOID || !row->stdlib ||
        !row->kind_present || row->builtin != BUILTIN_NOT_BUILTIN ||
        row->declaration != 0 || row->value_binding != 0 ||
        row->target_binding != 0 || row->target_shape != AST_IDENTIFIER ||
        row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx,
            row->deferred_syntax))
        return false;
    const ASTNode *local = row->private_binding;
    const ASTNode *initial = row->private_initializer;
    const ASTNode *receiver = ast_call_argument(call, 0);
    const ASTNode *index = set ? ast_call_argument(call, 1) : NULL;
    const ASTNode *payload = ast_call_argument(call, stored);
    if (local == NULL || local->type != AST_LET_DECL ||
        ast_let_is_alias(local) || ast_node_stable_id(local) !=
            row->private_binding_id ||
        initial == NULL || ast_let_type(local) !=
            row->private_binding_type_node ||
        row->private_binding_type_node == NULL ||
        row->private_binding_type_node->type != AST_TYPE ||
        ast_node_stable_id(row->private_binding_type_node) !=
            row->private_binding_type_id ||
        ast_let_initializer(local) != initial ||
        initial->type != AST_ARRAY_LITERAL ||
        ast_node_stable_id(initial) != row->private_initializer_id ||
        receiver == NULL || receiver->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(receiver) !=
            row->private_binding_id)
        return false;
    PublishedNode *iv = publication_current_value(ctx, initial);
    PublishedNode *rv = publication_current_value(ctx, receiver);
    PublishedNode *ix = set ? publication_current_value(ctx, index) : NULL;
    PublishedNode *pv = publication_current_value(ctx, payload);
    const Type *array = row->arguments[0].type;
    const Type *element = type_is_constructed_named(array, "Array")
        ? type_get_constructed_arg(array, 0) : NULL;
    return iv != NULL && rv != NULL && pv != NULL && !iv->meta &&
        !rv->meta && !pv->meta && type_equals(iv->value_type, array) &&
        type_equals(rv->value_type, array) && rv->array_binding_present &&
        !rv->array_binding_parameter && !rv->array_binding_host &&
        !rv->array_binding_consumed && rv->array_binding_exclusive &&
        !rv->array_binding_escaped && !rv->array_binding_pending_inout &&
        !rv->array_binding_pending_result &&
        type_equals(pv->value_type, element) &&
        (!set || (ix != NULL && ix->value_present && !ix->meta &&
            ix->physical_scalar && type_equals(ix->value_type, TYPE_INT) &&
            type_equals(row->arguments[1].type, TYPE_INT))) &&
        semantic_type_resolution_lookup_resolved_type(
            (SemanticContext *)ctx,
            (ASTNode *)row->private_binding_type_node) == array;
}

bool semantic_param_publication_private_array_push(const SemanticContext *ctx,
    const ASTNode *call, const ASTNode *owner)
{
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || !row->private_array_push ||
        !semantic_param_publication_private_array_store_receiver(
            ctx, call, owner)) return false;
    const ASTNode *local = row->private_binding, *initial = row->private_initializer;
    const ASTNode *receiver = ast_call_argument(call, 0);
    if (local == NULL || local->type != AST_LET_DECL || ast_let_is_alias(local) ||
        ast_node_stable_id(local) != row->private_binding_id || initial == NULL ||
        ast_let_type(local) != row->private_binding_type_node ||
        row->private_binding_type_node == NULL || row->private_binding_type_node->type != AST_TYPE ||
        ast_node_stable_id(row->private_binding_type_node) != row->private_binding_type_id ||
        ast_let_initializer(local) != initial || initial->type != AST_ARRAY_LITERAL ||
        ast_node_stable_id(initial) != row->private_initializer_id ||
        receiver == NULL || receiver->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(receiver) != row->private_binding_id) return false;
    PublishedNode *iv = publication_current_value(ctx, initial), *rv = publication_current_value(ctx, receiver);
    PublishedNode *pv = publication_current_value(ctx, ast_call_argument(call, 1));
    const Type *array = row->arguments[0].type;
    const Type *element = type_is_constructed_named(array, "Array") ? type_get_constructed_arg(array, 0) : NULL;
    if (iv == NULL || rv == NULL || pv == NULL || iv->meta || rv->meta || pv->meta ||
        !(type_equals(element, TYPE_STRING) || publication_physical_scalar(element)) ||
        pv->value_type != (type_equals(element, TYPE_STRING) ? TYPE_STRING : element) || (!type_equals(element, TYPE_STRING) && !pv->physical_scalar) ||
        !type_equals(iv->value_type, array) ||
        !type_equals(iv->string_array_type, type_equals(element, TYPE_STRING) ? array : NULL) ||
        !type_equals(rv->string_array_type, type_equals(element, TYPE_STRING) ? array : NULL) ||
        semantic_type_resolution_lookup_resolved_type((SemanticContext *)ctx,
            (ASTNode *)row->private_binding_type_node) != array) return false;
    return true;
}

bool semantic_param_publication_checked_array_store(const SemanticContext *ctx,
    const ASTNode *call, size_t *value_ordinal)
{
    PublishedNode *row = publication_result_current(ctx, call);
    if (value_ordinal == NULL || row == NULL || !row->stdlib || !row->kind_present ||
        row->builtin != BUILTIN_NOT_BUILTIN || row->declaration != 0 || row->value_binding != 0 ||
        row->target_binding != 0 || row->target_shape != AST_IDENTIFIER || row->target_name == NULL ||
        row->value_type != TYPE_VOID || row->argument_count == 0 ||
        !type_is_constructed_named(row->arguments[0].type, "Array")) return false;
    if (strcmp(row->target_name, "ArrayPush") == 0 && row->argument_count == 2) *value_ordinal = 1;
    else if (strcmp(row->target_name, "ArraySet") == 0 && row->argument_count == 3) *value_ordinal = 2;
    else return false;
    return ast_call_arg_count(call) == row->argument_count;
}

bool publication_record_private_physical_array_mutation(SemanticContext *ctx,
    ASTNode *call, PublishedNode *row)
{
    /* ArraySort permutes the scalars in place, like a run of ArraySet. It
     * also returns the Array, a result this route leaves unmodelled. */
    bool set = row->target_name != NULL && strcmp(row->target_name, "ArraySet") == 0;
    bool sort = row->target_name != NULL && strcmp(row->target_name, "ArraySort") == 0;
    bool pop = sort || (row->target_name != NULL && strcmp(row->target_name, "ArrayPop") == 0);
    if (ctx->has_error || (!set && !pop) || row->argument_count != (set ? 3u : 1u) ||
        !row->stdlib || !row->kind_present || row->builtin != BUILTIN_NOT_BUILTIN ||
        row->declaration != 0 || row->value_binding != 0 || row->target_shape != AST_IDENTIFIER ||
        row->target_binding != 0) return true;
    ASTNode *receiver = ast_call_argument(call, 0);
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    uint32_t id = binding == NULL ? 0 : binding->decl_syntax_id;
    PublishedNode *local_row = publication_node_by_id(store, id);
    const ASTNode *local = local_row != NULL ? local_row->node : NULL;
    const ASTNode *initial = local != NULL && local->type == AST_LET_DECL ? ast_let_initializer(local) : NULL;
    const Type *array = binding == NULL ? NULL : binding->type;
    const Type *element = type_is_constructed_named(array, "Array") ? type_get_constructed_arg(array, 0) : NULL;
    PublishedNode *iv = publication_node(store, initial, false);
    PublishedNode *rv = publication_node(store, receiver, false);
    PublishedNode *index = set ? publication_node(store, ast_call_argument(call, 1), false) : NULL;
    PublishedNode *payload = set ? publication_node(store, ast_call_argument(call, 2), false) : NULL;
    if (binding != NULL && !binding->is_parameter && !binding->is_host_field &&
        binding->has_exclusive_array_storage && !binding->has_escaped_array_storage &&
        !(binding->has_pending_inout_preservation || binding->has_pending_array_result) &&
        id == ast_identifier_binding_syntax_id(receiver) && row->deferred_syntax == NULL &&
        row->caller != NULL && row->value_present &&
        (row->value_type == TYPE_VOID || (sort && type_equals(row->value_type, array))) &&
        publication_physical_scalar(element) && type_equals(row->arguments[0].type, array) &&
        local != NULL && !ast_let_is_alias(local) && initial != NULL &&
        ast_let_type(local) != NULL && ast_let_type(local)->type == AST_TYPE &&
        ast_node_stable_id(ast_let_type(local)) != 0 &&
        semantic_type_resolution_lookup_resolved_type(ctx, ast_let_type(local)) == array &&
        initial->type == AST_ARRAY_LITERAL && iv != NULL && iv->value_present && !iv->meta &&
        type_equals(iv->value_type, array) && rv != NULL && rv->value_present && !rv->meta &&
        type_equals(rv->value_type, array) &&
        (!set || (index != NULL && index->value_present && !index->meta && index->physical_scalar &&
            type_equals(index->value_type, TYPE_INT) && payload != NULL && payload->value_present &&
            !payload->meta && payload->physical_scalar && type_equals(payload->value_type, element) &&
            type_equals(row->arguments[1].type, TYPE_INT) && type_equals(row->arguments[2].type, element)))) {
        FunctionParamFlowIndexClaims *syntax = NULL;
        if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, (ASTNode *)local) ||
            !function_param_flow_syntax_claims_snapshot(ctx, &syntax, call)) {
            function_param_flow_claims_destroy(syntax);
            return false;
        }
        row = publication_node(store, call, false);
        if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
        row->deferred_syntax = syntax;
        row->private_binding = local; row->private_binding_id = id;
        row->private_binding_type_node = ast_let_type(local);
        row->private_binding_type_id = ast_node_stable_id(row->private_binding_type_node);
        row->private_initializer = initial; row->private_initializer_id = ast_node_stable_id(initial);
        row->private_exclusive = true; row->private_escaped = false; row->private_pending = false;
        row->private_physical_mutation = true;
    }
    return true;
}

bool semantic_param_publication_private_physical_array_mutation(const SemanticContext *ctx,
    const ASTNode *call, const ASTNode *owner)
{
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || !row->private_physical_mutation || row->caller == NULL ||
        (owner != NULL && owner != row->caller) ||
        row->caller_id != ast_node_stable_id(row->caller) || !row->private_exclusive ||
        row->private_escaped || row->private_pending || !row->stdlib || !row->kind_present ||
        row->builtin != BUILTIN_NOT_BUILTIN || row->declaration != 0 || row->value_binding != 0 ||
        row->target_shape != AST_IDENTIFIER || row->target_binding != 0 ||
        row->target_name == NULL || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
    bool set = strcmp(row->target_name, "ArraySet") == 0;
    bool sort = strcmp(row->target_name, "ArraySort") == 0;
    if (!set && !sort && strcmp(row->target_name, "ArrayPop") != 0) return false;
    if (row->value_type != TYPE_VOID && !(sort && type_equals(row->value_type, row->arguments[0].type))) return false;
    if (row->argument_count != (set ? 3u : 1u)) return false;
    const ASTNode *local = row->private_binding, *initial = row->private_initializer;
    const ASTNode *receiver = ast_call_argument(call, 0);
    if (local == NULL || local->type != AST_LET_DECL || ast_let_is_alias(local) ||
        ast_node_stable_id(local) != row->private_binding_id || initial == NULL ||
        ast_let_type(local) != row->private_binding_type_node ||
        row->private_binding_type_node == NULL || row->private_binding_type_node->type != AST_TYPE ||
        ast_node_stable_id(row->private_binding_type_node) != row->private_binding_type_id ||
        ast_let_initializer(local) != initial || initial->type != AST_ARRAY_LITERAL ||
        ast_node_stable_id(initial) != row->private_initializer_id ||
        receiver == NULL || receiver->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(receiver) != row->private_binding_id) return false;
    PublishedNode *iv = publication_current_value(ctx, initial), *rv = publication_current_value(ctx, receiver);
    const Type *array = row->arguments[0].type;
    const Type *element = type_is_constructed_named(array, "Array") ? type_get_constructed_arg(array, 0) : NULL;
    if (iv == NULL || rv == NULL || iv->meta || rv->meta || !publication_physical_scalar(element) ||
        !type_equals(iv->value_type, array) || !type_equals(rv->value_type, array) ||
        semantic_type_resolution_lookup_resolved_type((SemanticContext *)ctx,
            (ASTNode *)row->private_binding_type_node) != array) return false;
    if (set) {
        PublishedNode *index = publication_current_value(ctx, ast_call_argument(call, 1));
        PublishedNode *payload = publication_current_value(ctx, ast_call_argument(call, 2));
        if (index == NULL || payload == NULL || index->meta || payload->meta ||
            !index->physical_scalar || !payload->physical_scalar ||
            !type_equals(index->value_type, TYPE_INT) || !type_equals(payload->value_type, element) ||
            !type_equals(row->arguments[1].type, TYPE_INT) ||
            !type_equals(row->arguments[2].type, element)) return false;
    }
    return true;
}

static bool private_array_literal_value_current(
    const SemanticContext *ctx, const ASTNode *value, const ASTNode *owner,
    const ASTNode *call, const ASTNode *callee_decl, size_t actual_ordinal,
    size_t formal_ordinal)
{
    if (!semantic_param_publication_inputs_ready(ctx) || value == NULL ||
        value->type != AST_IDENTIFIER || owner == NULL ||
        owner->type != AST_FUNC_DECL || ast_node_stable_id(owner) == 0)
        return false;
    PublishedNode *value_row = publication_current_value(ctx, value);
    uint32_t binding_id = value_row == NULL ? 0 : value_row->binding_id;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *local_row = binding_id == 0 ? NULL :
        publication_node_by_id(store, binding_id);
    const ASTNode *local = local_row != NULL ? local_row->node : NULL;
    const ASTNode *initializer = local != NULL && local->type == AST_LET_DECL
        ? ast_let_initializer(local) : NULL;
    PublishedNode *initializer_row = publication_current_value(ctx,
        initializer);
    const ASTNode *type_node = local != NULL && local->type == AST_LET_DECL
        ? ast_let_type(local) : NULL;
    bool current_value = value_row != NULL && value_row->value_present &&
        !value_row->meta;
    bool current_owner = current_value && value_row->caller == owner &&
        value_row->caller_id == ast_node_stable_id(owner) &&
        value_row->binding_id == binding_id;
    size_t selected_formal = SIZE_MAX;
    FuncParam *selected_parameter = callee_decl != NULL &&
        callee_decl->type == AST_FUNC_DECL &&
        formal_ordinal < ast_func_param_count(callee_decl)
        ? ast_func_param((ASTNode *)callee_decl, formal_ordinal) : NULL;
    bool pending_exact = current_owner &&
        value_row->array_binding_pending_inout && call != NULL &&
        call->type == AST_CALL && callee_decl != NULL &&
        semantic_param_publication_direct_callee(ctx, (ASTNode *)call) ==
            callee_decl &&
        actual_ordinal < ast_call_arg_count(call) &&
        ast_call_argument(call, actual_ordinal) == value &&
        semantic_param_publication_checked_argument(ctx, call,
            actual_ordinal) &&
        semantic_param_publication_formal_for_actual(ctx, call,
            actual_ordinal, &selected_formal) &&
        selected_formal == formal_ordinal && selected_parameter != NULL &&
        selected_parameter->mode == PARAM_MODE_MUT_REF &&
        ast_func_param_stable_id(selected_parameter) != 0;
    bool current_binding = current_value &&
        value_row->array_binding_present &&
        !value_row->array_binding_parameter && !value_row->array_binding_host &&
        value_row->array_binding_exclusive &&
        !value_row->array_binding_escaped &&
        (!value_row->array_binding_pending_inout || pending_exact) &&
        !value_row->array_binding_pending_result &&
        type_is_constructed_named(value_row->value_type, "Array");
    bool current_local = current_binding && local != NULL &&
        local->type == AST_LET_DECL && ast_node_stable_id(local) == binding_id &&
        !ast_let_is_alias(local) && initializer != NULL &&
        initializer->type == AST_ARRAY_LITERAL &&
        ast_node_stable_id(initializer) != 0;
    bool current_types = current_value && current_binding &&
        initializer_row != NULL && initializer_row->value_present &&
        !initializer_row->meta &&
        type_equals(value_row->value_type, initializer_row->value_type) &&
        type_node != NULL && type_node->type == AST_TYPE &&
        ast_node_stable_id(type_node) != 0 &&
        type_equals(semantic_type_resolution_lookup_resolved_type(
            (SemanticContext *)ctx, (ASTNode *)type_node), value_row->value_type);
    bool result = current_value && current_owner && current_binding &&
        current_local && current_types;
    if (!result && getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL)
        fprintf(stderr,
            "pgy: function-param-flow-private-array-literal-stale"
            " function=%s value=%s binding=%u value_current=%d owner=%d"
            " binding_current=%d local=%d types=%d exclusive=%d escaped=%d"
            " pending_inout=%d pending_exact=%d pending_result=%d\n",
            ast_declaration_name(owner), ast_identifier_name(value), binding_id,
            current_value, current_owner, current_binding, current_local,
            current_types,
            value_row != NULL && value_row->array_binding_exclusive,
            value_row != NULL && value_row->array_binding_escaped,
            value_row != NULL && value_row->array_binding_pending_inout,
            pending_exact,
            value_row != NULL && value_row->array_binding_pending_result);
    return result;
}

bool semantic_param_publication_private_array_literal_value(
    const SemanticContext *ctx, const ASTNode *value, const ASTNode *owner)
{
    return private_array_literal_value_current(ctx, value, owner, NULL, NULL,
        0, 0);
}

bool semantic_param_publication_private_array_literal_inout_actual_current(
    const SemanticContext *ctx, const ASTNode *value, const ASTNode *owner,
    const ASTNode *call, const ASTNode *callee_decl, size_t actual_ordinal,
    size_t formal_ordinal)
{
    return private_array_literal_value_current(ctx, value, owner, call,
        callee_decl, actual_ordinal, formal_ordinal);
}

bool semantic_param_publication_array_parameter_value_current(
    const SemanticContext *ctx, const ASTNode *value, const ASTNode *owner,
    size_t formal_index, ParamMode formal_mode)
{
    if (!semantic_param_publication_inputs_ready(ctx) || value == NULL ||
        value->type != AST_IDENTIFIER || owner == NULL ||
        owner->type != AST_FUNC_DECL || ast_node_stable_id(owner) == 0 ||
        formal_index >= ast_func_param_count(owner))
        return false;
    FuncParam *formal = ast_func_param(owner, formal_index);
    PublishedNode *row = publication_current_value(ctx, value);
    const Type *formal_type = formal != NULL
        ? semantic_type_resolution_lookup_resolved_type(
            (SemanticContext *)ctx, formal->type)
        : NULL;
    return formal != NULL && formal->mode == formal_mode &&
        ast_func_param_stable_id(formal) != 0 && row != NULL &&
        row->value_present && !row->meta && row->caller == owner &&
        row->caller_id == ast_node_stable_id(owner) &&
        row->binding_id == ast_func_param_stable_id(formal) &&
        row->array_binding_present && row->array_binding_parameter &&
        !row->array_binding_host && !row->array_binding_consumed &&
        type_is_constructed_named(row->value_type, "Array") &&
        type_equals(row->value_type, formal_type);
}
