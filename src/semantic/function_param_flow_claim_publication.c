#include "function_param_flow_summary_internal.h"
#include "typed_map_query_fact_owner.h"
#include "builtin_kind.h"
#include "../common/string_compat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool function_param_flow_claim_capture(SemanticContext *ctx,
    FunctionParamFlowIndexClaims *claims, ASTNode *node, bool semantic)
{
    if (claims == NULL || claims->failed || node == NULL) return false;
    FunctionParamFlowIndexClaim *row = NULL;
    for (size_t i = 0; i < claims->count; i++) if (claims->rows[i].node == node) {
        row = &claims->rows[i];
        if (!semantic || row->semantic_promoted) return true;
        goto promote;
    }
    bool supported = false;
    size_t children = function_param_flow_claim_child_count(node, &supported);
    if (!supported) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim syntax topology is unsupported"); return false;
    }
    if (children > SIZE_MAX / sizeof(ASTNode *)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim child storage is missing or its count is not addressable"); return false;
    }
    if (claims->count >= SIZE_MAX / sizeof(*claims->rows)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim size overflow"); return false;
    }
    FunctionParamFlowIndexClaim *rows = realloc(claims->rows, (claims->count + 1) * sizeof(*rows));
    if (rows == NULL) { claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim allocation failed"); return false; }
    claims->rows = rows;
    row = &rows[claims->count++];
    claims->generation++;
    *row = (FunctionParamFlowIndexClaim){.node = node, .id = ast_node_stable_id(node), .shape = node->type,
        .child_count = children, .control_step_ordinal = SIZE_MAX, .control_root_ordinal = SIZE_MAX};
    row->children = children == 0 ? NULL : calloc(children, sizeof(*row->children));
    row->child_ids = children == 0 ? NULL : calloc(children, sizeof(*row->child_ids));
    row->child_claim_ordinals = children == 0 ? NULL : malloc(children * sizeof(*row->child_claim_ordinals));
    if (children != 0 && (row->children == NULL || row->child_ids == NULL || row->child_claim_ordinals == NULL)) {
        claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "index claim edge allocation failed"); return false;
    }
    for (size_t c = 0; c < children; c++) {
        row->children[c] = function_param_flow_claim_child(node, c);
        row->child_ids[c] = ast_node_stable_id(row->children[c]);
        row->child_claim_ordinals[c] = SIZE_MAX;
    }
    if (node->type == AST_IDENTIFIER) row->binding = ast_identifier_binding_syntax_id(node);
    if (node->type == AST_LET_DECL) row->local_alias = ast_let_is_alias(node);
    if (node->type == AST_MATCH_CASE) row->match_family = ast_match_case_semantic_subject_family(node);
    if (node->type == AST_BINARY) row->syntax_operator = node->data.binary.op.type;
    if (node->type == AST_UNARY) row->syntax_operator = node->data.unary.op.type;
    if (node->type == AST_PARALLEL_BLOCK) row->parallel_join = ast_parallel_is_join_form(node);
    if (node->type == AST_BOOLEAN) row->syntax_literal_flags = ast_boolean_value(node);
    if (node->type == AST_NUMBER) {
        row->syntax_number = ast_number_value(node);
        row->syntax_long = ast_number_exact_long_value(node);
        row->syntax_literal_flags = ast_number_is_long(node) | (ast_number_is_float(node) << 1) | (ast_number_is_duration(node) << 2);
    }
    const char *syntax_name = node->type == AST_IDENTIFIER ? ast_identifier_name(node) :
        node->type == AST_MEMBER_ACCESS ? ast_member_name(node) :
        node->type == AST_TYPE ? ast_type_name(node) : node->type == AST_STRING ? ast_string_value(node) : NULL;
    row->syntax_name = syntax_name == NULL ? NULL : pergyra_strdup(syntax_name);
    if (syntax_name != NULL && row->syntax_name == NULL) { claims->failed = true; return false; }
    if (node->type == AST_TYPE || node->type == AST_CALL) {
        GenericParams *args = node->type == AST_TYPE ? ast_type_generic_args(node) : ast_call_generic_args(node);
        row->generic_name_count = args == NULL ? 0 : args->count;
        if (row->generic_name_count > SIZE_MAX / sizeof(*row->generic_names)) { claims->failed = true; return false; }
        row->generic_names = row->generic_name_count == 0 ? NULL : calloc(row->generic_name_count, sizeof(*row->generic_names));
        if (row->generic_name_count != 0 && row->generic_names == NULL) { claims->failed = true; return false; }
        for (size_t p = 0; p < row->generic_name_count; p++) {
            row->generic_names[p] = pergyra_strdup(args->params[p]->name);
            if (row->generic_names[p] == NULL) { claims->failed = true; return false; }
        }
    }
    if (node->type == AST_CAST || node->type == AST_TYPE_TEST) {
        const char *target = node->type == AST_CAST ? ast_cast_target_type(node) : ast_type_test_target_type(node);
        row->cast_target = target == NULL ? NULL : pergyra_strdup(target);
        if (target != NULL && row->cast_target == NULL) { claims->failed = true; return false; }
    }
    if (!semantic) return row->id != 0;
promote:
    claims->generation++;
    row->semantic_promoted = true;
    row->execution_present = semantic_param_publication_execution(ctx, node, &row->reachable);
    row->physical_value = semantic_param_publication_value_is_physical_scalar(ctx, node);
    row->unit_enum_selection = node->type == AST_MEMBER_ACCESS &&
        semantic_param_publication_checked_unit_enum_selection(ctx, node);
    SemanticPublishedValue value = semantic_param_publication_value(ctx, node);
    row->value_ready = value.ok;
    row->value_meta = value.ok && value.meta;
    row->value_origin_ready = value.ok && value.origin_ready;
    row->value_type = value.type;
    row->slot_token_operand = semantic_param_publication_slot_token_operand(ctx, node);
    if (node->type == AST_LET_DECL) row->channel_binding = semantic_param_publication_channel_binding(ctx, node);
    if (node->type == AST_LET_DECL) row->empty_map_binding = semantic_param_publication_empty_map_binding(ctx, node);
    if (node->type == AST_LET_DECL || node->type == AST_LET_DESTRUCTURE) {
        SemanticPublishedSlotBirth birth;
        row->owned_slot_binding = semantic_param_publication_owned_slot_binding(ctx, node, &birth);
    }
    if (node->type == AST_CALL) {
        row->collection_effect_present = ast_call_semantic_collection_effect(node,
            &row->collection_effect_kind, &row->collection_effect_receiver, &row->collection_effect_source);
        row->readfile_result = semantic_param_publication_checked_readfile_independent(ctx, node);
        row->readfile_execution_required = row->readfile_result && row->execution_present && row->reachable;
        if (row->readfile_result) {
            row->readfile_next_plus_one = claims->readfile_first_plus_one;
            claims->readfile_first_plus_one = (size_t)(row - claims->rows) + 1;
        }
        row->readfile_result_projection = row->readfile_result && ast_call_semantic_value_type_name(node) != NULL;
        row->readfile_argument_projection = row->readfile_result && ast_call_argument(node, 0)->type == AST_CALL &&
            ast_call_semantic_value_type_name(ast_call_argument(node, 0)) != NULL;
        row->slot_write = semantic_param_publication_checked_slot_write(ctx, node);
        row->local_owner_constructor = semantic_param_publication_checked_local_owner_constructor(ctx, node);
        row->private_array_push = semantic_param_publication_private_array_push(ctx, node, NULL);
        row->result_relation = semantic_param_publication_checked_result_relation(ctx, node, 0,
            &row->result_depends, &row->external_retains);
        row->backing_relation = semantic_param_publication_checked_backing_relation(ctx, node, 0,
            &row->backing_depends, &row->backing_external_retains);
        if (row->result_relation || row->backing_relation || row->readfile_result) {
            row->result_type_name = pergyra_strdup(row->readfile_result ? "String" : ast_call_semantic_value_type_name(node));
            if (row->result_type_name == NULL) {
                claims->failed = true; function_param_flow_fail(ctx->function_param_flow_summaries, node, "result claim allocation failed"); return false;
            }
        }
    }
    if (node->type == AST_CAST) {
        row->numeric_cast = semantic_param_publication_checked_numeric_cast(ctx, node);
    }
    if (node->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(node);
        row->binding = semantic_param_publication_binding(ctx, target);
        row->assignment_replacement = semantic_param_publication_assignment_replacement(ctx, node);
    }
    return row->id != 0;
}

/* Replays the original selected tuple without dereferencing the retired Type
 * pointers in typed claims. NULL ctx is the post-MOVE syntax boundary. */
bool function_param_flow_readfile_claim_current(const SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, const FunctionParamFlowIndexClaim *row)
{
    if (claims == NULL || claims->failed || claims->rows == NULL || row == NULL ||
        !row->readfile_result || !row->execution_present || row->readfile_execution_required != row->reachable ||
        !row->semantic_promoted || !row->snapshot_semantic_complete ||
        !row->snapshot_complete || row->snapshot_active || !row->value_ready || row->value_meta ||
        row->shape != AST_CALL || row->node == NULL || row->node->type != AST_CALL ||
        row->id == 0 || row->id != ast_node_stable_id(row->node) || row->child_count != 2 ||
        row->children == NULL || row->child_ids == NULL || row->child_claim_ordinals == NULL ||
        row->result_type_name == NULL || strcmp(row->result_type_name, "String") != 0) return false;
    uintptr_t start = (uintptr_t)claims->rows, address = (uintptr_t)row;
    if (claims->count > SIZE_MAX / sizeof(*row) || address < start ||
        address - start >= claims->count * sizeof(*row) || (address - start) % sizeof(*row) != 0) return false;
    const ASTNode *call = row->node;
    const ASTNode *target = ast_call_callee(call), *argument = ast_call_argument(call, 0);
    uint32_t kind = 0;
    if (ast_call_arg_count(call) != 1 || ast_call_generic_arg_count(call) != 0 ||
        target == NULL || target->type != AST_IDENTIFIER || argument == NULL ||
        ast_identifier_binding_syntax_id(target) != 0 ||
        ast_call_semantic_callee_is_stdlib(call) || ast_call_semantic_callee_declared_callable(call) ||
        ast_call_semantic_callee_decl_id(call) != 0 || ast_call_semantic_callee_value_binding_id(call) != 0 ||
        !ast_call_semantic_callee_builtin_kind(call, &kind) || kind != BUILTIN_READ_FILE) return false;
    const char *result = ast_call_semantic_value_type_name(call);
    if (row->readfile_result_projection != (result != NULL) ||
        (result != NULL && strcmp(result, row->result_type_name) != 0)) return false;
    for (size_t c = 0; c < 2; c++) {
        size_t at = row->child_claim_ordinals[c];
        const ASTNode *child = c == 0 ? target : argument;
        if (at >= claims->count || row->children[c] != child || row->child_ids[c] == 0 ||
            row->child_ids[c] != ast_node_stable_id(child)) return false;
        const FunctionParamFlowIndexClaim *saved = &claims->rows[at];
        if (saved->node != child || saved->id != row->child_ids[c] || saved->shape != child->type ||
            !saved->snapshot_complete || saved->snapshot_active ||
            (child->type == AST_IDENTIFIER && saved->binding != ast_identifier_binding_syntax_id(child))) return false;
        if (c == 0 && (saved->syntax_name == NULL || strcmp(saved->syntax_name, "ReadFile") != 0 ||
            ast_identifier_name(child) == NULL || strcmp(ast_identifier_name(child), saved->syntax_name) != 0)) return false;
        if (c == 1 && (!saved->semantic_promoted || !saved->snapshot_semantic_complete ||
            !saved->value_ready || saved->value_meta)) return false;
    }
    const char *projection = argument->type == AST_CALL ? ast_call_semantic_value_type_name(argument) : NULL;
    if (row->readfile_argument_projection != (projection != NULL) ||
        (projection != NULL && strcmp(projection, "String") != 0)) return false;
    return ctx == NULL || semantic_param_publication_checked_readfile_independent(ctx, call);
}

bool function_param_flow_readfile_claim_count(const FunctionParamFlowIndexClaims *claims,
    size_t *count)
{
    if (count == NULL) return false;
    *count = 0;
    if (claims == NULL || claims->failed || claims->rows == NULL || claims->count == 0) return false;
    for (size_t r = 0; r < claims->count; r++) if (claims->rows[r].readfile_result) {
        if (*count == SIZE_MAX || !function_param_flow_readfile_claim_current(NULL, claims, &claims->rows[r])) return false;
        (*count)++;
    }
    size_t visited = 0, next = claims->readfile_first_plus_one;
    while (next != 0) {
        if (next > claims->count || visited++ >= *count || !claims->rows[next - 1].readfile_result) return false;
        next = claims->rows[next - 1].readfile_next_plus_one;
    }
    if (visited != *count) return false;
    return true;
}

/* Only cached original edges are followed. A nested operand cannot become an
 * executable root by borrowing its original parent's syntax membership. */
static bool readfile_lowered_container_edge(ASTNodeType shape, size_t ordinal)
{
    switch (shape) {
    case AST_BLOCK: return true;
    case AST_UNSAFE_BLOCK: case AST_TRANSACTION_BLOCK: return ordinal == 0;
    case AST_MATCH_STMT: return true;
    case AST_MATCH_CASE: return ordinal == 1 || ordinal == 2;
    default: return false;
    }
}

static bool readfile_execution_root_current(const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *call, const ASTNode *root)
{
    const FunctionParamFlowIndexClaim *at = call;
    bool root_seen = false;
    for (size_t depth = 0; depth < claims->count; depth++) {
        if (at->node == NULL || at->id == 0 || at->id != ast_node_stable_id(at->node) ||
            at->shape != at->node->type || !at->snapshot_complete || at->snapshot_active ||
            at->control_projection_active) return false;
        if (root != NULL && at->node == root && at->id == ast_node_stable_id(root)) root_seen = true;
        if (at->control_projected && at->control_step_ordinal != SIZE_MAX) return root_seen;
        if (at->node == root && at->shape == AST_MATCH_CASE) return false;
        const FunctionParamFlowIndexClaim *parent = NULL;
        size_t ordinal = (size_t)(at - claims->rows);
        for (size_t p = 0; p < claims->count; p++) {
            const FunctionParamFlowIndexClaim *candidate = &claims->rows[p];
            for (size_t c = 0; c < candidate->child_count; c++) {
                if (candidate->child_claim_ordinals == NULL) return false;
                if (candidate->child_claim_ordinals[c] != ordinal) continue;
                if (parent != NULL || candidate->node == NULL || candidate->shape != candidate->node->type ||
                    candidate->children == NULL || candidate->child_ids == NULL ||
                    candidate->children[c] != at->node || candidate->child_ids[c] != at->id ||
                    function_param_flow_claim_child(candidate->node, c) != at->node ||
                    (root_seen && !readfile_lowered_container_edge(candidate->shape, c))) return false;
                parent = candidate;
            }
        }
        if (parent == NULL) return false;
        at = parent;
    }
    return false;
}

/* The original selected list numbers static CALL occurrences. This query
 * does not consume them; only an actual executable visitor owns its seen set. */
bool function_param_flow_readfile_occurrence_ordinal(const FunctionParamFlowIndexClaims *claims,
    const ASTNode *call, const ASTNode *execution_root, size_t *ordinal)
{
    if (ordinal == NULL) return false;
    *ordinal = SIZE_MAX;
    size_t count = 0;
    if (!function_param_flow_readfile_claim_count(claims, &count)) return false;
    size_t next = claims->readfile_first_plus_one, position = 0;
    while (next != 0) {
        if (next > claims->count || position >= count) return false;
        const FunctionParamFlowIndexClaim *row = &claims->rows[next - 1];
        if (call != NULL && (row->node == call || row->id == ast_node_stable_id(call))) {
            if (row->node != call || row->id != ast_node_stable_id(call) || *ordinal != SIZE_MAX ||
                !function_param_flow_readfile_claim_current(NULL, claims, row) ||
                !readfile_execution_root_current(claims, row, execution_root)) return false;
            *ordinal = position;
        }
        next = row->readfile_next_plus_one;
        position++;
    }
    return position == count;
}

/* Static defining occurrences, not uses of their reusable String results.
 * Original row order supplies a stable ordinal without a second inventory. */
static bool readfile_write_projection(const FunctionParamFlowIndexClaims *claims,
    const ASTNode *statement, size_t *count, size_t *ordinal,
    size_t requested, bool *required, size_t *required_total)
{
    size_t sources = 0;
    *count = 0;
    *ordinal = SIZE_MAX;
    if (required != NULL) *required = false;
    if (required_total != NULL) *required_total = 0;
    if (!function_param_flow_readfile_claim_count(claims, &sources)) return false;
    if (sources == 0) return true;
    for (size_t r = 0; r < claims->count; r++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[r];
        if (row->shape != AST_LET_DECL && row->shape != AST_ASSIGNMENT) continue;
        size_t edge = row->shape == AST_LET_DECL ? 0 : 1;
        if (!row->snapshot_complete || row->snapshot_active || row->child_count != 2 ||
            row->children == NULL || row->child_ids == NULL || row->child_claim_ordinals == NULL) return false;
        size_t child = row->child_claim_ordinals[edge];
        if (row->children[edge] == NULL && child == SIZE_MAX) continue;
        if (child >= claims->count) return false;
        if (!claims->rows[child].readfile_result) continue;
        if (row->node == NULL || row->node->type != row->shape || row->id == 0 ||
            ast_node_stable_id(row->node) != row->id ||
            row->children[edge] != claims->rows[child].node ||
            row->child_ids[edge] != claims->rows[child].id ||
            function_param_flow_claim_child(row->node, edge) != row->children[edge] ||
            *count == SIZE_MAX) return false;
        for (size_t c = 0; c < row->child_count; c++) {
            size_t at = row->child_claim_ordinals[c];
            const ASTNode *node = row->children[c];
            if (function_param_flow_claim_child(row->node, c) != node ||
                row->child_ids[c] != ast_node_stable_id(node) || (node == NULL ? at != SIZE_MAX :
                at >= claims->count || claims->rows[at].node != node || claims->rows[at].id != row->child_ids[c]))
                return false;
        }
        if (statement != NULL && (row->node == statement || row->id == ast_node_stable_id(statement))) {
            if (row->node != statement || *ordinal != SIZE_MAX) return false;
            *ordinal = *count;
        }
        bool needed = claims->rows[child].readfile_execution_required;
        if (required != NULL && *count == requested) *required = needed;
        if (required_total != NULL && needed) (*required_total)++;
        (*count)++;
    }
    return true;
}

bool function_param_flow_readfile_write_count(const FunctionParamFlowIndexClaims *claims,
    size_t *count)
{
    size_t ordinal = SIZE_MAX;
    return count != NULL && readfile_write_projection(claims, NULL, count, &ordinal, SIZE_MAX, NULL, NULL);
}

bool function_param_flow_readfile_write_ordinal(const FunctionParamFlowIndexClaims *claims,
    const ASTNode *statement, size_t *ordinal)
{
    size_t count = 0;
    return ordinal != NULL && readfile_write_projection(claims, statement, &count, ordinal, SIZE_MAX, NULL, NULL);
}

bool function_param_flow_readfile_occurrence_required(const FunctionParamFlowIndexClaims *claims,
    size_t ordinal, bool *required)
{
    if (required == NULL) return false;
    *required = false;
    size_t count = 0;
    if (!function_param_flow_readfile_claim_count(claims, &count) || ordinal >= count) return false;
    size_t next = claims->readfile_first_plus_one;
    for (size_t position = 0; position < ordinal; position++) next = claims->rows[next - 1].readfile_next_plus_one;
    *required = claims->rows[next - 1].readfile_execution_required;
    return true;
}

bool function_param_flow_readfile_write_required(const FunctionParamFlowIndexClaims *claims,
    size_t ordinal, bool *required)
{
    size_t count = 0, unused = SIZE_MAX;
    return required != NULL && readfile_write_projection(claims, NULL, &count, &unused,
        ordinal, required, NULL) && ordinal < count;
}

bool function_param_flow_readfile_execution_counts(const FunctionParamFlowIndexClaims *claims,
    size_t *required_calls, size_t *required_writes)
{
    if (required_calls == NULL || required_writes == NULL) return false;
    *required_calls = *required_writes = 0;
    size_t count = 0, unused = SIZE_MAX;
    if (!function_param_flow_readfile_claim_count(claims, &count)) return false;
    for (size_t next = claims->readfile_first_plus_one; next != 0; next = claims->rows[next - 1].readfile_next_plus_one)
        if (claims->rows[next - 1].readfile_execution_required) (*required_calls)++;
    return readfile_write_projection(claims, NULL, &count, &unused, SIZE_MAX, NULL, required_writes);
}
