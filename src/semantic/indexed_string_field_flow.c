#include "indexed_string_field_lineage_internal.h"

#include <stdlib.h>
#include <string.h>

#include "function_param_publication_internal.h"
#include "type_checker_internal.h"

static bool field_id(const ASTNode *node, uint32_t id)
{ return node != NULL && id != 0 && ast_node_stable_id(node) == id; }

static char *field_copy(const char *value)
{
    if (value == NULL) return NULL;
    size_t size = strlen(value) + 1;
    char *copy = malloc(size);
    if (copy != NULL) memcpy(copy, value, size);
    return copy;
}

static bool field_text_same(const char *left, const char *right)
{ return (left == NULL && right == NULL) ||
    (left != NULL && right != NULL && strcmp(left, right) == 0); }

static void field_call_release(IndexedStringFieldCall *row)
{
    free(row->callee_name);
    free(row->result_syntax);
    for (size_t i = 0; row->formals != NULL && i < row->actual_count; i++) {
        free(row->formals[i].name);
        free(row->formals[i].type_syntax);
        free(row->formals[i].argument_name);
    }
    free(row->formals);
}

static bool field_call_syntax(const IndexedStringFieldCall *row)
{
    if (!field_id(row->call, row->call_id) ||
        !field_id(row->target, row->target_id) ||
        row->call->type != AST_CALL || ast_call_callee(row->call) != row->target ||
        row->target->type != AST_IDENTIFIER || row->target_shape != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(row->target) != row->target_binding_id ||
        ast_call_semantic_callee_is_stdlib(row->call) != row->stdlib ||
        ast_call_semantic_callee_declared_callable(row->call) != row->declared_callable ||
        ast_call_semantic_callee_value_binding_id(row->call) != 0 ||
        ast_call_arg_count(row->call) != row->actual_count ||
        (row->actual_count != 0 && row->formals == NULL)) return false;
    if (row->builtin) {
        uint32_t kind = 0;
        if (row->callee != NULL || row->callee_id != 0 || row->body != NULL ||
            row->result_type != NULL || row->callee_name != NULL ||
            row->result_syntax != NULL || row->formal_count != 0 ||
            ast_call_semantic_callee_decl_id(row->call) != 0 ||
            !ast_call_semantic_callee_builtin_kind(row->call, &kind) ||
            kind != row->builtin_kind) return false;
    } else if (!field_id(row->callee, row->callee_id) ||
        !field_id(row->body, row->body_id) ||
        row->callee->type != AST_FUNC_DECL || row->callee->is_async_decl ||
        ast_func_body(row->callee) != row->body ||
        ast_func_param_count(row->callee) != row->formal_count ||
        row->formal_count != row->actual_count ||
        ast_func_return_type(row->callee) != row->result_type ||
        ast_node_stable_id(row->result_type) != row->result_type_id ||
        !field_text_same(ast_declaration_name(row->callee), row->callee_name) ||
        !row->declared_callable ||
        ast_call_semantic_callee_decl_id(row->call) != row->callee_id ||
        ast_call_semantic_callee_value_binding_id(row->call) != 0) return false;
    if (!row->builtin && row->result_type != NULL) {
        char *syntax = ast_capture_inline((ASTNode *)row->result_type);
        bool same = syntax != NULL && field_text_same(syntax, row->result_syntax);
        free(syntax);
        if (!same) return false;
    } else if (!row->builtin && row->result_syntax != NULL) return false;
    for (size_t i = 0; i < row->actual_count; i++) {
        const IndexedStringFieldFormal *saved = &row->formals[i];
        if (!field_id(saved->actual, saved->actual_id) ||
            ast_call_argument(row->call, i) != saved->actual ||
            !field_text_same(ast_call_argument_name(row->call, i),
                saved->argument_name)) return false;
        if (row->builtin) continue;
        FuncParam *formal = ast_func_param(row->callee, saved->formal_ordinal);
        if (formal == NULL || saved->formal_ordinal != i ||
            saved->formal_id == 0 || ast_func_param_stable_id(formal) != saved->formal_id ||
            formal->mode != saved->mode || formal->default_value != NULL ||
            formal->type != saved->type_node ||
            ast_node_stable_id(formal->type) != saved->type_id ||
            !field_text_same(formal->name, saved->name)) return false;
        char *syntax = ast_capture_inline(formal->type);
        bool same = syntax != NULL && field_text_same(syntax, saved->type_syntax);
        free(syntax);
        if (!same) return false;
    }
    return true;
}

bool indexed_string_field_flow_syntax_current(const IndexedStringFieldRecipe *recipe)
{
    if (recipe->call_count == 0 || recipe->call_count > recipe->call_capacity ||
        recipe->sealed_calls != recipe->call_count ||
        recipe->calls == NULL) return false;
    for (size_t i = 0; i < recipe->call_count; i++) {
        if (!field_call_syntax(&recipe->calls[i])) return false;
        for (size_t j = 0; j < i; j++)
            if (recipe->calls[i].call == recipe->calls[j].call) return false;
    }
    return true;
}

void indexed_string_field_flow_destroy(IndexedStringFieldRecipe *recipe)
{
    for (size_t i = 0; i < recipe->call_count; i++) field_call_release(&recipe->calls[i]);
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
            if (!field_call_syntax(row) || row->builtin != builtin ||
                row->callee != decl || row->builtin_kind != (builtin ? kind : 0)) return false;
            for (size_t a = 0; decl != NULL && a < row->actual_count; a++) {
                size_t formal = SIZE_MAX;
                if (!semantic_param_publication_formal_for_actual(ctx, call, a, &formal) ||
                    formal != row->formals[a].formal_ordinal) return false;
            }
            return true;
        }
    if (!prepare || recipe->call_count == SIZE_MAX ||
        recipe->call_count + 1 > SIZE_MAX / sizeof(*recipe->calls)) return false;
    IndexedStringFieldCall row = {0};
    row.call = call; row.call_id = ast_node_stable_id(call);
    row.target = target; row.target_id = ast_node_stable_id(target);
    row.target_shape = target->type;
    row.target_binding_id = ast_identifier_binding_syntax_id(target);
    row.stdlib = ast_call_semantic_callee_is_stdlib(call);
    row.declared_callable = ast_call_semantic_callee_declared_callable(call);
    row.callee = decl; row.callee_id = ast_node_stable_id(decl);
    row.body = decl == NULL ? NULL : ast_func_body(decl);
    row.body_id = ast_node_stable_id(row.body);
    row.result_type = decl == NULL ? NULL : ast_func_return_type(decl);
    row.result_type_id = ast_node_stable_id(row.result_type);
    row.actual_count = ast_call_arg_count(call);
    row.formal_count = decl == NULL ? 0 : ast_func_param_count(decl);
    row.builtin = builtin; row.builtin_kind = builtin ? kind : 0;
    row.callee_name = decl == NULL ? NULL : field_copy(ast_declaration_name(decl));
    row.result_syntax = row.result_type == NULL ? NULL :
        ast_capture_inline((ASTNode *)row.result_type);
    if (row.actual_count > SIZE_MAX / sizeof(*row.formals)) goto failed;
    row.formals = row.actual_count == 0 ? NULL : calloc(row.actual_count,
        sizeof(*row.formals));
    if (row.call_id == 0 || row.target_id == 0 ||
        (decl != NULL && (row.callee_id == 0 || row.body_id == 0 ||
            row.callee_name == NULL || (row.result_type != NULL &&
                (row.result_type_id == 0 || row.result_syntax == NULL)))) ||
        (row.actual_count != 0 && row.formals == NULL)) goto failed;
    for (size_t a = 0; a < row.actual_count; a++) {
        IndexedStringFieldFormal *saved = &row.formals[a];
        saved->actual = ast_call_argument(call, a);
        saved->actual_id = ast_node_stable_id(saved->actual);
        saved->argument_name = field_copy(ast_call_argument_name(call, a));
        if (saved->actual_id == 0 || (ast_call_argument_name(call, a) != NULL &&
            saved->argument_name == NULL)) goto failed;
        if (decl == NULL) continue;
        size_t formal_at = SIZE_MAX;
        if (!semantic_param_publication_formal_for_actual(ctx, call, a, &formal_at) ||
            formal_at != a) goto failed;
        FuncParam *formal = ast_func_param(decl, formal_at);
        if (formal == NULL || formal->name == NULL || formal->type == NULL ||
            formal->default_value != NULL) goto failed;
        saved->formal_ordinal = formal_at;
        saved->formal_id = ast_func_param_stable_id(formal);
        saved->mode = formal->mode;
        saved->type_node = formal->type;
        saved->type_id = ast_node_stable_id(formal->type);
        saved->name = field_copy(formal->name);
        saved->type_syntax = ast_capture_inline(formal->type);
        if (saved->formal_id == 0 || saved->type_id == 0 || saved->name == NULL ||
            saved->type_syntax == NULL) goto failed;
    }
    if (!field_call_syntax(&row)) goto failed;
    IndexedStringFieldCall *calls = realloc(recipe->calls,
        (recipe->call_count + 1) * sizeof(*calls));
    if (calls == NULL) goto failed;
    recipe->calls = calls;
    recipe->calls[recipe->call_count++] = row;
    recipe->call_capacity = recipe->call_count;
    return true;
failed:
    field_call_release(&row);
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
        (index->no_store_call_count != 0 && index->no_store_calls == NULL) ||
        !function_param_flow_index_roots_current(index) ||
        !function_param_flow_index_claims_current(ctx, index)) return NULL;
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
