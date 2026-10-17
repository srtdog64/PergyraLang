#include "function_param_flow_summary_internal.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_nonowner_risk_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "typed_map_query_fact_owner.h"
#include "../common/string_compat.h"

bool function_param_flow_claim_capture(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node, bool semantic)
{
    if (claims == NULL || claims->failed || node == NULL) return false;
    FunctionParamFlowIndexClaim *row = NULL;
    for (size_t i = 0; i < claims->count; i++) if (claims->rows[i].node == node) {
        row = &claims->rows[i];
        if (!semantic || row->semantic_promoted) return true;
        goto promote;
    }
    bool supported = false;
    size_t children = function_param_flow_claim_child_count(node, &supported);
    if (!supported) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim syntax topology is unsupported"); return false;
    }
    if (children > SIZE_MAX / sizeof(ASTNode *)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim child storage is missing or its count is not addressable"); return false;
    }
    if (claims->count >= SIZE_MAX / sizeof(*claims->rows)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim size overflow"); return false;
    }
    FunctionParamFlowIndexClaim *rows = realloc(claims->rows, (claims->count + 1) * sizeof(*rows));
    if (rows == NULL) { claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim allocation failed"); return false; }
    claims->rows = rows;
    row = &rows[claims->count++];
    *row = (FunctionParamFlowIndexClaim){.node = node, .id = ast_node_stable_id(node), .shape = node->type,
        .child_count = children, .control_step_ordinal = SIZE_MAX, .control_root_ordinal = SIZE_MAX};
    row->children = children == 0 ? NULL : calloc(children, sizeof(*row->children));
    row->child_ids = children == 0 ? NULL : calloc(children, sizeof(*row->child_ids));
    row->child_claim_ordinals = children == 0 ? NULL : malloc(children * sizeof(*row->child_claim_ordinals));
    if (children != 0 && (row->children == NULL || row->child_ids == NULL || row->child_claim_ordinals == NULL)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim edge allocation failed"); return false;
    }
    for (size_t c = 0; c < children; c++) {
        row->children[c] = function_param_flow_claim_child(node, c);
        row->child_ids[c] = ast_node_stable_id(row->children[c]);
        row->child_claim_ordinals[c] = SIZE_MAX;
    }
    if (node->type == AST_IDENTIFIER) row->binding = ast_identifier_binding_syntax_id(node);
    if (node->type == AST_LET_DECL) row->local_alias = ast_let_is_alias(node);
    if (node->type == AST_MATCH_CASE) row->match_family = ast_match_case_semantic_subject_family(node);
    if (node->type == AST_BINARY) row->syntax_operator = node->data.binary.op.type;
    if (node->type == AST_UNARY) row->syntax_operator = node->data.unary.op.type;
    if (node->type == AST_PARALLEL_BLOCK) row->parallel_join = ast_parallel_is_join_form(node);
    if (node->type == AST_BOOLEAN) row->syntax_literal_flags = ast_boolean_value(node);
    if (node->type == AST_NUMBER) {
        row->syntax_number = ast_number_value(node);
        row->syntax_long = ast_number_exact_long_value(node);
        row->syntax_literal_flags = ast_number_is_long(node) | (ast_number_is_float(node) << 1) | (ast_number_is_duration(node) << 2);
    }
    const char *syntax_name = node->type == AST_IDENTIFIER ? ast_identifier_name(node) :
        node->type == AST_MEMBER_ACCESS ? ast_member_name(node) :
        node->type == AST_TYPE ? ast_type_name(node) : node->type == AST_STRING ? ast_string_value(node) : NULL;
    row->syntax_name = syntax_name == NULL ? NULL : pergyra_strdup(syntax_name);
    if (syntax_name != NULL && row->syntax_name == NULL) { claims->failed = true; return false; }
    if (node->type == AST_TYPE || node->type == AST_CALL) {
        GenericParams *args = node->type == AST_TYPE ? ast_type_generic_args(node) : ast_call_generic_args(node);
        row->generic_name_count = args == NULL ? 0 : args->count;
        if (row->generic_name_count > SIZE_MAX / sizeof(*row->generic_names)) { claims->failed = true; return false; }
        row->generic_names = row->generic_name_count == 0 ? NULL : calloc(row->generic_name_count, sizeof(*row->generic_names));
        if (row->generic_name_count != 0 && row->generic_names == NULL) { claims->failed = true; return false; }
        for (size_t p = 0; p < row->generic_name_count; p++) {
            row->generic_names[p] = pergyra_strdup(args->params[p]->name);
            if (row->generic_names[p] == NULL) { claims->failed = true; return false; }
        }
    }
    if (node->type == AST_CAST || node->type == AST_TYPE_TEST) {
        const char *target = node->type == AST_CAST ? ast_cast_target_type(node) : ast_type_test_target_type(node);
        row->cast_target = target == NULL ? NULL : pergyra_strdup(target);
        if (target != NULL && row->cast_target == NULL) { claims->failed = true; return false; }
    }
    if (!semantic) return row->id != 0;
promote:
    row->semantic_promoted = true;
    row->execution_present = semantic_param_publication_execution(ctx, node, &row->reachable);
    row->physical_value = semantic_param_publication_value_is_physical_scalar(ctx, node);
    row->unit_enum_selection = node->type == AST_MEMBER_ACCESS &&
        semantic_param_publication_checked_unit_enum_selection(ctx, node);
    SemanticPublishedValue value = semantic_param_publication_value(ctx, node);
    row->value_ready = value.ok;
    row->value_meta = value.ok && value.meta;
    row->value_origin_ready = value.ok && value.origin_ready;
    row->value_type = value.type;
    row->slot_token_operand = semantic_param_publication_slot_token_operand(ctx, node);
    if (node->type == AST_LET_DECL) row->channel_binding = semantic_param_publication_channel_binding(ctx, node);
    if (node->type == AST_LET_DECL) row->empty_map_binding = semantic_param_publication_empty_map_binding(ctx, node);
    if (node->type == AST_LET_DECL || node->type == AST_LET_DESTRUCTURE) {
        SemanticPublishedSlotBirth birth;
        row->owned_slot_binding = semantic_param_publication_owned_slot_binding(ctx, node, &birth);
    }
    if (node->type == AST_CALL) {
        row->collection_effect_present = ast_call_semantic_collection_effect(node,
            &row->collection_effect_kind, &row->collection_effect_receiver, &row->collection_effect_source);
        row->slot_write = semantic_param_publication_checked_slot_write(ctx, node);
        row->local_owner_constructor = semantic_param_publication_checked_local_owner_constructor(ctx, node);
        row->private_array_push = semantic_param_publication_private_array_push(ctx, node, NULL);
        row->result_relation = semantic_param_publication_checked_result_relation(ctx, node, 0,
            &row->result_depends, &row->external_retains);
        row->backing_relation = semantic_param_publication_checked_backing_relation(ctx, node, 0,
            &row->backing_depends, &row->backing_external_retains);
        if (row->result_relation || row->backing_relation) {
            row->result_type_name = pergyra_strdup(ast_call_semantic_value_type_name(node));
            if (row->result_type_name == NULL) {
                claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "result claim allocation failed"); return false;
            }
        }
    }
    if (node->type == AST_CAST) {
        row->numeric_cast = semantic_param_publication_checked_numeric_cast(ctx, node);
    }
    if (node->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(node);
        row->binding = semantic_param_publication_binding(ctx, target);
        row->assignment_replacement = semantic_param_publication_assignment_replacement(ctx, node);
    }
    return row->id != 0;
}

size_t function_param_flow_claim_child_count(const ASTNode *node, bool *supported)
{
    *supported = true;
    switch (node->type) {
    case AST_BLOCK: return ast_block_statement_count(node);
    case AST_CALL: {
        GenericParams *args = ast_call_generic_args(node);
        size_t count = args == NULL ? 0 : args->count;
        size_t actuals = ast_call_arg_count(node);
        if (actuals == SIZE_MAX || count > (SIZE_MAX - actuals - 1) / 2 ||
            (count != 0 && args->params == NULL)) return SIZE_MAX;
        for (size_t p = 0; p < count; p++)
            if (args->params[p] == NULL || args->params[p]->name == NULL) return SIZE_MAX;
        return actuals + 1 + count * 2;
    }
    case AST_ARRAY_LITERAL: return ast_array_literal_count(node) != 0 && node->data.array_literal.elements == NULL ? SIZE_MAX : ast_array_literal_count(node);
    case AST_TUPLE_LITERAL: return ast_tuple_literal_count(node) != 0 && node->data.tuple_literal.elements == NULL ? SIZE_MAX : ast_tuple_literal_count(node);
    case AST_SET_LITERAL: return ast_set_literal_count(node) != 0 && node->data.set_literal.elements == NULL ? SIZE_MAX : ast_set_literal_count(node);
    case AST_MAP_LITERAL: return ast_map_literal_count(node) > SIZE_MAX / 2 || (ast_map_literal_count(node) != 0 &&
        (node->data.map_literal.keys == NULL || node->data.map_literal.values == NULL)) ? SIZE_MAX : ast_map_literal_count(node) * 2;
    case AST_SELECT_STMT: return ast_select_case_count(node) == SIZE_MAX ? SIZE_MAX : ast_select_case_count(node) + 1;
    case AST_MATCH_STMT: return ast_match_case_count(node) > SIZE_MAX - 2 ? SIZE_MAX : ast_match_case_count(node) + 2;
    case AST_MATCH_CASE: return ast_match_case_pattern_count(node) > SIZE_MAX - 3 ? SIZE_MAX : ast_match_case_pattern_count(node) + 3;
    case AST_PARALLEL_BLOCK: return ast_parallel_task_count(node) > SIZE_MAX - 2 ? SIZE_MAX : ast_parallel_task_count(node) + 2;
    case AST_ASYNC_BLOCK: return ast_async_block_statement_count(node);
    case AST_SPAWN_EXPR: return ast_spawn_arg_count(node) == SIZE_MAX ? SIZE_MAX : ast_spawn_arg_count(node) + 1;
    case AST_LAMBDA_EXPR: return ast_lambda_param_count(node) > SIZE_MAX - 2 ? SIZE_MAX : ast_lambda_param_count(node) + 2;
    case AST_PARTY_INSTANCE: return node->data.party_instance.assignment_count;
    case AST_CONTEXT_ACCESS: return 1;
    case AST_EVENT_HANDLER_TYPE: {
        size_t count = 0;
        ASTNode **params = ast_event_handler_param_types(node, &count);
        return count == SIZE_MAX || (count != 0 && params == NULL) ? SIZE_MAX : count + 1;
    }
    case AST_TRANSACTION_BLOCK:
        return node->data.transaction_block.compensation_count == SIZE_MAX ||
            (node->data.transaction_block.compensation_count != 0 && node->data.transaction_block.compensations == NULL)
            ? SIZE_MAX : node->data.transaction_block.compensation_count + 1;
    case AST_FUNC_DECL: return ast_func_param_count(node) > (SIZE_MAX - 2) / 2 ? SIZE_MAX : ast_func_param_count(node) * 2 + 2;
    case AST_TYPE: {
        GenericParams *args = ast_type_generic_args(node);
        size_t count = args == NULL ? 0 : args->count;
        if (count > (SIZE_MAX - ast_type_tuple_element_count(node)) / 2) return SIZE_MAX;
        if (count != 0 && args->params == NULL) return SIZE_MAX;
        for (size_t p = 0; p < count; p++)
            if (args->params[p] == NULL || args->params[p]->name == NULL) return SIZE_MAX;
        return count * 2 + ast_type_tuple_element_count(node);
    }
    case AST_CHANNEL_TYPE: return 2;
    case AST_FUTURE_TYPE: case AST_TYPE_ALIAS: case AST_LET_DESTRUCTURE:
    case AST_GIVE_STMT: case AST_TYPE_TEST: case AST_AWAIT_EXPR: case AST_CHANNEL_RECV:
    case AST_UNSAFE_BLOCK: case AST_FAIL_STMT: case AST_DEFER_STMT:
    case AST_BIND_STMT: return 1;
    case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE: return 2;
    case AST_EVENT_INVOKE: return ast_event_invoke_arg_count(node) == SIZE_MAX ? SIZE_MAX : ast_event_invoke_arg_count(node) + 1;
    case AST_IF_STMT: return 3;
    case AST_FOR_LOOP: return 4;
    case AST_LET_DECL: case AST_ASSIGNMENT: case AST_BINARY: case AST_ARRAY_ACCESS:
    case AST_WHILE_LOOP: case AST_WITH_STMT: case AST_CHANNEL_SEND: return 2;
    case AST_RETURN: case AST_UNARY: case AST_MEMBER_ACCESS: case AST_CAST: return 1;
    case AST_IDENTIFIER: case AST_NUMBER: case AST_STRING: case AST_BOOLEAN:
    case AST_BREAK: case AST_CONTINUE: case AST_IMPORT_DECL: case AST_USE_DECL: return 0;
    default: *supported = false; return 0;
    }
}
ASTNode *function_param_flow_claim_child(const ASTNode *node, size_t ordinal)
{
    switch (node->type) {
    case AST_BLOCK: return ast_block_statement(node, ordinal);
    case AST_CALL: {
        size_t actuals = ast_call_arg_count(node);
        if (ordinal == 0) return ast_call_callee(node);
        if (ordinal <= actuals) return ast_call_argument(node, ordinal - 1);
        GenericParam *param = ast_generic_param_at(ast_call_generic_args(node), (ordinal - actuals - 1) / 2);
        return param == NULL ? NULL : (ordinal - actuals - 1) % 2 == 0 ? param->constraint : param->default_type;
    }
    case AST_ARRAY_LITERAL: return ast_array_literal_element(node, ordinal);
    case AST_TUPLE_LITERAL: return ast_tuple_literal_element(node, ordinal);
    case AST_SET_LITERAL: return ast_set_literal_element(node, ordinal);
    case AST_MAP_LITERAL: return ordinal % 2 == 0 ? ast_map_literal_key(node, ordinal / 2) : ast_map_literal_value(node, ordinal / 2);
    case AST_SELECT_STMT: return ordinal == ast_select_case_count(node) ? ast_select_default_case(node) : ast_select_case(node, ordinal);
    case AST_MATCH_STMT: return ordinal == 0 ? ast_match_subject(node) : ordinal == 1 ? ast_match_default_body(node) : ast_match_case_at(node, ordinal - 2);
    case AST_MATCH_CASE: return ordinal == 0 ? ast_match_case_pattern(node) : ordinal == 1 ? ast_match_case_guard(node) : ordinal == 2 ? ast_match_case_body(node) : ast_match_case_pattern_at(node, ordinal - 3);
    case AST_PARALLEL_BLOCK: return ordinal == 0 ? ast_parallel_join_collection(node) : ordinal == 1 ? ast_parallel_join_range_end(node) : ast_parallel_task(node, ordinal - 2);
    case AST_ASYNC_BLOCK: return ast_async_block_statement(node, ordinal);
    case AST_SPAWN_EXPR: return ordinal == 0 ? ast_spawn_function(node) : ast_spawn_argument(node, ordinal - 1);
    case AST_LAMBDA_EXPR: return ordinal == 0 ? ast_lambda_body(node) : ordinal == 1 ? ast_lambda_return_type(node) : ast_lambda_param(node, ordinal - 2);
    case AST_PARTY_INSTANCE: return node->data.party_instance.assignments[ordinal].value;
    case AST_CONTEXT_ACCESS: return node->data.context_access.ability_type;
    case AST_EVENT_HANDLER_TYPE: {
        if (ordinal == 0) return ast_event_handler_return_type(node);
        size_t count = 0;
        ASTNode **params = ast_event_handler_param_types(node, &count);
        return params != NULL && ordinal - 1 < count ? params[ordinal - 1] : NULL;
    }
    case AST_FUNC_DECL: {
        if (ordinal == 0) return ast_func_body(node);
        if (ordinal == 1) return node->data.func_decl.return_type;
        FuncParam *param = ast_func_param(node, (ordinal - 2) / 2);
        return param == NULL ? NULL : ordinal % 2 == 0 ? param->type : param->default_value;
    }
    case AST_TYPE: {
        GenericParams *args = ast_type_generic_args(node);
        size_t count = args == NULL ? 0 : args->count;
        if (ordinal >= count * 2) return ast_type_tuple_element(node, ordinal - count * 2);
        GenericParam *param = args->params[ordinal / 2];
        return param == NULL ? NULL : ordinal % 2 == 0 ? param->constraint : param->default_type;
    }
    case AST_CHANNEL_TYPE: return ordinal == 0 ? node->data.channel_type.element_type : node->data.channel_type.capacity;
    case AST_FUTURE_TYPE: return node->data.future_type.value_type;
    case AST_TYPE_ALIAS: return ast_type_alias_target_type(node);
    case AST_LET_DESTRUCTURE: return ast_let_destructure_initializer(node);
    case AST_GIVE_STMT: return ast_give_value(node);
    case AST_TYPE_TEST: return ast_type_test_operand(node);
    case AST_AWAIT_EXPR: return ast_await_expression(node);
    case AST_CHANNEL_RECV: return ast_channel_recv_channel(node);
    case AST_UNSAFE_BLOCK: return ast_unsafe_block_body(node);
    case AST_TRANSACTION_BLOCK: return ordinal == 0 ? ast_transaction_block_body(node) : node->data.transaction_block.compensations[ordinal - 1];
    case AST_FAIL_STMT: return ast_fail_stmt_reason(node);
    case AST_DEFER_STMT: return ast_defer_body(node);
    case AST_BIND_STMT: return ast_bind_statement_subject(node);
    case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE: return ordinal == 0 ? ast_event_op_event(node) : ast_event_op_handler(node);
    case AST_EVENT_INVOKE: return ordinal == 0 ? ast_event_invoke_event(node) : ast_event_invoke_argument(node, ordinal - 1);
    case AST_CHANNEL_SEND: return ordinal == 0 ? ast_channel_send_value(node) : ast_channel_send_channel(node);
    case AST_CAST: return ast_cast_operand(node);
    case AST_IF_STMT: return ordinal == 0 ? ast_if_condition(node) : ordinal == 1 ? ast_if_then_branch(node) : ast_if_else_branch(node);
    case AST_FOR_LOOP: return ordinal == 0 ? ast_for_range_start(node) : ordinal == 1 ? ast_for_range_end(node) : ordinal == 2 ? ast_for_iterable(node) : ast_for_body(node);
    case AST_LET_DECL: return ordinal == 0 ? ast_let_initializer(node) : ast_let_type(node);
    case AST_ASSIGNMENT: return ordinal == 0 ? ast_assignment_target(node) : ast_assignment_value(node);
    case AST_BINARY: return ordinal == 0 ? ast_binary_left(node) : ast_binary_right(node);
    case AST_ARRAY_ACCESS: return ordinal == 0 ? ast_array_access_array(node) : ast_array_access_index(node);
    case AST_WHILE_LOOP: return ordinal == 0 ? ast_while_condition(node) : ast_while_body(node);
    case AST_WITH_STMT: return ordinal == 0 ? ast_with_slot_type(node) : ast_with_body(node);
    case AST_RETURN: return ast_return_value(node);
    case AST_UNARY: return ast_unary_operand(node);
    case AST_MEMBER_ACCESS: return ast_member_object(node);
    default: return NULL;
    }
}

/* Compilation-owned storage, bitset accumulation and epoch charging share
 * their exported snapshot lifetime. Effect selection, admission and currentness
 * remain with the backing dependency and relation owners. */

/* One shallow-edge vocabulary serves the initial syntax snapshot and its
 * current replay. Execution policy is carried separately by promotion. */
/* Only growth after a binding input was read needs another traversal. Forward
 * readers see the merged bits directly; earlier readers and expression caches
 * are replayed on the next epoch, including dependencies on an empty input. */
bool backing_merge(size_t words, uint64_t *into, const uint64_t *from)
{
    bool grew = false;
    for (size_t w = 0; w < words; w++) {
        uint64_t next = into[w] | from[w];
        grew |= next != into[w]; into[w] = next;
    }
    return grew;
}
void backing_effect(ReturnedBacking *state, const uint64_t *origins, unsigned effect)
{
    if (state->loan_seed != NULL && origins[0] != 0 && effect != 0 && state->escape_site == NULL)
        state->escape_site = state->consumer;
    for (size_t p = 0; p < state->fact->origin_count; p++)
        if ((origins[p / 64] & (UINT64_C(1) << (p % 64))) != 0) state->fact->effects[p] |= effect;
}
bool backing_any_origin(const ReturnedBacking *state, const uint64_t *origins)
{
    for (size_t w = 0; w < state->fact->origin_words; w++) if (origins[w] != 0) return true;
    return false;
}
bool backing_work(ReturnedBacking *state, size_t at)
{
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    if (row->work_epoch == state->fact->epoch) return true;
    size_t *work = state->loan_seed != NULL ? &state->loan_work : &state->store->work_units;
    if (*work >= state->store->work_budget) { function_param_flow_fail(state->store, state->decl, "recursive summary work budget exceeded"); state->fact->complete = false; return false; }
    (*work)++; row->work_epoch = state->fact->epoch; return true;
}


FunctionParamFlowBackingBinding *backing_binding(ReturnedBacking *state, uint32_t id, bool insert)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (id == 0) { fact->complete = false; return NULL; }
    for (size_t i = 0; i < fact->binding_count; i++) if (fact->bindings[i].binding == id) return &fact->bindings[i];
    if (!insert) return NULL;
    if (fact->binding_count == SIZE_MAX / sizeof(*fact->bindings)) { function_param_flow_fail(state->store, state->decl, "backing binding size overflow"); fact->complete = false; return NULL; }
    FunctionParamFlowBackingBinding *rows = realloc(fact->bindings, (fact->binding_count + 1) * sizeof(*rows));
    if (rows == NULL) { function_param_flow_fail(state->store, state->decl, "backing binding allocation failed"); fact->complete = false; return NULL; }
    fact->bindings = rows;
    FunctionParamFlowBackingBinding *row = &rows[fact->binding_count];
    *row = (FunctionParamFlowBackingBinding){.binding = id};
    row->origins = calloc(fact->origin_words, sizeof(uint64_t));
    row->destinations = calloc(fact->destination_words, sizeof(uint64_t));
    fact->binding_count++;
    if (row->origins == NULL || row->destinations == NULL) { function_param_flow_fail(state->store, state->decl, "backing binding bitset allocation failed"); fact->complete = false; return NULL; }
    return row;
}
size_t backing_node(ReturnedBacking *state, ASTNode *node)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (node == NULL || ast_node_stable_id(node) == 0) { fact->complete = false; return SIZE_MAX; }
    if (!function_param_flow_claim_record(state->store->ctx, fact->claims, node)) { fact->complete = false; return SIZE_MAX; }
    for (size_t i = 0; i < fact->node_count; i++) if (fact->nodes[i].node == node) return i;
    if (fact->node_count == SIZE_MAX / sizeof(*fact->nodes)) { function_param_flow_fail(state->store, state->decl, "backing node size overflow"); fact->complete = false; return SIZE_MAX; }
    FunctionParamFlowBackingNode *rows = realloc(fact->nodes, (fact->node_count + 1) * sizeof(*rows));
    if (rows == NULL) { function_param_flow_fail(state->store, state->decl, "backing node allocation failed"); fact->complete = false; return SIZE_MAX; }
    fact->nodes = rows;
    size_t at = fact->node_count++;
    rows[at] = (FunctionParamFlowBackingNode){.node = node, .id = ast_node_stable_id(node), .shape = node->type};
    rows[at].origins = calloc(fact->origin_words, sizeof(uint64_t));
    if (rows[at].origins == NULL) { function_param_flow_fail(state->store, state->decl, "backing origin allocation failed"); fact->complete = false; return SIZE_MAX; }
    return at;
}


void
pgy_function_param_flow_facts_destroy(PgyFunctionParamFlowFact *facts)
{
    free(facts);
}

bool
function_param_flow_summary_snapshot(SemanticContext *ctx)
{
    FunctionParamFlowSummaryStore *store;
    PgyFunctionParamFlowFact *facts;
    size_t count = 0;

    if (ctx == NULL || !semantic_param_publication_ready(ctx))
        return false;
    store = ctx->function_param_flow_summaries;
    if (store == NULL)
        return true;
    if (store->failed)
        return false;
    if (store->count > SIZE_MAX / sizeof(*facts))
        return false;

    facts = store->count == 0
        ? NULL
        : calloc(store->count, sizeof(*facts));
    if (store->count != 0 && facts == NULL)
        return false;
    for (size_t i = 0; i < store->count; i++) {
        const FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (entry->state != FUNCTION_PARAM_FLOW_COMPLETE)
            continue;
        facts[count].function_syntax_id = entry->function_id;
        facts[count].parameter_index = entry->param_index;
        facts[count].mask = entry->mask;
        count++;
    }
    if (count == 0) {
        free(facts);
        facts = NULL;
    }
    ctx->function_param_flow_facts = facts;
    ctx->function_param_flow_fact_count = count;
    ctx->function_param_flow_fact_capacity = count;
    return true;
}


void semantic_published_unit_enum_selection_destroy(
    SemanticPublishedUnitEnumSelection *selection)
{
    if (selection == NULL) return;
    free(selection->qualifier_name);
    free(selection->enum_name);
    free(selection->member_name);
    free(selection);
}

bool semantic_param_publication_reserve_storage(void **rows, size_t *capacity, size_t count, size_t size)
{
    if (count < *capacity) return true;
    size_t next = *capacity == 0 ? 32 : *capacity * 2;
    if (next < *capacity || next > SIZE_MAX / size) return false;
    void *grown = realloc(*rows, next * size);
    if (grown == NULL) return false;
    *rows = grown; *capacity = next; return true;
}

void semantic_param_publication_retire_value(PublishedNode *row)
{
    semantic_param_publication_projection_destroy(row);
    if (row->owned_string_result != NULL) {
        free(row->owned_string_result->sources);
        function_param_flow_claims_destroy(row->owned_string_result->syntax);
        free(row->owned_string_result);
    }
    semantic_param_publication_retire_deferred(row);
    void *metadata[] = {row->arguments, row->target_name,
        row->value_type_name, row->relation_argument_type, row->relation_result_type};
    for (size_t i = 0; i < sizeof(metadata) / sizeof(metadata[0]); i++) free(metadata[i]);
    semantic_nominal_result_snapshot_destroy(row->nominal_result);
    semantic_published_unit_enum_selection_destroy(row->unit_enum_selection);
    PublishedNode identity = {.node = row->node, .id = row->id, .routine_index = row->routine_index,
        .execution_present = row->execution_present, .reachable = row->reachable,
        .indexed_string_shallow_retention = row->indexed_string_shallow_retention,
        .private_string_push = row->private_string_push,
        .indexed_string_field = row->indexed_string_field,
        .indexed_string_nonowner = row->indexed_string_nonowner};
    *row = identity;
}

void semantic_param_publication_destroy(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->function_param_publications == NULL) return;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    for (size_t i = 0; i < store->routine_count; i++) {
        free(store->routines[i].types); free(store->routines[i].module);
        free(store->routines[i].formals);
    }
    for (size_t i = 0; i < store->call_count; i++) {
        free(store->calls[i].borrowed); free(store->calls[i].destination); free(store->calls[i].fix); free(store->calls[i].module);
        free(store->calls[i].target_name);
    }
    for (size_t n = 0; n < store->node_capacity; n++) {
        semantic_indexed_string_private_push_destroy(store->nodes[n].private_string_push);
        store->nodes[n].private_string_push = NULL;
        semantic_indexed_string_field_destroy(store->nodes[n].indexed_string_field);
        store->nodes[n].indexed_string_field = NULL;
        semantic_indexed_string_nonowner_destroy(store->nodes[n].indexed_string_nonowner);
        store->nodes[n].indexed_string_nonowner = NULL;
        semantic_param_publication_retire_value(&store->nodes[n]);
    }
    for (size_t i = 0; i < store->read_loan_count; i++) free(store->read_loans[i].module);
    free(store->read_loans);
    free(store->nodes); free(store->routines); free(store->calls); free(store->body_calls);
    free(store); ctx->function_param_publications = NULL;
    semantic_typed_map_query_destroy(ctx);
}

void function_param_flow_program_point_indexes_destroy(FunctionParamFlowSummaryStore *store)
{
    if (store == NULL) return;
    for (size_t i = 0; i < store->program_point_index_count; i++)
        function_param_flow_program_point_index_destroy(&store->program_point_indexes[i]);
    free(store->program_point_indexes);
    store->program_point_indexes = NULL;
    store->program_point_index_count = store->program_point_index_capacity = 0;
}

bool
function_param_flow_reserve_entries(FunctionParamFlowSummaryStore *store,
                                    size_t minimum)
{
    FunctionParamFlowSummaryEntry *entries;
    size_t capacity;

    if (store == NULL)
        return false;
    if (minimum <= store->capacity)
        return true;
    capacity = store->capacity == 0 ? 16 : store->capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*entries))
        return false;
    entries = realloc(store->entries, capacity * sizeof(*entries));
    if (entries == NULL)
        return false;
    store->entries = entries;
    store->capacity = capacity;
    return true;
}

FunctionParamFlowSummaryStore *
function_param_flow_store_get(const SlotFunctionLookup *lookup)
{
    FunctionParamFlowSummaryStore *store;
    SemanticContext *ctx;

    if (lookup == NULL || lookup->ctx == NULL || lookup->program_root == NULL)
        return NULL;
    ctx = lookup->ctx;
    if (ctx->function_param_flow_summaries != NULL)
        return ctx->function_param_flow_summaries;
    store = calloc(1, sizeof(*store));
    if (store == NULL) {
        semantic_error(ctx, lookup->program_root,
            "Function parameter flow summary store allocation failed");
        return NULL;
    }
    store->ctx = ctx;
    store->program_root = lookup->program_root;
    store->work_budget = 4096u;
    ctx->function_param_flow_summaries = store;
    return store;
}

void
function_param_flow_program_point_index_destroy(
    FunctionParamFlowProgramPointIndex *index)
{
    if (index == NULL)
        return;
    if (index->roots_by_param != NULL) {
        for (size_t i = 0; i < index->param_count; i++)
            free(index->roots_by_param[i]);
    }
    free(index->roots_by_param);
    free(index->root_counts);
    free(index->root_capacities);
    free(index->dependency_roots);
    free(index->dependency_root_ids);
    free(index->no_store_calls);
    free(index->local_definitions);
    free(index->local_ids);
    for (size_t f = 0; f <= index->backing_frame_count; f++) {
        FunctionParamFlowBackingFact *fact = f == index->backing_frame_count ? index->declared_result :
            index->backing_frames == NULL ? NULL : index->backing_frames[f];
        if (fact == NULL) continue;
        for (size_t n = 0; fact->nodes != NULL && n < fact->node_count; n++) {
            free(fact->nodes[n].origins);
            free(fact->nodes[n].result_argument_kinds);
        }
        for (size_t b = 0; fact->bindings != NULL && b < fact->binding_count; b++) {
            free(fact->bindings[b].origins); free(fact->bindings[b].destinations);
        }
        free(fact->nodes); free(fact->bindings); free(fact->outputs); free(fact->effects); free(fact->pending_relations); free(fact);
    }
    free(index->backing_frames);
    function_param_flow_claims_destroy(index->claims);
    if (index->control_projection != NULL) {
        free(index->control_projection->steps);
        free(index->control_projection->root_offsets);
        free(index->control_projection->root_claim_ordinals);
        free(index->control_projection);
    }
    memset(index, 0, sizeof(*index));
}

void
function_param_flow_summary_store_destroy(SemanticContext *ctx)
{
    FunctionParamFlowSummaryStore *store;

    if (ctx == NULL || ctx->function_param_flow_summaries == NULL)
        return;
    store = ctx->function_param_flow_summaries;
    if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
        fprintf(stderr,
            "pgy: function-param-flow entries=%zu body_evaluations=%zu"
            " cache_hits=%zu recursion_hits=%zu fixed_point_passes=%zu\n",
            store->count, store->body_evaluations, store->cache_hits,
            store->recursion_hits, store->fixed_point_passes);
        fprintf(stderr, "pgy: function-param-flow-work work_units=%zu work_budget=%zu\n",
            store->work_units, store->work_budget);
        fprintf(stderr,
            "pgy: function-param-flow-sparse functions=%zu"
            " statement_visits=%zu program_points=%zu\n",
            store->program_point_index_count,
            store->indexed_statement_visits,
            store->indexed_program_points);
    }
    function_param_flow_program_point_indexes_destroy(store);
    for (size_t i = 0; i < store->count; i++) free(store->entries[i].output_dependencies);
    free(store->entries);
    free(store->hash);
    free(store->admitted_no_store_indexes);
    free(store);
    ctx->function_param_flow_summaries = NULL;
}
