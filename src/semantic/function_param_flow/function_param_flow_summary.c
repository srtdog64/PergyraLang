/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Demanded function-parameter resource/escape summary owner.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../parser/ast_analysis.h"
#include "../slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "../type_checker/type_checker_internal.h"
#include "function_param_publication_owner.h"
#include "../typed_map_query_fact_owner.h"



void
function_param_flow_fail(FunctionParamFlowSummaryStore *store,
                         ASTNode *site,
                         const char *reason)
{
    if (store == NULL || store->failed)
        return;
    store->failed = true;
    if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
        const char *function_name = ast_declaration_name(site);
        const char *root_name = store->active_start < store->count
            ? ast_declaration_name(
                store->entries[store->active_start].function_decl)
            : NULL;
        fprintf(stderr,
            "pgy: function-param-flow-fail function=%s line=%u column=%u"
            " root=%s reason=%s prior_error=%d solving=%d work_units=%zu"
            " work_budget=%zu active_start=%zu entries=%zu\n",
            function_name != NULL ? function_name : "<unknown>",
            site != NULL ? site->line : 0,
            site != NULL ? site->column : 0,
            root_name != NULL ? root_name : "<none>",
            reason != NULL ? reason : "unknown owner failure",
            store->ctx->has_error, store->solving, store->work_units,
            store->work_budget, store->active_start, store->count);
    }
    if (store->ctx->has_error) return;
    semantic_error(store->ctx, site,
        "Function parameter flow summary failed closed: %s",
        reason != NULL ? reason : "unknown owner failure");
}


static bool function_param_flow_output_replay(FunctionParamFlowSummaryStore *store,
    size_t entry_index, bool pending_proof);

static bool
function_param_flow_advance_demand_epoch(FunctionParamFlowSummaryStore *store,
                                         ASTNode *site)
{
    if (store->demand_epoch == SIZE_MAX) {
        function_param_flow_fail(store, site,
            "summary demand identity overflow");
        return false;
    }
    store->demand_epoch++;
    store->admitted_no_store_count = 0;
    return true;
}

bool
function_param_flow_publication_demand_begin(SemanticContext *ctx,
                                             ASTNode *site)
{
    SlotFunctionLookup lookup = {ctx, ctx != NULL ? ctx->program_root : NULL};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    if (store == NULL || site == NULL || store->failed)
        return false;
    if (store->solving || store->publication_demand_active) {
        function_param_flow_fail(store, site,
            "publication summary demand is already active");
        return false;
    }
    if (!function_param_flow_advance_demand_epoch(store, site))
        return false;
    store->publication_demand_active = true;
    return true;
}

bool
function_param_flow_publication_demand_end(SemanticContext *ctx)
{
    FunctionParamFlowSummaryStore *store = ctx != NULL
        ? ctx->function_param_flow_summaries : NULL;
    if (store == NULL || !store->publication_demand_active)
        return false;
    store->publication_demand_active = false;
    /* Reuse is scoped to preparation. A later consumer must replay against
     * current inputs rather than inheriting the batch's successful epoch. */
    if (!function_param_flow_advance_demand_epoch(store, store->program_root))
        return false;
    return !store->failed;
}

/* A completed summary's output rests on sealed typed inputs and on frames
 * that later evaluations in the same demand do not change, so one successful
 * replay serves that demand. A new demand replays again. With
 * PGY_VERIFY_FLOW_CURRENCY_MEMO set, every reuse also replays and a
 * divergence fails the store closed. */
bool function_param_flow_output_current(FunctionParamFlowSummaryStore *store, size_t entry_index, bool pending_proof)
{
    static int verify_currency = -1;
    if (entry_index >= store->count) return false;
    if (verify_currency < 0)
        verify_currency = getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") != NULL;
    bool memo = !pending_proof && store->demand_epoch != 0 &&
        store->entries[entry_index].state == FUNCTION_PARAM_FLOW_COMPLETE;
    bool reused = memo && store->entries[entry_index].output_current_epoch == store->demand_epoch;
    if (reused && !verify_currency) return true;
    bool current = function_param_flow_output_replay(store, entry_index, pending_proof);
    if (reused && !current) {
        function_param_flow_fail(store, store->entries[entry_index].function_decl,
            "completed output currency memo diverged from its replay");
        return false;
    }
    /* The replay may grow the entries array; reacquire by index. */
    if (memo) store->entries[entry_index].output_current_epoch = current ? store->demand_epoch : 0;
    return current;
}

static bool function_param_flow_output_replay(FunctionParamFlowSummaryStore *store,
    size_t entry_index, bool pending_proof)
{
    if (entry_index >= store->count) return false;
    if (store->entries[entry_index].analysis_unsupported) return function_param_flow_unproved_current(store, entry_index);
    ASTNode *decl = store->entries[entry_index].function_decl;
    size_t ordinal = store->entries[entry_index].param_index;
    FunctionParamFlowOutputProducer producer = store->entries[entry_index].output_producer;
    FunctionParamFlowProgramPointIndex *points;
    if (!function_param_flow_program_point_index_read_sealed(
            store, decl, &points) &&
        !function_param_flow_program_point_index_build(store, decl, &points))
        return false;
    bool unused_formal = function_param_flow_unused_formal_current(points,
        ordinal);
    bool return_backing_required;
    if (!function_param_flow_backing_required(store, decl, ordinal, &return_backing_required)) return false;
    bool waiting = pending_proof && store->entries[entry_index].returned_backing_pending;
    if (return_backing_required && !unused_formal &&
        (producer != FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME ||
        (!waiting && !store->entries[entry_index].output_dependencies_ready) ||
        store->entries[entry_index].output_dependencies == NULL ||
        store->entries[entry_index].output_count != ast_func_param_count(decl) ||
        (!waiting && (store->entries[entry_index].returned_backing_unproved ||
        store->entries[entry_index].returned_backing_pending)))) return false;
    if (producer == FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN) {
        const FunctionParamFlowSummaryEntry *entry = &store->entries[entry_index];
        if (!entry->output_dependencies_ready || entry->output_dependencies == NULL ||
            entry->output_count != ast_func_param_count(decl) ||
            entry->returned_backing_unproved || entry->returned_backing_pending ||
            !semantic_param_publication_routine_return_dependency_is_scalar(store->ctx, decl)) return false;
        for (size_t p = 0; p < entry->output_count; p++)
            if (entry->output_dependencies[p]) return false;
    }
    /* Anchored handles have no descriptor-output receipt, but their cached
     * access/escape summary still depends on the current executed children. */
    if (!waiting && !store->entries[entry_index].output_dependencies_ready)
        return function_param_flow_backing_rows_current(store, decl);
    if (producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) {
        if (points->backing_frames == NULL || ordinal >= points->backing_frame_count) return false;
        FunctionParamFlowBackingFact *frame = points->backing_frames[ordinal];
        bool effect_facets_current = frame != NULL &&
            function_param_flow_backing_effect_facets_current(frame);
        bool result_path_current = frame != NULL && (waiting ||
            (frame->result_path_ready &&
             store->entries[entry_index].result_path_ready &&
             function_param_flow_result_path_equal(&frame->result_path,
                 &store->entries[entry_index].result_path)));
        bool frame_current = frame != NULL && frame->input_ordinal == ordinal &&
            frame->input_id != 0 &&
            frame->input_id == ast_func_param_stable_id(
                ast_func_param(decl, ordinal)) &&
            frame->origin_count == 1 && frame->origin_words == 1 &&
            effect_facets_current &&
            frame->destination_count == ast_func_param_count(decl) &&
            frame->destination_count <= SIZE_MAX - 63 &&
            frame->destination_words ==
                (frame->destination_count + 63) / 64 &&
            frame->epoch != 0 && !frame->building && frame->complete &&
            (!frame->pending || waiting) && result_path_current;
        if (!frame_current) {
            if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
                const char *name = ast_declaration_name(decl);
                fprintf(stderr,
                    "pgy: function-param-flow-output-frame-stale function=%s"
                    " ordinal=%zu frame=%d input_ordinal=%zu input_id=%u"
                    " formal_id=%u origins=%zu/%zu destinations=%zu/%zu"
                    " facets=%d epoch=%zu building=%d complete=%d pending=%d"
                    " waiting=%d frame_path=%d entry_path=%d path_equal=%d\n",
                    name != NULL ? name : "<unknown>", ordinal,
                    frame != NULL,
                    frame != NULL ? frame->input_ordinal : SIZE_MAX,
                    frame != NULL ? frame->input_id : 0,
                    ast_func_param_stable_id(ast_func_param(decl, ordinal)),
                    frame != NULL ? frame->origin_count : 0,
                    frame != NULL ? frame->origin_words : 0,
                    frame != NULL ? frame->destination_count : 0,
                    frame != NULL ? frame->destination_words : 0,
                    effect_facets_current,
                    frame != NULL ? frame->epoch : 0,
                    frame != NULL && frame->building,
                    frame != NULL && frame->complete,
                    frame != NULL && frame->pending, waiting,
                    frame != NULL && frame->result_path_ready,
                    store->entries[entry_index].result_path_ready,
                    result_path_current);
            }
            return false;
        }
        if (frame->private_stores_closed &&
            !function_param_flow_no_store_graph(store, decl)) {
            if (getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
                const char *name = ast_declaration_name(decl);
                fprintf(stderr,
                    "pgy: function-param-flow-output-private-store-stale"
                    " function=%s ordinal=%zu\n",
                    name != NULL ? name : "<unknown>", ordinal);
            }
            return false;
        }
    } else if (producer == FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR ||
        producer == FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN) {
        const FunctionParamFlowSummaryEntry *entry =
            &store->entries[entry_index];
        if (producer == FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR &&
            unused_formal) {
            if (entry->output_dependencies == NULL ||
                entry->output_count != ast_func_param_count(decl) ||
                entry->returned_backing_unproved ||
                entry->returned_backing_pending)
                return false;
            for (size_t p = 0; p < entry->output_count; p++)
                if (entry->output_dependencies[p]) return false;
            return true;
        }
        bool anchored = false;
        if (!semantic_param_publication_parameter_is_anchored_handle(store->ctx, decl, ordinal, &anchored) ||
            anchored || points->mutable_output_required || points->private_local_stores ||
            points->contextual_output_required ||
            !semantic_param_publication_routine_no_output_header_ready(
                store->ctx, decl)) return false;
        for (size_t p = 0; p < ast_func_param_count(decl); p++) {
            FuncParam *param = ast_func_param(decl, p);
            if (param == NULL || param->mode == PARAM_MODE_MUT_REF) return false;
        }
        if (!function_param_flow_no_store_graph(store, decl)) return false;
    } else return false;
    bool backing_rows_current = producer ==
            FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME
        ? function_param_flow_backing_frame_current(store, decl, ordinal)
        : function_param_flow_backing_rows_current(store, decl);
    if (!backing_rows_current &&
        getenv("PGY_DEBUG_FUNCTION_PARAM_FLOW") != NULL) {
        const char *name = ast_declaration_name(decl);
        fprintf(stderr,
            "pgy: function-param-flow-output-rows-stale"
            " function=%s ordinal=%zu\n",
            name != NULL ? name : "<unknown>", ordinal);
    }
    return backing_rows_current;
}

static unsigned
function_param_flow_demand_index(FunctionParamFlowSummaryStore *store,
                                 ASTNode *function_decl,
                                 size_t param_index)
{
    size_t index = function_param_flow_add(store, function_decl, param_index);
    FunctionParamFlowSummaryState state;

    if (index == SIZE_MAX) {
        function_param_flow_fail(store, function_decl,
            ast_node_stable_id(function_decl) == 0
                ? "function stable identity is missing"
                : "summary hash allocation failed");
        return SLOT_PARAM_SUMMARY_ALL;
    }

    state = store->entries[index].state;
    if (state == FUNCTION_PARAM_FLOW_COMPLETE) {
        if (!function_param_flow_output_current(store, index, false)) return SLOT_PARAM_SUMMARY_ALL;
        store->cache_hits++;
        return store->entries[index].mask;
    }
    unsigned mask;
    if (state == FUNCTION_PARAM_FLOW_COMPUTING) {
        store->active_had_recursion = true;
        store->recursion_hits++;
        mask = store->entries[index].mask;
    } else if (state == FUNCTION_PARAM_FLOW_EVALUATED)
        mask = store->entries[index].mask;
    else mask = function_param_flow_evaluate(store, index);
    return function_param_flow_closure_read(store, index) ? mask : SLOT_PARAM_SUMMARY_ALL;
}

unsigned
function_param_flow_evaluate_body(FunctionParamFlowSummaryStore *store,
                                  size_t index)
{
    ASTNode *function_decl;
    FuncParam *param;
    FunctionParamFlowProgramPointIndex *program_points;
    ASTNode *const *roots;
    size_t root_count;
    SlotSummaryOrigin origin = {0};
    SlotFunctionLookup lookup;
    unsigned candidate;
    unsigned previous;
    unsigned storage_unproved;
    bool payload_read;
    bool buffer_unkept = false;
    bool timing = getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL;
    clock_t timing_started = timing ? clock() : 0;
    clock_t timing_mark = timing_started;
    double timing_index = 0.0;
    double timing_slot = 0.0;
    double timing_premises = 0.0;
    double timing_backing = 0.0;
    double timing_reindex = 0.0;
    double timing_storage = 0.0;

    if (store == NULL || index >= store->count)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->failed)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->entries[index].state == FUNCTION_PARAM_FLOW_COMPUTING) {
        store->active_had_recursion = true;
        store->recursion_hits++;
        return store->entries[index].mask;
    }
    if (store->entries[index].state == FUNCTION_PARAM_FLOW_EVALUATED
        || store->entries[index].state == FUNCTION_PARAM_FLOW_COMPLETE) {
        if (store->entries[index].state == FUNCTION_PARAM_FLOW_COMPLETE &&
            !function_param_flow_output_current(store, index, false)) return SLOT_PARAM_SUMMARY_ALL;
        return store->entries[index].mask;
    }

    if (store->work_units >= store->work_budget) {
        function_param_flow_fail(store, store->entries[index].function_decl,
            "recursive summary work budget exceeded");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }
    store->work_units++;

    function_decl = store->entries[index].function_decl;
    param = ast_func_param(function_decl, store->entries[index].param_index);
    if (param == NULL || param->name == NULL
        || ast_func_body(function_decl) == NULL) {
        function_param_flow_fail(store, function_decl,
            "function parameter or body identity is missing");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }

    store->entries[index].state = FUNCTION_PARAM_FLOW_COMPUTING;
    origin.function_decl = function_decl;
    origin.param_index = store->entries[index].param_index;
    origin.param_name = param->name;
    lookup.ctx = store->ctx;
    lookup.program_root = store->program_root;
    if (!function_param_flow_program_point_index_build(
            store, function_decl, &program_points)
        || program_points == NULL
        || store->entries[index].param_index >= program_points->param_count) {
        function_param_flow_fail(store, function_decl,
            "parameter program-point index is unavailable or stale");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }
    if (timing) {
        timing_index = (double)(clock() - timing_mark) /
            (double)CLOCKS_PER_SEC;
        timing_mark = clock();
    }
    roots = program_points->roots_by_param[store->entries[index].param_index];
    root_count = program_points->root_counts[store->entries[index].param_index];
    /* The program-point index is formal-specific. Zero roots means this exact
     * formal name does not occur anywhere in the current body, regardless of
     * stores or calls reached by sibling formals. Publish that absence through
     * the existing selector producer instead of asking routine-global backing
     * machinery to classify an input the routine never reads. */
    if (root_count == 0 &&
        function_param_flow_unused_formal_current(program_points,
            store->entries[index].param_index)) {
        size_t output_count = ast_func_param_count(function_decl);
        FunctionParamFlowSummaryEntry *entry = &store->entries[index];
        if (entry->output_dependencies == NULL) {
            entry->output_dependencies = calloc(
                output_count == 0 ? 1 : output_count,
                sizeof(*entry->output_dependencies));
            if (entry->output_dependencies == NULL) {
                function_param_flow_fail(store, function_decl,
                    "unused formal output relation allocation failed");
                entry->mask = SLOT_PARAM_SUMMARY_ALL;
                entry->state = FUNCTION_PARAM_FLOW_EVALUATED;
                return SLOT_PARAM_SUMMARY_ALL;
            }
            entry->output_count = output_count;
            store->changed = true;
        } else if (entry->output_count != output_count) {
            function_param_flow_fail(store, function_decl,
                "unused formal output relation identity changed");
            entry->mask = SLOT_PARAM_SUMMARY_ALL;
            entry->state = FUNCTION_PARAM_FLOW_EVALUATED;
            return SLOT_PARAM_SUMMARY_ALL;
        }
        for (size_t p = 0; p < output_count; p++) {
            if (entry->output_dependencies[p]) {
                function_param_flow_fail(store, function_decl,
                    "unused formal acquired an output dependency");
                entry->mask = SLOT_PARAM_SUMMARY_ALL;
                entry->state = FUNCTION_PARAM_FLOW_EVALUATED;
                return SLOT_PARAM_SUMMARY_ALL;
            }
        }
        if (!entry->output_dependencies_ready ||
            entry->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR ||
            entry->returned_backing_unproved ||
            entry->returned_backing_pending || entry->mask != SLOT_PARAM_SUMMARY_NONE)
            store->changed = true;
        entry->output_dependencies_ready = true;
        entry->output_producer = FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR;
        entry->returned_backing_unproved = false;
        entry->returned_backing_pending = false;
        entry->analysis_unsupported = false;
        entry->result_path_ready = false;
        entry->mask = SLOT_PARAM_SUMMARY_NONE;
        entry->state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_NONE;
    }
    /* Demands below may grow the index table and move `program_points`. */
    bool private_stores = program_points->private_local_stores;
    bool graph_required = private_stores || program_points->no_store_call_count != 0;
    bool graph_closed = !program_points->mutable_output_required &&
        !program_points->contextual_output_required &&
        (!graph_required || function_param_flow_no_store_graph(store, function_decl));
    bool actual_store = !graph_closed;
    if (private_stores && graph_closed) {
        origin.private_stores_owner = function_decl;
        origin.private_stores_owner_id = ast_node_stable_id(function_decl);
    }
    candidate = slot_param_summary_in_program_points(
        roots, root_count, param->name, &lookup, &origin);
    if (timing) {
        timing_slot = (double)(clock() - timing_mark) /
            (double)CLOCKS_PER_SEC;
        timing_mark = clock();
    }
    storage_unproved = 0;
    bool scalar_return = (candidate & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 &&
        semantic_param_publication_routine_return_dependency_is_scalar(store->ctx, function_decl);
    bool anchored = false;
    bool source_kind_ready = semantic_param_publication_parameter_is_anchored_handle(
        store->ctx, function_decl, origin.param_index, &anchored);
    bool return_backing_required;
    if (!function_param_flow_backing_required(store, function_decl, origin.param_index, &return_backing_required)) {
        function_param_flow_fail(store, function_decl, "typed return backing classification is missing or stale");
        return SLOT_PARAM_SUMMARY_ALL;
    }
    bool mutable_output = false;
    size_t output_count = ast_func_param_count(function_decl);
    for (size_t p = 0; p < output_count; p++) {
        FuncParam *output = ast_func_param(function_decl, p);
        if (output == NULL) mutable_output = true;
        else if (output->mode == PARAM_MODE_MUT_REF) mutable_output = true;
    }
    mutable_output |= actual_store;
    if ((scalar_return || mutable_output || (private_stores && graph_closed)) && !source_kind_ready) {
        function_param_flow_fail(store, function_decl, "typed parameter source kind is missing or stale");
        return SLOT_PARAM_SUMMARY_ALL;
    }
    bool prior_scalar_return = store->entries[index].output_producer ==
        FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN;
    bool scalar_join = graph_closed && !mutable_output && !private_stores &&
        source_kind_ready && !anchored &&
        semantic_param_publication_routine_no_output_header_ready(store->ctx, function_decl) &&
        semantic_param_publication_routine_return_dependency_is_scalar(store->ctx, function_decl);
    if (prior_scalar_return && !scalar_join) {
        function_param_flow_fail(store, function_decl, "closed scalar return premise is missing or stale");
        return SLOT_PARAM_SUMMARY_ALL;
    }
    bool closed_scalar_return = (scalar_return || prior_scalar_return) && scalar_join;
    bool backing_projection_ready = false;
    unsigned backing_projection = SLOT_PARAM_SUMMARY_NONE;
    if (timing) {
        timing_premises = (double)(clock() - timing_mark) /
            (double)CLOCKS_PER_SEC;
        timing_mark = clock();
    }
    if (return_backing_required || (!anchored && ((scalar_return && !closed_scalar_return) || mutable_output ||
        (private_stores && graph_closed)))) {
        unsigned backing_effects = SLOT_PARAM_SUMMARY_NONE;
        bool pending = false;
        FunctionParamFlowResultPath backing_result_path = {0};
        bool *outputs = calloc(output_count, sizeof(*outputs));
        if (outputs == NULL) { function_param_flow_fail(store, function_decl, "output relation allocation failed"); return SLOT_PARAM_SUMMARY_ALL; }
        bool complete = function_param_flow_returned_backing_effects(store, function_decl,
            origin.param_index, &backing_effects, outputs, output_count, &pending,
            &backing_result_path, private_stores && graph_closed);
        bool unsupported = !complete && !pending && function_param_flow_backing_unsupported(store, function_decl, origin.param_index);
        store->entries[index].analysis_unsupported = unsupported;
        if (unsupported) { candidate |= SLOT_PARAM_SUMMARY_ALL; storage_unproved = FUNCTION_PARAM_ARRAY_ALL_UNPROVED; }
        buffer_unkept |= unsupported;
        /* A complete frame is the exact owner of outward storage effects.
         * Earlier passes may have published a conservative CALL while one of
         * its callees was unresolved; carrying that bit forever would leave
         * the public entry different from its owner. Access, mutation and
         * release remain monotone below. */
        const unsigned escape_projection =
            SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE |
            SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
        if (complete && !unsupported) {
            backing_projection_ready = true;
            backing_projection = backing_effects & escape_projection;
            candidate = (candidate & ~escape_projection) |
                backing_projection;
        } else {
            candidate |= backing_effects & escape_projection;
        }
        /* A pending frame is proved or failed when its recursive closure
         * converges; a conservative CALL here would outlive that proof. */
        if (!complete && !pending && private_stores && graph_closed)
            candidate |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
        if (!complete && mutable_output && !pending)
            candidate |= SLOT_PARAM_SUMMARY_CALL_ESCAPE;
        if (store->entries[index].output_dependencies_ready != complete ||
            store->entries[index].returned_backing_pending != pending) store->changed = true;
        store->entries[index].returned_backing_pending = pending;
        store->entries[index].returned_backing_unproved = !complete;
        if (store->entries[index].output_dependencies == NULL) {
            store->entries[index].output_dependencies = outputs;
            store->entries[index].output_count = output_count;
            outputs = NULL;
            if (complete) store->changed = true;
        } else if (outputs != NULL) {
            for (size_t p = 0; p < output_count; p++) if (outputs[p] &&
                !store->entries[index].output_dependencies[p]) {
                store->entries[index].output_dependencies[p] = true;
                store->changed = true;
            }
        }
        store->entries[index].output_dependencies_ready = complete;
        store->entries[index].output_producer = FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME;
        if (store->entries[index].result_path_ready != complete ||
            (complete && !function_param_flow_result_path_equal(
                &store->entries[index].result_path, &backing_result_path)))
            store->changed = true;
        store->entries[index].result_path_ready = complete;
        if (complete) store->entries[index].result_path = backing_result_path;
        free(outputs);
    } else if (source_kind_ready && !anchored &&
        semantic_param_publication_routine_no_output_header_ready(
            store->ctx, function_decl)) {
        /* Closed output storage and a scalar return are independent premises.
         * Only their conjunction can remove RETURN; other effects are kept. */
        if (closed_scalar_return) candidate &= ~SLOT_PARAM_SUMMARY_RETURN_ESCAPE;
        if (store->entries[index].output_dependencies == NULL) {
            store->entries[index].output_dependencies = calloc(output_count, sizeof(bool));
            if (store->entries[index].output_dependencies == NULL) { function_param_flow_fail(store, function_decl, "empty output relation allocation failed"); return SLOT_PARAM_SUMMARY_ALL; }
            store->entries[index].output_count = output_count;
        }
        if (closed_scalar_return && (!store->entries[index].output_dependencies_ready ||
            store->entries[index].returned_backing_pending)) store->changed = true;
        store->entries[index].output_dependencies_ready = store->entries[index].output_dependencies != NULL;
        store->entries[index].output_producer = closed_scalar_return
            ? FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SCALAR_RETURN : FUNCTION_PARAM_FLOW_OUTPUT_CLOSED_SELECTOR;
        if (closed_scalar_return) {
            store->entries[index].returned_backing_unproved = false;
            store->entries[index].returned_backing_pending = false;
        }
    } else {
        if (store->entries[index].output_dependencies_ready) store->changed = true;
        store->entries[index].output_dependencies_ready = false;
        store->entries[index].output_producer = FUNCTION_PARAM_FLOW_OUTPUT_UNPROVED;
        store->entries[index].result_path_ready = false;
    }
    if (timing) {
        timing_backing = (double)(clock() - timing_mark) /
            (double)CLOCKS_PER_SEC;
        timing_mark = clock();
    }

    /* The storage walk reads the summary the backing frame settled: an alias
     * returned inside a result is the formal's own return once that frame
     * proves it, a getter's loan rather than unproved storage. The frame's
     * demands may have rebuilt the index, so its roots are read again. */
    if (!function_param_flow_program_point_index_build(store, function_decl, &program_points)
        || program_points == NULL || store->entries[index].param_index >= program_points->param_count) {
        function_param_flow_fail(store, function_decl, "parameter program-point index is unavailable or stale");
        store->entries[index].mask = SLOT_PARAM_SUMMARY_ALL;
        store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
        return SLOT_PARAM_SUMMARY_ALL;
    }
    if (timing) {
        timing_reindex = (double)(clock() - timing_mark) /
            (double)CLOCKS_PER_SEC;
        timing_mark = clock();
    }
    storage_unproved |= function_param_array_storage_unproved_in_program_points(
        program_points->dependency_roots, program_points->dependency_root_count,
        program_points->roots_by_param[store->entries[index].param_index],
        program_points->root_counts[store->entries[index].param_index], &origin, &lookup,
        candidate | store->entries[index].mask, &payload_read, &buffer_unkept);
    if (timing) {
        timing_storage = (double)(clock() - timing_mark) /
            (double)CLOCKS_PER_SEC;
        double timing_total = (double)(clock() - timing_started) /
            (double)CLOCKS_PER_SEC;
        if (timing_total >= 0.02)
            fprintf(stderr,
                "[semantic timing] flow_body routine=%s formal=%zu"
                " total=%.3f index=%.3f slot=%.3f premises=%.3f"
                " backing=%.3f reindex=%.3f storage=%.3f\n",
                ast_declaration_name(function_decl) != NULL
                    ? ast_declaration_name(function_decl) : "<unknown>",
                store->entries[index].param_index,
                timing_total, timing_index, timing_slot, timing_premises,
                timing_backing, timing_reindex, timing_storage);
    }
    /* Recursive demands may grow the entries array, so reacquire by index. */
    previous = store->entries[index].mask;
    if (backing_projection_ready) {
        const unsigned escape_projection =
            SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
            SLOT_PARAM_SUMMARY_CALL_ESCAPE |
            SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE;
        candidate = (candidate | (previous & ~escape_projection)) |
            backing_projection;
    } else {
        candidate |= previous;
    }
    /* INOUT descriptor calls conservatively carry CALL even without an element
     * read. Direct descriptor uses are proved by the two-bit transfer itself. */
    if (getenv("PGY_DEBUG_ARRAY_PRESERVATION") != NULL
        && ((storage_unproved & FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED) != 0
            || (payload_read && (candidate &
                (SLOT_PARAM_SUMMARY_RETURN_ESCAPE
                 | SLOT_PARAM_SUMMARY_CALL_ESCAPE
                 | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE
                 | SLOT_PARAM_SUMMARY_RELEASE)) != 0))) {
        const char *name = ast_declaration_name(function_decl);
        fprintf(stderr,
            "pgy: array-element-summary-unproved owner=%s formal=%zu"
            " walk_elements=%d payload_read=%d mask=%u\n",
            name != NULL ? name : "<unknown>",
            store->entries[index].param_index,
            (storage_unproved & FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED) != 0,
            payload_read, candidate);
    }
    if (payload_read && (candidate & (SLOT_PARAM_SUMMARY_RETURN_ESCAPE | SLOT_PARAM_SUMMARY_CALL_ESCAPE
        | SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE | SLOT_PARAM_SUMMARY_RELEASE)))
        storage_unproved |= FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED;
    if (candidate != previous) {
        store->entries[index].mask = candidate;
        store->changed = true;
    }
    if ((storage_unproved & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) && !store->entries[index].array_storage_unproved) {
        store->entries[index].array_storage_unproved = true;
        store->changed = true;
    }
    if (buffer_unkept && !store->entries[index].array_buffer_unkept) {
        store->entries[index].array_buffer_unkept = true;
        store->changed = true;
    }
    if ((storage_unproved & FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED) && !store->entries[index].array_elements_unproved) {
        store->entries[index].array_elements_unproved = true;
        store->changed = true;
    }
    store->entries[index].state = FUNCTION_PARAM_FLOW_EVALUATED;
    store->body_evaluations++;
    return store->entries[index].mask;
}

static bool
function_param_flow_solve_active(FunctionParamFlowSummaryStore *store,
                                 ASTNode *site)
{
    size_t passes = 0;
    store->solving = true;
    store->active_had_recursion = false;
    /* Bound this demand closure, not the lifetime of the compilation. Completed
     * unrelated summaries remain cached; recursive revisits in this episode
     * still share the same cap. Body-evaluation telemetry stays cumulative. */
    store->work_units = 0;
    store->closure_frame_node_visits = 0;
    store->closure_read_count = 0;
    store->evaluating_plus_one = 0;

    /* After the first pass, a pass evaluates only the entries whose routine
     * read an entry that changed after the read. */
    bool marked;
    do {
        store->changed = false;
        for (size_t i = store->active_start; i < store->count; i++) {
            if (passes != 0 && !store->entries[i].dirty) { store->skipped_evaluations++; continue; }
            store->entries[i].state = FUNCTION_PARAM_FLOW_UNSEEN;
            store->entries[i].dirty = false;
        }
        for (size_t i = store->active_start; i < store->count; i++)
            (void)function_param_flow_evaluate(store, i);
        passes++;
        store->fixed_point_passes++;
        marked = false;
        if (store->failed)
            break;
        if (passes > (store->count - store->active_start + 1) * 7 + 1) {
            function_param_flow_fail(store, site,
                "recursive summary fixed point did not converge");
            for (size_t i = store->active_start; i < store->count; i++)
                store->entries[i].mask = SLOT_PARAM_SUMMARY_ALL;
            break;
        }
        for (size_t i = store->active_start; i < store->count; i++) marked |= store->entries[i].dirty;
        /* With no entry marked, every entry's last evaluation read what the
         * entries it read hold now: the closure is at its fixed point. */
        if (!marked) store->changed = false;
        /* Pending recursive backing is an expected provisional state, not a
         * worklist fixed point. Settle it first; if that publishes a new mask
         * or output it marks the closure dirty and the ordinary loop owns the
         * next pass. Only a closure with no newly published fact is eligible
         * for the full-pass worklist verifier. */
        if (!marked && store->active_had_recursion &&
            !function_param_flow_complete_pending_relations(store))
            function_param_flow_fail(store, site,
                "recursive backing relations are incomplete or stale");
        if (!marked && !store->failed)
            for (size_t i = store->active_start; i < store->count; i++)
                marked |= store->entries[i].dirty;
        if (!marked && store->active_had_recursion &&
            !function_param_flow_worklist_verified(store, site))
            function_param_flow_fail(store, site,
                "parameter flow worklist fixed point is incomplete or stale");
    } while (store->active_had_recursion && marked);

    function_param_flow_complete_return_relations(store);
    if (store->closure_frame_node_visits > store->max_closure_frame_node_visits)
        store->max_closure_frame_node_visits = store->closure_frame_node_visits;
    if (store->failed) {
        for (size_t i = store->active_start; i < store->count; i++)
            store->entries[i].mask = SLOT_PARAM_SUMMARY_ALL;
    }
    for (size_t i = store->active_start; i < store->count; i++)
        store->entries[i].state = FUNCTION_PARAM_FLOW_COMPLETE;
    store->solving = false;
    return !store->failed;
}

unsigned
function_param_flow_summary_demand(const SlotFunctionLookup *lookup,
                                   ASTNode *function_decl,
                                   size_t param_index)
{
    FunctionParamFlowSummaryStore *store;
    uint32_t function_id;
    size_t existing;
    size_t root_index;

    if (lookup == NULL || lookup->ctx == NULL || function_decl == NULL
        || function_decl->type != AST_FUNC_DECL
        || param_index >= ast_func_param_count(function_decl)) {
        return SLOT_PARAM_SUMMARY_ALL;
    }
    if (!semantic_param_publication_inputs_ready(lookup->ctx) ||
        !semantic_param_publication_routine_inputs_current(lookup->ctx, function_decl, true)) {
        semantic_error(lookup->ctx, function_decl, "Parameter flow demand requires sealed typed inputs");
        return SLOT_PARAM_SUMMARY_ALL;
    }
    store = function_param_flow_store_get(lookup);
    if (store == NULL)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->failed)
        return SLOT_PARAM_SUMMARY_ALL;
    if (store->program_root != lookup->program_root) {
        function_param_flow_fail(store, function_decl,
            "summary program owner changed during semantic analysis");
        return SLOT_PARAM_SUMMARY_ALL;
    }

    function_id = ast_node_stable_id(function_decl);
    existing = function_param_flow_find(store, function_id, param_index);
    if (store->validating_current)
        return existing != SIZE_MAX && store->entries[existing].function_decl == function_decl &&
            store->entries[existing].function_id == function_id && store->entries[existing].param_index == param_index &&
            store->entries[existing].state == FUNCTION_PARAM_FLOW_COMPLETE &&
            !store->entries[existing].analysis_unsupported && !store->entries[existing].returned_backing_unproved &&
            !store->entries[existing].returned_backing_pending &&
            function_param_flow_output_current(store, existing, false)
                ? store->entries[existing].mask : SLOT_PARAM_SUMMARY_ALL;
    if (existing != SIZE_MAX) {
        if (store->solving)
            return function_param_flow_demand_index(
                store, function_decl, param_index);
        if (store->entries[existing].state == FUNCTION_PARAM_FLOW_COMPLETE) {
            if (!function_param_flow_output_current(store, existing, false)) return SLOT_PARAM_SUMMARY_ALL;
            store->cache_hits++;
            return store->entries[existing].mask;
        }
    }

    if (store->solving)
        return function_param_flow_demand_index(store, function_decl,
                                                param_index);

    store->active_start = store->count;
    root_index = function_param_flow_add(store, function_decl, param_index);
    if (root_index == SIZE_MAX) {
        function_param_flow_fail(store, function_decl,
            function_id == 0
                ? "function stable identity is missing"
                : "summary hash allocation failed");
        return SLOT_PARAM_SUMMARY_ALL;
    }

    if (!store->publication_demand_active &&
        !function_param_flow_advance_demand_epoch(store, function_decl))
        return SLOT_PARAM_SUMMARY_ALL;
    if (!function_param_flow_solve_active(store, function_decl))
        return SLOT_PARAM_SUMMARY_ALL;
    return store->entries[root_index].mask;
}

unsigned
function_param_flow_summary_for_param(SemanticContext *ctx,
                                      ASTNode *function_decl,
                                      size_t param_index)
{
    SlotFunctionLookup lookup = {ctx, ctx != NULL ? ctx->program_root : NULL};

    return function_param_flow_summary_demand(&lookup, function_decl,
                                              param_index);
}

unsigned function_param_flow_summary_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t ordinal)
{
    if (lookup == NULL) return SLOT_PARAM_SUMMARY_ALL;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    size_t formal = 0;
    return decl == NULL || !semantic_param_publication_formal_for_actual(
        lookup->ctx, call, ordinal, &formal) ? SLOT_PARAM_SUMMARY_ALL :
        function_param_flow_summary_demand(lookup, decl, formal);
}

bool function_param_flow_output_dependency_for_call(const SlotFunctionLookup *lookup,
    const ASTNode *call, size_t source_ordinal, size_t output_ordinal, bool *dependent)
{
    if (lookup == NULL || dependent == NULL ||
        !semantic_param_publication_checked_argument(lookup->ctx, call, output_ordinal) ||
        !semantic_param_publication_value(lookup->ctx, ast_call_argument(call, source_ordinal)).ok ||
        !semantic_param_publication_value(lookup->ctx, ast_call_argument(call, output_ordinal)).ok) return false;
    ASTNode *decl = semantic_param_publication_callee(lookup->ctx, call, source_ordinal);
    size_t source_formal = 0, output_formal = 0;
    if (decl == NULL || !semantic_param_publication_formal_for_actual(lookup->ctx,
            call, source_ordinal, &source_formal) ||
        !semantic_param_publication_formal_for_actual(lookup->ctx,
            call, output_ordinal, &output_formal)) return false;
    size_t ordinals[2] = {source_ordinal, output_ordinal};
    for (size_t i = 0; i < 2; i++) {
        ASTNode *root = ast_call_argument(call, ordinals[i]);
        while (root != NULL && (root->type == AST_MEMBER_ACCESS || root->type == AST_ARRAY_ACCESS))
            root = root->type == AST_MEMBER_ACCESS ? ast_member_object(root) : ast_array_access_array(root);
        if (root == NULL || !semantic_param_publication_value(lookup->ctx, root).ok) return false;
    }
    (void)function_param_flow_summary_demand(lookup, decl, source_formal);
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    if (store == NULL || store->failed) return false;
    size_t index = function_param_flow_find(store, ast_node_stable_id(decl), source_formal);
    if (index == SIZE_MAX || store->entries[index].state == FUNCTION_PARAM_FLOW_COMPUTING ||
        !store->entries[index].output_dependencies_ready ||
        output_formal >= store->entries[index].output_count) return false;
    /* The recursive settlement owner validated every pending edge before it
     * starts rebuilding any frame.  A self-edge may read this provisional
     * output while its frame is building; outside that bounded phase the
     * ordinary currentness replay remains mandatory. */
    bool recursive_snapshot = store->settling_recursive_backing &&
        index >= store->active_start &&
        store->entries[index].state == FUNCTION_PARAM_FLOW_EVALUATED &&
        !store->entries[index].analysis_unsupported &&
        !store->entries[index].returned_backing_unproved &&
        !store->entries[index].returned_backing_pending &&
        store->entries[index].output_dependencies_ready;
    if (!recursive_snapshot &&
        !function_param_flow_output_current(store, index, false)) return false;
    *dependent = store->entries[index].output_dependencies[output_formal];
    return true;
}
