/* Currency of a routine's backing rows: the routines its frames read and
 * the facts each row was built from, walked once per demand. */
#include "function_param_flow_summary_internal.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "function_param_publication_owner.h"
#include "../parser/ast_analysis.h"
#include "../common/string_compat.h"
#include "typed_map_query_fact_owner.h"
#include "../common/match_variant_policy.h"
#include "indexed_string_borrow_owner.h"

/* An argument's typed answers: checked with a current value, and the callee
 * it binds to. */
typedef struct {
    const ASTNode *call;
    size_t ordinal;
    ASTNode *callee;
    bool checked;
} RowsCaptureArgument;

/* Indexes one rows capture already found current, and argument answers it
 * already read. The capture only reads, so both stay current until it
 * returns; positions survive index growth. Arguments are an open-addressing
 * table keyed by call and ordinal. */
typedef struct {
    ASTNode **decls;
    size_t *positions;
    size_t count, capacity;
    RowsCaptureArgument *arguments;
    size_t argument_count, argument_capacity;
    bool verify;
} RowsCaptureIndexes;

/* With PGY_VERIFY_FLOW_CURRENCY_MEMO set, every lookup is checked in full
 * and a reuse the full check refuses fails the store. */
static bool rows_capture_index(FunctionParamFlowSummaryStore *store, RowsCaptureIndexes *seen,
    ASTNode *decl, FunctionParamFlowProgramPointIndex **index)
{
    size_t reused = SIZE_MAX;
    for (size_t i = 0; i < seen->count && reused == SIZE_MAX; i++)
        if (seen->decls[i] == decl) reused = seen->positions[i];
    if (reused != SIZE_MAX && reused < store->program_point_index_count &&
        store->program_point_indexes[reused].function_decl == decl) {
        *index = &store->program_point_indexes[reused];
        if (!seen->verify) return true;
        if (!function_param_flow_program_point_index_build(store, decl, index)) {
            function_param_flow_fail(store, decl, "rows capture index reuse diverged from its full check");
            return false;
        }
        return true;
    }
    if (!function_param_flow_program_point_index_build(store, decl, index)) return false;
    if (seen->count == seen->capacity) {
        size_t next = seen->capacity == 0 ? 8 : seen->capacity * 2;
        if (next < seen->capacity || next > SIZE_MAX / sizeof(*seen->positions)) return true;
        ASTNode **decls = realloc(seen->decls, next * sizeof(*decls));
        if (decls == NULL) return true;
        seen->decls = decls;
        size_t *positions = realloc(seen->positions, next * sizeof(*positions));
        if (positions == NULL) return true;
        seen->positions = positions;
        seen->capacity = next;
    }
    seen->decls[seen->count] = decl;
    seen->positions[seen->count++] = (size_t)(*index - store->program_point_indexes);
    return true;
}

static size_t rows_capture_argument_slot(const RowsCaptureIndexes *seen, const ASTNode *call, size_t ordinal)
{
    size_t mask = seen->argument_capacity - 1;
    size_t slot = (((size_t)(uintptr_t)call >> 4) * (size_t)2654435761u + ordinal * (size_t)40503u) & mask;
    while (seen->arguments[slot].call != NULL &&
        (seen->arguments[slot].call != call || seen->arguments[slot].ordinal != ordinal)) slot = (slot + 1) & mask;
    return slot;
}

/* Argument `ordinal` of `call`: whether it is checked with a current value,
 * and in `callee` the routine it binds to. Read once per capture; with
 * PGY_VERIFY_FLOW_CURRENCY_MEMO every reuse is read again and a different
 * answer fails the store. A table that cannot grow only stops caching. */
static bool rows_capture_argument(FunctionParamFlowSummaryStore *store, RowsCaptureIndexes *seen,
    ASTNode *call, size_t ordinal, ASTNode **callee)
{
    if (seen->argument_capacity != 0) {
        RowsCaptureArgument *known = &seen->arguments[rows_capture_argument_slot(seen, call, ordinal)];
        if (known->call == call && !seen->verify) { *callee = known->callee; return known->checked; }
    }
    bool checked = semantic_param_publication_checked_argument(store->ctx, call, ordinal) &&
        semantic_param_publication_value(store->ctx, ast_call_argument(call, ordinal)).ok;
    *callee = semantic_param_publication_callee(store->ctx, call, ordinal);
    if (seen->argument_capacity != 0) {
        RowsCaptureArgument *known = &seen->arguments[rows_capture_argument_slot(seen, call, ordinal)];
        if (known->call == call) {
            if (known->checked != checked || known->callee != *callee)
                function_param_flow_fail(store, call, "rows capture argument reuse diverged from its full read");
            return checked;
        }
    }
    if (seen->argument_count + 1 > seen->argument_capacity / 2) {
        size_t next = seen->argument_capacity == 0 ? 64 : seen->argument_capacity * 2;
        if (next < seen->argument_capacity || next > SIZE_MAX / sizeof(*seen->arguments)) return checked;
        RowsCaptureArgument *grown = calloc(next, sizeof(*grown));
        if (grown == NULL) return checked;
        RowsCaptureArgument *old = seen->arguments;
        size_t old_capacity = seen->argument_capacity;
        seen->arguments = grown;
        seen->argument_capacity = next;
        for (size_t i = 0; i < old_capacity; i++)
            if (old[i].call != NULL) seen->arguments[rows_capture_argument_slot(seen, old[i].call, old[i].ordinal)] = old[i];
        free(old);
    }
    seen->arguments[rows_capture_argument_slot(seen, call, ordinal)] =
        (RowsCaptureArgument){call, ordinal, *callee, checked};
    seen->argument_count++;
    return checked;
}

/* Appends `callee` to the validation queue unless it is already there. */
static bool rows_capture_enqueue(FunctionParamFlowSummaryStore *store, ASTNode *current,
    ASTNode ***queue, size_t *count, size_t *capacity, ASTNode *callee)
{
    for (size_t reached = 0; reached < *count; reached++) if ((*queue)[reached] == callee) return true;
    if (*count == *capacity) {
        if (*capacity > SIZE_MAX / 2 / sizeof(**queue)) { function_param_flow_fail(store, current, "backing validation queue size overflow"); return false; }
        ASTNode **grown = realloc(*queue, *capacity * 2 * sizeof(**queue));
        if (grown == NULL) { function_param_flow_fail(store, current, "backing validation queue allocation failed"); return false; }
        *queue = grown;
        *capacity *= 2;
    }
    (*queue)[(*count)++] = callee;
    return true;
}

bool function_param_flow_backing_rows_capture(FunctionParamFlowSummaryStore *store, ASTNode *decl,
    FunctionParamFlowInputCapture capture, void *user)
{
    if (store == NULL || decl == NULL) return false;
    size_t count = 1, capacity = 8;
    ASTNode **queue = malloc(capacity * sizeof(*queue));
    if (queue == NULL) { function_param_flow_fail(store, decl, "backing validation queue allocation failed"); return false; }
    queue[0] = decl;
    RowsCaptureIndexes seen = {.verify = getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") != NULL};
    bool valid = true;
    for (size_t at = 0; at < count && valid; at++) {
        ASTNode *current = queue[at];
        FunctionParamFlowProgramPointIndex *index;
        if (!rows_capture_index(store, &seen, current, &index) ||
            !function_param_flow_index_roots_current(index) ||
            !semantic_param_publication_routine_relation_header_ready(store->ctx, current)) { valid = false; break; }
        if (index->declared_result != NULL &&
            (!function_param_flow_no_store_graph_capture(store, current, capture, user) ||
             !function_param_flow_program_point_index_build(store, current, &index))) { valid = false; break; }
        FunctionParamFlowBackingFact **frames = index->backing_frames;
        size_t frame_count = index->backing_frame_count;
        size_t parameter_count = index->param_count;
        FunctionParamFlowIndexClaims *claims = index->claims;
        FunctionParamFlowBackingFact *result = index->declared_result;
        if (capture != NULL && !capture(store->ctx, current, claims, NULL, user)) { valid = false; break; }
        if (frames == NULL && result == NULL) {
            if (frame_count != 0) valid = false;
            continue;
        }
        if (frame_count != parameter_count || (parameter_count != 0 && frames == NULL) ||
            parameter_count > SIZE_MAX - 63 ||
            claims == NULL) { valid = false; break; }
        /* A passed check whose shapes are unchanged answers again. With
         * PGY_VERIFY_FLOW_CURRENCY_MEMO it is also checked in full, and a
         * divergence fails the store. */
        size_t ordinal = (size_t)(index - store->program_point_indexes);
        bool memo_reused = capture == NULL && function_param_flow_rows_memo_current(store, ordinal);
        if (memo_reused) store->rows_memo_reuses++;
        if (memo_reused && !seen.verify) {
            const FunctionParamFlowRowsMemo *memo = &store->program_point_indexes[ordinal].rows_memo;
            for (size_t c = 0; c < memo->callee_count && valid; c++)
                valid = rows_capture_enqueue(store, current, &queue, &count, &capacity, memo->callees[c]);
            continue;
        }
        FunctionParamFlowRowsMemo memo_notes = {0};
        bool notes_kept = capture == NULL;
        for (size_t input = 0; input < frame_count + (result != NULL) && valid; input++) {
            bool result_frame = input == frame_count;
            FunctionParamFlowBackingFact *fact = result_frame ? result : frames[input];
            /* Only an established declared result (built, complete, neither
             * building nor pending) is a premise of a summary:
             * declared_result_frame_current answers any other with NULL, and
             * its callers use the callee's summaries. One an enclosing check
             * is still building (a demand opened inside that build, as
             * between mutually recursive routines), one pending on such a
             * build, or one not built yet may become established later, so a
             * pass that skips it keeps no memo. One built incomplete and not
             * pending is never rebuilt. */
            if (fact == NULL) continue;
            if (result_frame && (fact->building || fact->pending || fact->epoch == 0)) { notes_kept = false; continue; }
            if (result_frame && !fact->complete) continue;
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
                /* A row holds the call, or holds the intent call it is a step of. */
                bool owned = false;
                for (size_t n = 0; n < fact->node_count && !owned; n++)
                    owned = fact->nodes[n].node == fact->unsupported_relation.call &&
                        fact->nodes[n].id == fact->unsupported_relation.call_id;
                for (size_t n = 0; n < fact->node_count && !owned; n++)
                    owned = function_param_flow_intent_step_call(store->ctx, fact->nodes[n].node,
                        fact->unsupported_relation.call);
                if (!owned || fact->complete || fact->pending || fact->building ||
                    !function_param_flow_unsupported_relation_current(store, &fact->unsupported_relation)) { valid = false; break; }
            }
            for (size_t e = 0; e < fact->pending_relation_count && valid; e++) {
                FunctionParamFlowPendingRelation *edge = &fact->pending_relations[e];
                ASTNode *callee = semantic_param_publication_callee(store->ctx, edge->call, edge->ordinal);
                size_t formal = 0;
                bool owned = false;
                for (size_t n = 0; n < fact->node_count && !owned; n++)
                    owned = fact->nodes[n].node == edge->call && fact->nodes[n].id == edge->call_id;
                for (size_t n = 0; n < fact->node_count && !owned; n++)
                    owned = function_param_flow_intent_step_call(store->ctx, fact->nodes[n].node, edge->call);
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
                            !rows_capture_index(store, &seen, callee, &callee_index) ||
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
                     /* Only a parameter this call owns can be rebound to hold the input. */
                     (p != input && param->mode != PARAM_MODE_DEFAULT && param->mode != PARAM_MODE_OWN &&
                      binding->origins[0] != 0) ||
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
                    if (claim->id != binding->binding || claim->node == NULL ||
                        ast_node_stable_id(claim->node) != binding->binding) continue;
                    /* A local is a let declaration or a match binding, the
                     * one identifier whose binding is itself. */
                    if ((claim->shape == AST_LET_DECL && claim->node->type == AST_LET_DECL) ||
                        (claim->shape == AST_IDENTIFIER && claim->node->type == AST_IDENTIFIER &&
                         ast_identifier_binding_syntax_id(claim->node) == binding->binding)) declaration = true;
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
                if (row->receiver == NULL ? (row->receiver_id != 0 || row->receiver_type != NULL || row->receiver_callee_id != 0 || row->receiver_formal_id != 0 || row->receiver_summarized) : row->node->type != AST_CALL) { valid = false; break; }
                if (row->result_callee_id == 0 ?
                    (row->result_argument_count != 0 || row->result_argument_kinds != NULL) :
                    (row->result_argument_count != ast_call_arg_count(row->node) ||
                     (row->result_argument_count != 0 && row->result_argument_kinds == NULL))) { valid = false; break; }
                if (row->node->type != AST_CALL) continue;
                if (capture != NULL && !capture(store->ctx, current, NULL, row->node, user)) { valid = false; break; }
                ASTNode *edge_call = row->node;
                uint32_t edge_callee_id = row->result_callee_id;
                const Type *edge_type = row->result_type;
                unsigned char *edge_modes = row->result_argument_kinds;
                size_t edge_arity = ast_call_arg_count(edge_call);
                bool result_edge = row->result_callee_id != 0;
                bool receiver_edge = row->receiver != NULL;
                /* An intent call depends on the actions its steps call. */
                size_t call_edges = edge_arity + result_edge + receiver_edge, intent_edges = 0;
                while (function_param_flow_intent_action_callee(store->ctx, edge_call, intent_edges) != NULL) intent_edges++;
                for (size_t p = 0; p < call_edges + intent_edges && valid; p++) {
                    ASTNode *callee;
                    if (p >= call_edges) {
                        callee = function_param_flow_intent_action_callee(store->ctx, edge_call, p - call_edges);
                    } else if (receiver_edge && p == edge_arity + result_edge) {
                        ASTNode *received = NULL;
                        /* A summarized receiver is revalidated through its
                         * callee's rows, as an argument edge is. */
                        callee = row->receiver_summarized
                            ? semantic_param_publication_direct_callee(store->ctx, edge_call)
                            : function_param_flow_receiver_readonly_current(store, edge_call, false);
                        valid = callee != NULL && ast_node_stable_id(callee) == row->receiver_callee_id &&
                            semantic_param_publication_instance_receiver(store->ctx, edge_call, &received) &&
                            received == row->receiver && ast_node_stable_id(received) == row->receiver_id &&
                            semantic_param_publication_value(store->ctx, received).type == row->receiver_type &&
                            ast_func_param_stable_id(ast_func_param(callee, 0)) == row->receiver_formal_id;
                        if (valid && !row->receiver_summarized)
                            notes_kept = notes_kept && function_param_flow_rows_memo_note(
                                &memo_notes.receiver_calls, &memo_notes.receiver_count, edge_call);
                    } else if (result_edge && p == edge_arity) {
                        const Type *type = NULL;
                        callee = semantic_param_publication_declared_result(store->ctx, edge_call, &type);
                        FunctionParamFlowProgramPointIndex *callee_index;
                        if (callee == NULL || ast_node_stable_id(callee) != edge_callee_id ||
                            type != edge_type || edge_arity + receiver_edge != ast_func_param_count(callee) ||
                            !rows_capture_index(store, &seen, callee, &callee_index) ||
                            callee_index->declared_result == NULL ||
                            callee_index->declared_result->declared_result_type != type) { valid = false; break; }
                    } else {
                        valid = rows_capture_argument(store, &seen, edge_call, p, &callee);
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
                                    /* A reused frame is checked before this pass demands
                                     * every callee it names, so an entry of the closure the
                                     * pass has not reached yet answers by its last
                                     * evaluation. A change there marks the closure changed,
                                     * and the seal runs only after a pass with none. */
                                    if (entry->function_decl == callee && !entry->analysis_unsupported &&
                                        !entry->returned_backing_unproved && !entry->returned_backing_pending &&
                                        entry->output_producer == FUNCTION_PARAM_FLOW_OUTPUT_BACKING_FRAME &&
                                        entry->output_dependencies_ready && entry->output_dependencies != NULL &&
                                        entry->output_count == ast_func_param_count(callee) &&
                                        (entry->state == FUNCTION_PARAM_FLOW_COMPLETE ||
                                         (store->solving && (entry->state == FUNCTION_PARAM_FLOW_EVALUATED ||
                                          (entry->state == FUNCTION_PARAM_FLOW_UNSEEN && s >= store->active_start))))) {
                                        FunctionParamFlowProgramPointIndex *callee_index;
                                        if (!rows_capture_index(store, &seen, callee, &callee_index) ||
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
                                            !function_param_flow_no_store_graph_capture(store, callee, capture, user))) break;
                                        if (source->private_stores_closed)
                                            notes_kept = notes_kept && function_param_flow_rows_memo_note(
                                                &memo_notes.no_store_routines, &memo_notes.no_store_count, callee);
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
                    notes_kept = notes_kept && function_param_flow_rows_memo_note(
                        &memo_notes.callees, &memo_notes.callee_count, callee);
                    valid = rows_capture_enqueue(store, current, &queue, &count, &capacity, callee);
                }
            }
        }
        if (memo_reused && (!valid ||
                (notes_kept && !function_param_flow_rows_memo_matches(store, ordinal, &memo_notes))))
            function_param_flow_fail(store, current, "rows check memo diverged from its full check");
        if (capture == NULL) function_param_flow_rows_memo_record(store, ordinal, &memo_notes, valid && notes_kept);
    }
    free(queue);
    free(seen.decls); free(seen.positions); free(seen.arguments);
    return valid;
}

bool function_param_flow_backing_rows_current(FunctionParamFlowSummaryStore *store, ASTNode *decl)
{
    return function_param_flow_backing_rows_capture(store, decl, NULL, NULL);
}
