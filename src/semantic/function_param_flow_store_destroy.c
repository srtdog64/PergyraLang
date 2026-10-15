#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "../common/string_compat.h"
#include "typed_map_query_fact_owner.h"

void semantic_nominal_result_snapshot_destroy(SemanticNominalResultSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    for (size_t i = 0; i < snapshot->field_count && snapshot->fields != NULL; i++) {
        SemanticNominalResultField *field = &snapshot->fields[i];
        free(field->field_name);
        free(field->argument_name);
        free(field->field_type_name);
        free(field->argument_type_name);
        free(field->type_syntax_name);
    }
    free(snapshot->fields);
    free(snapshot->decl_name);
    free(snapshot->result_type_name);
    free(snapshot);
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
    void *metadata[] = {row->arguments, row->target_name,
        row->value_type_name, row->relation_argument_type, row->relation_result_type};
    for (size_t i = 0; i < sizeof(metadata) / sizeof(metadata[0]); i++) free(metadata[i]);
    semantic_nominal_result_snapshot_destroy(row->nominal_result);
    PublishedNode identity = {.node = row->node, .id = row->id, .routine_index = row->routine_index,
        .execution_present = row->execution_present, .reachable = row->reachable};
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
    }
    for (size_t n = 0; n < store->node_capacity; n++) semantic_param_publication_retire_value(&store->nodes[n]);
    free(store->nodes); free(store->routines); free(store->calls); free(store->body_calls);
    free(store); ctx->function_param_publications = NULL;
    semantic_typed_map_query_destroy(ctx);
}

/* Shallow owned-edge projection for the exact selector's admitted syntax.
 * This never discovers descendants or reopens a program/routine root. */
static size_t function_param_flow_claim_child_count(const ASTNode *node)
{
    switch (node->type) {
    case AST_BLOCK: return ast_block_statement_count(node);
    case AST_CALL: return ast_call_arg_count(node) == SIZE_MAX ? SIZE_MAX : ast_call_arg_count(node) + 1;
    case AST_ARRAY_LITERAL: return ast_array_literal_count(node);
    case AST_TUPLE_LITERAL: return ast_tuple_literal_count(node);
    case AST_IF_STMT: return 3;
    case AST_FOR_LOOP: return 4;
    case AST_LET_DECL: case AST_ASSIGNMENT: case AST_BINARY: case AST_ARRAY_ACCESS:
    case AST_WHILE_LOOP: case AST_WITH_STMT: case AST_CHANNEL_SEND: return 2;
    case AST_RETURN: case AST_UNARY: case AST_MEMBER_ACCESS: case AST_CAST: return 1;
    default: return 0;
    }
}
static ASTNode *function_param_flow_claim_child(const ASTNode *node, size_t ordinal)
{
    switch (node->type) {
    case AST_BLOCK: return ast_block_statement(node, ordinal);
    case AST_CALL: return ordinal == 0 ? ast_call_callee(node) : ast_call_argument(node, ordinal - 1);
    case AST_ARRAY_LITERAL: return ast_array_literal_element(node, ordinal);
    case AST_TUPLE_LITERAL: return ast_tuple_literal_element(node, ordinal);
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
bool function_param_flow_claim_record(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node)
{
    if (claims == NULL || claims->failed || node == NULL) return false;
    for (size_t i = 0; i < claims->count; i++) if (claims->rows[i].node == node) return true;
    size_t children = function_param_flow_claim_child_count(node);
    if (claims->count >= SIZE_MAX / sizeof(*claims->rows) || children > SIZE_MAX / sizeof(ASTNode *)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim size overflow"); return false;
    }
    FunctionParamFlowIndexClaim *rows = realloc(claims->rows, (claims->count + 1) * sizeof(*rows));
    if (rows == NULL) { claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim allocation failed"); return false; }
    claims->rows = rows;
    FunctionParamFlowIndexClaim *row = &rows[claims->count++];
    *row = (FunctionParamFlowIndexClaim){.node = node, .id = ast_node_stable_id(node), .shape = node->type, .child_count = children};
    row->execution_present = semantic_param_publication_execution(ctx, node, &row->reachable);
    row->physical_value = semantic_param_publication_value_is_physical_scalar(ctx, node);
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
    if (node->type == AST_CALL) {
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
        const char *target = ast_cast_target_type(node);
        row->cast_target = target == NULL ? NULL : pergyra_strdup(target);
        if (target != NULL && row->cast_target == NULL) {
            claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "cast claim target allocation failed"); return false;
        }
        row->numeric_cast = semantic_param_publication_checked_numeric_cast(ctx, node);
    }
    if (node->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(node);
        row->binding = semantic_param_publication_binding(ctx, target);
        bool local = false;
        for (size_t i = 0; i < claims->count; i++)
            if (claims->rows[i].shape == AST_LET_DECL && claims->rows[i].id == row->binding) local = true;
        row->physical_assignment = local && target != NULL && target->type == AST_IDENTIFIER &&
            semantic_param_publication_value_is_physical_scalar(ctx, target) &&
            semantic_param_publication_value_is_physical_scalar(ctx, ast_assignment_value(node));
    }
    return row->id != 0;
}
bool function_param_flow_index_claims_current(SemanticContext *ctx,
    const FunctionParamFlowProgramPointIndex *index)
{
    if (index->claims == NULL || index->claims->failed) return false;
    for (size_t i = 0; i < index->scalar_local_count; i++)
        if (index->scalar_local_definitions[i]->type != AST_LET_DECL ||
            ast_node_stable_id(index->scalar_local_definitions[i]) != index->scalar_local_ids[i]) return false;
    for (size_t i = 0; i < index->claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &index->claims->rows[i];
        if (row->shape != row->node->type || row->id != ast_node_stable_id(row->node) ||
            row->child_count != function_param_flow_claim_child_count(row->node)) return false;
        for (size_t c = 0; c < row->child_count; c++)
            if (row->children[c] != function_param_flow_claim_child(row->node, c) ||
                row->child_ids[c] != ast_node_stable_id(row->children[c])) return false;
        if (row->shape == AST_IDENTIFIER && row->binding != ast_identifier_binding_syntax_id(row->node)) return false;
        bool reachable = false;
        bool execution_present = semantic_param_publication_execution(ctx, row->node, &reachable);
        if (row->execution_present != execution_present ||
            (execution_present && row->reachable != reachable)) return false;
        if (execution_present && !reachable) continue;
        if (row->physical_value != semantic_param_publication_value_is_physical_scalar(ctx, row->node)) return false;
        if (row->shape == AST_CALL) {
            if (!semantic_param_publication_checked_call(ctx, row->node)) return false;
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
        if (row->physical_assignment && (row->binding == 0 ||
            row->binding != semantic_param_publication_binding(ctx, ast_assignment_target(row->node)) ||
            !semantic_param_publication_value_is_physical_scalar(ctx, ast_assignment_target(row->node)) ||
            !semantic_param_publication_value_is_physical_scalar(ctx, ast_assignment_value(row->node)))) return false;
    }
    return true;
}

/* Structural replay of an already produced row is not a recursive permission.
 * Every reached caller checks its own nodes even when a cycle is visited. */
bool function_param_flow_backing_rows_current(FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index)) return false;
    if (index->backing == NULL) return function_param_flow_index_roots_current(index) &&
        semantic_param_publication_routine_relation_header_ready(store->ctx, decl);
    FunctionParamFlowBackingFact *fact = index->backing;
    if (!function_param_flow_index_roots_current(index) || index->param_count != fact->count ||
        !semantic_param_publication_routine_relation_header_ready(store->ctx, decl)) return false;
    for (size_t p = 0; p < fact->count; p++) {
        uint32_t id = ast_func_param_stable_id(ast_func_param(decl, p));
        size_t b = 0;
        while (b < fact->binding_count && fact->bindings[b].binding != id) b++;
        if (b == fact->binding_count) return false;
    }
    if (fact->validating) return true;
    fact->validating = true;
    bool valid = true;
    for (size_t n = 0; n < fact->node_count && valid; n++) {
        FunctionParamFlowBackingNode *row = &fact->nodes[n];
        valid = row->id == ast_node_stable_id(row->node) && row->shape == row->node->type;
        if (row->node->type == AST_IDENTIFIER && semantic_param_publication_binding(store->ctx, row->node) == 0) valid = false;
        if (row->node->type == AST_CALL)
            for (size_t p = 0; p < ast_call_arg_count(row->node) && valid; p++) {
                valid = semantic_param_publication_checked_argument(store->ctx, row->node, p) &&
                    semantic_param_publication_value(store->ctx, ast_call_argument(row->node, p)).ok;
                ASTNode *callee = semantic_param_publication_callee(store->ctx, row->node, p);
                if (valid && callee != NULL) valid = function_param_flow_backing_rows_current(store, callee);
            }
    }
    fact->validating = false;
    return valid;
}

bool function_param_flow_callee_relation_ready(const SlotFunctionLookup *lookup, ASTNode *call, size_t ordinal)
{
    if (lookup == NULL || lookup->ctx == NULL) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    if (decl == NULL || store == NULL || store->failed) return false;
    for (size_t i = 0; i < store->count; i++) {
        FunctionParamFlowSummaryEntry *row = &store->entries[i];
        if (row->function_id == ast_node_stable_id(decl) && row->param_index == ordinal)
            return (row->state == FUNCTION_PARAM_FLOW_EVALUATED || row->state == FUNCTION_PARAM_FLOW_COMPLETE) &&
                !row->returned_backing_unproved && !row->returned_backing_pending;
    }
    return false;
}
bool function_param_flow_callee_relation_pending(const SlotFunctionLookup *lookup, ASTNode *call, size_t ordinal)
{
    if (lookup == NULL || lookup->ctx == NULL) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    if (decl == NULL || store == NULL || store->failed) return false;
    for (size_t i = 0; i < store->count; i++) {
        FunctionParamFlowSummaryEntry *row = &store->entries[i];
        if (row->function_id == ast_node_stable_id(decl) && row->param_index == ordinal)
            return row->state == FUNCTION_PARAM_FLOW_COMPUTING ||
                (row->state != FUNCTION_PARAM_FLOW_COMPLETE && row->returned_backing_pending && !store->settling_pending);
    }
    return false;
}
void function_param_flow_callee_unfinished(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal, bool *pending, bool *complete)
{
    if (function_param_flow_callee_relation_pending(lookup, call, ordinal)) *pending = true;
    else *complete = false;
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
    free(index->scalar_local_definitions);
    free(index->scalar_local_ids);
    if (index->claims != NULL) {
        for (size_t c = 0; c < index->claims->count; c++) {
            free(index->claims->rows[c].children); free(index->claims->rows[c].child_ids);
            free(index->claims->rows[c].cast_target);
            free(index->claims->rows[c].result_type_name);
        }
        free(index->claims->rows); free(index->claims);
    }
    if (index->backing != NULL) {
        FunctionParamFlowBackingFact *fact = index->backing;
        for (size_t n = 0; n < fact->node_count; n++) free(fact->nodes[n].origins);
        for (size_t b = 0; b < fact->binding_count; b++) {
            free(fact->bindings[b].origins); free(fact->bindings[b].destinations);
        }
        free(fact->nodes); free(fact->bindings); free(fact->outputs); free(fact->effects); free(fact);
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
    free(store);
    ctx->function_param_flow_summaries = NULL;
}
