#include "function_param_flow_summary_internal.h"
#include "function_param_publication_owner.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "typed_map_query_fact_owner.h"

/* Compilation-owned fact storage and its exported snapshot share one lifetime.
 * Admission and currentness remain with their existing semantic owners. */

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
        for (size_t n = 0; fact->nodes != NULL && n < fact->node_count; n++) free(fact->nodes[n].origins);
        for (size_t b = 0; fact->bindings != NULL && b < fact->binding_count; b++) {
            free(fact->bindings[b].origins); free(fact->bindings[b].destinations);
        }
        free(fact->nodes); free(fact->bindings); free(fact->outputs); free(fact->effects); free(fact->pending_relations); free(fact);
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
