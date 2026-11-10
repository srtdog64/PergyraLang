/* The declared-result frame of a routine: where its result's backing comes
 * from, built once per demand epoch and read by the calls whose result has
 * a declared type. */
#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_resolution_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

FunctionParamFlowBackingFact *function_param_flow_declared_result_frame_current(FunctionParamFlowSummaryStore *store,
    ASTNode *call, FunctionParamFlowBackingFact *consumer)
{
    const Type *result_type = NULL;
    ASTNode *decl = store == NULL ? NULL : semantic_param_publication_declared_result(store->ctx, call, &result_type);
    if (store == NULL || store->failed || decl == NULL || result_type == NULL ||
        (store->validating_current && consumer != NULL)) return NULL;
    ASTNode *receiver = NULL;
    if (!semantic_param_publication_instance_receiver(store->ctx, call, &receiver)
        || ast_call_arg_count(call) + (receiver != NULL) != ast_func_param_count(decl)) return NULL;
    if (receiver != NULL) {
        if (consumer != NULL) {
            ReturnedBacking relation = {.store = store, .decl = decl, .fact = consumer};
            if (!function_param_flow_receiver_independent(&relation, call)) return NULL;
        } else if (function_param_flow_receiver_readonly_current(store, call, true) == NULL) return NULL;
    }
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index))
        return NULL;
    size_t arity = ast_func_param_count(decl);
    if (index->param_count != arity) return NULL;
    if (store->validating_current) {
        FunctionParamFlowBackingFact *fact = index->declared_result;
        return fact != NULL && fact->epoch != 0 && fact->complete && !fact->building && !fact->pending &&
            !fact->analysis_unsupported && fact->private_stores_closed && fact->claims == index->claims &&
            fact->declared_result_type == result_type && fact->input_id == 0 && fact->input_ordinal == SIZE_MAX &&
            fact->origin_count == 1 && fact->origin_words == 1 && fact->destination_count == 1 &&
            fact->destination_words == 1 &&
            function_param_flow_backing_effect_facets_current(fact) &&
            semantic_param_publication_routine_result_current(store->ctx, decl, result_type) &&
            function_param_flow_declared_result_rows_current(store, decl)
                ? fact : NULL;
    }
    if (arity != 0 && index->backing_frames == NULL) {
        if (arity > SIZE_MAX / sizeof(*index->backing_frames)) return NULL;
        index->backing_frames = calloc(arity, sizeof(*index->backing_frames));
        if (index->backing_frames == NULL) { function_param_flow_fail(store, call, "backing frame vector allocation failed"); return NULL; }
        index->backing_frame_count = arity;
    }
    if (index->backing_frame_count != arity) return NULL;
    if (index->declared_result == NULL) {
        FunctionParamFlowBackingFact *created = calloc(1, sizeof(*created));
        if (created == NULL) { function_param_flow_fail(store, call, "declared result frame allocation failed"); return NULL; }
        index->declared_result = created;
        created->origin_count = created->origin_words = 1;
        created->destination_count = created->destination_words = 1;
        created->input_ordinal = SIZE_MAX;
        created->declared_result_type = result_type;
        created->claims = index->claims;
        created->effects = calloc(1, sizeof(*created->effects));
        created->outward_effects = calloc(1,
            sizeof(*created->outward_effects));
        created->outputs = calloc(1, sizeof(*created->outputs));
        if (created->effects == NULL || created->outward_effects == NULL ||
            created->outputs == NULL) {
            function_param_flow_fail(store, call, "declared result matrix allocation failed"); return NULL;
        }
        ReturnedBacking seed = {.store = store, .decl = decl, .fact = created};
        for (size_t p = 0; p < arity; p++) {
            FuncParam *param = ast_func_param(decl, p);
            if (param == NULL || backing_binding(&seed, ast_func_param_stable_id(param), true) == NULL) return NULL;
        }
    }
    /* Only heap-owned roots, claims and frames survive recursive index growth. */
    FunctionParamFlowBackingFact *fact = index->declared_result;
    FunctionParamFlowControlProjection *projection = index->control_projection;
    if (fact->declared_result_type != result_type || fact->input_id != 0 || fact->input_ordinal != SIZE_MAX ||
        fact->origin_count != 1 || fact->origin_words != 1 ||
        !function_param_flow_backing_effect_facets_current(fact) ||
        fact->destination_count != 1 || fact->destination_words != 1)
        return NULL;
    if (fact->building) { if (consumer != NULL) consumer->pending = true; return NULL; }
    if (!function_param_flow_no_store_graph(store, decl)) return NULL;
    fact->private_stores_closed = true;
    if (fact->epoch != 0 &&
        !function_param_flow_declared_result_rows_current(store, decl))
        return NULL;
    if (fact->epoch == 0 || (fact->pending && fact->evaluation_pass != store->fixed_point_passes + 1)) {
        fact->building = true; fact->complete = true; fact->pending = false;
        fact->evaluation_pass = store->fixed_point_passes + 1;
        ReturnedBacking state = {.store = store, .lookup = {store->ctx, store->program_root},
            .decl = decl, .fact = fact, .private_stores_closed = true, .checking_declared_result = true};
        size_t rounds = 0;
        do {
            if (fact->epoch == SIZE_MAX) { function_param_flow_fail(store, decl, "declared result iteration identity overflow"); fact->complete = false; break; }
            /* Same monotone growth bound as an input frame. */
            if (rounds++ > fact->binding_count * (fact->origin_count + fact->destination_count)) {
                function_param_flow_fail(store, decl, "backing frame rounds exceeded its growth bound"); fact->complete = false; break;
            }
            fact->epoch++; fact->changed = false; state.result_return_seen = false;
            function_param_flow_backing_control_roots(&state, projection);
            if (!state.result_return_seen) fact->complete = false;
        } while (fact->changed && fact->complete);
        fact->building = false;
    }
    if (!fact->complete || fact->pending || fact->analysis_unsupported ||
        !semantic_param_publication_routine_result_current(store->ctx, decl, result_type) ||
        !function_param_flow_declared_result_rows_current(store, decl))
        return NULL;
    if (consumer != NULL) {
        ReturnedBacking destination = {.store = store, .decl = decl, .fact = consumer};
        size_t at = backing_node(&destination, call);
        if (at == SIZE_MAX) return NULL;
        consumer->nodes[at].result_callee_id = ast_node_stable_id(decl);
        consumer->nodes[at].result_type = result_type;
    }
    return fact;
}

bool function_param_flow_declared_result_closed(FunctionParamFlowSummaryStore *store,
    ASTNode *call, FunctionParamFlowBackingFact *consumer)
{
    return consumer != NULL && function_param_flow_declared_result_frame_current(store, call, consumer) != NULL;
}
