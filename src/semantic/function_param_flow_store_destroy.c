#include "function_param_flow_summary_internal.h"

#include <limits.h>
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
        (FunctionParamFlowPendingRelation){
            .call = call, .call_id = ast_node_stable_id(call),
            .callee_id = 0, .ordinal = SIZE_MAX,
            .claim_ordinal = SIZE_MAX};
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
    if (!function_param_flow_program_point_index_build(store, decl, &index))
        return NULL;
    if (producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) {
        FunctionParamFlowBackingFact *frame = index->backing_frames != NULL
            && index->backing_frame_count > 0 ? index->backing_frames[0] : NULL;
        if (frame == NULL || frame->epoch == 0 || !frame->complete || frame->building
            || frame->pending || frame->analysis_unsupported || frame->claims != index->claims
            || frame->input_ordinal != 0 || frame->input_id == 0
            || frame->input_id != ast_func_param_stable_id(ast_func_param(decl, 0))
            || !function_param_flow_backing_effect_facets_current(frame)
            || frame->outputs == NULL
            || frame->origin_count != 1 || frame->origin_words != 1
            || frame->destination_count != ast_func_param_count(decl)
            || frame->destination_words != (frame->destination_count + 63) / 64
            || (frame->effects[0] & ~SLOT_PARAM_SUMMARY_READ) != 0) return NULL;
        for (size_t w = 0; w < frame->destination_words; w++) if (frame->outputs[w] != 0) return NULL;
        if (frame->private_stores_closed && !function_param_flow_no_store_graph(store, decl)) return NULL;
    } else if (producer != FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN
        || !semantic_param_publication_routine_return_dependency_is_scalar(store->ctx, decl)
        || index->mutable_output_required || index->private_local_stores
        || index->contextual_output_required
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
            (!function_param_flow_program_point_index_read_sealed(
                store, decl, &index) &&
             !function_param_flow_program_point_index_build(store, decl, &index)) ||
            !index->dependency_roots_ready || index->mutable_output_required ||
            index->contextual_output_required) return false;
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
    uint32_t function_id = ast_node_stable_id(decl);
    size_t i = function_id == 0 ? SIZE_MAX :
        function_param_flow_find(store, function_id, formal);
    if (i == SIZE_MAX || i >= store->count) return false;
    FunctionParamFlowSummaryEntry *row = &store->entries[i];
    bool ready =
        (row->state == FUNCTION_PARAM_FLOW_EVALUATED ||
         row->state == FUNCTION_PARAM_FLOW_COMPLETE) &&
        !row->returned_backing_unproved && !row->returned_backing_pending &&
        (store->solving || function_param_flow_output_current(store, i, false));
    if (!ready || row->output_producer !=
            FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME)
        return ready;
    unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
    return function_param_flow_entry_outward_effects_current(store, i,
        &outward_effects);
}

bool function_param_flow_entry_outward_effects_current(
    FunctionParamFlowSummaryStore *store, size_t entry_index,
    unsigned *effects)
{
    if (effects != NULL) *effects = SLOT_PARAM_SUMMARY_NONE;
    if (store == NULL || store->failed || effects == NULL ||
        entry_index >= store->count) return false;
    FunctionParamFlowSummaryEntry *entry = &store->entries[entry_index];
    ASTNode *decl = entry->function_decl;
    size_t formal = entry->param_index;
    if (decl == NULL || decl->type != AST_FUNC_DECL ||
        formal >= ast_func_param_count(decl) ||
        (entry->state != FUNCTION_PARAM_FLOW_EVALUATED &&
         entry->state != FUNCTION_PARAM_FLOW_COMPLETE) ||
        entry->analysis_unsupported || entry->returned_backing_unproved ||
        entry->returned_backing_pending || !entry->output_dependencies_ready ||
        (!store->solving &&
         !function_param_flow_output_current(store, entry_index, false)))
        return false;

    const unsigned outward = SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
        SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    if (entry->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) {
        if (entry->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR &&
            entry->output_producer !=
                FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN)
            return false;
        if (entry->output_producer ==
                FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR) {
            FunctionParamFlowProgramPointIndex *index = NULL;
            if ((function_param_flow_program_point_index_read_sealed(
                    store, decl, &index) ||
                 function_param_flow_program_point_index_build(store, decl,
                    &index)) &&
                function_param_flow_unused_formal_current(index, formal)) {
                *effects = SLOT_PARAM_SUMMARY_NONE;
                return true;
            }
        }
        *effects = entry->mask & outward;
        return true;
    }

    FunctionParamFlowProgramPointIndex *index = NULL;
    if ((!function_param_flow_program_point_index_read_sealed(
            store, decl, &index) &&
         !function_param_flow_program_point_index_build(store, decl, &index)) ||
        index == NULL || index->backing_frames == NULL ||
        formal >= index->backing_frame_count) return false;
    FunctionParamFlowBackingFact *frame = index->backing_frames[formal];
    if (frame == NULL || frame->input_ordinal != formal ||
        frame->input_id == 0 ||
        frame->input_id != ast_func_param_stable_id(
            ast_func_param(decl, formal)) ||
        frame->origin_count != 1 || frame->origin_words != 1 ||
        !function_param_flow_backing_effect_facets_current(frame) ||
        (frame->building && !store->settling_recursive_backing) ||
        !frame->complete || frame->pending ||
        frame->analysis_unsupported)
        return false;

    /* The backing frame owns the exact outward answer. During recursive SCC
     * replay it grows before the public entry mask is published at the end of
     * the replay step. A recursive edge must read that provisional owner, not
     * reject the expected one-step projection lag. Outside that bounded
     * settlement window the public projection must already be exact. */
    bool provisional_recursive_owner = store->settling_recursive_backing &&
        frame->building;
    if (!provisional_recursive_owner &&
        !function_param_flow_backing_effect_projection_current(
            frame, entry->mask))
        return false;
    *effects = frame->outward_effects[0] & outward;
    return true;
}

bool function_param_flow_formal_outward_effects_current(
    FunctionParamFlowSummaryStore *store, ASTNode *decl, size_t formal,
    unsigned *effects)
{
    if (effects != NULL) *effects = SLOT_PARAM_SUMMARY_NONE;
    if (store == NULL || store->failed || decl == NULL ||
        decl->type != AST_FUNC_DECL || formal >= ast_func_param_count(decl) ||
        effects == NULL) return false;
    size_t entry = function_param_flow_find(store, ast_node_stable_id(decl),
        formal);
    if (entry == SIZE_MAX) {
        if (ast_func_body(decl) == NULL ||
            !semantic_param_publication_inputs_ready(store->ctx) ||
            !semantic_param_publication_routine_inputs_current(
                store->ctx, decl, true))
            return false;
        (void)function_param_flow_summary_for_param(store->ctx, decl, formal);
        store = store->ctx->function_param_flow_summaries;
        if (store == NULL || store->failed) return false;
        entry = function_param_flow_find(store, ast_node_stable_id(decl),
            formal);
    }
    return !store->failed && entry != SIZE_MAX &&
        function_param_flow_entry_outward_effects_current(store, entry,
            effects);
}

bool function_param_flow_callee_outward_effects_current(
    const SlotFunctionLookup *lookup, ASTNode *call, size_t ordinal,
    unsigned *effects)
{
    if (effects != NULL) *effects = SLOT_PARAM_SUMMARY_NONE;
    if (lookup == NULL || lookup->ctx == NULL || call == NULL ||
        call->type != AST_CALL || effects == NULL ||
        ordinal >= ast_call_arg_count(call)) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call,
        ordinal);
    size_t formal = 0;
    FunctionParamFlowSummaryStore *store =
        lookup->ctx->function_param_flow_summaries;
    if (decl == NULL || store == NULL || store->failed ||
        !semantic_param_publication_checked_argument(lookup->ctx, call,
            ordinal) ||
        !semantic_param_publication_formal_for_actual(lookup->ctx, call,
            ordinal, &formal)) return false;

    size_t entry_index = function_param_flow_find(store,
        ast_node_stable_id(decl), formal);
    /* Prepared flow summaries normally own this row already. Demand it only
     * while the typed publication is still current; a post-landing or opaque
     * external query must fail closed without emitting a secondary internal
     * "sealed inputs" diagnostic. */
    if (entry_index == SIZE_MAX) {
        if (ast_func_body(decl) == NULL ||
            !semantic_param_publication_inputs_ready(lookup->ctx) ||
            !semantic_param_publication_routine_inputs_current(
                lookup->ctx, decl, true))
            return false;
        (void)function_param_flow_summary_for_call(lookup, call, ordinal);
        store = lookup->ctx->function_param_flow_summaries;
        if (store == NULL || store->failed) return false;
        entry_index = function_param_flow_find(store,
            ast_node_stable_id(decl), formal);
    }
    return !store->failed && entry_index != SIZE_MAX &&
        function_param_flow_callee_relation_ready(lookup, call, ordinal) &&
        function_param_flow_entry_outward_effects_current(store, entry_index,
            effects);
}

bool function_param_flow_callee_returned_dependent_clone_current(
    const SlotFunctionLookup *lookup, ASTNode *call, size_t ordinal,
    bool *dependent_clone)
{
    if (dependent_clone != NULL) *dependent_clone = false;
    if (lookup == NULL || lookup->ctx == NULL || call == NULL ||
        call->type != AST_CALL || dependent_clone == NULL ||
        ordinal >= ast_call_arg_count(call))
        return false;
    if ((function_param_flow_summary_for_call(lookup, call, ordinal) &
            SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0 ||
        !function_param_flow_callee_relation_ready(lookup, call, ordinal))
        return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call,
        ordinal);
    size_t formal = 0;
    FunctionParamFlowSummaryStore *store =
        lookup->ctx->function_param_flow_summaries;
    if (decl == NULL || store == NULL || store->failed ||
        !semantic_param_publication_formal_for_actual(lookup->ctx, call,
            ordinal, &formal))
        return false;
    size_t entry = function_param_flow_find(store, ast_node_stable_id(decl),
        formal);
    if (entry == SIZE_MAX || entry >= store->count)
        return false;
    FunctionParamFlowSummaryEntry *summary = &store->entries[entry];
    if (summary->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME)
        return true;
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (!function_param_flow_program_point_index_read_sealed(store, decl,
            &index) ||
        index == NULL || index->backing_frames == NULL ||
        formal >= index->backing_frame_count ||
        index->backing_frames[formal] == NULL)
        return false;
    *dependent_clone =
        index->backing_frames[formal]->returned_dependent_clone;
    return true;
}

bool function_param_flow_callee_relation_pending(const SlotFunctionLookup *lookup, ASTNode *call, size_t ordinal)
{
    if (lookup == NULL || lookup->ctx == NULL) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    size_t formal = 0;
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    if (decl == NULL || store == NULL || store->failed ||
        !semantic_param_publication_formal_for_actual(lookup->ctx, call, ordinal, &formal)) return false;
    uint32_t function_id = ast_node_stable_id(decl);
    size_t i = function_id == 0 ? SIZE_MAX :
        function_param_flow_find(store, function_id, formal);
    if (i == SIZE_MAX || i >= store->count) return false;
    FunctionParamFlowSummaryEntry *row = &store->entries[i];
    if (row->state == FUNCTION_PARAM_FLOW_COMPUTING ||
        (row->state != FUNCTION_PARAM_FLOW_COMPLETE &&
         row->returned_backing_pending)) return true;
    if (row->output_producer !=
            FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME)
        return false;
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (!function_param_flow_program_point_index_build(store, decl,
            &index) || index == NULL || index->backing_frames == NULL ||
        formal >= index->backing_frame_count) return false;
    FunctionParamFlowBackingFact *frame = index->backing_frames[formal];
    if (frame != NULL && (frame->building || frame->pending))
        return true;

    /* An active frame can observe another closure member after that
     * member evaluated but before its backing relation is settled.
     * Preserve this exact call relation as pending; whether it is a
     * recursive SCC is decided only after the whole closure reaches
     * a fixed point. This is not admission: a relation outside the
     * selected SCC remains pending during replay and is rejected. */
    return store->solving &&
        i >= store->active_start &&
        row->function_decl == decl &&
        row->state == FUNCTION_PARAM_FLOW_EVALUATED &&
        !row->analysis_unsupported &&
        row->returned_backing_unproved &&
        !row->returned_backing_pending &&
        !row->output_dependencies_ready &&
        row->output_dependencies != NULL &&
        row->output_count == ast_func_param_count(decl) &&
        frame != NULL && frame->epoch != 0 &&
        !frame->analysis_unsupported && !frame->building &&
        !frame->complete && !frame->pending &&
        frame->claims == index->claims &&
        frame->input_ordinal == formal && frame->input_id != 0 &&
        frame->input_id == ast_func_param_stable_id(
            ast_func_param(decl, formal)) &&
        frame->origin_count == 1 && frame->origin_words == 1 &&
        frame->destination_count == row->output_count &&
        frame->destination_words == (row->output_count + 63) / 64 &&
        function_param_flow_backing_effect_facets_current(frame) &&
        (frame->pending_relation_count == 0 ||
         frame->pending_relations != NULL) &&
        frame->sealed_pending_relation_count ==
            frame->pending_relation_count;
}
/* A receiver into a method whose receiver-formal summary is unsupported
 * makes this frame unsupported, as an argument into an unsupported formal
 * does in function_param_flow_callee_unfinished. */
bool function_param_flow_receiver_unsupported(ReturnedBacking *state, ASTNode *call, ASTNode *decl)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (fact->analysis_unsupported) return true;
    if (!fact->complete || decl == NULL) return false;
    FunctionParamFlowPendingRelation unsupported = {
        .call = call,
        .call_id = ast_node_stable_id(call),
        .callee_id = ast_node_stable_id(decl),
        .ordinal = FUNCTION_PARAM_FLOW_RECEIVER_ORDINAL,
        .claim_ordinal = SIZE_MAX};
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
    size_t claim_ordinal = SIZE_MAX, formal = SIZE_MAX;
    ASTNode *target = NULL;
    if (!function_param_flow_claim_record_ordinal(
            lookup->ctx, fact->claims, call, &claim_ordinal) ||
        !function_param_flow_call_argument_claim_current(
            fact->claims, claim_ordinal, call, ordinal, &target, &formal,
            NULL)) {
        fact->complete = false; return;
    }
    FunctionParamFlowPendingRelation unsupported = {
        .call = call,
        .call_id = ast_node_stable_id(call),
        .callee_id = ast_node_stable_id(target),
        .ordinal = ordinal,
        .claim_ordinal = claim_ordinal};
    if (target != NULL && function_param_flow_unsupported_relation_current(store, &unsupported)) {
        fact->analysis_unsupported = true; fact->unsupported_relation = unsupported;
        fact->complete = false; fact->pending = false;
        free(fact->pending_relations); fact->pending_relations = NULL;
        fact->pending_relation_count = fact->sealed_pending_relation_count = 0;
        return;
    }
    if (!function_param_flow_callee_relation_pending(lookup, call, ordinal)) { fact->complete = false; return; }
    ASTNode *callee = target;
    uint32_t call_id = ast_node_stable_id(call), callee_id = ast_node_stable_id(callee);
    if (call_id == 0 || callee_id == 0 || !semantic_param_publication_checked_argument(lookup->ctx, call, ordinal)) {
        fact->complete = false; return;
    }
    /* A pending edge reopens the whole relation set, including existing edges. */
    fact->pending = true; fact->sealed_pending_relation_count = 0;
    for (size_t e = 0; e < fact->pending_relation_count; e++) {
        FunctionParamFlowPendingRelation *edge = &fact->pending_relations[e];
        if (edge->call == call && edge->call_id == call_id && edge->callee_id == callee_id && edge->ordinal == ordinal) {
            if (edge->claim_ordinal != claim_ordinal) fact->complete = false;
            return;
        }
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
    rows[fact->pending_relation_count++] = (FunctionParamFlowPendingRelation){
        .call = call,
        .call_id = call_id,
        .callee_id = callee_id,
        .ordinal = ordinal,
        .claim_ordinal = claim_ordinal};
}

bool function_param_flow_complete_pending_relations(FunctionParamFlowSummaryStore *store)
{
    if (store == NULL || store->failed || !store->solving || store->changed ||
        !store->active_had_recursion) {
        return false;
    }
    size_t end = store->count;
    for (size_t i = store->active_start; i < end; i++) {
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (entry->state != FUNCTION_PARAM_FLOW_EVALUATED) {
            return false;
        }
        /* Validate the exact pending seeds before changing any frame. Other
         * required formals can be unresolved members of the same routine SCC;
         * selecting and replaying that component below is what completes them.
         * A required row outside the component remains unproved and the final
         * output-current consumer still rejects it. */
        if (!entry->returned_backing_pending) continue;
        if (!function_param_flow_output_current(store, i, true)) {
            return false;
        }
        if (store->count != end)
            return false;
        if (!entry->returned_backing_pending) continue;
        FunctionParamFlowProgramPointIndex *index;
        if (entry->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME ||
            !function_param_flow_program_point_index_build(
                store, entry->function_decl, &index) ||
            index->backing_frames == NULL ||
            entry->param_index >= index->backing_frame_count) {
            return false;
        }
        FunctionParamFlowBackingFact *fact = index->backing_frames[entry->param_index];
        if (fact == NULL || !fact->pending || !fact->complete || fact->building || fact->pending_relation_count == 0 ||
            fact->pending_relations == NULL || entry->output_dependencies == NULL ||
            entry->output_count != fact->destination_count ||
            (fact->effects[0] & ~entry->mask) != 0) {
            return false;
        }
        for (size_t p = 0; p < entry->output_count; p++)
            if (entry->output_dependencies[p] != ((fact->outputs[p / 64] & (UINT64_C(1) << (p % 64))) != 0)) {
                return false;
            }
        for (size_t e = 0; e < fact->pending_relation_count; e++) {
            FunctionParamFlowPendingRelation edge = fact->pending_relations[e];
            ASTNode *callee = NULL;
            size_t formal = SIZE_MAX;
            if (!function_param_flow_call_argument_claim_current(
                    index->claims, edge.claim_ordinal, edge.call,
                    edge.ordinal, &callee, &formal, NULL) ||
                callee == NULL || ast_node_stable_id(callee) != edge.callee_id) {
                return false;
            }
            size_t target = 0;
            while (target < end && (store->entries[target].function_id != edge.callee_id ||
                store->entries[target].param_index != formal)) target++;
            if (target == end) {
                return false;
            }
            if (target < store->active_start) {
                if (store->entries[target].state != FUNCTION_PARAM_FLOW_COMPLETE ||
                    store->entries[target].returned_backing_pending || store->entries[target].returned_backing_unproved ||
                    !function_param_flow_output_current(store, target, false))
                    return false;
            } else if (store->entries[target].state != FUNCTION_PARAM_FLOW_EVALUATED)
                return false;
        }
    }

    size_t active_count = end - store->active_start;
    bool *replay = calloc(active_count == 0 ? 1 : active_count, sizeof(*replay));
    if (replay == NULL) {
        function_param_flow_fail(store, store->entries[store->active_start].function_decl,
            "recursive backing replay set allocation failed");
        return false;
    }
    for (size_t i = store->active_start; i < end; i++) {
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (!entry->returned_backing_pending) continue;
        replay[i - store->active_start] = true;
    }

    /* Validation above observes every pending edge while all entries still
     * have their fixed-point state.  Seal them simultaneously so replay of a
     * self or mutual edge reads one coherent provisional snapshot. */
    for (size_t i = store->active_start; i < end; i++) {
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (!replay[i - store->active_start]) continue;
        FunctionParamFlowProgramPointIndex *index;
        if (!function_param_flow_program_point_index_build(store, entry->function_decl, &index)) {
            free(replay);
            return false;
        }
        FunctionParamFlowBackingFact *fact = index->backing_frames[entry->param_index];
        fact->sealed_pending_relation_count = fact->pending_relation_count;
        fact->pending = false;
        fact->result_path_ready =
            (fact->outward_effects[0] & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0 ||
            fact->result_path.present;
        if (!fact->result_path_ready) {
            free(replay);
            return false;
        }
        entry->returned_backing_pending = false; entry->returned_backing_unproved = false;
        entry->output_dependencies_ready = true;
        entry->result_path = fact->result_path;
        entry->result_path_ready = true;
        entry->output_current_epoch = 0;
    }

    /* A relation can have completed earlier while reading a provisional mask
     * from another member of the same recursion.  Pending edges alone cannot
     * recover that member: by definition its edge was already complete.  Use
     * publication's sealed body-call graph to select the exact routine SCC.
     * Replaying every active relation is too broad because it can disturb a
     * one-way inout dependency that never participates in the cycle. */
    SemanticParamPublicationView publication = {0};
    if (!semantic_param_publication_admitted_view(store->ctx, &publication) ||
        !semantic_param_publication_view_current(store->ctx, &publication)) {
        free(replay);
        return false;
    }
    for (size_t e = 0; e < publication.body_call_count; e++) {
        const PendingBodyCall *edge = NULL;
        if (!semantic_param_publication_body_call_at_current(
                store->ctx, e, &edge) ||
            edge != &publication.body_calls[e]) {
            free(replay);
            return false;
        }
    }
    bool *forward = calloc(active_count == 0 ? 1 : active_count,
        sizeof(*forward));
    bool *backward = calloc(active_count == 0 ? 1 : active_count,
        sizeof(*backward));
    if (forward == NULL || backward == NULL) {
        free(forward); free(backward); free(replay);
        function_param_flow_fail(store,
            store->entries[store->active_start].function_decl,
            "recursive backing component allocation failed");
        return false;
    }
    for (size_t i = 0; i < active_count; i++)
        forward[i] = backward[i] = replay[i];

    bool graph_changed;
    do {
        graph_changed = false;
        for (size_t offset = 0; offset < active_count; offset++) {
            if (!forward[offset]) continue;
            size_t i = store->active_start + offset;
            FunctionParamFlowSummaryEntry *entry = &store->entries[i];
            for (size_t e = 0; e < publication.body_call_count; e++) {
                const PendingBodyCall *edge = &publication.body_calls[e];
                if (edge->caller_id != entry->function_id) continue;
                for (size_t target = store->active_start;
                     target < end; target++) {
                    if (store->entries[target].function_id != edge->callee_id)
                        continue;
                    size_t target_offset = target - store->active_start;
                    if (!forward[target_offset]) {
                        forward[target_offset] = true;
                        graph_changed = true;
                    }
                    break;
                }
            }
        }
    } while (graph_changed);

    do {
        graph_changed = false;
        for (size_t offset = 0; offset < active_count; offset++) {
            if (backward[offset]) continue;
            size_t i = store->active_start + offset;
            FunctionParamFlowSummaryEntry *entry = &store->entries[i];
            bool reaches_seed = false;
            for (size_t e = 0;
                 e < publication.body_call_count && !reaches_seed; e++) {
                const PendingBodyCall *edge = &publication.body_calls[e];
                if (edge->caller_id != entry->function_id) continue;
                for (size_t target = store->active_start;
                     target < end; target++) {
                    if (store->entries[target].function_id == edge->callee_id &&
                        backward[target - store->active_start]) {
                        reaches_seed = true;
                        break;
                    }
                }
            }
            if (reaches_seed) {
                backward[offset] = true;
                graph_changed = true;
            }
        }
    } while (graph_changed);

    size_t growth_bound = 1;
    for (size_t offset = 0; offset < active_count; offset++) {
        replay[offset] = forward[offset] && backward[offset];
        if (!replay[offset]) continue;
        size_t i = store->active_start + offset;
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        if (entry->output_producer !=
            FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) {
            replay[offset] = false;
            continue;
        }
        FunctionParamFlowProgramPointIndex *index;
        if (!function_param_flow_program_point_index_build(store,
                entry->function_decl, &index) || index->backing_frames == NULL ||
            entry->param_index >= index->backing_frame_count) {
            free(forward); free(backward); free(replay);
            return false;
        }
        FunctionParamFlowBackingFact *fact =
            index->backing_frames[entry->param_index];
        /* A member can have completed before the edge that exposed the
         * recursion, so zero pending relations is valid here. The sealed
         * body-call SCC is its replay authority. */
        if (fact == NULL || !fact->complete || fact->pending ||
            fact->building ||
            !function_param_flow_backing_effect_facets_current(fact) ||
            (fact->pending_relation_count != 0 &&
             fact->pending_relations == NULL) ||
            fact->sealed_pending_relation_count != fact->pending_relation_count ||
            entry->returned_backing_pending || entry->returned_backing_unproved ||
            !entry->output_dependencies_ready ||
            entry->output_dependencies == NULL ||
            entry->output_count != fact->destination_count) {
            free(forward); free(backward); free(replay);
            return false;
        }
        size_t entry_bound = sizeof(unsigned) * CHAR_BIT +
            entry->output_count + FUNCTION_PARAM_FLOW_RESULT_PATH_LIMIT +
            FUNCTION_PARAM_FLOW_RESULT_BRANCH_LIMIT + 2;
        if (entry_bound < entry->output_count ||
            growth_bound > SIZE_MAX - entry_bound) {
            free(forward); free(backward); free(replay);
            function_param_flow_fail(store, entry->function_decl,
                "recursive backing replay growth bound overflow");
            return false;
        }
        growth_bound += entry_bound;
    }
    free(forward); free(backward);

    /* Every replay frame accumulated effects while at least one recursive
     * edge was unresolved. Those provisional effects are not evidence: a
     * RETURN-only cycle can otherwise retain a transient CALL forever. Drop
     * only the outward effect lattice, simultaneously for the sealed set;
     * keep access/release bits and all origin/destination/path evidence. The
     * full body replay below then rebuilds the least outward fixed point. */
    const unsigned escape_mask = SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
        SLOT_PARAM_SUMMARY_CALL_ESCAPE | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
    bool any_change = false;
    for (size_t i = store->active_start; i < end; i++) {
        if (!replay[i - store->active_start]) continue;
        FunctionParamFlowSummaryEntry *entry = &store->entries[i];
        FunctionParamFlowProgramPointIndex *index;
        if (!function_param_flow_program_point_index_build(store,
                entry->function_decl, &index) || index->backing_frames == NULL ||
            entry->param_index >= index->backing_frame_count) {
            free(replay);
            return false;
        }
        FunctionParamFlowBackingFact *fact =
            index->backing_frames[entry->param_index];
        any_change |= (entry->mask & escape_mask) != 0 ||
            fact->effects[0] != SLOT_PARAM_SUMMARY_NONE ||
            fact->outward_effects[0] != SLOT_PARAM_SUMMARY_NONE;
        entry->mask &= ~escape_mask;
        fact->effects[0] = SLOT_PARAM_SUMMARY_NONE;
        fact->outward_effects[0] = SLOT_PARAM_SUMMARY_NONE;
        entry->output_current_epoch = 0;
    }

    bool settled = true, grew;
    size_t rounds = 0;
    store->settling_recursive_backing = true;
    do {
        grew = false;
        if (rounds++ >= growth_bound) {
            function_param_flow_fail(store, store->entries[store->active_start].function_decl,
                "recursive backing replay exceeded its fact growth bound");
            settled = false;
            break;
        }
        for (size_t i = store->active_start; i < end; i++) {
            if (!replay[i - store->active_start]) continue;
            FunctionParamFlowSummaryEntry *entry = &store->entries[i];
            ASTNode *entry_decl = entry->function_decl;
            size_t entry_param = entry->param_index;
            FunctionParamFlowProgramPointIndex *index;
            if (!function_param_flow_program_point_index_build(store,
                    entry_decl, &index) || index->backing_frames == NULL ||
                entry_param >= index->backing_frame_count) {
                settled = false; break;
            }
            FunctionParamFlowBackingFact *fact = index->backing_frames[entry_param];
            size_t relation_count = fact->pending_relation_count;
            FunctionParamFlowPendingRelation *relations = relation_count == 0
                ? NULL : malloc(relation_count * sizeof(*relations));
            if (relation_count != 0 && relations == NULL) {
                settled = false; break;
            }
            if (relation_count != 0)
                memcpy(relations, fact->pending_relations,
                    relation_count * sizeof(*relations));
            size_t output_count = entry->output_count;
            bool private_stores_closed = fact->private_stores_closed;
            bool *outputs = calloc(output_count == 0 ? 1 : output_count,
                sizeof(*outputs));
            unsigned effects = SLOT_PARAM_SUMMARY_NONE;
            unsigned previous_outward = fact->outward_effects[0];
            bool pending = false;
            FunctionParamFlowResultPath path = {0};
            bool replayed = outputs != NULL &&
                function_param_flow_replay_recursive_backing_effects(store,
                    entry_decl, entry_param, &effects, outputs,
                    output_count, &pending, &path, private_stores_closed);
            bool shape_current = fact->pending_relation_count >= relation_count &&
                fact->sealed_pending_relation_count == fact->pending_relation_count &&
                (relation_count == 0 || (fact->pending_relations != NULL &&
                 memcmp(relations, fact->pending_relations,
                    relation_count * sizeof(*relations)) == 0));
            if (!replayed || pending || !shape_current) {
                free(relations); free(outputs);
                settled = false; break;
            }
            if (fact->pending_relation_count != relation_count)
                any_change = true;
            free(relations);

            /* The replay can grow only the already-owned outward lattice.
             * Reacquire the entry because nested demands may move the array. */
            entry = &store->entries[i];
            unsigned next_mask = (entry->mask & ~escape_mask) |
                (effects & escape_mask);
            if (next_mask != entry->mask) {
                entry->mask = next_mask;
                grew = any_change = true;
            }
            if (fact->outward_effects[0] != previous_outward)
                grew = any_change = true;
            for (size_t p = 0; p < output_count; p++) {
                if (outputs[p] && !entry->output_dependencies[p]) {
                    entry->output_dependencies[p] = true;
                    grew = any_change = true;
                }
            }
            free(outputs);
            if (!entry->result_path_ready ||
                !function_param_flow_result_path_equal(&entry->result_path, &path)) {
                entry->result_path = path;
                grew = any_change = true;
            }
            entry->returned_backing_pending = false;
            entry->returned_backing_unproved = false;
            entry->output_dependencies_ready = true;
            entry->result_path_ready = true;
            entry->output_current_epoch = 0;
        }
    } while (settled && grew);
    store->settling_recursive_backing = false;
    free(replay);
    if (!settled || store->failed) {
        return false;
    }
    /* Replaying a newly visible relation can demand another formal or grow a
     * published mask/output/path.  Re-enter the ordinary closure instead of
     * sealing a partial graph; the next pass owns those new readers and any
     * pending frames they introduce. */
    if (any_change || store->count != end) {
        for (size_t i = store->active_start; i < store->count; i++)
            store->entries[i].dirty = true;
        return true;
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
        index->backing_frames == NULL ||
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
    for (size_t i = store->active_start;
         !store->failed && i < store->count; i++) {
        bool required;
        bool required_known = function_param_flow_backing_required(store,
            store->entries[i].function_decl,
            store->entries[i].param_index, &required);
        bool output_current = required_known && (!required ||
            function_param_flow_output_current(store, i, false));
        if (!required_known || !output_current) {
            function_param_flow_fail(store, store->entries[i].function_decl,
                "required return backing relation is incomplete or stale");
        }
    }
}
