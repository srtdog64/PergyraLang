/* Descriptor and element preservation over the same demanded indexed roots. */
#include "function_param_flow_summary_internal.h"
#include "function_param_publication_internal.h"
#include "type_checker_resolution_metadata_internal.h"
#include "type_checker_internal.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
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
    bool set = row->target_name != NULL && strcmp(row->target_name, "ArraySet") == 0;
    bool pop = row->target_name != NULL && strcmp(row->target_name, "ArrayPop") == 0;
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
        row->caller != NULL && row->value_present && row->value_type == TYPE_VOID &&
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
        row->target_name == NULL || row->value_type != TYPE_VOID || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
    bool set = strcmp(row->target_name, "ArraySet") == 0;
    if (!set && strcmp(row->target_name, "ArrayPop") != 0) return false;
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

static bool
array_storage_formal(const ASTNode *node, const SlotSummaryOrigin *origin,
                     const SlotFunctionLookup *lookup)
{
    if (node == NULL || node->type != AST_IDENTIFIER) return false;
    FuncParam *param = ast_func_param(origin->function_decl, origin->param_index);
    if (param == NULL || ast_func_param_stable_id(param) == 0
        || ast_identifier_binding_syntax_id(node) != ast_func_param_stable_id(param)) return false;
    SemanticPublishedValue value = semantic_param_publication_value(lookup->ctx, node);
    return value.ok && !value.meta && type_is_constructed_named(value.type, "Array");
}

static unsigned array_storage_use_unproved(ASTNode *node,
    const SlotSummaryOrigin *origin, const SlotFunctionLookup *lookup, bool *payload_read);

static unsigned
array_storage_call_argument_unproved(ASTNode *call, size_t ordinal,
    const SlotFunctionLookup *lookup)
{
    ASTNode *callee = ast_call_callee(call);
    uint32_t identity = ast_call_semantic_callee_decl_id(call);
    if (!semantic_param_publication_checked_argument(lookup->ctx, call, ordinal))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    if (semantic_stdlib_array_argument_preserves_storage(call, ordinal))
        return semantic_stdlib_array_argument_preserves_elements(call, ordinal, lookup->ctx)
            ? 0 : FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED;
    if (identity == 0 || ast_call_semantic_callee_value_binding_id(call) != 0
        || callee == NULL || callee->type != AST_IDENTIFIER)
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    ASTNode *decl = slot_analyzer_find_function_decl(lookup, ast_identifier_name(callee));
    if (decl == NULL || ast_node_stable_id(decl) != identity)
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return function_param_flow_array_preservation_failures(lookup->ctx, decl, ordinal);
}

static unsigned
array_storage_use_unproved(ASTNode *node, const SlotSummaryOrigin *origin,
                           const SlotFunctionLookup *lookup, bool *payload_read)
{
    if (node == NULL) return 0;
    unsigned bad = 0;
#define USE(child) array_storage_use_unproved((child), origin, lookup, payload_read)
#define FORMAL(child) array_storage_formal((child), origin, lookup)
#define ADD(child) do { bad |= USE(child); if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) return bad; } while (0)
#define EACH(count, child) do { for (size_t i = 0; i < (count); i++) { ADD(child); } return bad; } while (0)
    switch (node->type) {
    case AST_IDENTIFIER:
        /* A stale or shadowed spelling is not a proof of this formal. */
        if (ast_identifier_name(node) != NULL && origin->param_name != NULL
            && strcmp(ast_identifier_name(node), origin->param_name) == 0)
            return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
        return FORMAL(node) ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED : 0;
    case AST_NUMBER: case AST_STRING: case AST_BOOLEAN: case AST_BREAK: case AST_CONTINUE:
        return 0;
    case AST_BLOCK:
        EACH(ast_block_statement_count(node), ast_block_statement(node, i));
    case AST_LET_DECL: return USE(ast_let_initializer(node));
    case AST_IF_STMT:
        ADD(ast_if_condition(node)); ADD(ast_if_then_branch(node)); ADD(ast_if_else_branch(node)); return bad;
    case AST_WHILE_LOOP: ADD(ast_while_condition(node)); ADD(ast_while_body(node)); return bad;
    case AST_FOR_LOOP:
        ADD(ast_for_range_start(node)); ADD(ast_for_range_end(node)); ADD(ast_for_body(node)); return bad;
    case AST_MATCH_STMT:
        ADD(ast_match_subject(node)); ADD(ast_match_default_body(node));
        EACH(ast_match_case_count(node), ast_match_case_at(node, i));
    case AST_MATCH_CASE:
        ADD(ast_match_case_pattern(node)); ADD(ast_match_case_guard(node)); ADD(ast_match_case_body(node)); return bad;
    case AST_ASSIGNMENT: {
        ASTNode *target = ast_assignment_target(node);
        ADD(target); ADD(ast_assignment_value(node));
        if (target != NULL && target->type == AST_ARRAY_ACCESS
            && FORMAL(ast_array_access_array(target))) bad |= FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED;
        return bad;
    }
    case AST_BINARY: ADD(ast_binary_left(node)); ADD(ast_binary_right(node)); return bad;
    case AST_UNARY: return USE(ast_unary_operand(node));
    case AST_CAST: return USE(ast_cast_operand(node));
    case AST_TYPE_TEST: return USE(ast_type_test_operand(node));
    case AST_LET_DESTRUCTURE: return USE(ast_let_destructure_initializer(node));
    case AST_RETURN: return USE(ast_return_value(node));
    case AST_MEMBER_ACCESS: return USE(ast_member_object(node));
    case AST_ARRAY_ACCESS:
        if (FORMAL(ast_array_access_array(node))) *payload_read = true;
        else { ADD(ast_array_access_array(node)); }
        ADD(ast_array_access_index(node)); return bad;
    case AST_ARRAY_LITERAL: EACH(ast_array_literal_count(node), ast_array_literal_element(node, i));
    case AST_TUPLE_LITERAL: EACH(ast_tuple_literal_count(node), ast_tuple_literal_element(node, i));
    case AST_SET_LITERAL: EACH(ast_set_literal_count(node), ast_set_literal_element(node, i));
    case AST_MAP_LITERAL:
        for (size_t i = 0; i < ast_map_literal_count(node); i++) {
            ADD(ast_map_literal_key(node, i)); ADD(ast_map_literal_value(node, i));
        }
        return bad;
    case AST_CALL:
        ADD(ast_call_callee(node));
        for (size_t i = 0; i < ast_call_arg_count(node); i++) {
            ASTNode *arg = ast_call_argument(node, i);
            bad |= FORMAL(arg) ? array_storage_call_argument_unproved(node, i, lookup) : USE(arg);
            if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) break;
        }
        return bad;
    default: return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    }
#undef USE
#undef FORMAL
#undef ADD
#undef EACH
}

unsigned
function_param_array_storage_unproved_in_program_points(
    ASTNode *const *roots, size_t count, const SlotSummaryOrigin *origin,
    const SlotFunctionLookup *lookup, bool *payload_read)
{
    if (payload_read == NULL) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    *payload_read = false;
    FuncParam *param = origin != NULL
        ? ast_func_param(origin->function_decl, origin->param_index) : NULL;
    if (lookup == NULL || lookup->ctx == NULL || param == NULL || param->type == NULL
        || ast_func_body(origin->function_decl) == NULL || origin->function_decl->is_async_decl
        || ast_type_name(param->type) == NULL || strcmp(ast_type_name(param->type), "Array") != 0
        || param->mode == PARAM_MODE_OWN || origin->param_name == NULL
        || ast_func_param_stable_id(param) == 0 || (roots == NULL && count != 0))
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    unsigned bad = 0;
    for (size_t i = 0; i < count; i++) {
        bad |= roots[i] == NULL ? FUNCTION_PARAM_ARRAY_ALL_UNPROVED
            : array_storage_use_unproved(roots[i], origin, lookup, payload_read);
        if (bad == FUNCTION_PARAM_ARRAY_ALL_UNPROVED) break;
    }
    return bad;
}

unsigned
function_param_flow_array_preservation_failures(SemanticContext *ctx,
    ASTNode *function_decl, size_t param_index)
{
    FuncParam *param = ast_func_param(function_decl, param_index);
    if (ctx == NULL || function_decl == NULL || param == NULL
        || ast_func_body(function_decl) == NULL || function_decl->is_async_decl
        || param->mode == PARAM_MODE_OWN || param->type == NULL
        || ast_type_name(param->type) == NULL
        || strcmp(ast_type_name(param->type), "Array") != 0)
        return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    (void)function_param_flow_summary_for_param(ctx, function_decl, param_index);
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    size_t index = function_param_flow_find(store, ast_node_stable_id(function_decl), param_index);
    if (index == SIZE_MAX || store->entries[index].function_decl != function_decl
        || (!store->solving && (store->entries[index].state != FUNCTION_PARAM_FLOW_COMPLETE
            || store->entries[index].analysis_unsupported || !store->entries[index].output_dependencies_ready
            || !function_param_flow_output_current(store, index, false)))) return FUNCTION_PARAM_ARRAY_ALL_UNPROVED;
    return (store->entries[index].array_storage_unproved ? FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED : 0)
        | (store->entries[index].array_elements_unproved ? FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED : 0);
}

bool
function_param_flow_preserves_array_storage(SemanticContext *ctx, ASTNode *decl, size_t ordinal)
{
    return (function_param_flow_array_preservation_failures(ctx, decl, ordinal)
        & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) == 0;
}
