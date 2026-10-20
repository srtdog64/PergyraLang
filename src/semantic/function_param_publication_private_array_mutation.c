/* Private Array mutation publication: the ArrayPush, ArraySet, ArrayPop and
 * ArraySort calls on a local Array literal that nothing else holds, recorded
 * with the syntax they were proved on, and the checked Array stores. */
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_internal.h"
#include "type_checker_resolution_metadata_internal.h"
#include "type_checker_internal.h"
#include <stdlib.h>
#include <string.h>

bool publication_record_private_array_push(SemanticContext *ctx, ASTNode *call,
    PublishedNode *row)
{
    /* Private store is a dependency premise, not element cleanup authority. */
    if (ctx->has_error || row->argument_count != 2 || !row->stdlib || !row->kind_present ||
        row->builtin != BUILTIN_NOT_BUILTIN || row->declaration != 0 || row->value_binding != 0 ||
        row->target_shape != AST_IDENTIFIER || row->target_binding != 0 ||
        row->target_name == NULL || strcmp(row->target_name, "ArrayPush") != 0) return true;
    ASTNode *receiver = ast_call_argument(call, 0);
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? lookup_identifier_symbol(receiver, ctx) : NULL;
    const Type *element = binding == NULL ? NULL : type_get_constructed_arg(binding->type, 0);
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    uint32_t id = binding == NULL ? 0 : binding->decl_syntax_id;
    size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
        slot = (slot + 1) & (store->node_capacity - 1);
    const ASTNode *local = id != 0 && store->nodes[slot].id == id ? store->nodes[slot].node : NULL;
    const ASTNode *initializer = local != NULL && local->type == AST_LET_DECL ? ast_let_initializer(local) : NULL;
    PublishedNode *initial_value = publication_node(store, initializer, false);
    PublishedNode *receiver_value = publication_node(store, receiver, false);
    PublishedNode *payload_value = publication_node(store, ast_call_argument(call, 1), false);
    if (binding != NULL && !binding->is_parameter && !binding->is_host_field &&
        binding->has_exclusive_array_storage && !binding->has_escaped_array_storage &&
        !(binding->has_pending_inout_preservation || binding->has_pending_array_result) &&
        id == ast_identifier_binding_syntax_id(receiver) && row->deferred_syntax == NULL &&
        row->caller != NULL && row->value_present && row->value_type == TYPE_VOID &&
        type_is_constructed_named(binding->type, "Array") &&
        type_equals(row->arguments[0].type, binding->type) &&
        (type_equals(element, TYPE_STRING) || publication_physical_scalar(element)) &&
        local != NULL && !ast_let_is_alias(local) && initializer != NULL &&
        ast_let_type(local) != NULL && ast_let_type(local)->type == AST_TYPE &&
        ast_node_stable_id(ast_let_type(local)) != 0 &&
        semantic_type_resolution_lookup_resolved_type(ctx, ast_let_type(local)) == binding->type &&
        initializer->type == AST_ARRAY_LITERAL && initial_value != NULL &&
        initial_value->value_present && !initial_value->meta &&
        type_equals(initial_value->value_type, binding->type) &&
        type_equals(initial_value->string_array_type, type_equals(element, TYPE_STRING) ? binding->type : NULL) &&
        receiver_value != NULL && receiver_value->value_present && !receiver_value->meta &&
        type_equals(receiver_value->string_array_type, type_equals(element, TYPE_STRING) ? binding->type : NULL) &&
        payload_value != NULL && payload_value->value_present && !payload_value->meta &&
        payload_value->value_type == (type_equals(element, TYPE_STRING) ? TYPE_STRING : element)) {
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
        row->private_array_push = true;
    }
    return true;
}

bool semantic_param_publication_private_array_push(const SemanticContext *ctx,
    const ASTNode *call, const ASTNode *owner)
{
    PublishedNode *row = publication_result_current(ctx, call);
    if (row == NULL || !row->private_array_push || row->caller == NULL ||
        (owner != NULL && row->caller != owner) ||
        row->caller_id != ast_node_stable_id(row->caller) || !row->private_exclusive ||
        row->private_escaped || row->private_pending || row->argument_count != 2 ||
        row->value_type != TYPE_VOID ||
        !row->stdlib || !row->kind_present || row->builtin != BUILTIN_NOT_BUILTIN ||
        row->declaration != 0 || row->value_binding != 0 || row->target_binding != 0 ||
        row->target_shape != AST_IDENTIFIER || row->target_name == NULL ||
        strcmp(row->target_name, "ArrayPush") != 0 || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
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
    size_t slot = (id * (size_t)2654435761u) & (store->node_capacity - 1);
    while (store->nodes[slot].id != 0 && store->nodes[slot].id != id)
        slot = (slot + 1) & (store->node_capacity - 1);
    const ASTNode *local = id != 0 && store->nodes[slot].id == id ? store->nodes[slot].node : NULL;
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
