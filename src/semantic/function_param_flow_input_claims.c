#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "../common/string_compat.h"
#include "typed_map_query_fact_owner.h"
#include "../common/match_variant_policy.h"

static size_t function_param_flow_claim_child_count(const ASTNode *node, bool *supported)
{
    *supported = true;
    switch (node->type) {
    case AST_BLOCK: return ast_block_statement_count(node);
    case AST_CALL: return ast_call_arg_count(node) == SIZE_MAX ? SIZE_MAX : ast_call_arg_count(node) + 1;
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
        return count > (SIZE_MAX - ast_type_tuple_element_count(node)) / 2 ? SIZE_MAX : count * 2 + ast_type_tuple_element_count(node);
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
static ASTNode *function_param_flow_claim_child(const ASTNode *node, size_t ordinal)
{
    switch (node->type) {
    case AST_BLOCK: return ast_block_statement(node, ordinal);
    case AST_CALL: return ordinal == 0 ? ast_call_callee(node) : ast_call_argument(node, ordinal - 1);
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
bool function_param_flow_index_claim_record(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (index->claims == NULL) index->claims = calloc(1, sizeof(*index->claims));
    if (index->claims == NULL) { function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim carrier allocation failed"); return false; }
    return function_param_flow_claim_record(ctx, index->claims, node);
}
static bool function_param_flow_claim_capture(SemanticContext *ctx,
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
    *row = (FunctionParamFlowIndexClaim){.node = node, .id = ast_node_stable_id(node), .shape = node->type, .child_count = children};
    row->children = children == 0 ? NULL : calloc(children, sizeof(*row->children));
    row->child_ids = children == 0 ? NULL : calloc(children, sizeof(*row->child_ids));
    if (children != 0 && (row->children == NULL || row->child_ids == NULL)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim edge allocation failed"); return false;
    }
    for (size_t c = 0; c < children; c++) {
        row->children[c] = function_param_flow_claim_child(node, c);
        row->child_ids[c] = ast_node_stable_id(row->children[c]);
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
        node->type == AST_TYPE ? ast_type_name(node) : node->type == AST_STRING ? ast_string_value(node) : NULL;
    row->syntax_name = syntax_name == NULL ? NULL : pergyra_strdup(syntax_name);
    if (syntax_name != NULL && row->syntax_name == NULL) { claims->failed = true; return false; }
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
    row->value_ready = semantic_param_publication_value(ctx, node).ok;
    if (node->type == AST_CALL) {
        row->private_array_push = semantic_param_publication_private_array_push(ctx, node, NULL);
        row->result_relation = semantic_param_publication_checked_result_relation(ctx, node, 0,
            &row->result_depends, &row->external_retains);
        if (row->result_relation) {
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
bool function_param_flow_claim_record(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node)
{
    return function_param_flow_claim_capture(ctx, claims, node, true);
}
static bool function_param_flow_claim_snapshot(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node, bool eager)
{
    if (node == NULL) return true;
    if (node->type == AST_TYPE || node->type == AST_CHANNEL_TYPE || node->type == AST_FUTURE_TYPE ||
        node->type == AST_FUNC_DECL || node->type == AST_TYPE_ALIAS || node->type == AST_EVENT_HANDLER_TYPE) eager = false;
    if (!function_param_flow_claim_capture(ctx, claims, node, eager)) return false;
    size_t at = 0;
    while (at < claims->count && claims->rows[at].node != node) at++;
    if (at == claims->count || claims->rows[at].snapshot_active) {
        claims->failed = true; return false;
    }
    if (claims->rows[at].snapshot_complete &&
        (!eager || claims->rows[at].snapshot_semantic_complete)) return true;
    claims->rows[at].snapshot_active = true;
    bool descendants_eager = eager;
    if (eager && claims->rows[at].execution_present && !claims->rows[at].reachable) descendants_eager = false;
    if (node->type == AST_LAMBDA_EXPR || node->type == AST_ASYNC_BLOCK || node->type == AST_SPAWN_EXPR ||
        node->type == AST_EVENT_SUBSCRIBE || node->type == AST_EVENT_UNSUBSCRIBE || node->type == AST_EVENT_INVOKE)
        descendants_eager = false;
    size_t children = claims->rows[at].child_count;
    for (size_t c = 0; c < children; c++) {
        ASTNode *child = claims->rows[at].children[c];
        if (child == NULL && (node->type == AST_ARRAY_LITERAL || node->type == AST_TUPLE_LITERAL ||
            node->type == AST_SET_LITERAL || node->type == AST_MAP_LITERAL ||
            (node->type == AST_TRANSACTION_BLOCK && c != 0) ||
            (node->type == AST_EVENT_HANDLER_TYPE && c != 0))) {
            claims->failed = true; return false;
        }
        bool child_eager = descendants_eager;
        if ((node->type == AST_LET_DECL && c == 1) || (node->type == AST_WITH_STMT && c == 0) ||
            node->type == AST_CONTEXT_ACCESS) child_eager = false;
        if (node->type == AST_MATCH_CASE && (c == 0 || c >= 3) &&
            ast_match_case_semantic_subject_family(node) != PGY_MATCH_SUBJECT_UNKNOWN) child_eager = false;
        if (!function_param_flow_claim_snapshot(ctx, claims, child, child_eager)) {
            claims->failed = true; return false;
        }
    }
    claims->rows[at].snapshot_active = false;
    claims->rows[at].snapshot_complete = true;
    claims->rows[at].snapshot_semantic_complete |= eager;
    return true;
}
bool function_param_flow_index_claims_snapshot(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (ctx == NULL || index == NULL) return false;
    if (index->claims == NULL) index->claims = calloc(1, sizeof(*index->claims));
    return index->claims != NULL && function_param_flow_claim_snapshot(ctx, index->claims, node, true);
}
bool function_param_flow_syntax_claims_snapshot(SemanticContext *ctx,
    FunctionParamFlowIndexClaims **claims, ASTNode *node)
{
    if (ctx == NULL || claims == NULL || node == NULL) return false;
    if (*claims == NULL) *claims = calloc(1, sizeof(**claims));
    return *claims != NULL && function_param_flow_claim_snapshot(ctx, *claims, node, false);
}
static bool function_param_flow_claims_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, bool semantic)
{
    if (claims == NULL || claims->failed || claims->count == 0 || claims->rows == NULL) return false;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        bool supported = false;
        size_t children = row->node == NULL ? 0 : function_param_flow_claim_child_count(row->node, &supported);
        if (row->node == NULL || (row->child_count != 0 && (row->children == NULL || row->child_ids == NULL)) ||
            row->shape != row->node->type || row->id != ast_node_stable_id(row->node) ||
            !supported || row->child_count != children) return false;
        for (size_t c = 0; c < row->child_count; c++)
            if (row->children[c] != function_param_flow_claim_child(row->node, c) ||
                row->child_ids[c] != ast_node_stable_id(row->children[c])) return false;
        if (row->shape == AST_IDENTIFIER && row->binding != ast_identifier_binding_syntax_id(row->node)) return false;
        if (row->shape == AST_LET_DECL && row->local_alias != ast_let_is_alias(row->node)) return false;
        if (row->shape == AST_MATCH_CASE && row->match_family != ast_match_case_semantic_subject_family(row->node)) return false;
        if (row->shape == AST_BINARY && row->syntax_operator != (int)row->node->data.binary.op.type) return false;
        if (row->shape == AST_UNARY && row->syntax_operator != (int)row->node->data.unary.op.type) return false;
        if (row->shape == AST_PARALLEL_BLOCK && row->parallel_join != ast_parallel_is_join_form(row->node)) return false;
        if (row->shape == AST_BOOLEAN && row->syntax_literal_flags != (unsigned)ast_boolean_value(row->node)) return false;
        if (row->shape == AST_NUMBER && (row->syntax_number != ast_number_value(row->node) ||
            row->syntax_long != ast_number_exact_long_value(row->node) || row->syntax_literal_flags !=
            (unsigned)(ast_number_is_long(row->node) | (ast_number_is_float(row->node) << 1) | (ast_number_is_duration(row->node) << 2)))) return false;
        const char *syntax_name = row->shape == AST_IDENTIFIER ? ast_identifier_name(row->node) :
            row->shape == AST_TYPE ? ast_type_name(row->node) : row->shape == AST_STRING ? ast_string_value(row->node) : NULL;
        if ((syntax_name == NULL) != (row->syntax_name == NULL) ||
            (syntax_name != NULL && strcmp(syntax_name, row->syntax_name) != 0)) return false;
        if (row->shape == AST_CAST || row->shape == AST_TYPE_TEST) {
            const char *target = row->shape == AST_CAST ? ast_cast_target_type(row->node) : ast_type_test_target_type(row->node);
            if ((target == NULL) != (row->cast_target == NULL) ||
                (target != NULL && strcmp(target, row->cast_target) != 0)) return false;
        }
        if (!semantic || !row->semantic_promoted) continue;
        bool reachable = false;
        bool execution_present = semantic_param_publication_execution(ctx, row->node, &reachable);
        if (row->execution_present != execution_present ||
            (execution_present && row->reachable != reachable)) return false;
        if (execution_present && !reachable) continue;
        if (row->value_ready != semantic_param_publication_value(ctx, row->node).ok) return false;
        if (row->physical_value != semantic_param_publication_value_is_physical_scalar(ctx, row->node)) return false;
        if (row->shape == AST_ARRAY_LITERAL && row->child_count == 0) {
            /* Replay the same current typed premise as the no-output selector.
             * Child count/identity above seal the absence of evaluations. */
            SemanticPublishedValue value = semantic_param_publication_value(ctx, row->node);
            if (!value.ok || value.meta) return false;
        }
        if (row->shape == AST_CALL) {
            if (!semantic_param_publication_checked_call(ctx, row->node)) return false;
            if (row->private_array_push != semantic_param_publication_private_array_push(ctx, row->node, NULL)) return false;
            bool depends = false, retains = false;
            bool relation = semantic_param_publication_checked_result_relation(ctx, row->node, 0, &depends, &retains);
            if (row->result_relation != relation || (relation &&
                (row->result_depends != depends || row->external_retains != retains ||
                 row->result_type_name == NULL || ast_call_semantic_value_type_name(row->node) == NULL ||
                 strcmp(row->result_type_name, ast_call_semantic_value_type_name(row->node)) != 0))) return false;
        }
        if (row->shape == AST_CAST) {
            const char *target = ast_cast_target_type(row->node);
            if ((target == NULL) != (row->cast_target == NULL) ||
                (target != NULL && strcmp(target, row->cast_target) != 0) ||
                row->numeric_cast != semantic_param_publication_checked_numeric_cast(ctx, row->node)) return false;
        }
        if (row->shape == AST_ASSIGNMENT && ast_assignment_target(row->node) != NULL &&
            ast_assignment_target(row->node)->type == AST_IDENTIFIER &&
            row->binding != semantic_param_publication_binding(ctx, ast_assignment_target(row->node))) return false;
        if (row->shape == AST_ASSIGNMENT && row->assignment_replacement !=
            semantic_param_publication_assignment_replacement(ctx, row->node)) return false;
    }
    return true;
}
bool function_param_flow_syntax_claims_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims)
{
    if (claims == NULL || claims->failed || claims->rows == NULL) return false;
    for (size_t i = 0; i < claims->count; i++)
        if (!claims->rows[i].snapshot_complete || claims->rows[i].snapshot_active) return false;
    return function_param_flow_claims_current(ctx, claims, false);
}
bool function_param_flow_index_claims_current(SemanticContext *ctx,
    const FunctionParamFlowProgramPointIndex *index)
{
    if (index == NULL) return false;
    for (size_t i = 0; i < index->local_count; i++)
        if (index->local_definitions[i]->type != AST_LET_DECL || ast_let_is_alias(index->local_definitions[i]) ||
            ast_node_stable_id(index->local_definitions[i]) != index->local_ids[i]) return false;
    return function_param_flow_claims_current(ctx, index->claims, true);
}

/* Each synchronous validation owns its reached declaration set. Shared DAG
 * descendants and cycles replay once here, never across separate queries. */
void function_param_flow_claims_destroy(FunctionParamFlowIndexClaims *claims)
{
    if (claims == NULL) return;
    for (size_t c = 0; c < claims->count; c++) {
        free(claims->rows[c].children); free(claims->rows[c].child_ids);
        free(claims->rows[c].cast_target); free(claims->rows[c].syntax_name);
        free(claims->rows[c].result_type_name);
    }
    free(claims->rows); free(claims);
}
