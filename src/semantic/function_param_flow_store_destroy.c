#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "../parser/ast_analysis.h"
#include "../common/string_compat.h"
#include "typed_map_query_fact_owner.h"
#include "../common/match_variant_policy.h"
#include "indexed_string_borrow_owner.h"

bool function_param_flow_unsupported_relation_current(FunctionParamFlowSummaryStore *store,
    const FunctionParamFlowPendingRelation *edge)
{
    if (edge->call == NULL || edge->call->type != AST_CALL || edge->call_id == 0 ||
        edge->call_id != ast_node_stable_id(edge->call) ||
        !semantic_param_publication_checked_call(store->ctx, edge->call) ||
        !semantic_param_publication_value(store->ctx, edge->call).ok) return false;
    for (size_t p = 0; p < ast_call_arg_count(edge->call); p++)
        if (!semantic_param_publication_checked_argument(store->ctx, edge->call, p) ||
            !semantic_param_publication_value(store->ctx, ast_call_argument(edge->call, p)).ok) return false;
    if (edge->callee_id == 0)
        return edge->ordinal == SIZE_MAX && semantic_param_publication_call_analysis_unmodelled(store->ctx, edge->call);
    ASTNode *callee, *receiver = NULL;
    size_t formal = 0;
    if (edge->ordinal == FUNCTION_PARAM_FLOW_RECEIVER_ORDINAL) {
        callee = semantic_param_publication_direct_callee(store->ctx, edge->call);
        if (callee == NULL || !semantic_param_publication_instance_receiver(store->ctx, edge->call, &receiver) ||
            receiver == NULL) return false;
    } else {
        callee = semantic_param_publication_callee(store->ctx, edge->call, edge->ordinal);
        if (callee == NULL ||
            !semantic_param_publication_formal_for_actual(store->ctx, edge->call, edge->ordinal, &formal)) return false;
    }
    if (edge->callee_id != ast_node_stable_id(callee)) return false;
    for (size_t i = 0; i < store->count; i++) {
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (entry->function_decl != callee || entry->param_index != formal) continue;
        if (!entry->analysis_unsupported || entry->mask != SLOT_PARAM_SUMMARY_ALL ||
            !entry->returned_backing_unproved || entry->returned_backing_pending || entry->output_dependencies_ready) return false;
        FunctionParamFlowProgramPointIndex *index;
        if (!function_param_flow_program_point_index_build(store, callee, &index) || index->backing_frames == NULL ||
            formal >= index->backing_frame_count) return false;
        FunctionParamFlowBackingFact *fact = index->backing_frames[formal];
        return fact != NULL && fact->analysis_unsupported && !fact->complete && !fact->pending;
    }
    return false;
}
bool function_param_flow_backing_unsupported_call(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowBackingFact *fact, ASTNode *call)
{
    if (!fact->complete && !fact->analysis_unsupported) return false;
    if (!semantic_param_publication_call_analysis_unmodelled(store->ctx, call)) return false;
    if (!fact->analysis_unsupported) fact->unsupported_relation =
        (FunctionParamFlowPendingRelation){call, ast_node_stable_id(call), 0, SIZE_MAX};
    fact->analysis_unsupported = true; fact->complete = false; fact->pending = false;
    free(fact->pending_relations); fact->pending_relations = NULL;
    fact->pending_relation_count = fact->sealed_pending_relation_count = 0;
    return true;
}
ASTNode *function_param_flow_receiver_readonly_current(FunctionParamFlowSummaryStore *store,
    ASTNode *call, bool demand)
{
    if (store == NULL || store->failed) return NULL;
    ASTNode *receiver = NULL;
    ASTNode *decl = semantic_param_publication_direct_callee(store->ctx, call);
    if (decl == NULL || !semantic_param_publication_instance_receiver(store->ctx, call, &receiver)
        || receiver == NULL || !semantic_param_publication_value(store->ctx, receiver).ok) return NULL;
    if (demand && !store->validating_current) (void)function_param_flow_summary_demand(
        &(SlotFunctionLookup){store->ctx, store->program_root}, decl, 0);
    size_t at = function_param_flow_find(store, ast_node_stable_id(decl), 0);
    if (store->failed || at == SIZE_MAX) return NULL;
    const FunctionParamFlowSummaryEntry *entry = &store->entries[at];
    if (entry->function_decl != decl || entry->analysis_unsupported
        || (entry->state != FUNCTION_PARAM_FLOW_COMPLETE
            && !(store->solving && entry->state == FUNCTION_PARAM_FLOW_EVALUATED))
        || (entry->mask & ~SLOT_PARAM_SUMMARY_READ) != 0
        || entry->returned_backing_unproved || entry->returned_backing_pending
        || !entry->output_dependencies_ready || entry->output_dependencies == NULL
        || entry->output_count != ast_func_param_count(decl)) return NULL;
    for (size_t p = 0; p < entry->output_count; p++)
        if (entry->output_dependencies[p]) return NULL;
    FunctionParamFlowOutputProducer producer = entry->output_producer;
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index)
        || !function_param_flow_index_roots_current(index)) return NULL;
    if (producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) {
        FunctionParamFlowBackingFact *frame = index->backing_frames != NULL
            && index->backing_frame_count > 0 ? index->backing_frames[0] : NULL;
        if (frame == NULL || frame->epoch == 0 || !frame->complete || frame->building
            || frame->pending || frame->analysis_unsupported || frame->claims != index->claims
            || frame->input_ordinal != 0 || frame->input_id == 0
            || frame->input_id != ast_func_param_stable_id(ast_func_param(decl, 0))
            || frame->effects == NULL || frame->outputs == NULL
            || frame->origin_count != 1 || frame->origin_words != 1
            || frame->destination_count != ast_func_param_count(decl)
            || frame->destination_words != (frame->destination_count + 63) / 64
            || (frame->effects[0] & ~SLOT_PARAM_SUMMARY_READ) != 0) return NULL;
        for (size_t w = 0; w < frame->destination_words; w++) if (frame->outputs[w] != 0) return NULL;
        if (frame->private_stores_closed && !function_param_flow_no_store_graph(store, decl)) return NULL;
    } else if (producer != FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN
        || !semantic_param_publication_routine_return_dependency_is_scalar(store->ctx, decl)
        || index->mutable_output_required || index->private_local_stores
        || !semantic_param_publication_routine_no_output_header_ready(store->ctx, decl)
        || !function_param_flow_no_store_graph(store, decl)) return NULL;
    return decl;
}


bool function_param_flow_no_store_member_admitted(
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
 * never permission; current typed facts and exact membership must still join.
 * The set only grows within a demand and its typed inputs are sealed, so a
 * query without a capture replays only the members added since the last
 * complete replay in this demand. A capture needs every member, and
 * PGY_VERIFY_FLOW_CURRENCY_MEMO replays all of them. */
bool function_param_flow_no_store_admitted_current(FunctionParamFlowSummaryStore *store,
    FunctionParamFlowInputCapture capture, void *user)
{
    size_t first = capture == NULL && store->demand_epoch != 0 &&
        store->admitted_no_store_replayed_epoch == store->demand_epoch &&
        store->admitted_no_store_replayed_count <= store->admitted_no_store_count &&
        getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") == NULL
        ? store->admitted_no_store_replayed_count : 0;
    for (size_t m = first; m < store->admitted_no_store_count; m++) {
        size_t ordinal = store->admitted_no_store_indexes[m];
        if (!function_param_flow_no_store_member_admitted(store, ordinal)) return false;
        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[ordinal];
        ASTNode *decl = index->function_decl;
        if (index->function_id != ast_node_stable_id(decl) ||
            index->failed_no_store_epoch == store->demand_epoch ||
            !semantic_param_publication_routine_no_output_header_ready(store->ctx, decl) ||
            !function_param_flow_program_point_index_build(store, decl, &index) ||
            !index->dependency_roots_ready || index->mutable_output_required) return false;
        if (capture != NULL && !capture(store->ctx, decl, index->claims, NULL, user)) return false;
        for (size_t e = 0; e < index->no_store_call_count; e++) {
            ASTNode *call = index->no_store_calls[e];
            ASTNode *callee = semantic_param_publication_direct_callee(store->ctx, call);
            if (callee == NULL) return false;
            for (size_t p = 0; p < ast_call_arg_count(call); p++)
                if (!semantic_param_publication_checked_argument(store->ctx, call, p) ||
                    !semantic_param_publication_value(store->ctx, ast_call_argument(call, p)).ok) return false;
            size_t target = function_param_flow_program_point_index_find(store, ast_node_stable_id(callee));
            if (!function_param_flow_no_store_member_admitted(store, target) ||
                store->program_point_indexes[target].function_decl != callee) return false;
            if (capture != NULL && !capture(store->ctx, decl, NULL, call, user)) return false;
        }
    }
    if (capture == NULL) {
        store->admitted_no_store_replayed_epoch = store->demand_epoch;
        store->admitted_no_store_replayed_count = store->admitted_no_store_count;
    }
    return true;
}
bool function_param_flow_callee_relation_ready(const SlotFunctionLookup *lookup, ASTNode *call, size_t ordinal)
{
    if (lookup == NULL || lookup->ctx == NULL) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    size_t formal = 0;
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    if (decl == NULL || store == NULL || store->failed ||
        !semantic_param_publication_formal_for_actual(lookup->ctx, call, ordinal, &formal)) return false;
    for (size_t i = 0; i < store->count; i++) {
        FunctionParamFlowSummaryEntry *row = &store->entries[i];
        if (row->function_id == ast_node_stable_id(decl) && row->param_index == formal)
            return (row->state == FUNCTION_PARAM_FLOW_EVALUATED || row->state == FUNCTION_PARAM_FLOW_COMPLETE) &&
                !row->returned_backing_unproved && !row->returned_backing_pending &&
                (store->solving || function_param_flow_output_current(store, i, false));
    }
    return false;
}
bool function_param_flow_callee_relation_pending(const SlotFunctionLookup *lookup, ASTNode *call, size_t ordinal)
{
    if (lookup == NULL || lookup->ctx == NULL) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    size_t formal = 0;
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    if (decl == NULL || store == NULL || store->failed ||
        !semantic_param_publication_formal_for_actual(lookup->ctx, call, ordinal, &formal)) return false;
    for (size_t i = 0; i < store->count; i++) {
        FunctionParamFlowSummaryEntry *row = &store->entries[i];
        if (row->function_id == ast_node_stable_id(decl) && row->param_index == formal)
            return row->state == FUNCTION_PARAM_FLOW_COMPUTING ||
                (row->state != FUNCTION_PARAM_FLOW_COMPLETE && row->returned_backing_pending);
    }
    return false;
}
/* A receiver into a method whose receiver-formal summary is unsupported
 * makes this frame unsupported, as an argument into an unsupported formal
 * does in function_param_flow_callee_unfinished. */
bool function_param_flow_receiver_unsupported(ReturnedBacking *state, ASTNode *call, ASTNode *decl)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (fact->analysis_unsupported) return true;
    if (!fact->complete || decl == NULL) return false;
    FunctionParamFlowPendingRelation unsupported = {call, ast_node_stable_id(call), ast_node_stable_id(decl),
        FUNCTION_PARAM_FLOW_RECEIVER_ORDINAL};
    if (!function_param_flow_unsupported_relation_current(state->store, &unsupported)) return false;
    fact->analysis_unsupported = true; fact->unsupported_relation = unsupported;
    fact->complete = false; fact->pending = false;
    free(fact->pending_relations); fact->pending_relations = NULL;
    fact->pending_relation_count = fact->sealed_pending_relation_count = 0;
    return true;
}
void function_param_flow_callee_unfinished(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal, FunctionParamFlowBackingFact *fact)
{
    if (fact->analysis_unsupported) return;
    if (!fact->complete) return;
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    ASTNode *target = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    FunctionParamFlowPendingRelation unsupported = {call, ast_node_stable_id(call), ast_node_stable_id(target), ordinal};
    if (target != NULL && function_param_flow_unsupported_relation_current(store, &unsupported)) {
        fact->analysis_unsupported = true; fact->unsupported_relation = unsupported;
        fact->complete = false; fact->pending = false;
        free(fact->pending_relations); fact->pending_relations = NULL;
        fact->pending_relation_count = fact->sealed_pending_relation_count = 0;
        return;
    }
    if (!function_param_flow_callee_relation_pending(lookup, call, ordinal)) { fact->complete = false; return; }
    ASTNode *callee = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    uint32_t call_id = ast_node_stable_id(call), callee_id = ast_node_stable_id(callee);
    if (call_id == 0 || callee_id == 0 || !semantic_param_publication_checked_argument(lookup->ctx, call, ordinal)) {
        fact->complete = false; return;
    }
    /* A pending edge reopens the whole relation set, including existing edges. */
    fact->pending = true; fact->sealed_pending_relation_count = 0;
    for (size_t e = 0; e < fact->pending_relation_count; e++) {
        FunctionParamFlowPendingRelation *edge = &fact->pending_relations[e];
        if (edge->call == call && edge->call_id == call_id && edge->callee_id == callee_id && edge->ordinal == ordinal) return;
    }
    if (fact->pending_relation_count == SIZE_MAX / sizeof(*fact->pending_relations)) {
        function_param_flow_fail(lookup->ctx->function_param_flow_summaries, call, "pending relation size overflow"); fact->complete = false; return;
    }
    FunctionParamFlowPendingRelation *rows = realloc(fact->pending_relations,
        (fact->pending_relation_count + 1) * sizeof(*rows));
    if (rows == NULL) {
        function_param_flow_fail(lookup->ctx->function_param_flow_summaries, call, "pending relation allocation failed"); fact->complete = false; return;
    }
    fact->pending_relations = rows;
    rows[fact->pending_relation_count++] = (FunctionParamFlowPendingRelation){call, call_id, callee_id, ordinal};
}

bool function_param_flow_complete_pending_relations(FunctionParamFlowSummaryStore *store)
{
    if (store == NULL || store->failed || !store->solving || store->changed || !store->active_had_recursion) return false;
    size_t end = store->count;
    for (size_t i = store->active_start; i < end; i++) {
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (entry->state != FUNCTION_PARAM_FLOW_EVALUATED ||
            !function_param_flow_output_current(store, i, true) || store->count != end) return false;
        if (!entry->returned_backing_pending) continue;
        FunctionParamFlowProgramPointIndex *index;
        if (entry->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME ||
            !function_param_flow_program_point_index_build(store, entry->function_decl, &index) ||
            index->backing_frames == NULL || entry->param_index >= index->backing_frame_count) return false;
        FunctionParamFlowBackingFact *fact = index->backing_frames[entry->param_index];
        if (fact == NULL || !fact->pending || !fact->complete || fact->building || fact->pending_relation_count == 0 ||
            fact->pending_relations == NULL || entry->output_dependencies == NULL ||
            entry->output_count != fact->destination_count || (fact->effects[0] & ~entry->mask) != 0) return false;
        for (size_t p = 0; p < entry->output_count; p++)
            if (entry->output_dependencies[p] != ((fact->outputs[p / 64] & (UINT64_C(1) << (p % 64))) != 0)) return false;
        for (size_t e = 0; e < fact->pending_relation_count; e++) {
            FunctionParamFlowPendingRelation edge = fact->pending_relations[e];
            size_t formal = 0;
            if (!semantic_param_publication_formal_for_actual(store->ctx,
                    edge.call, edge.ordinal, &formal)) return false;
            size_t target = 0;
            while (target < end && (store->entries[target].function_id != edge.callee_id ||
                store->entries[target].param_index != formal)) target++;
            if (target == end) return false;
            if (target < store->active_start) {
                if (store->entries[target].state != FUNCTION_PARAM_FLOW_COMPLETE ||
                    store->entries[target].returned_backing_pending || store->entries[target].returned_backing_unproved ||
                    !function_param_flow_output_current(store, target, false)) return false;
            } else if (store->entries[target].state != FUNCTION_PARAM_FLOW_EVALUATED ||
                (!store->entries[target].returned_backing_pending && store->entries[target].returned_backing_unproved)) return false;
        }
    }
    for (size_t i = store->active_start; i < end; i++) {
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (!entry->returned_backing_pending) continue;
        FunctionParamFlowProgramPointIndex *index;
        if (!function_param_flow_program_point_index_build(store, entry->function_decl, &index)) return false;
        FunctionParamFlowBackingFact *fact = index->backing_frames[entry->param_index];
        fact->sealed_pending_relation_count = fact->pending_relation_count;
        fact->pending = false;
        entry->returned_backing_pending = false; entry->returned_backing_unproved = false;
        entry->output_dependencies_ready = true;
    }
    for (size_t i = store->active_start; i < end; i++)
        if (!function_param_flow_output_current(store, i, false)) {
            function_param_flow_fail(store, store->entries[i].function_decl, "completed recursive backing proof is stale"); return false;
        }
    return !store->failed && store->count == end;
}

bool function_param_flow_backing_unsupported(FunctionParamFlowSummaryStore *store, ASTNode *decl, size_t ordinal)
{
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index) ||
        !function_param_flow_index_roots_current(index) || index->backing_frames == NULL ||
        ordinal >= index->backing_frame_count) return false;
    FunctionParamFlowBackingFact *fact = index->backing_frames[ordinal];
    return fact != NULL && fact->analysis_unsupported && !fact->complete && !fact->pending && !fact->building &&
        function_param_flow_backing_rows_current(store, decl);
}
bool function_param_flow_unproved_current(FunctionParamFlowSummaryStore *store, size_t i)
{
    if (i >= store->count) return false;
    const FunctionParamFlowSummaryEntry *entry = &store->entries[i];
    bool required;
    return entry->analysis_unsupported && entry->mask == SLOT_PARAM_SUMMARY_ALL && entry->array_storage_unproved && entry->array_elements_unproved &&
        entry->array_buffer_unkept &&
        entry->returned_backing_unproved && !entry->returned_backing_pending && !entry->output_dependencies_ready &&
        entry->output_producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME && entry->output_dependencies != NULL &&
        entry->output_count == ast_func_param_count(entry->function_decl) &&
        function_param_flow_backing_required(store, entry->function_decl, entry->param_index, &required) &&
        function_param_flow_backing_unsupported(store, entry->function_decl, entry->param_index);
}
void function_param_flow_complete_return_relations(FunctionParamFlowSummaryStore *store)
{
    for (size_t i = store->active_start; !store->failed && i < store->count; i++) {
        bool required;
        if (!function_param_flow_backing_required(store,
                store->entries[i].function_decl, store->entries[i].param_index, &required) ||
            (required && !function_param_flow_output_current(store, i, false)))
            function_param_flow_fail(store, store->entries[i].function_decl,
                "required return backing relation is incomplete or stale");
    }
}
