#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "../common/string_compat.h"
#include "typed_map_query_fact_owner.h"
#include "../common/match_variant_policy.h"

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
    semantic_param_publication_retire_deferred(row);
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

/* One shallow-edge vocabulary serves the initial syntax snapshot and its
 * current replay. Execution policy is carried separately by promotion. */
bool function_param_flow_backing_rows_current(FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
    if (store == NULL || decl == NULL) return false;
    size_t count = 1, capacity = 8;
    ASTNode **queue = malloc(capacity * sizeof(*queue));
    if (queue == NULL) { function_param_flow_fail(store, decl, "backing validation queue allocation failed"); return false; }
    queue[0] = decl;
    bool valid = true;
    for (size_t at = 0; at < count && valid; at++) {
        ASTNode *current = queue[at];
        FunctionParamFlowProgramPointIndex *index;
        if (!function_param_flow_program_point_index_build(store, current, &index) ||
            !function_param_flow_index_roots_current(index) ||
            !semantic_param_publication_routine_relation_header_ready(store->ctx, current)) { valid = false; break; }
        FunctionParamFlowBackingFact **frames = index->backing_frames;
        size_t frame_count = index->backing_frame_count;
        size_t parameter_count = index->param_count;
        FunctionParamFlowIndexClaims *claims = index->claims;
        if (frames == NULL) {
            if (frame_count != 0) valid = false;
            continue;
        }
        if (frame_count != parameter_count || parameter_count == 0 ||
            parameter_count > SIZE_MAX - 63 || claims == NULL) { valid = false; break; }
        size_t destination_words = (parameter_count + 63) / 64;
        uint64_t final_mask = parameter_count % 64 == 0 ? UINT64_MAX :
            (UINT64_C(1) << (parameter_count % 64)) - 1;
        for (size_t input = 0; input < frame_count && valid; input++) {
            FunctionParamFlowBackingFact *fact = frames[input];
            if (fact == NULL) continue;
            uint32_t input_id = ast_func_param_stable_id(ast_func_param(current, input));
            if (input_id == 0 || fact->input_ordinal != input || fact->input_id != input_id ||
                fact->origin_count != 1 || fact->origin_words != 1 ||
                fact->destination_count != parameter_count || fact->destination_words != destination_words ||
                fact->claims != claims || fact->effects == NULL || fact->outputs == NULL ||
                (fact->node_count != 0 && fact->nodes == NULL) ||
                (fact->binding_count != 0 && fact->bindings == NULL) ||
                (fact->outputs[destination_words - 1] & ~final_mask) != 0) { valid = false; break; }
            for (size_t p = 0; p < parameter_count && valid; p++) {
                FuncParam *param = ast_func_param(current, p);
                uint32_t id = ast_func_param_stable_id(param);
                size_t found = SIZE_MAX;
                for (size_t b = 0; b < fact->binding_count; b++) if (fact->bindings[b].binding == id) {
                    if (found != SIZE_MAX) { valid = false; break; }
                    found = b;
                }
                if (!valid || id == 0 || found == SIZE_MAX) { valid = false; break; }
                FunctionParamFlowBackingBinding *binding = &fact->bindings[found];
                if (binding->local || binding->origins == NULL || binding->destinations == NULL ||
                    binding->origins[0] != (p == input ? UINT64_C(1) : UINT64_C(0)) ||
                    ((param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF) &&
                     (binding->destinations[p / 64] & (UINT64_C(1) << (p % 64))) == 0)) valid = false;
            }
            for (size_t b = 0; b < fact->binding_count && valid; b++) {
                FunctionParamFlowBackingBinding *binding = &fact->bindings[b];
                if (binding->binding == 0 || binding->origins == NULL || binding->destinations == NULL ||
                    binding->origin_read_epoch > fact->epoch || binding->destination_read_epoch > fact->epoch ||
                    (binding->origins[0] & ~UINT64_C(1)) != 0 ||
                    (binding->destinations[destination_words - 1] & ~final_mask) != 0) { valid = false; break; }
                if (!binding->local) {
                    bool formal = false;
                    for (size_t p = 0; p < parameter_count; p++)
                        if (binding->binding == ast_func_param_stable_id(ast_func_param(current, p))) formal = true;
                    if (!formal) valid = false;
                    continue;
                }
                bool declaration = false;
                for (size_t c = 0; c < claims->count; c++) {
                    FunctionParamFlowIndexClaim *claim = &claims->rows[c];
                    if (claim->id == binding->binding && claim->shape == AST_LET_DECL &&
                        claim->node != NULL && claim->node->type == AST_LET_DECL &&
                        ast_node_stable_id(claim->node) == binding->binding) declaration = true;
                }
                if (!declaration) valid = false;
            }
            for (size_t n = 0; n < fact->node_count && valid; n++) {
                FunctionParamFlowBackingNode *row = &fact->nodes[n];
                if (row->node == NULL || row->origins == NULL ||
                    (row->origins[0] & ~UINT64_C(1)) != 0 ||
                    row->id == 0 || row->id != ast_node_stable_id(row->node) ||
                    row->shape != row->node->type) { valid = false; break; }
                if (row->node->type == AST_IDENTIFIER && semantic_param_publication_binding(store->ctx, row->node) == 0) valid = false;
                if (row->node->type != AST_CALL) continue;
                for (size_t p = 0; p < ast_call_arg_count(row->node) && valid; p++) {
                    valid = semantic_param_publication_checked_argument(store->ctx, row->node, p) &&
                        semantic_param_publication_value(store->ctx, ast_call_argument(row->node, p)).ok;
                    ASTNode *callee = semantic_param_publication_callee(store->ctx, row->node, p);
                    if (!valid || callee == NULL) continue;
                    size_t reached = 0;
                    while (reached < count && queue[reached] != callee) reached++;
                    if (reached != count) continue;
                    if (count == capacity) {
                        if (capacity > SIZE_MAX / 2 / sizeof(*queue)) { function_param_flow_fail(store, current, "backing validation queue size overflow"); valid = false; break; }
                        capacity *= 2;
                        ASTNode **grown = realloc(queue, capacity * sizeof(*queue));
                        if (grown == NULL) { function_param_flow_fail(store, current, "backing validation queue allocation failed"); valid = false; break; }
                        queue = grown;
                    }
                    queue[count++] = callee;
                }
            }
        }
    }
    free(queue);
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
    for (size_t f = 0; index->backing_frames != NULL && f < index->backing_frame_count; f++) {
        FunctionParamFlowBackingFact *fact = index->backing_frames[f];
        if (fact == NULL) continue;
        for (size_t n = 0; fact->nodes != NULL && n < fact->node_count; n++) free(fact->nodes[n].origins);
        for (size_t b = 0; fact->bindings != NULL && b < fact->binding_count; b++) {
            free(fact->bindings[b].origins); free(fact->bindings[b].destinations);
        }
        free(fact->nodes); free(fact->bindings); free(fact->outputs); free(fact->effects); free(fact);
    }
    free(index->backing_frames);
    function_param_flow_claims_destroy(index->claims);
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
