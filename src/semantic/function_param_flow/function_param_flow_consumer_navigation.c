#include "function_param_flow_summary_internal.h"
#include "../../parser/ast_analysis.h"
#include "function_param_publication_owner.h"
#include "function_param_publication_internal.h"
#include "function_param_flow_loan_order.h"
#include "../type_checker/type_checker_builtins_stdlib_collections_internal.h"
#include "../type_checker/type_checker_ownership_internal.h"
#include "../type_checker/type_checker_resolution_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Frame creation and fixed-point execution share the control-root owner. */

typedef struct FunctionParamNonlocalRead {
    ASTNode *routine;
    size_t ordinal;
    bool direct_only;
    bool saw_private_array_push;
    const struct FunctionParamNonlocalRead *previous;
} FunctionParamNonlocalRead;

static bool function_param_flow_nonlocal_read(SemanticContext *ctx, ASTNode *routine,
    size_t ordinal, const FunctionParamNonlocalRead *previous);

/* This is a root non-injection proof, not a storage, element or source0 grant.
 * Mutable copies are refused rather than reconstructed as an alias analysis. */
static bool function_param_flow_nonlocal_call(SemanticContext *ctx, ASTNode *routine,
    ASTNode *call, ASTNode *argument, FunctionParamNonlocalRead *active)
{
    if (!semantic_param_publication_checked_call(ctx, call)) return false;
    if (active->direct_only &&
        semantic_param_publication_value(ctx, call).type == TYPE_STRING) return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, argument);
    bool leaf = value.ok && !value.meta && (type_equals(value.type, TYPE_STRING) ||
        semantic_param_publication_value_is_physical_scalar(ctx, argument));
    ASTNode *receiver = NULL;
    ASTNode *selected = semantic_param_publication_direct_callee(ctx, call);
    if (selected != NULL && !semantic_param_publication_instance_receiver(ctx, call, &receiver)) return false;
    if (receiver == argument) {
        ASTNode *callee = semantic_param_publication_direct_callee(ctx, call);
        return callee != NULL && function_param_flow_navigation_result_shape(ctx, call, NULL) &&
            function_param_flow_nonlocal_read(ctx, callee, 0, active);
    }
    for (size_t a = 0; a < ast_call_arg_count(call); a++) {
        if (ast_call_argument(call, a) != argument) continue;
        if (!semantic_param_publication_checked_argument(ctx, call, a)) return false;
        if (a == 1 && leaf && semantic_param_publication_private_array_push(ctx, call, routine)) {
            if (active->direct_only) return false;
            active->saw_private_array_push = true;
            return true;
        }
        ASTNode *target = ast_call_callee(call);
        if (a == 0 && ast_call_arg_count(call) == 1 && target != NULL &&
            target->type == AST_IDENTIFIER &&
            ast_identifier_name(target) != NULL && strcmp(ast_identifier_name(target), "ArrayLength") == 0 &&
            semantic_stdlib_array_argument_preserves_elements(call, a, ctx)) return true;
        if (leaf && type_equals(value.type, TYPE_STRING) &&
            semantic_param_publication_string_argument_read_only(ctx, call, a)) return true;
        ASTNode *callee = semantic_param_publication_direct_callee(ctx, call);
        size_t formal = SIZE_MAX;
        bool independent_array = false;
        if (callee == NULL || !function_param_flow_navigation_result_shape(ctx, call, &independent_array) ||
            !semantic_param_publication_formal_for_actual(ctx, call, a, &formal)) return false;
        if (independent_array && semantic_param_publication_value_is_physical_scalar(ctx, argument))
            return function_param_flow_nonlocal_physical_input_current(ctx, call, a);
        return function_param_flow_nonlocal_read(ctx, callee, formal, active);
    }
    return false;
}

static bool function_param_flow_nonlocal_read(SemanticContext *ctx, ASTNode *routine,
    size_t ordinal, const FunctionParamNonlocalRead *previous)
{
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (routine == NULL || routine->type != AST_FUNC_DECL ||
        !semantic_param_publication_routine_inputs_current(ctx, routine, true)) return false;
    FuncParam *formal = ast_func_param(routine, ordinal);
    uint32_t binding = formal == NULL ? 0 : ast_func_param_stable_id(formal);
    if (binding == 0) return false;
    for (const FunctionParamNonlocalRead *p = previous; p != NULL; p = p->previous)
        if (p->routine == routine && p->ordinal == ordinal) return false;
    bool complete = false;
    size_t entry_at = SIZE_MAX;
    for (size_t e = 0; e < store->count; e++) {
        const FunctionParamFlowSummaryEntry *entry = &store->entries[e];
        if (entry->function_decl == routine && entry->function_id == ast_node_stable_id(routine) &&
            entry->param_index == ordinal) {
            complete = entry->state == FUNCTION_PARAM_FLOW_COMPLETE && !entry->analysis_unsupported &&
                (entry->mask & ~(SLOT_PARAM_SUMMARY_READ |
                    SLOT_PARAM_SUMMARY_RETURN_ESCAPE |
                    SLOT_PARAM_SUMMARY_CALL_ESCAPE |
                    SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) == 0;
            entry_at = e;
            break;
        }
    }
    unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
    if (!complete ||
        !function_param_flow_entry_outward_effects_current(store, entry_at,
            &outward_effects) ||
        (outward_effects & (SLOT_PARAM_SUMMARY_CALL_ESCAPE |
            SLOT_PARAM_SUMMARY_CHANNEL_ESCAPE)) != 0 ||
        !function_param_flow_no_store_graph(store, routine)) return false;
    FunctionParamFlowProgramPointIndex *index = NULL;
    for (size_t i = 0; i < store->program_point_index_count; i++)
        if (store->program_point_indexes[i].function_decl == routine &&
            store->program_point_indexes[i].function_id == ast_node_stable_id(routine))
            index = &store->program_point_indexes[i];
    if (index == NULL || !function_param_flow_index_roots_current(index) ||
        !function_param_flow_index_claims_current(ctx, index)) return false;
    /* Alias provenance remains owned by the completed normal backing frame.
     * Without it, no local String copy can be declared unrelated to this input. */
    if (!function_param_flow_backing_rows_current(store, routine)) return false;
    index = NULL;
    for (size_t i = 0; i < store->program_point_index_count; i++)
        if (store->program_point_indexes[i].function_decl == routine)
            index = &store->program_point_indexes[i];
    if (index == NULL) return false;
    const FunctionParamFlowIndexClaims *claims = index->claims;
    const FunctionParamFlowBackingFact *frame = index->backing_frames != NULL &&
        ordinal < index->backing_frame_count ? index->backing_frames[ordinal] : NULL;
    bool direct_only = frame == NULL;
    if (direct_only && (entry_at >= store->count ||
        !function_param_flow_physical_result_header_current(ctx, routine) ||
        !function_param_flow_output_current(store, entry_at, false))) return false;
    if (frame != NULL && (frame->epoch == 0 || !frame->complete || frame->building || frame->pending || frame->analysis_unsupported ||
        frame->input_ordinal != ordinal || frame->input_id != binding || frame->claims != claims ||
        frame->origin_count != 1 || frame->origin_words != 1)) return false;
    FunctionParamNonlocalRead active = {routine, ordinal, direct_only, false, previous};
    for (size_t p = 0; p < claims->count; p++) {
        const FunctionParamFlowIndexClaim *parent = &claims->rows[p];
        if (direct_only && parent->private_array_push) return false;
        if (direct_only && parent->shape == AST_CALL && parent->value_type == TYPE_STRING) return false;
        if (direct_only && (parent->shape == AST_LET_DECL || parent->shape == AST_ASSIGNMENT)) {
            ASTNode *value = parent->shape == AST_LET_DECL ? ast_let_initializer(parent->node) :
                ast_assignment_value(parent->node);
            if (semantic_param_publication_value(ctx, value).type == TYPE_STRING) return false;
        }
        for (size_t c = 0; c < parent->child_count; c++) {
            ASTNode *child = parent->children[c], *caller = NULL, *root = NULL;
            if (child == NULL || (child->type != AST_IDENTIFIER && child->type != AST_MEMBER_ACCESS &&
                child->type != AST_ARRAY_ACCESS)) continue;
            bool path = semantic_param_publication_projection_root_current(ctx, child, &caller, &root);
            if (!path) {
                if (child->type == AST_IDENTIFIER && ast_identifier_binding_syntax_id(child) == binding)
                    return false;
                continue;
            }
            uint32_t root_binding = ast_identifier_binding_syntax_id(root);
            if (caller != routine) {
                if (root_binding == binding) return false;
                continue;
            }
            bool related = root_binding == binding;
            SemanticPublishedValue root_value = semantic_param_publication_value(ctx, root);
            if (!related && frame != NULL && root_value.ok && !root_value.meta &&
                (type_equals(root_value.type, TYPE_STRING) ||
                 semantic_param_publication_value_is_physical_scalar(ctx, root)))
                for (size_t b = 0; b < frame->binding_count; b++)
                    if (frame->bindings[b].binding == root_binding && frame->bindings[b].origins != NULL &&
                        (frame->bindings[b].origins[0] & UINT64_C(1)) != 0) related = true;
            if (!related) continue;
            SemanticPublishedValue value = semantic_param_publication_value(ctx, child);
            bool leaf = value.ok && !value.meta && (type_equals(value.type, TYPE_STRING) ||
                semantic_param_publication_value_is_physical_scalar(ctx, child));
            ASTNode *node = parent->node;
            if ((node->type == AST_MEMBER_ACCESS && ast_member_object(node) == child) ||
                (node->type == AST_ARRAY_ACCESS && ast_array_access_array(node) == child)) {
                ASTNode *next_caller = NULL, *next_root = NULL;
                if (!semantic_param_publication_projection_root_current(ctx, node, &next_caller, &next_root) ||
                    next_caller != routine || next_root != root) return false;
                continue;
            }
            if (node->type == AST_CALL) {
                if (!function_param_flow_nonlocal_call(ctx, routine, node, child, &active)) return false;
                continue;
            }
            if (!leaf) return false;
            switch (node->type) {
            case AST_LET_DECL:
                if (ast_let_is_alias(node)) return false;
                break;
            case AST_ASSIGNMENT:
                if (ast_assignment_target(node) == child) return false;
                break;
            case AST_UNARY:
                if (ast_unary_operator(node).type != TOKEN_NOT &&
                    ast_unary_operator(node).type != TOKEN_MINUS &&
                    ast_unary_operator(node).type != TOKEN_PLUS) return false;
                break;
            case AST_RETURN: case AST_BINARY: case AST_CAST: case AST_TYPE_TEST:
            case AST_IF_STMT: case AST_WHILE_LOOP: case AST_FOR_LOOP:
            case AST_ARRAY_ACCESS: case AST_ARRAY_LITERAL: case AST_TUPLE_LITERAL:
                break;
            default: return false;
            }
        }
    }
    if (active.saw_private_array_push && (frame == NULL || !frame->private_stores_closed)) return false;
    if (!function_param_flow_no_store_graph(store, routine)) return false;
    index = NULL;
    for (size_t i = 0; i < store->program_point_index_count; i++)
        if (store->program_point_indexes[i].function_decl == routine)
            index = &store->program_point_indexes[i];
    return index != NULL && function_param_flow_index_claims_current(ctx, index);
}

bool function_param_flow_nonlocal_input_current(SemanticContext *ctx,
    ASTNode *routine, size_t formal_ordinal)
{
    return ctx != NULL && !ctx->has_error && semantic_param_publication_inputs_ready(ctx) &&
        ctx->function_param_flow_summaries != NULL && !ctx->function_param_flow_summaries->failed &&
        !ctx->function_param_flow_summaries->solving &&
        function_param_flow_nonlocal_read(ctx, routine, formal_ordinal, NULL);
}

bool function_param_flow_place_path_current(SemanticContext *ctx,
    ASTNode *routine, const ASTNode *place, uint32_t binding,
    FunctionParamFlowResultPath *path)
{
    if (path != NULL) memset(path, 0, sizeof(*path));
    if (ctx == NULL || routine == NULL || place == NULL || binding == 0 ||
        path == NULL) return false;
    const ASTNode *at = place;
    uint32_t reverse[FUNCTION_PARAM_FLOW_RESULT_PATH_LIMIT];
    size_t count = 0;
    while (at != NULL && at->type == AST_MEMBER_ACCESS) {
        ASTNode *published_caller = NULL, *receiver = NULL, *root = NULL;
        SemanticPublishedSelectedField selected = {0};
        if (count == FUNCTION_PARAM_FLOW_RESULT_PATH_LIMIT ||
            !semantic_param_publication_member_projection_field_current(ctx,
                at, &published_caller, &receiver, &root, &selected) ||
            published_caller != routine || selected.field_id == 0 ||
            receiver == NULL || root == NULL) return false;
        reverse[count++] = selected.field_id;
        at = receiver;
    }
    if (at == NULL || at->type != AST_IDENTIFIER ||
        semantic_param_publication_binding(ctx, at) != binding) return false;
    ASTNode *published_caller = NULL, *root = NULL;
    if (!semantic_param_publication_projection_root_current(ctx, place,
            &published_caller, &root) || published_caller != routine ||
        root != at) return false;
    path->present = true;
    path->count = count;
    for (size_t i = 0; i < count; i++)
        path->fields[i] = reverse[count - i - 1];
    return true;
}

bool function_param_flow_value_backing_formal_current(SemanticContext *ctx,
    ASTNode *routine, const ASTNode *value, size_t *formal_ordinal)
{
    if (formal_ordinal != NULL) *formal_ordinal = SIZE_MAX;
    /* A prior diagnostic does not retire the sealed typed generation. Every
     * frame, claim and identity below must still replay as current. */
    if (ctx == NULL || routine == NULL ||
        routine->type != AST_FUNC_DECL || value == NULL ||
        formal_ordinal == NULL ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_routine_inputs_current(ctx, routine, true))
        return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving ||
        store->validating_current ||
        !function_param_flow_backing_rows_current(store, routine) ||
        !function_param_flow_program_point_index_build(store, routine, &index) ||
        index == NULL)
        return false;
    ASTNode *value_caller = NULL, *value_root = NULL;
    uint32_t binding = 0;
    if (semantic_param_publication_projection_root_current(ctx, value,
            &value_caller, &value_root) && value_caller == routine &&
        value_root != NULL && value_root->type == AST_IDENTIFIER)
        binding = semantic_param_publication_binding(ctx, value_root);
    if (binding == 0) return false;
    bool local = false;
    for (size_t i = 0; i < index->local_count; i++)
        if (index->local_ids[i] == binding && index->local_definitions[i] != NULL &&
            ast_node_stable_id(index->local_definitions[i]) == binding) {
            if (local) return false;
            local = true;
        }
    if (!local) return false;
    bool matched = false;
    for (size_t p = 0; p < ast_func_param_count(routine); p++) {
        FuncParam *formal = ast_func_param(routine, p);
        if (formal == NULL ||
            index->backing_frames == NULL || p >= index->backing_frame_count)
            continue;
        const FunctionParamFlowBackingFact *frame = index->backing_frames[p];
        if (frame == NULL) continue;
        if (frame->epoch == 0 || !frame->complete || frame->building ||
            frame->pending || frame->analysis_unsupported ||
            frame->input_ordinal != p || frame->input_id == 0 ||
            frame->input_id != ast_func_param_stable_id(formal) ||
            frame->origin_count != 1 || frame->origin_words != 1 ||
            frame->claims != index->claims)
            return false;

        if (value->type == AST_IDENTIFIER) {
            const FunctionParamFlowBackingBinding *found = NULL;
            for (size_t b = 0; b < frame->binding_count; b++)
                if (frame->bindings[b].binding == binding) {
                    if (found != NULL) return false;
                    found = &frame->bindings[b];
                }
            if (found != NULL && found->local && found->origins != NULL &&
                (found->origins[0] & UINT64_C(1)) != 0) {
                if (matched) return false;
                matched = true;
                *formal_ordinal = p;
            }
            continue;
        }

        uint32_t value_id = ast_node_stable_id(value);
        const FunctionParamFlowBackingNode *found = NULL;
        for (size_t n = 0; value_id != 0 && n < frame->node_count; n++)
            if (frame->nodes[n].node == value && frame->nodes[n].id == value_id) {
                if (found != NULL) return false;
                found = &frame->nodes[n];
            }
        bool carries = found != NULL && found->origins != NULL &&
            (found->origins[0] & UINT64_C(1)) != 0;
        if (found == NULL) {
            FunctionParamFlowResultPath carrier_path, value_path;
            bool carrier_current =
                function_param_flow_backing_binding_path_current(store,
                    routine, (FunctionParamFlowBackingFact *)frame, binding,
                    NULL, NULL, &carrier_path);
            bool value_current = function_param_flow_place_path_current(ctx,
                routine, value, binding, &value_path);
            carries = carrier_current && carrier_path.present && value_current &&
                value_path.present && value_path.count <= carrier_path.count;
            for (size_t i = 0; carries && i < value_path.count; i++)
                carries = value_path.fields[i] == carrier_path.fields[i];
        }
        if (carries) {
            if (matched) return false;
            matched = true;
            *formal_ordinal = p;
        }
    }
    return matched;
}

bool function_param_flow_value_borrows_ref_formal_current(SemanticContext *ctx,
    ASTNode *routine, const ASTNode *value)
{
    size_t formal_ordinal = SIZE_MAX;
    return function_param_flow_value_backing_formal_current(ctx, routine,
            value, &formal_ordinal) &&
        formal_ordinal < ast_func_param_count(routine) &&
        ast_func_param(routine, formal_ordinal) != NULL &&
        ast_func_param(routine, formal_ordinal)->mode == PARAM_MODE_REF;
}

const ASTNode *function_param_flow_local_ref_carrier_copy_source_current(
    SemanticContext *ctx, ASTNode *routine, const ASTNode *value)
{
    if (value == NULL || value->type != AST_IDENTIFIER ||
        !function_param_flow_value_borrows_ref_formal_current(
            ctx, routine, value))
        return NULL;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving ||
        store->validating_current ||
        !function_param_flow_program_point_index_build(store, routine, &index) ||
        index == NULL)
        return NULL;
    uint32_t binding = semantic_param_publication_binding(ctx, value);
    const ASTNode *definition = NULL;
    for (size_t i = 0; i < index->local_count; i++) {
        ASTNode *candidate = index->local_definitions[i];
        if (index->local_ids[i] != binding || candidate == NULL ||
            candidate->type != AST_LET_DECL ||
            ast_node_stable_id(candidate) != binding)
            continue;
        if (definition != NULL || ast_let_is_mutable(candidate) ||
            ast_let_is_alias(candidate))
            return NULL;
        definition = candidate;
    }
    return definition != NULL ? ast_let_initializer(definition) : NULL;
}

void function_param_flow_result_path_clear(FunctionParamFlowResultPath *path)
{
    if (path != NULL) memset(path, 0, sizeof(*path));
}

bool function_param_flow_result_path_valid(
    const FunctionParamFlowResultPath *path)
{
    if (path == NULL || path->count > FUNCTION_PARAM_FLOW_RESULT_PATH_LIMIT ||
        path->branch_count > FUNCTION_PARAM_FLOW_RESULT_BRANCH_LIMIT)
        return false;
    if (!path->present)
        return path->count == 0 && path->branch_count == 0;
    for (size_t i = 0; i < path->count; i++)
        if (path->fields[i] == 0) return false;
    for (size_t i = 0; i < path->branch_count; i++) {
        if (path->branch_fields[i] == 0 ||
            (i != 0 && path->branch_fields[i - 1] >=
                path->branch_fields[i]))
            return false;
    }
    return true;
}

bool function_param_flow_result_path_equal(const FunctionParamFlowResultPath *left,
    const FunctionParamFlowResultPath *right)
{
    if (left == NULL || right == NULL || left->present != right->present ||
        left->count != right->count ||
        left->branch_count != right->branch_count) return false;
    for (size_t i = 0; i < left->count; i++)
        if (left->fields[i] != right->fields[i]) return false;
    for (size_t i = 0; i < left->branch_count; i++)
        if (left->branch_fields[i] != right->branch_fields[i]) return false;
    return true;
}

/* Add one immediate subtree below the current prefix. The branch set is
 * canonical and sorted so fixed-point equality is independent of visit
 * order. If it exceeds the bounded representation, collapse to the whole
 * prefix: that loses precision but cannot hide an origin. */
static bool result_path_add_branch(FunctionParamFlowResultPath *path,
    uint32_t field)
{
    if (path == NULL || field == 0 || !path->present) return false;
    size_t at = 0;
    while (at < path->branch_count && path->branch_fields[at] < field) at++;
    if (at < path->branch_count && path->branch_fields[at] == field)
        return true;
    if (path->branch_count == FUNCTION_PARAM_FLOW_RESULT_BRANCH_LIMIT) {
        memset(path->branch_fields, 0, sizeof(path->branch_fields));
        path->branch_count = 0;
        return true;
    }
    if (at < path->branch_count)
        memmove(&path->branch_fields[at + 1], &path->branch_fields[at],
            (path->branch_count - at) * sizeof(*path->branch_fields));
    path->branch_fields[at] = field;
    path->branch_count++;
    return true;
}

static bool result_path_contributes_whole_prefix(
    const FunctionParamFlowResultPath *path, size_t prefix_count)
{
    return path->count == prefix_count && path->branch_count == 0;
}

static bool result_path_add_below_prefix(FunctionParamFlowResultPath *result,
    const FunctionParamFlowResultPath *source, size_t prefix_count)
{
    if (source->count > prefix_count)
        return result_path_add_branch(result, source->fields[prefix_count]);
    for (size_t i = 0; i < source->branch_count; i++) {
        if (!result_path_add_branch(result, source->branch_fields[i]))
            return false;
        if (result->branch_count == 0) return true;
    }
    return true;
}

/* Both projections contain the same origin. Preserve the exact common prefix
 * and the immediate sibling subtrees below it. This is the bounded union
 * needed by nominal results whose disjoint fields carry one input. */
bool function_param_flow_result_path_join(FunctionParamFlowResultPath *into,
    const FunctionParamFlowResultPath *from)
{
    if (into == NULL || !function_param_flow_result_path_valid(from) ||
        !function_param_flow_result_path_valid(into))
        return false;
    if (!from->present) return true;
    if (!into->present) {
        *into = *from;
        return true;
    }
    FunctionParamFlowResultPath left = *into;
    size_t common = left.count < from->count ? left.count : from->count;
    size_t at = 0;
    while (at < common && left.fields[at] == from->fields[at]) at++;
    common = at;

    function_param_flow_result_path_clear(into);
    into->present = true;
    into->count = common;
    if (common != 0)
        memcpy(into->fields, left.fields,
            common * sizeof(*into->fields));
    if (result_path_contributes_whole_prefix(&left, common) ||
        result_path_contributes_whole_prefix(from, common))
        return true;
    return result_path_add_below_prefix(into, &left, common) &&
        (into->branch_count == 0 ||
         result_path_add_below_prefix(into, from, common));
}

bool function_param_flow_result_path_prepend(uint32_t field,
    const FunctionParamFlowResultPath *inside,
    FunctionParamFlowResultPath *result)
{
    if (field == 0 || result == NULL ||
        !function_param_flow_result_path_valid(inside) || !inside->present ||
        inside->count >= FUNCTION_PARAM_FLOW_RESULT_PATH_LIMIT) return false;
    *result = *inside;
    if (inside->count != 0)
        memmove(&result->fields[1], result->fields,
            inside->count * sizeof(*result->fields));
    result->fields[0] = field;
    result->count = inside->count + 1;
    return true;
}

bool function_param_flow_result_path_select(
    const FunctionParamFlowResultPath *carrier,
    const FunctionParamFlowResultPath *selected,
    FunctionParamFlowResultPath *result, bool *carries)
{
    function_param_flow_result_path_clear(result);
    if (carries != NULL) *carries = false;
    if (result == NULL || carries == NULL ||
        !function_param_flow_result_path_valid(carrier) ||
        !function_param_flow_result_path_valid(selected) ||
        !carrier->present || !selected->present ||
        selected->branch_count != 0)
        return false;
    size_t common = carrier->count < selected->count
        ? carrier->count : selected->count;
    size_t at = 0;
    while (at < common && carrier->fields[at] == selected->fields[at]) at++;
    if (at != common) return true;
    if (selected->count > carrier->count && carrier->branch_count != 0) {
        uint32_t child = selected->fields[carrier->count];
        size_t branch = 0;
        while (branch < carrier->branch_count &&
            carrier->branch_fields[branch] < child) branch++;
        if (branch == carrier->branch_count ||
            carrier->branch_fields[branch] != child)
            return true;
        *carries = true;
        result->present = true;
        return true;
    }
    *carries = true;
    result->present = true;
    result->branch_count = carrier->branch_count;
    if (carrier->branch_count != 0)
        memcpy(result->branch_fields, carrier->branch_fields,
            carrier->branch_count * sizeof(*result->branch_fields));
    if (selected->count < carrier->count) {
        result->count = carrier->count - selected->count;
        memcpy(result->fields, &carrier->fields[selected->count],
            result->count * sizeof(*result->fields));
    }
    return true;
}

static bool backing_fact_value_has_origin(FunctionParamFlowSummaryStore *store,
    const FunctionParamFlowBackingFact *fact, const ASTNode *value, bool *has)
{
    if (has != NULL) *has = false;
    if (store == NULL || fact == NULL || value == NULL || has == NULL ||
        fact->origin_count != 1 || fact->origin_words != 1) return false;
    SemanticPublishedValue published = semantic_param_publication_value(
        store->ctx, value);
    if (value->type == AST_STRING || value->type == AST_NUMBER ||
        value->type == AST_BOOLEAN || (published.ok &&
        (published.meta || published.origin_ready ||
         semantic_param_publication_value_is_physical_scalar(store->ctx,
            value)))) return true;
    if (value->type == AST_IDENTIFIER) {
        uint32_t binding = semantic_param_publication_binding(store->ctx, value);
        const FunctionParamFlowBackingBinding *found = NULL;
        for (size_t b = 0; binding != 0 && b < fact->binding_count; b++) {
            if (fact->bindings[b].binding != binding) continue;
            if (found != NULL) return false;
            found = &fact->bindings[b];
        }
        if (found == NULL || found->origins == NULL) {
            if (published.ok && !published.meta && published.type != NULL &&
                semantic_classify_ownership_type(published.type, store->ctx) ==
                    OWNERSHIP_TYPE_COPY_ONLY) return true;
            return false;
        }
        *has = (found->origins[0] & UINT64_C(1)) != 0;
        return true;
    }
    uint32_t id = ast_node_stable_id(value);
    const FunctionParamFlowBackingNode *found = NULL;
    for (size_t n = 0; id != 0 && n < fact->node_count; n++) {
        if (fact->nodes[n].node != value || fact->nodes[n].id != id) continue;
        if (found != NULL) return false;
        found = &fact->nodes[n];
    }
    if (found == NULL || found->origins == NULL) return false;
    *has = (found->origins[0] & UINT64_C(1)) != 0;
    return true;
}

bool function_param_flow_backing_value_origin_current(
    FunctionParamFlowSummaryStore *store,
    const FunctionParamFlowBackingFact *fact, const ASTNode *value,
    bool *has_origin)
{
    return backing_fact_value_has_origin(store, fact, value, has_origin);
}

static bool backing_value_result_path(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, const FunctionParamFlowBackingFact *fact, const ASTNode *value,
    const ASTNode *seed, const FunctionParamFlowResultPath *seed_path,
    size_t depth, FunctionParamFlowResultPath *path);

static bool call_result_path(FunctionParamFlowSummaryStore *store,
    ASTNode *call, size_t actual_ordinal, FunctionParamFlowResultPath *path)
{
    function_param_flow_result_path_clear(path);
    if (store == NULL || store->failed || call == NULL || call->type != AST_CALL ||
        path == NULL || actual_ordinal >= ast_call_arg_count(call) ||
        !semantic_param_publication_checked_argument(store->ctx, call,
            actual_ordinal)) return false;
    ASTNode *callee = semantic_param_publication_callee(store->ctx, call,
        actual_ordinal);
    size_t formal = SIZE_MAX;
    if (callee == NULL || !semantic_param_publication_formal_for_actual(
            store->ctx, call, actual_ordinal, &formal) ||
        formal >= ast_func_param_count(callee))
        return false;
    size_t entry = function_param_flow_find(store, ast_node_stable_id(callee),
        formal);
    if (store->solving && entry != SIZE_MAX &&
        (store->entries[entry].state == FUNCTION_PARAM_FLOW_EVALUATED ||
         store->entries[entry].state == FUNCTION_PARAM_FLOW_COMPLETE)) {
        /* backing_origin already demanded this call edge in the same frame.
         * Register this projection as another read of that admitted entry,
         * without replaying its whole currentness graph a second time. */
        if (!function_param_flow_closure_read(store, entry)) return false;
    } else {
        SlotFunctionLookup lookup = {store->ctx, store->program_root};
        (void)function_param_flow_summary_for_call(&lookup, call,
            actual_ordinal);
        entry = function_param_flow_find(store, ast_node_stable_id(callee),
            formal);
    }
    if (store->failed || entry == SIZE_MAX ||
        (store->entries[entry].state != FUNCTION_PARAM_FLOW_EVALUATED &&
         store->entries[entry].state != FUNCTION_PARAM_FLOW_COMPLETE) ||
        store->entries[entry].returned_backing_unproved ||
        store->entries[entry].returned_backing_pending)
        return false;
    unsigned outward_effects = SLOT_PARAM_SUMMARY_NONE;
    if (!function_param_flow_entry_outward_effects_current(store, entry,
            &outward_effects))
        return false;
    if ((outward_effects & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0) return true;

    if (entry == SIZE_MAX || store->entries[entry].function_decl != callee ||
        store->entries[entry].analysis_unsupported ||
        store->entries[entry].returned_backing_unproved ||
        store->entries[entry].returned_backing_pending ||
        !store->entries[entry].output_dependencies_ready ||
        !store->entries[entry].result_path_ready ||
        !store->entries[entry].result_path.present)
        return false;
    *path = store->entries[entry].result_path;
    return true;
}

static bool backing_identifier_result_path(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, const FunctionParamFlowBackingFact *fact,
    const ASTNode *value, const ASTNode *seed,
    const FunctionParamFlowResultPath *seed_path, size_t depth,
    FunctionParamFlowResultPath *path)
{
    uint32_t binding = semantic_param_publication_binding(store->ctx, value);
    ASTNode *definition = NULL;
    for (size_t p = 0; binding != 0 && p < ast_func_param_count(decl); p++) {
        FuncParam *formal = ast_func_param(decl, p);
        if (formal != NULL && ast_func_param_stable_id(formal) == binding) {
            path->present = true;
            return true;
        }
    }
    for (size_t c = 0; binding != 0 && c < fact->claims->count; c++) {
        const FunctionParamFlowIndexClaim *claim = &fact->claims->rows[c];
        if (claim->id == binding && claim->node != NULL &&
            claim->node->type == AST_LET_DECL) {
            if (definition != NULL) return false;
            definition = claim->node;
        }
        if (claim->node != NULL && claim->node->type == AST_ASSIGNMENT) {
            ASTNode *target = ast_assignment_target(claim->node);
            if (target != NULL && target->type == AST_IDENTIFIER &&
                semantic_param_publication_binding(store->ctx, target) == binding) {
                path->present = true;
                return true;
            }
        }
    }
    if (definition == NULL || ast_node_stable_id(definition) != binding) {
        path->present = true;
        return true;
    }
    ASTNode *initializer = ast_let_initializer(definition);
    return initializer != NULL && backing_value_result_path(store, decl, fact,
        initializer, seed, seed_path, depth + 1, path);
}

static bool backing_value_result_path(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, const FunctionParamFlowBackingFact *fact, const ASTNode *value,
    const ASTNode *seed, const FunctionParamFlowResultPath *seed_path,
    size_t depth, FunctionParamFlowResultPath *path)
{
    function_param_flow_result_path_clear(path);
    size_t bound = fact == NULL || store == NULL ? 0 :
        fact->node_count + fact->binding_count + store->count + 1;
    if (store == NULL || decl == NULL || fact == NULL || value == NULL ||
        path == NULL || bound < fact->node_count || depth > bound) return false;
    bool carries = false;
    if (!backing_fact_value_has_origin(store, fact, value, &carries)) return false;
    if (!carries) return true;
    if (value == seed) {
        if (!function_param_flow_result_path_valid(seed_path) ||
            !seed_path->present) return false;
        *path = *seed_path;
        return true;
    }
    if (value->type == AST_IDENTIFIER)
        return backing_identifier_result_path(store, decl, fact, value, seed,
            seed_path, depth, path);
    if (value->type == AST_CAST)
        return backing_value_result_path(store, decl, fact,
            ast_cast_operand((ASTNode *)value), seed, seed_path, depth + 1,
            path);
    if (value->type != AST_CALL) {
        /* A selector, array/tuple aggregate or unsupported value still has a
         * safe enclosure: its whole value. It grants no sibling distinction. */
        path->present = true;
        return true;
    }

    PublishedNode *published = publication_result_current(store->ctx, value);
    const SemanticNominalResultSnapshot *nominal = published != NULL
        ? published->nominal_result : NULL;
    if (nominal != NULL && semantic_nominal_result_snapshot_current(store->ctx,
            value, nominal) && nominal->field_count == ast_call_arg_count(value)) {
        for (size_t a = 0; a < nominal->field_count; a++) {
            ASTNode *argument = ast_call_argument(value, a);
            bool argument_carries = false;
            if (!backing_fact_value_has_origin(store, fact, argument,
                    &argument_carries)) return false;
            if (!argument_carries) continue;
            FunctionParamFlowResultPath inside, selected;
            if (!backing_value_result_path(store, decl, fact, argument, seed,
                    seed_path, depth + 1, &inside) || !inside.present ||
                !function_param_flow_result_path_prepend(
                    nominal->fields[a].field_id, &inside, &selected) ||
                !function_param_flow_result_path_join(path, &selected))
                return false;
        }
        if (path->present) return true;
    }

    ASTNode *direct = semantic_param_publication_direct_callee(store->ctx,
        value);
    if (direct != NULL) {
        for (size_t a = 0; a < ast_call_arg_count(value); a++) {
            ASTNode *argument = ast_call_argument(value, a);
            bool argument_carries = false;
            if (!backing_fact_value_has_origin(store, fact, argument,
                    &argument_carries)) return false;
            if (!argument_carries) continue;
            FunctionParamFlowResultPath selected;
            if (!call_result_path(store, (ASTNode *)value, a, &selected))
                return false;
            if (selected.present &&
                !function_param_flow_result_path_join(path, &selected))
                return false;
        }
        if (path->present) return true;
    }

    /* The frame says this call carries the origin, but no current exact result
     * mapping was available. Whole-result enclosure preserves fail-closed
     * behavior without inventing a field. */
    path->present = true;
    path->count = 0;
    return true;
}

bool function_param_flow_call_result_path_current(FunctionParamFlowSummaryStore *store,
    ASTNode *call, size_t actual_ordinal, FunctionParamFlowResultPath *path)
{
    return call_result_path(store, call, actual_ordinal, path);
}

static bool call_result_claim_rows_current(
    FunctionParamFlowSummaryStore *store, ASTNode *caller, ASTNode *call,
    FunctionParamFlowIndexClaims **claims_out, size_t *call_row_out,
    size_t *parent_row_out)
{
    if (store == NULL || store->failed || caller == NULL ||
        caller->type != AST_FUNC_DECL || call == NULL ||
        call->type != AST_CALL || claims_out == NULL || call_row_out == NULL ||
        parent_row_out == NULL || ast_node_stable_id(caller) == 0 ||
        ast_node_stable_id(call) == 0)
        return false;
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (!function_param_flow_program_point_index_build(store, caller,
            &index) || index == NULL || index->function_decl != caller ||
        index->function_id != ast_node_stable_id(caller) ||
        index->claims == NULL)
        return false;
    FunctionParamFlowIndexClaims *claims = index->claims;
    size_t call_row = SIZE_MAX;
    for (size_t i = 0; i < claims->count; i++) {
        if (claims->rows[i].node != call) continue;
        if (call_row != SIZE_MAX ||
            claims->rows[i].id != ast_node_stable_id(call) ||
            claims->rows[i].shape != AST_CALL)
            return false;
        call_row = i;
    }
    if (call_row == SIZE_MAX) return false;
    size_t parent_row = function_param_flow_claim_parent_row(claims,
        call_row);
    if (parent_row >= claims->count) return false;
    *claims_out = claims;
    *call_row_out = call_row;
    *parent_row_out = parent_row;
    return true;
}

bool function_param_flow_call_result_consumed_current(
    FunctionParamFlowSummaryStore *store, ASTNode *caller, ASTNode *call)
{
    FunctionParamFlowIndexClaims *claims = NULL;
    size_t call_row = SIZE_MAX, parent_row = SIZE_MAX;
    if (!call_result_claim_rows_current(store, caller, call, &claims,
            &call_row, &parent_row))
        return false;
    (void)call_row;
    const FunctionParamFlowIndexClaim *parent = &claims->rows[parent_row];
    return parent->node != NULL && parent->shape == parent->node->type &&
        parent->shape != AST_BLOCK;
}

bool function_param_flow_call_result_argument_consumer_current(
    FunctionParamFlowSummaryStore *store, ASTNode *caller, ASTNode *call,
    ASTNode **consumer, size_t *actual_ordinal)
{
    if (consumer == NULL || actual_ordinal == NULL) return false;
    *consumer = NULL;
    *actual_ordinal = SIZE_MAX;
    FunctionParamFlowIndexClaims *claims = NULL;
    size_t call_row = SIZE_MAX, parent_row = SIZE_MAX;
    if (!call_result_claim_rows_current(store, caller, call, &claims,
            &call_row, &parent_row))
        return false;
    (void)call_row;
    const FunctionParamFlowIndexClaim *parent = &claims->rows[parent_row];
    ASTNode *outer = parent->node;
    if (outer == NULL || parent->shape != AST_CALL || outer->type != AST_CALL ||
        parent->id == 0 || parent->id != ast_node_stable_id(outer))
        return false;
    size_t found = SIZE_MAX;
    for (size_t a = 0; a < ast_call_arg_count(outer); a++) {
        if (ast_call_argument(outer, a) != call) continue;
        if (found != SIZE_MAX) return false;
        found = a;
    }
    if (found == SIZE_MAX ||
        !semantic_param_publication_checked_argument(store->ctx, outer, found))
        return false;
    *consumer = outer;
    *actual_ordinal = found;
    return true;
}

bool function_param_flow_call_result_restores_actual_current(
    FunctionParamFlowSummaryStore *store, ASTNode *caller, ASTNode *call,
    size_t actual_ordinal)
{
    if (store == NULL || caller == NULL || call == NULL ||
        call->type != AST_CALL || actual_ordinal >= ast_call_arg_count(call))
        return false;
    FunctionParamFlowIndexClaims *claims = NULL;
    size_t call_row = SIZE_MAX, parent_row = SIZE_MAX;
    if (!call_result_claim_rows_current(store, caller, call, &claims,
            &call_row, &parent_row))
        return false;
    (void)call_row;
    const FunctionParamFlowIndexClaim *parent = &claims->rows[parent_row];
    ASTNode *assignment = parent->node;
    if (assignment == NULL || parent->shape != AST_ASSIGNMENT ||
        assignment->type != AST_ASSIGNMENT ||
        ast_assignment_value(assignment) != call)
        return false;

    ASTNode *actual = ast_call_argument(call, actual_ordinal);
    ASTNode *target = ast_assignment_target(assignment);
    const ASTNode *actual_root = NULL;
    const ASTNode *target_root = NULL;
    if (actual == NULL || target == NULL ||
        !function_param_flow_loan_storage_root(store, claims, caller,
            actual, &actual_root) ||
        !function_param_flow_loan_storage_root(store, claims, caller,
            target, &target_root) ||
        actual_root == NULL || target_root == NULL)
        return false;
    uint32_t actual_binding = semantic_param_publication_binding(
        store->ctx, actual_root);
    if (actual_binding == 0 || actual_binding !=
            semantic_param_publication_binding(store->ctx, target_root))
        return false;

    FunctionParamFlowResultPath actual_path = {0};
    FunctionParamFlowResultPath target_path = {0};
    FunctionParamFlowResultPath returned_path = {0};
    bool actual_path_current = function_param_flow_place_path_current(
        store->ctx, caller, actual, actual_binding, &actual_path);
    bool target_path_current = function_param_flow_place_path_current(
        store->ctx, caller, target, actual_binding, &target_path);
    bool result_path_current = function_param_flow_call_result_path_current(
        store, call, actual_ordinal, &returned_path);
    return actual_path_current && target_path_current &&
        function_param_flow_result_path_equal(&actual_path, &target_path) &&
        result_path_current && returned_path.present &&
        returned_path.count == 0;
}

bool function_param_flow_backing_binding_path_current(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, FunctionParamFlowBackingFact *fact, uint32_t binding,
    const ASTNode *seed, const FunctionParamFlowResultPath *seed_path,
    FunctionParamFlowResultPath *path)
{
    function_param_flow_result_path_clear(path);
    if (store == NULL || store->failed || decl == NULL || fact == NULL ||
        binding == 0 || path == NULL || fact->claims == NULL ||
        fact->origin_count != 1 || fact->origin_words != 1 ||
        !fact->complete || fact->pending || fact->analysis_unsupported ||
        !function_param_flow_backing_rows_current(store, decl)) return false;
    const FunctionParamFlowBackingBinding *carrier = NULL;
    for (size_t b = 0; b < fact->binding_count; b++) {
        if (fact->bindings[b].binding != binding) continue;
        if (carrier != NULL) return false;
        carrier = &fact->bindings[b];
    }
    if (carrier == NULL || carrier->origins == NULL ||
        (carrier->origins[0] & UINT64_C(1)) == 0) return false;
    if (seed == NULL) {
        if (seed_path != NULL || carrier->origin_path == NULL ||
            !carrier->origin_path->present ||
            !function_param_flow_result_path_valid(carrier->origin_path))
            return false;
        *path = *carrier->origin_path;
        return true;
    }
    /* The real defining identifier is not retained on a binding row. Resolve
     * through the current claim instead of fabricating publication identity. */
    ASTNode *definition = NULL;
    for (size_t c = 0; c < fact->claims->count; c++) {
        const FunctionParamFlowIndexClaim *claim = &fact->claims->rows[c];
        if (claim->id == binding && claim->node != NULL &&
            claim->node->type == AST_LET_DECL) {
            if (definition != NULL) return false;
            definition = claim->node;
        }
    }
    if (definition == NULL) {
        path->present = true;
        return true;
    }
    bool found_source = false;
    if (definition != NULL) {
        ASTNode *initializer = ast_let_initializer(definition);
        bool carries = false;
        if (initializer == NULL || !backing_fact_value_has_origin(store, fact,
                initializer, &carries)) return false;
        if (carries) {
            FunctionParamFlowResultPath source_path;
            if (!backing_value_result_path(store, decl, fact, initializer,
                    seed, seed_path, 0, &source_path) ||
                !source_path.present ||
                !function_param_flow_result_path_join(path, &source_path))
                return false;
            found_source = true;
        }
    }
    for (size_t c = 0; c < fact->claims->count; c++) {
        ASTNode *assignment = fact->claims->rows[c].node;
        if (assignment == NULL || assignment->type != AST_ASSIGNMENT) continue;
        ASTNode *target = ast_assignment_target(assignment);
        if (target == NULL || target->type != AST_IDENTIFIER ||
            semantic_param_publication_binding(store->ctx, target) != binding)
            continue;
        ASTNode *value = ast_assignment_value(assignment);
        bool carries = false;
        if (value == NULL || !backing_fact_value_has_origin(store, fact, value,
                &carries)) return false;
        if (!carries) continue;
        FunctionParamFlowResultPath source_path;
        if (!backing_value_result_path(store, decl, fact, value, seed,
                seed_path, 0, &source_path) || !source_path.present ||
            !function_param_flow_result_path_join(path, &source_path))
            return false;
        found_source = true;
    }
    if (found_source) return path->present;
    if (definition == NULL) {
        path->present = true;
        return true;
    }
    return false;
}

static bool returned_backing_effects(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, unsigned *effects, bool *outputs, size_t output_count, bool *pending,
    FunctionParamFlowResultPath *result_path, bool private_stores_closed,
    bool replay_recursive)
{
    function_param_flow_result_path_clear(result_path);
    if (store == NULL || store->failed || effects == NULL || pending == NULL || decl == NULL || outputs == NULL ||
        result_path == NULL || output_count != ast_func_param_count(decl) || ordinal >= output_count ||
        !semantic_param_publication_inputs_ready(store->ctx)) return false;
    FunctionParamFlowProgramPointIndex *index;
    if (!function_param_flow_program_point_index_build(store, decl, &index))
        return false;
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
        created->outward_effects = calloc(1, sizeof(unsigned));
        created->outputs = calloc(words, sizeof(uint64_t));
        if (created->effects == NULL || created->outward_effects == NULL ||
            created->outputs == NULL) {
            function_param_flow_fail(store, decl,
                "backing matrix allocation failed");
            return false;
        }
        ReturnedBacking seed = {.store = store, .decl = decl, .fact = created};
        for (size_t p = 0; p < output_count; p++) {
            FuncParam *param = ast_func_param(decl, p);
            FunctionParamFlowBackingBinding *row = backing_binding(&seed, ast_func_param_stable_id(param), true);
            if (row == NULL) return false;
            if (p == ordinal) {
                row->origins[0] = 1;
                row->origin_path = calloc(1, sizeof(*row->origin_path));
                if (row->origin_path == NULL) {
                    function_param_flow_fail(store, decl,
                        "backing input result path allocation failed");
                    return false;
                }
                row->origin_path->present = true;
            }
            if (param->mode == PARAM_MODE_REF || param->mode == PARAM_MODE_MUT_REF)
                row->destinations[p / 64] |= UINT64_C(1) << (p % 64);
        }
    }
    /* Heap-owned fact/root rows survive recursive index and entry growth. */
    FunctionParamFlowBackingFact *fact = index->backing_frames[ordinal];
    if (fact->input_ordinal != ordinal || fact->input_id == 0 ||
        fact->input_id != ast_func_param_stable_id(ast_func_param(decl, ordinal)) ||
        fact->origin_count != 1 || fact->origin_words != 1 ||
        fact->claims != index->claims ||
        !function_param_flow_backing_effect_facets_current(fact) ||
        fact->destination_count != output_count ||
        fact->destination_words != (output_count + 63) / 64) return false;
    /* Effects are monotone within one admission mode. Never reuse a precise
     * private-store evaluation for a query lacking its fresh graph witness,
     * nor pretend a previously conservative CALL mask can be subtracted. */
    if (fact->epoch != 0 && fact->private_stores_closed != private_stores_closed) return false;
    fact->private_stores_closed = private_stores_closed;
    FunctionParamFlowControlProjection *projection = index->control_projection;
    ReturnedBacking state = {.store = store, .lookup = {store->ctx, store->program_root}, .decl = decl,
        .fact = fact, .private_stores_closed = private_stores_closed};
    size_t evaluating = store->evaluating_plus_one == 0
        ? SIZE_MAX : store->evaluating_plus_one - 1;
    bool active_incomplete = store->solving && evaluating < store->count &&
        evaluating >= store->active_start &&
        store->entries[evaluating].function_decl == decl &&
        store->entries[evaluating].param_index == ordinal &&
        store->entries[evaluating].state == FUNCTION_PARAM_FLOW_COMPUTING &&
        fact->epoch != 0 && !fact->analysis_unsupported &&
        !fact->building && !fact->complete && !fact->pending;
    if (fact->epoch != 0 && !active_incomplete &&
        !function_param_flow_backing_frame_current(store, decl, ordinal))
        return false;
    if (replay_recursive && (!store->settling_recursive_backing ||
        fact->epoch == 0 || fact->building || fact->pending || !fact->complete ||
        fact->sealed_pending_relation_count != fact->pending_relation_count))
        return false;
    if (fact->building) { *pending = true; return false; }
    /* A physical scalar input carries no backing: it reaches no output and
     * has no escape, so its frame is complete without walking the body. */
    FuncParam *input = ast_func_param(decl, ordinal);
    const Type *input_type = input == NULL || input->type == NULL ? NULL :
        semantic_type_resolution_lookup_metadata_type_ref(store->ctx, input->type);
    if (fact->epoch == 0 && publication_physical_scalar(input_type)) {
        fact->epoch = 1; fact->complete = true; fact->pending = false; fact->changed = false;
        fact->result_path_ready = true;
        fact->evaluation_pass = store->fixed_point_passes + 1;
    }
    if (fact->epoch == 0 || replay_recursive || active_incomplete ||
        (fact->pending && fact->evaluation_pass != store->fixed_point_passes + 1)) {
        fact->building = true; fact->complete = true; fact->pending = false;
        fact->result_path_ready = false;
        if (!replay_recursive) {
            free(fact->pending_relations); fact->pending_relations = NULL;
            fact->pending_relation_count = 0;
            fact->sealed_pending_relation_count = 0;
        }
        fact->evaluation_pass = store->fixed_point_passes + 1;
        size_t rounds = 0;
        do {
            if (fact->epoch == SIZE_MAX) { function_param_flow_fail(store, decl, "backing iteration identity overflow"); fact->complete = false; break; }
            /* A round repeats only after a binding's origin or destination
             * bit grew, so a monotone frame stops within this many rounds. */
            if (rounds++ > fact->binding_count * (fact->origin_count + fact->destination_count)) {
                function_param_flow_fail(store, decl, "backing frame rounds exceeded its growth bound"); fact->complete = false; break;
            }
            fact->epoch++; fact->changed = false;
            function_param_flow_backing_control_roots(&state, projection);
        } while (fact->changed && fact->complete);
        fact->building = false;
        fact->result_path_ready = fact->complete && !fact->pending &&
            ((fact->outward_effects[0] & SLOT_PARAM_SUMMARY_RETURN_ESCAPE) == 0 ||
             fact->result_path.present);
    }
    *effects = fact->effects[0];
    for (size_t p = 0; p < output_count; p++)
        outputs[p] = (fact->outputs[p / 64] & (UINT64_C(1) << (p % 64))) != 0;
    *pending = fact->pending && fact->complete;
    *result_path = fact->result_path;
    return fact->complete && !fact->pending && fact->result_path_ready;
}

bool function_param_flow_returned_backing_effects(FunctionParamFlowSummaryStore *store,
    ASTNode *decl, size_t ordinal, unsigned *effects, bool *outputs,
    size_t output_count, bool *pending, FunctionParamFlowResultPath *result_path,
    bool private_stores_closed)
{
    return returned_backing_effects(store, decl, ordinal, effects,
        outputs, output_count, pending, result_path, private_stores_closed,
        false);
}

bool function_param_flow_replay_recursive_backing_effects(
    FunctionParamFlowSummaryStore *store, ASTNode *decl, size_t ordinal,
    unsigned *effects, bool *outputs, size_t output_count, bool *pending,
    FunctionParamFlowResultPath *result_path, bool private_stores_closed)
{
    return returned_backing_effects(store, decl, ordinal, effects, outputs,
        output_count, pending, result_path, private_stores_closed, true);
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
        if (begin > end || end > projection->step_count) {
            state->fact->complete = false; return;
        }
        for (size_t s = begin; s < end; s++) {
            if (state->fact->analysis_unsupported) return;
            if (projection->steps[s].node == NULL || projection->steps[s].syntax_id == 0 ||
                projection->steps[s].root_ordinal != r) {
                state->fact->complete = false; return;
            }
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

static bool index_claims_full_check(SemanticContext *ctx,
    const FunctionParamFlowProgramPointIndex *index)
{
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

/* Typed facts are sealed while the flow store lives, so a passed check holds
 * again until a claim row, the projection or the index's locals change. With
 * PGY_VERIFY_FLOW_CURRENCY_MEMO every reuse is checked in full as well, and
 * a divergence fails the store. */
bool function_param_flow_index_claims_current(SemanticContext *ctx,
    const FunctionParamFlowProgramPointIndex *index)
{
    static int verify = -1;
    if (index == NULL || index->claims == NULL) return false;
    if (verify < 0) verify = getenv("PGY_VERIFY_FLOW_CURRENCY_MEMO") != NULL;
    FunctionParamFlowIndexClaims *claims = index->claims;
    bool reused = claims->checked && !claims->failed && claims->checked_generation == claims->generation &&
        claims->checked_locals == index->local_count;
    if (reused && !verify) return true;
    bool current = index_claims_full_check(ctx, index);
    if (current && !function_param_flow_readfile_receipt_seal(claims)) {
        function_param_flow_fail(ctx->function_param_flow_summaries, index->function_decl,
            "readfile claim count receipt could not be sealed");
        current = false;
    }
    if (reused && !current)
        function_param_flow_fail(ctx->function_param_flow_summaries, index->function_decl,
            "index claims currency memo diverged from its full check");
    claims->checked = current;
    claims->checked_generation = claims->generation;
    claims->checked_locals = index->local_count;
    return current;
}
