#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "../parser/ast_analysis.h"
#include "../common/string_compat.h"
#include "typed_map_query_fact_owner.h"
#include "../common/match_variant_policy.h"


/* One shallow-edge vocabulary serves the initial syntax snapshot and its
 * current replay. Execution policy is carried separately by promotion. */
/* Only growth after a binding input was read needs another traversal. Forward
 * readers see the merged bits directly; earlier readers and expression caches
 * are replayed on the next epoch, including dependencies on an empty input. */
bool backing_merge(size_t words, uint64_t *into, const uint64_t *from)
{
    bool grew = false;
    for (size_t w = 0; w < words; w++) {
        uint64_t next = into[w] | from[w];
        grew |= next != into[w]; into[w] = next;
    }
    return grew;
}
void backing_effect(ReturnedBacking *state, const uint64_t *origins, unsigned effect)
{
    if (state->loan_seed != NULL && origins[0] != 0 && effect != 0 && state->escape_site == NULL)
        state->escape_site = state->consumer;
    for (size_t p = 0; p < state->fact->origin_count; p++)
        if ((origins[p / 64] & (UINT64_C(1) << (p % 64))) != 0) state->fact->effects[p] |= effect;
}
bool backing_any_origin(const ReturnedBacking *state, const uint64_t *origins)
{
    for (size_t w = 0; w < state->fact->origin_words; w++) if (origins[w] != 0) return true;
    return false;
}
FunctionParamFlowBackingBinding *backing_binding(ReturnedBacking *state, uint32_t id, bool insert)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (id == 0) { fact->complete = false; return NULL; }
    for (size_t i = 0; i < fact->binding_count; i++) if (fact->bindings[i].binding == id) return &fact->bindings[i];
    if (!insert) return NULL;
    if (fact->binding_count == SIZE_MAX / sizeof(*fact->bindings)) { function_param_flow_fail(state->store, state->decl, "backing binding size overflow"); fact->complete = false; return NULL; }
    FunctionParamFlowBackingBinding *rows = realloc(fact->bindings, (fact->binding_count + 1) * sizeof(*rows));
    if (rows == NULL) { function_param_flow_fail(state->store, state->decl, "backing binding allocation failed"); fact->complete = false; return NULL; }
    fact->bindings = rows;
    FunctionParamFlowBackingBinding *row = &rows[fact->binding_count];
    *row = (FunctionParamFlowBackingBinding){.binding = id};
    row->origins = calloc(fact->origin_words, sizeof(uint64_t));
    row->destinations = calloc(fact->destination_words, sizeof(uint64_t));
    fact->binding_count++;
    if (row->origins == NULL || row->destinations == NULL) { function_param_flow_fail(state->store, state->decl, "backing binding bitset allocation failed"); fact->complete = false; return NULL; }
    return row;
}
size_t backing_node(ReturnedBacking *state, ASTNode *node)
{
    FunctionParamFlowBackingFact *fact = state->fact;
    if (node == NULL || ast_node_stable_id(node) == 0) { fact->complete = false; return SIZE_MAX; }
    if (!function_param_flow_claim_record(state->store->ctx, fact->claims, node)) { fact->complete = false; return SIZE_MAX; }
    for (size_t i = 0; i < fact->node_count; i++) if (fact->nodes[i].node == node) return i;
    if (fact->node_count == SIZE_MAX / sizeof(*fact->nodes)) { function_param_flow_fail(state->store, state->decl, "backing node size overflow"); fact->complete = false; return SIZE_MAX; }
    FunctionParamFlowBackingNode *rows = realloc(fact->nodes, (fact->node_count + 1) * sizeof(*rows));
    if (rows == NULL) { function_param_flow_fail(state->store, state->decl, "backing node allocation failed"); fact->complete = false; return SIZE_MAX; }
    fact->nodes = rows;
    size_t at = fact->node_count++;
    rows[at] = (FunctionParamFlowBackingNode){.node = node, .id = ast_node_stable_id(node), .shape = node->type};
    rows[at].origins = calloc(fact->origin_words, sizeof(uint64_t));
    if (rows[at].origins == NULL) { function_param_flow_fail(state->store, state->decl, "backing origin allocation failed"); fact->complete = false; return SIZE_MAX; }
    return at;
}
bool backing_work(ReturnedBacking *state, size_t at)
{
    FunctionParamFlowBackingNode *row = &state->fact->nodes[at];
    if (row->work_epoch == state->fact->epoch) return true;
    size_t *work = state->loan_seed != NULL ? &state->loan_work : &state->store->work_units;
    if (*work >= state->store->work_budget) { function_param_flow_fail(state->store, state->decl, "recursive summary work budget exceeded"); state->fact->complete = false; return false; }
    (*work)++; row->work_epoch = state->fact->epoch; return true;
}
static bool unsupported_relation_current(FunctionParamFlowSummaryStore *store, const FunctionParamFlowPendingRelation *edge)
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
    ASTNode *callee = semantic_param_publication_callee(store->ctx, edge->call, edge->ordinal);
    if (callee == NULL || edge->callee_id != ast_node_stable_id(callee)) return false;
    size_t formal = 0;
    if (!semantic_param_publication_formal_for_actual(store->ctx, edge->call, edge->ordinal, &formal)) return false;
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
bool function_param_flow_declared_result_closed(FunctionParamFlowSummaryStore *store,
    ASTNode *call, FunctionParamFlowBackingFact *consumer)
{
    const Type *result_type = NULL;
    ASTNode *decl = store == NULL ? NULL : semantic_param_publication_declared_result(store->ctx, call, &result_type);
    /* An implicit self has no corresponding explicit actual in this result edge. */
    if (store == NULL || store->failed || consumer == NULL || decl == NULL || result_type == NULL ||
        ast_call_arg_count(call) != ast_func_param_count(decl)) return false;
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index) ||
        !function_param_flow_index_roots_current(index)) return false;
    size_t arity = ast_func_param_count(decl);
    if (index->param_count != arity) return false;
    if (arity != 0 && index->backing_frames == NULL) {
        if (arity > SIZE_MAX / sizeof(*index->backing_frames)) return false;
        index->backing_frames = calloc(arity, sizeof(*index->backing_frames));
        if (index->backing_frames == NULL) { function_param_flow_fail(store, call, "backing frame vector allocation failed"); return false; }
        index->backing_frame_count = arity;
    }
    if (index->backing_frame_count != arity) return false;
    if (index->declared_result == NULL) {
        FunctionParamFlowBackingFact *created = calloc(1, sizeof(*created));
        if (created == NULL) { function_param_flow_fail(store, call, "declared result frame allocation failed"); return false; }
        index->declared_result = created;
        created->origin_count = created->origin_words = 1;
        created->destination_count = created->destination_words = 1;
        created->input_ordinal = SIZE_MAX;
        created->declared_result_type = result_type;
        created->claims = index->claims;
        created->effects = calloc(1, sizeof(*created->effects));
        created->outputs = calloc(1, sizeof(*created->outputs));
        if (created->effects == NULL || created->outputs == NULL) {
            function_param_flow_fail(store, call, "declared result matrix allocation failed"); return false;
        }
        ReturnedBacking seed = {.store = store, .decl = decl, .fact = created};
        for (size_t p = 0; p < arity; p++) {
            FuncParam *param = ast_func_param(decl, p);
            if (param == NULL || backing_binding(&seed, ast_func_param_stable_id(param), true) == NULL) return false;
        }
    }
    /* Only heap-owned roots, claims and frames survive recursive index growth. */
    FunctionParamFlowBackingFact *fact = index->declared_result;
    FunctionParamFlowControlProjection *projection = index->control_projection;
    if (fact->declared_result_type != result_type || fact->input_id != 0 || fact->input_ordinal != SIZE_MAX ||
        fact->origin_count != 1 || fact->origin_words != 1 || fact->destination_count != 1 || fact->destination_words != 1)
        return false;
    if (fact->building) { consumer->pending = true; return false; }
    if (!function_param_flow_no_store_graph(store, decl)) return false;
    fact->private_stores_closed = true;
    if (fact->epoch != 0 && !function_param_flow_backing_rows_current(store, decl)) return false;
    if (fact->epoch == 0 || (fact->pending && fact->evaluation_pass != store->fixed_point_passes + 1)) {
        fact->building = true; fact->complete = true; fact->pending = false;
        fact->evaluation_pass = store->fixed_point_passes + 1;
        ReturnedBacking state = {.store = store, .lookup = {store->ctx, store->program_root},
            .decl = decl, .fact = fact, .private_stores_closed = true, .checking_declared_result = true};
        do {
            if (fact->epoch == SIZE_MAX) { function_param_flow_fail(store, decl, "declared result iteration identity overflow"); fact->complete = false; break; }
            fact->epoch++; fact->changed = false; state.result_return_seen = false;
            function_param_flow_backing_control_roots(&state, projection);
            if (!state.result_return_seen) fact->complete = false;
        } while (fact->changed && fact->complete);
        fact->building = false;
    }
    if (!fact->complete || fact->pending || fact->analysis_unsupported ||
        !semantic_param_publication_routine_result_current(store->ctx, decl, result_type) ||
        !function_param_flow_backing_rows_current(store, decl)) return false;
    ReturnedBacking destination = {.store = store, .decl = decl, .fact = consumer};
    size_t at = backing_node(&destination, call);
    if (at == SIZE_MAX) return false;
    consumer->nodes[at].result_callee_id = ast_node_stable_id(decl);
    consumer->nodes[at].result_type = result_type;
    return true;
}
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
        if (index->declared_result != NULL &&
            (!function_param_flow_no_store_graph(store, current) ||
             !function_param_flow_program_point_index_build(store, current, &index))) { valid = false; break; }
        FunctionParamFlowBackingFact **frames = index->backing_frames;
        size_t frame_count = index->backing_frame_count;
        size_t parameter_count = index->param_count;
        FunctionParamFlowIndexClaims *claims = index->claims;
        FunctionParamFlowBackingFact *result = index->declared_result;
        if (frames == NULL && result == NULL) {
            if (frame_count != 0) valid = false;
            continue;
        }
        if (frame_count != parameter_count || (parameter_count != 0 && frames == NULL) ||
            parameter_count > SIZE_MAX - 63 ||
            claims == NULL) { valid = false; break; }
        for (size_t input = 0; input < frame_count + (result != NULL) && valid; input++) {
            bool result_frame = input == frame_count;
            FunctionParamFlowBackingFact *fact = result_frame ? result : frames[input];
            if (fact == NULL) continue;
            if (!result_frame && !store->solving && !fact->analysis_unsupported &&
                (fact->epoch == 0 || !fact->complete || fact->building || fact->pending)) {
                bool required = false;
                if (!semantic_param_publication_return_backing_required(store->ctx, current, input, &required) ||
                    required) { valid = false; break; }
            }
            size_t destination_count = result_frame ? 1 : parameter_count;
            size_t destination_words = (destination_count + 63) / 64;
            uint64_t final_mask = destination_count % 64 == 0 ? UINT64_MAX :
                (UINT64_C(1) << (destination_count % 64)) - 1;
            uint32_t input_id = result_frame ? 0 : ast_func_param_stable_id(ast_func_param(current, input));
            if ((result_frame ? (fact->input_id != 0 || fact->input_ordinal != SIZE_MAX ||
                 fact->epoch == 0 || !fact->complete || fact->building || fact->pending ||
                 fact->analysis_unsupported || !fact->private_stores_closed ||
                 fact->pending_relation_count != 0 || !semantic_param_publication_routine_result_current(
                     store->ctx, current, fact->declared_result_type)) :
                (input_id == 0 || fact->input_ordinal != input || fact->input_id != input_id ||
                 fact->declared_result_type != NULL)) ||
                fact->origin_count != 1 || fact->origin_words != 1 ||
                fact->destination_count != destination_count || fact->destination_words != destination_words ||
                fact->claims != claims || fact->effects == NULL || fact->outputs == NULL ||
                (fact->node_count != 0 && fact->nodes == NULL) ||
                (fact->binding_count != 0 && fact->bindings == NULL) ||
                (fact->pending_relation_count != 0 && fact->pending_relations == NULL) ||
                (fact->sealed_pending_relation_count != 0 && fact->sealed_pending_relation_count != fact->pending_relation_count) ||
                (!fact->pending && !fact->building && fact->pending_relation_count != fact->sealed_pending_relation_count) ||
                (fact->outputs[destination_words - 1] & ~final_mask) != 0) { valid = false; break; }
            if (result_frame && (fact->effects[0] != 0 || fact->outputs[0] != 0)) { valid = false; break; }
            if (fact->analysis_unsupported) {
                bool owned = false;
                for (size_t n = 0; n < fact->node_count; n++)
                    if (fact->nodes[n].node == fact->unsupported_relation.call &&
                        fact->nodes[n].id == fact->unsupported_relation.call_id) owned = true;
                if (!owned || fact->complete || fact->pending || fact->building ||
                    !unsupported_relation_current(store, &fact->unsupported_relation)) { valid = false; break; }
            }
            for (size_t e = 0; e < fact->pending_relation_count && valid; e++) {
                FunctionParamFlowPendingRelation *edge = &fact->pending_relations[e];
                ASTNode *callee = semantic_param_publication_callee(store->ctx, edge->call, edge->ordinal);
                size_t formal = 0;
                bool owned = false;
                for (size_t n = 0; n < fact->node_count; n++)
                    if (fact->nodes[n].node == edge->call && fact->nodes[n].id == edge->call_id) owned = true;
                valid = owned && edge->call != NULL && edge->call->type == AST_CALL && edge->call_id != 0 &&
                    edge->call_id == ast_node_stable_id(edge->call) && edge->callee_id != 0 &&
                    callee != NULL && edge->callee_id == ast_node_stable_id(callee) &&
                    semantic_param_publication_formal_for_actual(store->ctx, edge->call, edge->ordinal, &formal);
                if (valid && !store->solving) {
                    size_t target = 0;
                    while (target < store->count && (store->entries[target].function_id != edge->callee_id ||
                        store->entries[target].param_index != formal)) target++;
                    if (target == store->count) { valid = false; break; }
                    FunctionParamFlowSummaryEntry *entry = &store->entries[target];
                    bool required;
                    valid = entry->function_decl == callee && entry->state == FUNCTION_PARAM_FLOW_COMPLETE &&
                        !entry->analysis_unsupported && !entry->returned_backing_pending &&
                        !entry->returned_backing_unproved &&
                        function_param_flow_backing_required(store, callee, formal, &required);
                    if (valid && required && entry->output_producer != FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) valid = false;
                    if (valid && entry->output_producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME) {
                        FunctionParamFlowProgramPointIndex *callee_index;
                        if (!entry->output_dependencies_ready || entry->output_dependencies == NULL ||
                            entry->output_count != ast_func_param_count(callee) ||
                            !function_param_flow_program_point_index_build(store, callee, &callee_index) ||
                            callee_index->backing_frames == NULL || formal >= callee_index->backing_frame_count) { valid = false; break; }
                        FunctionParamFlowBackingFact *callee_fact = callee_index->backing_frames[formal];
                        valid = callee_fact != NULL && !callee_fact->analysis_unsupported &&
                            callee_fact->epoch != 0 && callee_fact->complete &&
                            !callee_fact->pending && !callee_fact->building && callee_fact->input_ordinal == formal &&
                            callee_fact->input_id == ast_func_param_stable_id(ast_func_param(callee, formal));
                    }
                }
            }
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
                    (result_frame ? (binding->origins[0] != 0 || binding->destinations[0] != 0) :
                    ((p == input && binding->origins[0] != UINT64_C(1)) ||
                     (p != input && param->mode != PARAM_MODE_DEFAULT && binding->origins[0] != 0) ||
                     ((param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF) &&
                      (binding->destinations[p / 64] & (UINT64_C(1) << (p % 64))) == 0)))) valid = false;
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
                if (row->node->type == AST_IDENTIFIER) {
                    SemanticPublishedValue value = semantic_param_publication_value(store->ctx, row->node);
                    if (!value.ok || (!value.meta && semantic_param_publication_binding(store->ctx, row->node) == 0)) valid = false;
                }
                if ((row->result_callee_id == 0) != (row->result_type == NULL)) { valid = false; break; }
                if (row->result_callee_id != 0 && row->node->type != AST_CALL) { valid = false; break; }
                if (row->result_callee_id == 0 ?
                    (row->result_argument_count != 0 || row->result_argument_kinds != NULL) :
                    (row->result_argument_count != ast_call_arg_count(row->node) ||
                     (row->result_argument_count != 0 && row->result_argument_kinds == NULL))) { valid = false; break; }
                if (row->node->type != AST_CALL) continue;
                ASTNode *edge_call = row->node;
                uint32_t edge_callee_id = row->result_callee_id;
                const Type *edge_type = row->result_type;
                unsigned char *edge_modes = row->result_argument_kinds;
                size_t edge_arity = ast_call_arg_count(edge_call);
                bool result_edge = row->result_callee_id != 0;
                for (size_t p = 0; p < edge_arity + result_edge && valid; p++) {
                    ASTNode *callee;
                    if (result_edge && p == edge_arity) {
                        const Type *type = NULL;
                        callee = semantic_param_publication_declared_result(store->ctx, edge_call, &type);
                        FunctionParamFlowProgramPointIndex *callee_index;
                        if (callee == NULL || ast_node_stable_id(callee) != edge_callee_id ||
                            type != edge_type ||
                            !function_param_flow_program_point_index_build(store, callee, &callee_index) ||
                            callee_index->declared_result == NULL ||
                            callee_index->declared_result->declared_result_type != type) { valid = false; break; }
                    } else {
                        valid = semantic_param_publication_checked_argument(store->ctx, edge_call, p) &&
                            semantic_param_publication_value(store->ctx, ast_call_argument(edge_call, p)).ok;
                        callee = semantic_param_publication_callee(store->ctx, edge_call, p);
                        if (valid && result_edge) {
                            unsigned char kind = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_UNPROVED;
                            ASTNode *arg = ast_call_argument(edge_call, p);
                            if (semantic_param_publication_value_is_physical_scalar(store->ctx, arg))
                                kind = FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_PHYSICAL;
                            else if (callee != NULL && ast_node_stable_id(callee) == edge_callee_id) {
                                size_t formal = 0;
                                if (!semantic_param_publication_formal_for_actual(store->ctx,
                                        edge_call, p, &formal)) { valid = false; break; }
                                for (size_t s = 0; s < store->count; s++) {
                                    const FunctionParamFlowSummaryEntry *entry = &store->entries[s];
                                    if (entry->function_id != edge_callee_id || entry->param_index != formal) continue;
                                    if (entry->function_decl == callee && !entry->analysis_unsupported &&
                                        !entry->returned_backing_unproved && !entry->returned_backing_pending &&
                                        entry->output_producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME &&
                                        entry->output_dependencies_ready && entry->output_dependencies != NULL &&
                                        entry->output_count == ast_func_param_count(callee) &&
                                        (entry->state == FUNCTION_PARAM_FLOW_COMPLETE ||
                                         (store->solving && entry->state == FUNCTION_PARAM_FLOW_EVALUATED))) {
                                        FunctionParamFlowProgramPointIndex *callee_index;
                                        if (!function_param_flow_program_point_index_build(store, callee, &callee_index) ||
                                            callee_index->backing_frames == NULL || formal >= callee_index->backing_frame_count) break;
                                        FunctionParamFlowBackingFact *source = callee_index->backing_frames[formal];
                                        if (source == NULL || source->epoch == 0 || !source->complete ||
                                            source->building || source->pending || source->analysis_unsupported ||
                                            source->claims != callee_index->claims || source->input_ordinal != formal ||
                                            source->input_id != ast_func_param_stable_id(ast_func_param(callee, formal)) ||
                                            entry->output_count > SIZE_MAX - 63 ||
                                            source->destination_count != entry->output_count ||
                                            source->destination_words != (entry->output_count + 63) / 64 ||
                                            source->effects == NULL || source->outputs == NULL ||
                                            ((source->effects[0] & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 &&
                                             (entry->mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0)) break;
                                        bool output_current = true;
                                        for (size_t q = 0; q < entry->output_count; q++) if (
                                            (source->outputs[q / 64] & (UINT64_C(1) << (q % 64))) != 0 &&
                                            !entry->output_dependencies[q]) output_current = false;
                                        if (!output_current || (source->private_stores_closed &&
                                            !function_param_flow_no_store_graph(store, callee))) break;
                                        kind = (entry->mask & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) != 0 ?
                                            FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_DEPENDENT :
                                            FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_INDEPENDENT;
                                    }
                                    break;
                                }
                            }
                            valid = kind != FUNCTION_PARAM_FLOW_RESULT_ARGUMENT_UNPROVED &&
                                kind == edge_modes[p];
                        }
                    }
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
void function_param_flow_callee_unfinished(const SlotFunctionLookup *lookup,
    ASTNode *call, size_t ordinal, FunctionParamFlowBackingFact *fact)
{
    if (fact->analysis_unsupported) return;
    if (!fact->complete) return;
    FunctionParamFlowSummaryStore *store = lookup->ctx->function_param_flow_summaries;
    ASTNode *target = semantic_param_publication_callee(lookup->ctx, call, ordinal);
    FunctionParamFlowPendingRelation unsupported = {call, ast_node_stable_id(call), ast_node_stable_id(target), ordinal};
    if (target != NULL && unsupported_relation_current(store, &unsupported)) {
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
