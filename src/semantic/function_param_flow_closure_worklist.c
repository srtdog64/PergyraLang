/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Which entries a pass of one solving parameter-flow closure evaluates. The
 * closure records which routine read which entry; after the first pass, a pass
 * evaluates again only the entries of routines that read an entry after which
 * it changed.
 */

#include <stdint.h>
#include <stdlib.h>

#include "function_param_flow_summary_internal.h"

/* A read is recorded after the demanded entry answered, so a change made
 * while answering it is already seen. A routine, not one entry, is the
 * reader: its other entries check the frames this entry's evaluation built,
 * and those frames name the entries it read. */
bool function_param_flow_closure_read(FunctionParamFlowSummaryStore *store, size_t read)
{
    if (store == NULL || !store->solving || store->evaluating_plus_one == 0 ||
        read < store->active_start || read >= store->count) return true;
    size_t evaluating = store->evaluating_plus_one - 1;
    if (evaluating >= store->count) {
        function_param_flow_fail(store, store->program_root, "closure reader identity is missing");
        return false;
    }
    ASTNode *reader = store->entries[evaluating].function_decl;
    for (size_t at = store->entries[read].readers_plus_one; at != 0; at = store->closure_reads[at - 1].next_plus_one)
        if (store->closure_reads[at - 1].reader == reader) return true;
    if (store->closure_read_count == store->closure_read_capacity) {
        size_t capacity = store->closure_read_capacity == 0 ? 64 : store->closure_read_capacity * 2;
        FunctionParamFlowClosureRead *grown = capacity < store->closure_read_capacity ||
            capacity > SIZE_MAX / sizeof(*grown) ? NULL :
            realloc(store->closure_reads, capacity * sizeof(*grown));
        if (grown == NULL) {
            function_param_flow_fail(store, reader, "closure read list allocation failed");
            return false;
        }
        store->closure_reads = grown;
        store->closure_read_capacity = capacity;
    }
    store->closure_reads[store->closure_read_count] = (FunctionParamFlowClosureRead){
        reader, store->entries[read].readers_plus_one};
    store->entries[read].readers_plus_one = ++store->closure_read_count;
    return true;
}

/* An entry not yet evaluated in this pass reads the change when it is. */
void function_param_flow_closure_changed(FunctionParamFlowSummaryStore *store, size_t changed)
{
    if (store == NULL || !store->solving || changed < store->active_start || changed >= store->count) return;
    for (size_t at = store->entries[changed].readers_plus_one; at != 0; at = store->closure_reads[at - 1].next_plus_one) {
        ASTNode *reader = store->closure_reads[at - 1].reader;
        uint32_t id = ast_node_stable_id(reader);
        for (size_t p = 0; p < ast_func_param_count(reader); p++) {
            size_t entry = function_param_flow_find(store, id, p);
            if (entry != SIZE_MAX && entry >= store->active_start &&
                store->entries[entry].state != FUNCTION_PARAM_FLOW_UNSEEN)
                store->entries[entry].dirty = true;
        }
    }
}

FunctionParamFlowEntryFacts function_param_flow_entry_facts(const FunctionParamFlowSummaryEntry *entry)
{
    FunctionParamFlowEntryFacts facts = {entry->mask, entry->array_storage_unproved, entry->array_elements_unproved,
        entry->returned_backing_unproved, entry->returned_backing_pending, entry->analysis_unsupported,
        entry->output_dependencies_ready, entry->output_dependencies != NULL, entry->output_producer, 0};
    for (size_t p = 0; facts.outputs_present && p < entry->output_count; p++)
        facts.outputs += entry->output_dependencies[p];
    return facts;
}

bool function_param_flow_entry_facts_equal(FunctionParamFlowEntryFacts a, FunctionParamFlowEntryFacts b)
{
    return a.mask == b.mask && a.storage == b.storage && a.elements == b.elements &&
        a.unproved == b.unproved && a.pending == b.pending && a.unsupported == b.unsupported &&
        a.ready == b.ready && a.outputs_present == b.outputs_present && a.producer == b.producer &&
        a.outputs == b.outputs;
}

/* Evaluates an entry not yet evaluated in this pass as the reader of what it
 * demands; a change marks the routines that already read it. */
unsigned
function_param_flow_evaluate(FunctionParamFlowSummaryStore *store,
                             size_t index)
{
    if (store == NULL || index >= store->count || store->entries[index].state != FUNCTION_PARAM_FLOW_UNSEEN)
        return function_param_flow_evaluate_body(store, index);
    FunctionParamFlowEntryFacts before = function_param_flow_entry_facts(&store->entries[index]);
    size_t outer = store->evaluating_plus_one;
    store->evaluating_plus_one = index + 1;
    unsigned mask = function_param_flow_evaluate_body(store, index);
    store->evaluating_plus_one = outer;
    if (!function_param_flow_entry_facts_equal(before, function_param_flow_entry_facts(&store->entries[index])))
        function_param_flow_closure_changed(store, index);
    return mask;
}

/* PGY_VERIFY_FLOW_WORKLIST: once no entry is marked, a full pass, uncharged,
 * must change nothing; otherwise a pass skipped an entry whose reads changed. */
bool function_param_flow_worklist_verified(FunctionParamFlowSummaryStore *store, ASTNode *site)
{
    if (getenv("PGY_VERIFY_FLOW_WORKLIST") == NULL) return true;
    size_t end = store->count, work = store->work_units, budget = store->work_budget;
    size_t evaluations = store->body_evaluations;
    FunctionParamFlowEntryFacts *facts = malloc((end - store->active_start + 1) * sizeof(*facts));
    if (facts == NULL) { function_param_flow_fail(store, site, "worklist check allocation failed"); return false; }
    for (size_t i = store->active_start; i < end; i++) {
        facts[i - store->active_start] = function_param_flow_entry_facts(&store->entries[i]);
        store->entries[i].state = FUNCTION_PARAM_FLOW_UNSEEN;
    }
    store->work_budget = SIZE_MAX;
    for (size_t i = store->active_start; i < end && !store->failed; i++)
        (void)function_param_flow_evaluate(store, i);
    store->work_units = work;
    store->work_budget = budget;
    store->body_evaluations = evaluations;
    bool same = !store->failed && store->count == end;
    for (size_t i = store->active_start; same && i < end; i++)
        same = !store->entries[i].dirty &&
            function_param_flow_entry_facts_equal(facts[i - store->active_start], function_param_flow_entry_facts(&store->entries[i]));
    free(facts);
    if (!same) function_param_flow_fail(store, site, "parameter flow worklist skipped an entry whose reads changed");
    return same;
}
