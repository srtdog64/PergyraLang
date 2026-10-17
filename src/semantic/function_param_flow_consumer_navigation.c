#include "function_param_flow_summary_internal.h"
#include "../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include <stdint.h>
#include <stdlib.h>

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
