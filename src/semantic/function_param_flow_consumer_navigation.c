#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Frame creation and fixed-point execution share the control-root owner. */

FunctionParamFlowBackingFact *function_param_flow_declared_result_frame_current(FunctionParamFlowSummaryStore *store,
    ASTNode *call, FunctionParamFlowBackingFact *consumer)
{
    const Type *result_type = NULL;
    ASTNode *decl = store == NULL ? NULL : semantic_param_publication_declared_result(store->ctx, call, &result_type);
    if (store == NULL || store->failed || decl == NULL || result_type == NULL) return NULL;
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
    if (!function_param_flow_program_point_index_build(store, decl, &index) ||
        !function_param_flow_index_roots_current(index)) return NULL;
    size_t arity = ast_func_param_count(decl);
    if (index->param_count != arity) return NULL;
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
        created->outputs = calloc(1, sizeof(*created->outputs));
        if (created->effects == NULL || created->outputs == NULL) {
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
        fact->origin_count != 1 || fact->origin_words != 1 || fact->destination_count != 1 || fact->destination_words != 1)
        return NULL;
    if (fact->building) { if (consumer != NULL) consumer->pending = true; return NULL; }
    if (!function_param_flow_no_store_graph(store, decl)) return NULL;
    fact->private_stores_closed = true;
    if (fact->epoch != 0 && !function_param_flow_backing_rows_current(store, decl)) return NULL;
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
        !function_param_flow_backing_rows_current(store, decl)) return NULL;
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

bool function_param_flow_returned_backing_effects(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, unsigned *effects, bool *outputs, size_t output_count, bool *pending,
    bool private_stores_closed)
{
    if (store == NULL || store->failed || effects == NULL || pending == NULL || decl == NULL || outputs == NULL ||
        output_count != ast_func_param_count(decl) || ordinal >= output_count ||
        !semantic_param_publication_inputs_ready(store->ctx)) return false;
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index) ||
        !function_param_flow_index_roots_current(index)) return false;
    if (index->backing_frames == NULL) {
        if (output_count > SIZE_MAX / sizeof(*index->backing_frames)) { function_param_flow_fail(store, decl, "backing frame vector size overflow"); return false; }
        index->backing_frames = calloc(output_count, sizeof(*index->backing_frames));
        if (index->backing_frames == NULL) { function_param_flow_fail(store, decl, "backing frame vector allocation failed"); return false; }
        index->backing_frame_count = output_count;
    }
    if (index->backing_frame_count != output_count) return false;
    if (index->backing_frames[ordinal] == NULL) {
        if (output_count > SIZE_MAX - 63) { function_param_flow_fail(store, decl, "backing width overflow"); return false; }
        size_t words = (output_count + 63) / 64;
        if (words == 0 || words > SIZE_MAX / sizeof(uint64_t)) { function_param_flow_fail(store, decl, "backing matrix size overflow"); return false; }
        FunctionParamFlowBackingFact *created = calloc(1, sizeof(*created));
        if (created == NULL) { function_param_flow_fail(store, decl, "backing fact allocation failed"); return false; }
        index->backing_frames[ordinal] = created;
        created->origin_count = 1; created->origin_words = 1;
        created->destination_count = output_count; created->destination_words = words;
        created->input_ordinal = ordinal;
        created->input_id = ast_func_param_stable_id(ast_func_param(decl, ordinal));
        created->claims = index->claims;
        created->effects = calloc(1, sizeof(unsigned));
        created->outputs = calloc(words, sizeof(uint64_t));
        if (created->effects == NULL || created->outputs == NULL) { function_param_flow_fail(store, decl, "backing matrix allocation failed"); return false; }
        ReturnedBacking seed = {.store = store, .decl = decl, .fact = created};
        for (size_t p = 0; p < output_count; p++) {
            FuncParam *param = ast_func_param(decl, p);
            FunctionParamFlowBackingBinding *row = backing_binding(&seed, ast_func_param_stable_id(param), true);
            if (row == NULL) return false;
            if (p == ordinal) row->origins[0] = 1;
            if (param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF)
                row->destinations[p / 64] |= UINT64_C(1) << (p % 64);
        }
    }
    /* Heap-owned fact/root rows survive recursive index and entry growth. */
    FunctionParamFlowBackingFact *fact = index->backing_frames[ordinal];
    if (fact->input_ordinal != ordinal || fact->input_id == 0 ||
        fact->input_id != ast_func_param_stable_id(ast_func_param(decl, ordinal)) ||
        fact->origin_count != 1 || fact->origin_words != 1 || fact->destination_count != output_count ||
        fact->destination_words != (output_count + 63) / 64) return false;
    /* Effects are monotone within one admission mode. Never reuse a precise
     * private-store evaluation for a query lacking its fresh graph witness,
     * nor pretend a previously conservative CALL mask can be subtracted. */
    if (fact->epoch != 0 && fact->private_stores_closed != private_stores_closed) return false;
    fact->private_stores_closed = private_stores_closed;
    FunctionParamFlowControlProjection *projection = index->control_projection;
    ReturnedBacking state = {.store = store, .lookup = {store->ctx, store->program_root}, .decl = decl,
        .fact = fact, .private_stores_closed = private_stores_closed};
    if (!function_param_flow_backing_rows_current(store, decl)) return false;
    if (fact->building) { *pending = true; return false; }
    if (fact->epoch == 0 || (fact->pending && fact->evaluation_pass != store->fixed_point_passes + 1)) {
        fact->building = true; fact->complete = true; fact->pending = false;
        free(fact->pending_relations); fact->pending_relations = NULL; fact->pending_relation_count = 0;
        fact->sealed_pending_relation_count = 0;
        fact->evaluation_pass = store->fixed_point_passes + 1;
        do {
            if (fact->epoch == SIZE_MAX) { function_param_flow_fail(store, decl, "backing iteration identity overflow"); fact->complete = false; break; }
            fact->epoch++; fact->changed = false;
            function_param_flow_backing_control_roots(&state, projection);
        } while (fact->changed && fact->complete);
        fact->building = false;
    }
    *effects = fact->effects[0];
    for (size_t p = 0; p < output_count; p++)
        outputs[p] = (fact->outputs[p / 64] & (UINT64_C(1) << (p % 64))) != 0;
    *pending = fact->pending && fact->complete;
    return fact->complete && !fact->pending;
}

bool function_param_flow_control_append(SemanticContext *ctx,
    FunctionParamFlowControlProjection *projection, ASTNode *node, size_t claim, size_t root)
{
    if (projection->step_count == projection->step_capacity) {
        size_t capacity = projection->step_capacity == 0 ? 16 : projection->step_capacity * 2;
        if (capacity < projection->step_capacity || capacity > SIZE_MAX / sizeof(*projection->steps)) {
            function_param_flow_fail(ctx->function_param_flow_summaries, node, "control projection size overflow");
            return false;
        }
        FunctionParamFlowControlStep *steps = realloc(projection->steps, capacity * sizeof(*steps));
        if (steps == NULL) {
            function_param_flow_fail(ctx->function_param_flow_summaries, node, "control projection allocation failed");
            return false;
        }
        projection->steps = steps;
        projection->step_capacity = capacity;
    }
    projection->steps[projection->step_count++] = (FunctionParamFlowControlStep){
        node, ast_node_stable_id(node), claim, root};
    return true;
}

void function_param_flow_backing_control_roots(ReturnedBacking *state,
    const FunctionParamFlowControlProjection *projection)
{
    if (state == NULL || state->fact == NULL) return;
    if (projection == NULL || !projection->ready || projection->root_offsets == NULL ||
        projection->step_count > projection->step_capacity ||
        (projection->step_count != 0 && projection->steps == NULL)) {
        state->fact->complete = false; return;
    }
    for (size_t r = 0; r < projection->root_count && state->fact->complete; r++) {
        size_t begin = projection->root_offsets[r], end = projection->root_offsets[r + 1];
        if (begin > end || end > projection->step_count) { state->fact->complete = false; return; }
        for (size_t s = begin; s < end; s++) {
            if (state->fact->analysis_unsupported) return;
            if (projection->steps[s].node == NULL || projection->steps[s].syntax_id == 0 ||
                projection->steps[s].root_ordinal != r) { state->fact->complete = false; return; }
            backing_consumers(state, projection->steps[s].node);
        }
    }
}

static bool function_param_flow_consumer_edges_current(
    const FunctionParamFlowIndexClaims *claims, const FunctionParamFlowIndexClaim *row)
{
    if (row->node == NULL || (row->child_count != 0 && (row->children == NULL || row->child_ids == NULL ||
        row->child_claim_ordinals == NULL))) return false;
    for (size_t c = 0; c < row->child_count; c++) {
        ASTNode *actual = row->shape == AST_BINARY ? (c == 0 ? ast_binary_left(row->node) : ast_binary_right(row->node)) :
            row->shape == AST_UNARY ? ast_unary_operand(row->node) :
            row->shape == AST_MEMBER_ACCESS ? ast_member_object(row->node) :
            row->shape == AST_ARRAY_ACCESS ? (c == 0 ? ast_array_access_array(row->node) : ast_array_access_index(row->node)) : NULL;
        if (actual != row->children[c] || row->child_ids[c] != ast_node_stable_id(actual)) return false;
        size_t ordinal = row->child_claim_ordinals[c];
        if (row->children[c] == NULL) {
            if (ordinal != SIZE_MAX) return false;
            continue;
        }
        if (ordinal >= claims->count || claims->rows[ordinal].node != row->children[c] ||
            claims->rows[ordinal].id != row->child_ids[c] ||
            !claims->rows[ordinal].snapshot_complete || claims->rows[ordinal].snapshot_active) return false;
    }
    return true;
}
static bool function_param_flow_consumer_value_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaim *row)
{
    if (row->node == NULL || !row->semantic_promoted || !row->snapshot_complete ||
        !row->snapshot_semantic_complete || row->snapshot_active || row->id == 0 ||
        row->id != ast_node_stable_id(row->node) || row->shape != row->node->type ||
        (row->shape == AST_IDENTIFIER && row->binding != ast_identifier_binding_syntax_id(row->node)) ||
        (row->shape == AST_BINARY && row->syntax_operator != (int)row->node->data.binary.op.type) ||
        (row->shape == AST_UNARY && row->syntax_operator != (int)row->node->data.unary.op.type)) return false;
    if (row->shape == AST_MEMBER_ACCESS) {
        const char *name = ast_member_name(row->node);
        if ((name == NULL) != (row->syntax_name == NULL) ||
            (name != NULL && strcmp(name, row->syntax_name) != 0)) return false;
    }
    bool reachable = false;
    bool execution = semantic_param_publication_execution(ctx, row->node, &reachable);
    if (row->execution_present != execution || (execution && row->reachable != reachable)) return false;
    if (row->shape == AST_MEMBER_ACCESS && row->unit_enum_selection !=
        semantic_param_publication_checked_unit_enum_selection(ctx, row->node)) return false;
    if (execution && !reachable) return true;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, row->node);
    return row->value_ready == value.ok && row->value_meta == (value.ok && value.meta) &&
        row->value_origin_ready == (value.ok && value.origin_ready) && row->value_type == value.type &&
        row->physical_value == semantic_param_publication_value_is_physical_scalar(ctx, row->node) &&
        row->slot_token_operand == semantic_param_publication_slot_token_operand(ctx, row->node);
}

/* EMPTY shares old C's data-child/leaf preparation across normal frames.
 * It says nothing about operator purity or overload safety; D, O and loan
 * traversal keep their existing contracts and visit order. */
bool function_param_flow_consumer_empty_derive(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, size_t ordinal)
{
    if (ctx == NULL || claims == NULL || claims->rows == NULL || ordinal >= claims->count) return false;
    const FunctionParamFlowIndexClaim *row = &claims->rows[ordinal];
    if (!row->semantic_promoted || !row->snapshot_complete || !row->snapshot_semantic_complete ||
        row->snapshot_active || row->id == 0) return false;
    if (row->shape == AST_NUMBER || row->shape == AST_STRING || row->shape == AST_BOOLEAN) return true;
    if (row->shape == AST_IDENTIFIER)
        return row->value_ready && (row->value_meta ||
            (row->binding != 0 && semantic_param_publication_binding(ctx, row->node) == row->binding) ||
            (row->slot_token_operand && semantic_param_publication_slot_token_operand(ctx, row->node)));
    size_t required = row->shape == AST_BINARY || row->shape == AST_ARRAY_ACCESS ? 2 :
        row->shape == AST_UNARY || row->shape == AST_MEMBER_ACCESS ? 1 : 0;
    if (required == 0 || row->child_count != required ||
        !function_param_flow_consumer_edges_current(claims, row) ||
        !function_param_flow_consumer_value_current(ctx, row)) return false;
    if ((row->shape == AST_MEMBER_ACCESS || row->shape == AST_ARRAY_ACCESS) &&
        row->execution_present && !row->reachable) return false;
    if (row->shape == AST_MEMBER_ACCESS && row->unit_enum_selection) return true;
    for (size_t c = 0; c < required; c++) {
        size_t child = row->child_claim_ordinals[c];
        if (child == SIZE_MAX || !claims->rows[child].snapshot_semantic_complete ||
            !claims->rows[child].consumer_empty) return false;
    }
    return true;
}

bool function_param_flow_consumer_empty_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, ASTNode *node, bool *empty)
{
    if (ctx == NULL || empty == NULL || claims == NULL || claims->failed || claims->rows == NULL ||
        node == NULL || (node->type != AST_BINARY && node->type != AST_UNARY &&
        node->type != AST_MEMBER_ACCESS && node->type != AST_ARRAY_ACCESS)) return false;
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[c];
        if (row->node != node) continue;
        if (!row->snapshot_complete || !row->snapshot_semantic_complete || !row->semantic_promoted ||
            row->snapshot_active || row->id == 0 || row->id != ast_node_stable_id(node) ||
            row->shape != node->type ||
            ((node->type == AST_BINARY || node->type == AST_UNARY) &&
             row->syntax_operator != (int)(node->type == AST_BINARY ?
                node->data.binary.op.type : node->data.unary.op.type)) ||
            row->child_count != (node->type == AST_BINARY || node->type == AST_ARRAY_ACCESS ? 2u : 1u) ||
            !function_param_flow_consumer_edges_current(claims, row) ||
            !function_param_flow_consumer_value_current(ctx, row) ||
            row->consumer_empty != function_param_flow_consumer_empty_derive(ctx, claims, c)) return false;
        if (row->consumer_empty && !(node->type == AST_MEMBER_ACCESS && row->unit_enum_selection))
            for (size_t child = 0; child < row->child_count; child++) {
                size_t ordinal = row->child_claim_ordinals[child];
                if (ordinal >= claims->count ||
                    !function_param_flow_consumer_value_current(ctx, &claims->rows[ordinal])) return false;
            }
        *empty = row->consumer_empty;
        return true;
    }
    return false;
}

bool function_param_flow_index_claims_current(SemanticContext *ctx,
    const FunctionParamFlowProgramPointIndex *index)
{
    if (index == NULL) return false;
    const FunctionParamFlowIndexClaims *claims = index->claims;
    const FunctionParamFlowControlProjection *projection = index->control_projection;
    ASTNode *const *roots = index->dependency_roots;
    const uint32_t *root_ids = index->dependency_root_ids;
    size_t root_count = index->dependency_root_count;
    for (size_t i = 0; i < index->local_count; i++)
        if (index->local_definitions[i]->type != AST_LET_DECL || ast_let_is_alias(index->local_definitions[i]) ||
            ast_node_stable_id(index->local_definitions[i]) != index->local_ids[i]) return false;
    if (!function_param_flow_claims_current(ctx, claims, true)) return false;
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[c];
        if (row->consumer_empty != function_param_flow_consumer_empty_derive(ctx, claims, c)) return false;
    }
    if (projection == NULL || !projection->ready || projection->root_count != root_count ||
        (root_count != 0 && (roots == NULL || root_ids == NULL)) ||
        projection->root_offsets == NULL || projection->root_offsets[0] != 0 ||
        projection->root_offsets[projection->root_count] != projection->step_count ||
        projection->step_count > projection->step_capacity ||
        (projection->step_count != 0 && projection->steps == NULL) ||
        (projection->root_count != 0 && projection->root_claim_ordinals == NULL) ||
        projection->step_count > claims->count) return false;
    size_t projected = 0, emitted = 0, incoming = 0, outgoing = 0;
    for (size_t c = 0; c < claims->count; c++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[c];
        if (row->control_projection_active) return false;
        if (!row->control_projected) {
            if (row->control_step_ordinal != SIZE_MAX || row->control_root_ordinal != SIZE_MAX ||
                row->control_incoming_count != 0 || row->control_outgoing_count != 0) return false;
            continue;
        }
        if (!row->snapshot_complete || row->control_root_ordinal >= projection->root_count ||
            row->control_incoming_count == 0 ||
            incoming > SIZE_MAX - row->control_incoming_count ||
            outgoing > SIZE_MAX - row->control_outgoing_count) return false;
        incoming += row->control_incoming_count;
        outgoing += row->control_outgoing_count;
        size_t expected_children = 0;
        if (function_param_flow_control_expands(row->shape) &&
            !(row->execution_present && !row->reachable))
            for (size_t child = 0; child < row->child_count; child++)
                if (row->children[child] != NULL && (row->shape != AST_WITH_STMT || child == 1))
                    expected_children++;
        if (row->control_outgoing_count != expected_children) return false;
        projected++;
        if (row->control_step_ordinal == SIZE_MAX) {
            if (!function_param_flow_control_expands(row->shape) &&
                row->shape != AST_BREAK && row->shape != AST_CONTINUE) return false;
        } else {
            if (function_param_flow_control_expands(row->shape) || row->shape == AST_BREAK ||
                row->shape == AST_CONTINUE || row->control_step_ordinal >= projection->step_count) return false;
            const FunctionParamFlowControlStep *step = &projection->steps[row->control_step_ordinal];
            if (step->node != row->node || step->syntax_id != row->id ||
                step->claim_ordinal != c || step->root_ordinal != row->control_root_ordinal) return false;
            emitted++;
        }
    }
    if (projected != projection->projected_claim_count || emitted != projection->step_count ||
        outgoing > SIZE_MAX - projection->root_count ||
        incoming != outgoing + projection->root_count) return false;
    for (size_t r = 0; r < projection->root_count; r++) {
        size_t root_claim = projection->root_claim_ordinals[r];
        if (root_claim >= claims->count || !claims->rows[root_claim].control_projected ||
            claims->rows[root_claim].node != roots[r] ||
            claims->rows[root_claim].id != root_ids[r]) return false;
        if (projection->root_offsets[r] > projection->root_offsets[r + 1] ||
            projection->root_offsets[r + 1] > projection->step_count) return false;
        for (size_t s = projection->root_offsets[r]; s < projection->root_offsets[r + 1]; s++)
            if (projection->steps[s].root_ordinal != r) return false;
    }
    return true;
}
