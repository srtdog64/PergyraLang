#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "typed_map_query_fact_owner.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool
function_param_flow_identifier_matches(const char *candidate, void *userdata)
{
    const char *name = userdata;
    return candidate != NULL && name != NULL && strcmp(candidate, name) == 0;
}

static bool
function_param_flow_has_identifier_ref(const ASTNode *node, const char *name)
{
    return node != NULL && name != NULL
        && ast_contains_identifier_ref(
            node, function_param_flow_identifier_matches, (void *)name);
}

static size_t
function_param_flow_program_point_index_find(
    const FunctionParamFlowSummaryStore *store,
    uint32_t function_id)
{
    if (store == NULL || function_id == 0)
        return SIZE_MAX;
    for (size_t i = 0; i < store->program_point_index_count; i++) {
        if (store->program_point_indexes[i].function_id == function_id)
            return i;
    }
    return SIZE_MAX;
}

static bool
function_param_flow_program_point_index_reserve(
    FunctionParamFlowSummaryStore *store,
    size_t minimum)
{
    FunctionParamFlowProgramPointIndex *grown;
    size_t capacity;

    if (store == NULL)
        return false;
    if (minimum <= store->program_point_index_capacity)
        return true;
    capacity = store->program_point_index_capacity == 0
        ? 8 : store->program_point_index_capacity;
    while (capacity < minimum) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(store->program_point_indexes,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    memset(grown + store->program_point_index_capacity, 0,
           (capacity - store->program_point_index_capacity) * sizeof(*grown));
    store->program_point_indexes = grown;
    store->program_point_index_capacity = capacity;
    return true;
}

static bool
function_param_flow_program_point_append(
    FunctionParamFlowProgramPointIndex *index,
    size_t param_index,
    ASTNode *root)
{
    ASTNode **grown;
    size_t capacity;

    if (index == NULL || root == NULL || param_index >= index->param_count)
        return false;
    if (index->root_counts[param_index]
            == index->root_capacities[param_index]) {
        capacity = index->root_capacities[param_index] == 0
            ? 8 : index->root_capacities[param_index];
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
        if (capacity > SIZE_MAX / sizeof(*grown))
            return false;
        grown = realloc(index->roots_by_param[param_index],
                        capacity * sizeof(*grown));
        if (grown == NULL)
            return false;
        index->roots_by_param[param_index] = grown;
        index->root_capacities[param_index] = capacity;
    }
    index->roots_by_param[param_index][index->root_counts[param_index]++] = root;
    index->relevant_root_count++;
    return true;
}

bool
function_param_flow_program_point_index_build(
    FunctionParamFlowSummaryStore *store,
    ASTNode *function_decl,
    FunctionParamFlowProgramPointIndex **index_out)
{
    FunctionParamFlowProgramPointIndex *index;
    ASTNode *body;
    uint32_t function_id;
    size_t existing;
    size_t param_count;

    if (store == NULL || function_decl == NULL || index_out == NULL)
        return false;
    function_id = ast_node_stable_id(function_decl);
    if (function_id == 0)
        return false;
    existing = function_param_flow_program_point_index_find(
        store, function_id);
    if (existing != SIZE_MAX) {
        *index_out = &store->program_point_indexes[existing];
        return (*index_out)->function_decl == function_decl && (*index_out)->dependency_roots_ready &&
            function_param_flow_index_roots_current(*index_out) &&
            function_param_flow_index_claims_current(store->ctx, *index_out);
    }

    param_count = ast_func_param_count(function_decl);
    if (!function_param_flow_program_point_index_reserve(
            store, store->program_point_index_count + 1)
        || (param_count > SIZE_MAX / sizeof(ASTNode **))
        || (param_count > SIZE_MAX / sizeof(size_t))) {
        return false;
    }

    existing = store->program_point_index_count++;
    index = &store->program_point_indexes[existing];
    memset(index, 0, sizeof(*index));
    index->function_id = function_id;
    index->function_decl = function_decl;
    index->param_count = param_count;
    if (param_count > 0) {
        index->roots_by_param = calloc(param_count, sizeof(ASTNode **));
        index->root_counts = calloc(param_count, sizeof(size_t));
        index->root_capacities = calloc(param_count, sizeof(size_t));
        if (index->roots_by_param == NULL
            || index->root_counts == NULL
            || index->root_capacities == NULL) {
            function_param_flow_program_point_index_destroy(index);
            store->program_point_index_count--;
            return false;
        }
    }

    body = ast_func_body(function_decl);
    index->dependency_body = body;
    index->dependency_body_id = ast_node_stable_id(body);
    if (body == NULL) {
        index->dependency_roots_ready = true;
        *index_out = index;
        return true;
    }

    if (body->type == AST_BLOCK) {
        index->dependency_root_count = ast_block_statement_count(body);
        if (index->dependency_root_count > SIZE_MAX / sizeof(*index->dependency_roots)) return false;
        index->dependency_roots = calloc(index->dependency_root_count, sizeof(*index->dependency_roots));
        index->dependency_root_ids = calloc(index->dependency_root_count, sizeof(*index->dependency_root_ids));
        if (index->dependency_root_count != 0 && (index->dependency_roots == NULL || index->dependency_root_ids == NULL)) return false;
        for (size_t s = 0; s < ast_block_statement_count(body); s++) {
            ASTNode *statement = ast_block_statement(body, s);
            index->dependency_roots[s] = statement;
            index->dependency_root_ids[s] = ast_node_stable_id(statement);
            index->statement_count++;
            store->indexed_statement_visits++;
            for (size_t p = 0; p < param_count; p++) {
                FuncParam *param = ast_func_param(function_decl, p);
                if (param == NULL || param->name == NULL
                    || !function_param_flow_has_identifier_ref(
                        statement, param->name))
                    continue;
                if (!function_param_flow_program_point_append(
                        index, p, statement)) {
                    store->indexed_program_points -=
                        index->relevant_root_count;
                    function_param_flow_program_point_index_destroy(index);
                    store->program_point_index_count--;
                    return false;
                }
                store->indexed_program_points++;
            }
        }
    } else {
        index->dependency_roots = malloc(sizeof(*index->dependency_roots));
        index->dependency_root_ids = malloc(sizeof(*index->dependency_root_ids));
        if (index->dependency_roots == NULL || index->dependency_root_ids == NULL) return false;
        index->dependency_roots[0] = body;
        index->dependency_root_ids[0] = ast_node_stable_id(body);
        index->dependency_root_count = 1;
        index->statement_count = 1;
        store->indexed_statement_visits++;
        for (size_t p = 0; p < param_count; p++) {
            FuncParam *param = ast_func_param(function_decl, p);
            if (param == NULL || param->name == NULL
                || !function_param_flow_has_identifier_ref(body, param->name))
                continue;
            if (!function_param_flow_program_point_append(index, p, body)) {
                store->indexed_program_points -= index->relevant_root_count;
                function_param_flow_program_point_index_destroy(index);
                store->program_point_index_count--;
                return false;
            }
            store->indexed_program_points++;
        }
    }

    if (!function_param_flow_index_claims_snapshot(store->ctx, index, body)) return false;
    index->mutable_output_required = function_param_flow_mutable_output_required(store->ctx, function_decl, index, body);
    index->dependency_roots_ready = true;
    *index_out = index;
    return true;
}

bool function_param_flow_no_store_call_append(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *call)
{
    ASTNode *callee = semantic_param_publication_direct_callee(ctx, call);
    if (callee == NULL || ast_call_arg_count(call) != ast_func_param_count(callee) ||
        !semantic_param_publication_routine_no_output_header_ready(ctx, callee)) return false;
    for (size_t p = 0; p < ast_call_arg_count(call); p++)
        if (!semantic_param_publication_value(ctx, ast_call_argument(call, p)).ok) return false;
    size_t count = index->no_store_call_count;
    for (size_t e = 0; e < count; e++) if (index->no_store_calls[e] == call) return true;
    if (count == SIZE_MAX / sizeof(ASTNode *)) return false;
    ASTNode **calls = realloc(index->no_store_calls, (count + 1) * sizeof(*calls));
    if (calls == NULL) return false;
    index->no_store_calls = calls;
    calls[count] = call;
    index->no_store_call_count++;
    return true;
}

static bool function_param_flow_no_store_member_admitted(
    const FunctionParamFlowSummaryStore *store, size_t ordinal)
{
    if (ordinal >= store->program_point_index_count) return false;
    const FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[ordinal];
    size_t position = index->admitted_no_store_position;
    return store->solving && store->demand_epoch != 0 &&
        index->admitted_no_store_epoch == store->demand_epoch &&
        position != 0 && position <= store->admitted_no_store_count &&
        store->admitted_no_store_indexes[position - 1] == ordinal;
}
/* The union is one admitted set in this demand, not copied per root. Replaying
 * it checks every premise, including edges into the set. An epoch alone is
 * never permission; current typed facts and exact membership must still join. */
static bool function_param_flow_no_store_admitted_current(FunctionParamFlowSummaryStore *store)
{
    for (size_t m = 0; m < store->admitted_no_store_count; m++) {
        size_t ordinal = store->admitted_no_store_indexes[m];
        if (!function_param_flow_no_store_member_admitted(store, ordinal)) return false;
        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[ordinal];
        ASTNode *decl = index->function_decl;
        if (index->function_id != ast_node_stable_id(decl) ||
            index->failed_no_store_epoch == store->demand_epoch ||
            !semantic_param_publication_routine_no_output_header_ready(store->ctx, decl) ||
            !function_param_flow_program_point_index_build(store, decl, &index) ||
            !index->dependency_roots_ready || index->mutable_output_required) return false;
        for (size_t e = 0; e < index->no_store_call_count; e++) {
            ASTNode *call = index->no_store_calls[e];
            ASTNode *callee = semantic_param_publication_direct_callee(store->ctx, call);
            if (callee == NULL || ast_call_arg_count(call) != ast_func_param_count(callee)) return false;
            for (size_t p = 0; p < ast_call_arg_count(call); p++)
                if (!semantic_param_publication_checked_argument(store->ctx, call, p) ||
                    !semantic_param_publication_value(store->ctx, ast_call_argument(call, p)).ok) return false;
            size_t target = function_param_flow_program_point_index_find(store, ast_node_stable_id(callee));
            if (!function_param_flow_no_store_member_admitted(store, target) ||
                store->program_point_indexes[target].function_decl != callee) return false;
        }
    }
    return true;
}
/* The queue is owned by this closure, not an early parameter mask or an index
 * pointer retained across growable table allocation. Cycles close only after
 * every reached header and local store predicate has been checked. */
bool function_param_flow_no_store_graph(FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
    if (store == NULL || decl == NULL || store->failed) return false;
    uint32_t declaration_id = ast_node_stable_id(decl);
    /* Repeating a refusal cannot grant permission. Its only consumer is the
     * unchanged conservative backing path. Positive set reuse replays every
     * current premise; out-of-solver queries reconstruct the graph. No borrowed
     * index pointer survives closure construction. */
    if (store->solving && store->demand_epoch != 0)
        for (size_t i = 0; i < store->program_point_index_count; i++) {
            FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
            if (index->function_decl == decl && index->function_id == declaration_id &&
                index->failed_no_store_epoch == store->demand_epoch) return false;
        }
    bool admitted_current = store->solving && store->demand_epoch != 0 &&
        store->admitted_no_store_count != 0;
    if (admitted_current) {
        if (!function_param_flow_no_store_admitted_current(store)) return false;
        size_t ordinal = function_param_flow_program_point_index_find(store, declaration_id);
        if (function_param_flow_no_store_member_admitted(store, ordinal) &&
            store->program_point_indexes[ordinal].function_decl == decl) return true;
    }
    ASTNode **queue = malloc(sizeof(*queue));
    if (queue == NULL) return false;
    queue[0] = decl;
    size_t count = 1;
    /* Solver work belongs to its current demand. A fresh synchronous currency
     * check outside that solve has its own bounded closure, not the last solve's
     * exhausted counter. Every reached member is still charged below. */
    size_t local_work = 0;
    size_t *work = store->solving ? &store->work_units : &local_work;
    bool closed = true;
    for (size_t at = 0; at < count && closed; at++) {
        if (*work >= store->work_budget) { closed = false; break; }
        (*work)++;
        FunctionParamFlowProgramPointIndex *index;
        if (!semantic_param_publication_routine_no_output_header_ready(store->ctx, queue[at]) ||
            !function_param_flow_program_point_index_build(store, queue[at], &index) ||
            !index->dependency_roots_ready || index->mutable_output_required) { closed = false; break; }
        if (!function_param_flow_index_roots_current(index)) { closed = false; break; }
        for (size_t e = 0; e < index->no_store_call_count; e++) {
            ASTNode *call = index->no_store_calls[e];
            ASTNode *callee = semantic_param_publication_direct_callee(store->ctx, call);
            if (callee == NULL || ast_call_arg_count(call) != ast_func_param_count(callee)) { closed = false; break; }
            for (size_t p = 0; p < ast_call_arg_count(call); p++)
                if (!semantic_param_publication_value(store->ctx, ast_call_argument(call, p)).ok) closed = false;
            if (!closed) break;
            if (admitted_current) {
                size_t ordinal = function_param_flow_program_point_index_find(store, ast_node_stable_id(callee));
                if (function_param_flow_no_store_member_admitted(store, ordinal) &&
                    store->program_point_indexes[ordinal].function_decl == callee) continue;
            }
            size_t found = 0;
            while (found < count && queue[found] != callee) found++;
            if (found != count) continue;
            if (count == SIZE_MAX / sizeof(*queue)) { closed = false; break; }
            ASTNode **grown = realloc(queue, (count + 1) * sizeof(*queue));
            if (grown == NULL) { closed = false; break; }
            queue = grown;
            queue[count++] = callee;
        }
    }
    if (closed && store->solving && store->demand_epoch != 0) {
        size_t prior = store->admitted_no_store_count;
        if (count > SIZE_MAX - prior || prior + count > SIZE_MAX / sizeof(size_t)) closed = false;
        else {
            size_t *members = realloc(store->admitted_no_store_indexes, (prior + count) * sizeof(*members));
            if (members == NULL) closed = false;
            else {
                store->admitted_no_store_indexes = members;
                for (size_t m = 0; m < count; m++) {
                    size_t ordinal = function_param_flow_program_point_index_find(store, ast_node_stable_id(queue[m]));
                    if (ordinal == SIZE_MAX || store->program_point_indexes[ordinal].function_decl != queue[m]) { closed = false; break; }
                    members[prior + m] = ordinal;
                }
                if (closed) {
                    store->admitted_no_store_count = prior + count;
                    for (size_t m = prior; m < prior + count; m++) {
                        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[members[m]];
                        index->admitted_no_store_epoch = store->demand_epoch;
                        index->admitted_no_store_position = m + 1;
                    }
                }
            }
        }
    }
    free(queue);
    if (!closed && store->solving && store->demand_epoch != 0)
        for (size_t i = 0; i < store->program_point_index_count; i++) {
            FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
            if (index->function_decl == decl && index->function_id == declaration_id) {
                index->failed_no_store_epoch = store->demand_epoch;
                break;
            }
        }
    return closed;
}

static bool function_param_flow_local_replacement(SemanticContext *ctx,
    FunctionParamFlowProgramPointIndex *index, ASTNode *assignment)
{
    if (!semantic_param_publication_assignment_replacement(ctx, assignment)) return false;
    ASTNode *node = ast_assignment_target(assignment);
    if (!function_param_flow_index_claim_record(ctx, index, node)) return false;
    uint32_t binding = semantic_param_publication_binding(ctx, node);
    if (binding == 0 || node->type != AST_IDENTIFIER) return false;
    for (size_t i = 0; i < index->local_count; i++)
        if (binding == index->local_ids[i] && !ast_let_is_alias(index->local_definitions[i]) &&
            binding == ast_node_stable_id(index->local_definitions[i])) return true;
    return false;
}
static bool function_param_flow_local_record(FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (ast_let_is_alias(node)) return true;
    size_t count = index->local_count;
    if (ast_node_stable_id(node) == 0 || count == SIZE_MAX / sizeof(ASTNode *)) return false;
    ASTNode **definitions = realloc(index->local_definitions, (count + 1) * sizeof(*definitions));
    if (definitions == NULL) return false;
    index->local_definitions = definitions;
    uint32_t *ids = realloc(index->local_ids, (count + 1) * sizeof(*ids));
    if (ids == NULL) return false;
    index->local_ids = ids;
    definitions[count] = node;
    ids[count] = ast_node_stable_id(node);
    index->local_count++;
    return true;
}
bool function_param_flow_mutable_output_required(SemanticContext *ctx, ASTNode *decl, FunctionParamFlowProgramPointIndex *index, ASTNode *node)
{
    if (node == NULL) return false;
    if (!function_param_flow_index_claim_record(ctx, index, node)) return true;
    bool reachable;
    if (semantic_param_publication_execution(ctx, node, &reachable) && !reachable) return false;
    switch (node->type) {
    case AST_LET_DECL:
        return !function_param_flow_local_record(index, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_let_initializer(node));
    case AST_RETURN: return function_param_flow_mutable_output_required(ctx, decl, index, ast_return_value(node));
    case AST_CALL:
        if (semantic_param_publication_private_array_push(ctx, node, decl)) {
            index->private_local_stores = true;
            for (size_t p = 0; p < ast_call_arg_count(node); p++)
                if (!semantic_param_publication_checked_argument(ctx, node, p) ||
                    function_param_flow_mutable_output_required(ctx, decl, index, ast_call_argument(node, p))) return true;
            return false;
        }
        if (!semantic_param_publication_value_call_target(ctx, node) &&
            !function_param_flow_no_store_call_append(ctx, index, node)) return true;
        if (ast_call_arg_count(node) == 0)
            return !function_param_flow_no_store_call_append(ctx, index, node);
        for (size_t p = 0; p < ast_call_arg_count(node); p++) {
            if (!semantic_param_publication_checked_argument(ctx, node, p)) return true;
            bool depends = false, retains = true;
            bool relation = semantic_param_publication_checked_result_relation(ctx, node, p, &depends, &retains);
            /* Physical scalars carry no backing into an output. Their argument
             * evaluation is still traversed below, including nested stores. */
            if (!semantic_param_publication_value_is_physical_scalar(ctx, ast_call_argument(node, p)) &&
                !(relation && !retains) && !semantic_param_publication_argument_borrowed_for_call(ctx, node, p) &&
                !semantic_typed_map_query_receiver_read(ctx, node, p) &&
                !function_param_flow_no_store_call_append(ctx, index, node)) return true;
            if (function_param_flow_mutable_output_required(ctx, decl, index, ast_call_argument(node, p))) return true;
        }
        return false;
    case AST_ASSIGNMENT:
        return !function_param_flow_local_replacement(ctx, index, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_assignment_value(node));
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++)
            if (function_param_flow_mutable_output_required(ctx, decl, index, ast_block_statement(node, i))) return true;
        return false;
    case AST_IF_STMT:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_if_condition(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_if_then_branch(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_if_else_branch(node));
    case AST_FOR_LOOP:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_for_range_start(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_range_end(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_iterable(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_for_body(node));
    case AST_WHILE_LOOP:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_while_condition(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_while_body(node));
    case AST_WITH_STMT: return function_param_flow_mutable_output_required(ctx, decl, index, ast_with_body(node));
    case AST_IDENTIFIER: case AST_STRING: case AST_NUMBER: case AST_BOOLEAN: case AST_BREAK: case AST_CONTINUE: return false;
    case AST_BINARY:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_binary_left(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_binary_right(node));
    case AST_UNARY: return function_param_flow_mutable_output_required(ctx, decl, index, ast_unary_operand(node));
    case AST_CAST:
        return !semantic_param_publication_checked_numeric_cast(ctx, node) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_cast_operand(node));
    case AST_MEMBER_ACCESS: return function_param_flow_mutable_output_required(ctx, decl, index, ast_member_object(node));
    case AST_ARRAY_ACCESS:
        return function_param_flow_mutable_output_required(ctx, decl, index, ast_array_access_array(node)) ||
            function_param_flow_mutable_output_required(ctx, decl, index, ast_array_access_index(node));
    case AST_ARRAY_LITERAL: {
        /* No elements means no evaluation or store edge. The checked value
         * publication, rather than literal syntax, admits this narrow case;
         * it grants no returned-backing or cleanup permission. */
        SemanticPublishedValue value = semantic_param_publication_value(ctx, node);
        return ast_array_literal_count(node) != 0 || !value.ok || value.meta;
    }
    default: return true;
    }
}
