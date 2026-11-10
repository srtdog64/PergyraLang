#include "indexed_string_field_lineage_internal.h"
#include "indexed_string_result_inputs.h"

#include <stdlib.h>
#include <string.h>

#include "function_param_publication_internal.h"
#include "type_checker_internal.h"

static bool field_id(const ASTNode *node, uint32_t id)
{ return node != NULL && id != 0 && ast_node_stable_id(node) == id; }

bool indexed_string_field_flow_syntax_current(const IndexedStringFieldRecipe *recipe)
{
    if (recipe->call_count == 0 || recipe->call_count > recipe->call_capacity ||
        recipe->sealed_calls != recipe->call_count ||
        recipe->calls == NULL) return false;
    for (size_t i = 0; i < recipe->call_count; i++) {
        if (!indexed_string_field_call_syntax(&recipe->calls[i])) return false;
        for (size_t j = 0; j < i; j++)
            if (recipe->calls[i].call == recipe->calls[j].call) return false;
    }
    return true;
}

void indexed_string_field_flow_destroy(IndexedStringFieldRecipe *recipe)
{
    for (size_t i = 0; i < recipe->call_count; i++) indexed_string_field_call_destroy(&recipe->calls[i]);
    free(recipe->calls);
}

bool indexed_string_field_flow_call_current(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe, ASTNode *call, bool prepare)
{
    if (call == NULL || call->type != AST_CALL ||
        !semantic_param_publication_checked_call(ctx, call)) return false;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    uint32_t kind = 0;
    bool builtin = decl == NULL && ast_call_semantic_callee_builtin_kind(call, &kind);
    if (decl == NULL && !builtin) return false;
    ASTNode *target = ast_call_callee(call);
    if (target == NULL || target->type != AST_IDENTIFIER ||
        (decl != NULL && (decl->is_async_decl || ast_func_body(decl) == NULL ||
            ast_call_arg_count(call) != ast_func_param_count(decl) ||
            !semantic_param_publication_routine_inputs_current(ctx, decl, true)))) return false;
    for (size_t i = 0; i < recipe->call_count; i++)
        if (recipe->calls[i].call == call) {
            IndexedStringFieldCall *row = &recipe->calls[i];
            if (!indexed_string_field_call_current(ctx, row) || row->builtin != builtin ||
                row->callee != decl || row->builtin_kind != (builtin ? kind : 0)) return false;
            return true;
        }
    if (!prepare || recipe->call_count == SIZE_MAX ||
        recipe->call_count + 1 > SIZE_MAX / sizeof(*recipe->calls)) return false;
    IndexedStringFieldCall row = {0};
    if (!indexed_string_field_call_capture(ctx, call, &row)) return false;
    IndexedStringFieldCall *calls = realloc(recipe->calls,
        (recipe->call_count + 1) * sizeof(*calls));
    if (calls == NULL) goto failed;
    recipe->calls = calls;
    recipe->calls[recipe->call_count++] = row;
    recipe->call_capacity = recipe->call_count;
    return true;
failed:
    indexed_string_field_call_destroy(&row);
    return false;
}

static bool field_unsafe_shape(ASTNodeType shape);

FunctionParamFlowProgramPointIndex *indexed_string_field_flow_index(SemanticContext *ctx,
    ASTNode *decl)
{
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving ||
        !semantic_param_publication_routine_inputs_current(ctx, decl, true) ||
        !function_param_flow_program_point_index_build(store, decl, &index) ||
        index == NULL || index->function_decl != decl ||
        index->function_id != ast_node_stable_id(decl) ||
        index->dependency_body != ast_func_body(decl) ||
        !field_id(index->dependency_body, index->dependency_body_id) ||
        (index->no_store_call_count != 0 && index->no_store_calls == NULL))
        return NULL;
    for (size_t i = 0; i < index->claims->count; i++)
        if (field_unsafe_shape(index->claims->rows[i].shape)) return NULL;
    return index;
}

bool indexed_string_field_flow_dependency(IndexedStringFieldRecipe *recipe,
    const FunctionParamFlowProgramPointIndex *index)
{
    if (index == NULL || index->claims == NULL) return false;
    for (size_t i = 0; i < recipe->dependency_count; i++) {
        IndexedStringFieldDependency *row = &recipe->dependencies[i];
        if (row->decl == index->function_decl)
            return row->decl_id == index->function_id &&
                row->body == index->dependency_body &&
                row->body_id == index->dependency_body_id &&
                row->claims == index->claims &&
                row->no_store_call_count == index->no_store_call_count;
    }
    if (recipe->dependency_count == SIZE_MAX ||
        recipe->dependency_count + 1 > SIZE_MAX / sizeof(*recipe->dependencies)) return false;
    size_t count = recipe->dependency_count + 1;
    IndexedStringFieldDependency *rows = realloc(recipe->dependencies,
        count * sizeof(*rows));
    if (rows == NULL) return false;
    recipe->dependencies = rows;
    rows[recipe->dependency_count] = (IndexedStringFieldDependency){
        index->function_decl, index->dependency_body, index->function_id,
        index->dependency_body_id, index->claims, index->no_store_call_count};
    recipe->dependency_count = count;
    recipe->dependency_capacity = count;
    return true;
}

static bool field_dependency_current(const IndexedStringFieldRecipe *recipe,
    const FunctionParamFlowProgramPointIndex *index)
{
    for (size_t i = 0; i < recipe->dependency_count; i++) {
        const IndexedStringFieldDependency *row = &recipe->dependencies[i];
        if (row->decl != index->function_decl) continue;
        return row->decl_id == index->function_id &&
            row->body == index->dependency_body &&
            row->body_id == index->dependency_body_id && row->claims == index->claims &&
            row->no_store_call_count == index->no_store_call_count;
    }
    return false;
}

/* Follow the already-owned no-store call edges, not routine AST roots. The
 * immutable recipe must cover every transitive claim that gave READ-only. */
bool indexed_string_field_flow_closure_current(SemanticContext *ctx,
    IndexedStringFieldRecipe *recipe, ASTNode *decl, bool prepare)
{
    ASTNode **queue = malloc(sizeof(*queue));
    if (queue == NULL) return false;
    queue[0] = decl;
    size_t count = 1;
    bool valid = true;
    for (size_t at = 0; at < count && valid; at++) {
        FunctionParamFlowProgramPointIndex *index = indexed_string_field_flow_index(ctx,
            queue[at]);
        if (index == NULL || (prepare ?
            !indexed_string_field_flow_dependency(recipe, index) :
            !field_dependency_current(recipe, index))) { valid = false; break; }
        for (size_t e = 0; e < index->no_store_call_count; e++) {
            ASTNode *call = index->no_store_calls[e];
            ASTNode *callee = semantic_param_publication_direct_callee(ctx, call);
            if (callee == NULL ||
                !indexed_string_field_flow_call_current(ctx, recipe, call, prepare)) {
                valid = false; break;
            }
            size_t seen = 0;
            while (seen < count && queue[seen] != callee) seen++;
            if (seen != count) continue;
            if (count == SIZE_MAX / sizeof(*queue)) { valid = false; break; }
            ASTNode **grown = realloc(queue, (count + 1) * sizeof(*grown));
            if (grown == NULL) { valid = false; break; }
            queue = grown;
            queue[count++] = callee;
        }
    }
    free(queue);
    return valid;
}

static bool field_selected_formal(SemanticContext *ctx, ASTNode *call,
    size_t actual, ASTNode **callee_out, size_t *formal_out)
{
    if (callee_out != NULL) *callee_out = NULL;
    if (formal_out != NULL) *formal_out = SIZE_MAX;
    if (!semantic_param_publication_checked_argument(ctx, call, actual) ||
        callee_out == NULL || formal_out == NULL) return false;
    ASTNode *decl = semantic_param_publication_direct_callee(ctx, call);
    size_t formal = SIZE_MAX;
    if (decl == NULL || decl->is_async_decl ||
        !semantic_param_publication_formal_for_actual(ctx, call, actual, &formal) ||
        formal >= ast_func_param_count(decl)) return false;
    FuncParam *param = ast_func_param(decl, formal);
    if (param == NULL || (param->mode != PARAM_MODE_DEFAULT &&
        param->mode != PARAM_MODE_REF)) return false;
    *callee_out = decl;
    *formal_out = formal;
    return true;
}

static bool field_readonly_actual(SemanticContext *ctx, ASTNode *call,
    size_t actual, IndexedStringFieldRecipe *recipe, bool prepare)
{
    ASTNode *decl = NULL;
    size_t formal = SIZE_MAX;
    if (!field_selected_formal(ctx, call, actual, &decl, &formal) ||
        !semantic_param_publication_routine_no_output_header_ready(ctx, decl)) return false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (store == NULL) return false;
    if (prepare) (void)function_param_flow_summary_demand(
        &(SlotFunctionLookup){.ctx = ctx, .program_root = ctx->program_root},
        decl, formal);
    size_t at = function_param_flow_find(store, ast_node_stable_id(decl), formal);
    if (at == SIZE_MAX) return false;
    const FunctionParamFlowSummaryEntry *entry = &store->entries[at];
    if (entry->function_decl != decl || entry->function_id != ast_node_stable_id(decl) ||
        entry->param_index != formal || entry->state != FUNCTION_PARAM_FLOW_COMPLETE ||
        entry->analysis_unsupported || entry->returned_backing_unproved ||
        entry->returned_backing_pending || (entry->mask & ~SLOT_PARAM_SUMMARY_READ) != 0 ||
        !entry->output_dependencies_ready || entry->output_dependencies == NULL ||
        entry->output_count != ast_func_param_count(decl)) return false;
    for (size_t i = 0; i < entry->output_count; i++)
        if (entry->output_dependencies[i]) return false;
    if (!function_param_flow_output_current(store, at, false) ||
        !function_param_flow_no_store_graph(store, decl) ||
        !indexed_string_field_flow_call_current(ctx, recipe, call, prepare) ||
        !indexed_string_field_flow_closure_current(ctx, recipe, decl, prepare)) return false;
    FunctionParamFlowProgramPointIndex *index = indexed_string_field_flow_index(ctx, decl);
    return index != NULL && (!prepare || indexed_string_field_flow_dependency(recipe, index));
}

static bool field_string_actual(SemanticContext *ctx, ASTNode *call,
    size_t actual, IndexedStringFieldRecipe *recipe, bool prepare)
{
    if (!semantic_param_publication_checked_argument(ctx, call, actual)) return false;
    ASTNode *decl = NULL;
    size_t formal = SIZE_MAX;
    if (field_selected_formal(ctx, call, actual, &decl, &formal)) {
        FunctionParamFlowProgramPointIndex *index = indexed_string_field_flow_index(ctx, decl);
        return index != NULL && indexed_string_field_flow_call_current(ctx, recipe,
            call, prepare) && (!prepare || indexed_string_field_flow_dependency(recipe, index));
    }
    return semantic_param_publication_string_argument_read_only(ctx, call, actual) &&
        indexed_string_field_flow_call_current(ctx, recipe, call, prepare);
}

static bool field_call_use(SemanticContext *ctx, ASTNode *call,
    const ASTNode *operand, IndexedStringFieldRecipe *recipe,
    bool string_value, bool prepare)
{
    if (call == NULL || call->type != AST_CALL) return false;
    size_t actual = SIZE_MAX;
    for (size_t i = 0; i < ast_call_arg_count(call); i++)
        if (ast_call_argument(call, i) == operand) {
            if (actual != SIZE_MAX) return false;
            actual = i;
        }
    if (actual == SIZE_MAX) return false;
    return string_value ? field_string_actual(ctx, call, actual, recipe, prepare) :
        field_readonly_actual(ctx, call, actual, recipe, prepare);
}

static bool field_unsafe_shape(ASTNodeType shape)
{
    return shape == AST_PARALLEL_BLOCK || shape == AST_ASYNC_BLOCK ||
        shape == AST_SPAWN_EXPR || shape == AST_AWAIT_EXPR ||
        shape == AST_LAMBDA_EXPR || shape == AST_FUNC_DECL ||
        shape == AST_DEFER_STMT || shape == AST_WITH_STMT ||
        shape == AST_UNSAFE_BLOCK || shape == AST_TRANSACTION_BLOCK ||
        shape == AST_CHANNEL_SEND || shape == AST_CHANNEL_RECV ||
        shape == AST_EVENT_SUBSCRIBE || shape == AST_EVENT_UNSUBSCRIBE ||
        shape == AST_EVENT_INVOKE;
}

/* Parentage comes from the owner's sealed child ordinals, never an AST walk. */
bool indexed_string_field_flow_uses_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, uint32_t binding,
    bool aggregate, IndexedStringFieldRecipe *recipe, bool prepare)
{
    if (claims == NULL || claims->rows == NULL || claims->failed || binding == 0 ||
        claims->count > SIZE_MAX / sizeof(size_t)) return false;
    size_t *parents = malloc(claims->count * sizeof(*parents));
    if (parents == NULL) return false;
    for (size_t i = 0; i < claims->count; i++) parents[i] = SIZE_MAX;
    bool valid = true, seen = false;
    for (size_t i = 0; i < claims->count && valid; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->child_count != 0 && row->child_claim_ordinals == NULL) {
            valid = false; break;
        }
        for (size_t c = 0; c < row->child_count; c++) {
            if (row->children[c] == NULL) continue;
            size_t child = row->child_claim_ordinals[c];
            if (child >= claims->count || claims->rows[child].node != row->children[c] ||
                (parents[child] != SIZE_MAX && parents[child] != i)) {
                valid = false; break;
            }
            parents[child] = i;
        }
    }
    for (size_t i = 0; i < claims->count && valid; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->shape != AST_IDENTIFIER || row->binding != binding) continue;
        seen = true;
        if (!row->snapshot_semantic_complete || !row->value_ready || row->value_meta ||
            parents[i] == SIZE_MAX) { valid = false; break; }
        const FunctionParamFlowIndexClaim *parent = &claims->rows[parents[i]];
        if (parent->shape == AST_MEMBER_ACCESS && aggregate &&
            ast_member_object(parent->node) == row->node) {
            ASTNode *caller = NULL, *root = NULL;
            if (!semantic_param_publication_projection_root_current(ctx,
                    parent->node, &caller, &root) ||
                caller != (recipe->caller) || root != row->node ||
                parents[parents[i]] == SIZE_MAX) { valid = false; break; }
            const FunctionParamFlowIndexClaim *outer = &claims->rows[parents[parents[i]]];
            SemanticPublishedValue value = semantic_param_publication_value(ctx, parent->node);
            if (!value.ok || value.meta || value.type == TYPE_UNKNOWN) {
                valid = false; break;
            }
            if (outer->shape == AST_CALL) {
                valid = field_call_use(ctx, outer->node, parent->node, recipe,
                    value.type == TYPE_STRING, prepare);
            } else {
                valid = (outer->shape == AST_BINARY || outer->shape == AST_UNARY) &&
                    publication_physical_scalar(value.type);
            }
        } else if (parent->shape == AST_CALL) {
            if (aggregate)
                valid = field_call_use(ctx, parent->node, row->node, recipe,
                    false, prepare);
            else {
                bool result_constructor = false;
                for (size_t r = 0; r < recipe->return_count; r++)
                    if (recipe->returns[r].constructor == parent->node &&
                        recipe->returns[r].source == row->node)
                        result_constructor = true;
                valid = result_constructor || field_call_use(ctx, parent->node,
                    row->node, recipe, true, prepare);
            }
        } else valid = false;
    }
    free(parents);
    return valid && seen;
}
