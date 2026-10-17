#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "indexed_string_definition_recipe.h"
#include "function_param_publication_internal.h"
#include "type_checker_internal.h"
#include "indexed_string_borrow_owner.h"

typedef struct IndexedStringLineageVisit {
    const ASTNode *value;
    const struct IndexedStringLineageVisit *parent;
} IndexedStringLineageVisit;

static bool
indexed_string_dependency_at(SemanticContext *ctx, ASTNode *value,
    const ASTNode **dependency, const IndexedStringLineageVisit *parent)
{
    if (value == NULL || value->type != AST_IDENTIFIER) return false;
    for (const IndexedStringLineageVisit *visit = parent; visit != NULL; visit = visit->parent)
        if (visit->value == value) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, value, false);
    uint32_t binding_id = ast_identifier_binding_syntax_id(value);
    PublishedNode *let = publication_node_by_id(ctx->function_param_publications, binding_id);
    if (row == NULL || row->id == 0 || row->id != ast_node_stable_id(value)
        || !row->value_present || row->meta || row->value_shape != AST_IDENTIFIER
        || row->value_type != TYPE_STRING || row->binding_id != binding_id
        || binding_id == 0 || !semantic_param_publication_value(ctx, value).ok) return false;
    if (let == NULL || !let->indexed_string_let.present) {
        if (row->indexed_string_let.present) return false;
        *dependency = NULL;
        return true;
    }
    const IndexedStringLetLineage *issued = &row->indexed_string_let;
    const IndexedStringLetLineage *current = &let->indexed_string_let;
    if (!current->available || current->declaration != let->node
        || current->declaration == NULL || current->declaration->type != AST_LET_DECL
        || current->declaration_id != binding_id
        || current->declaration_id != ast_node_stable_id(current->declaration)
        || current->binding_id != binding_id || current->initializer == NULL
        || row->indexed_string_source_id != current->source_id
        || current->initializer != ast_let_initializer(current->declaration)
        || current->initializer_id == 0
        || current->initializer_id != ast_node_stable_id(current->initializer)
        || current->initializer_shape != current->initializer->type
        || current->caller == NULL || current->caller->type != AST_FUNC_DECL
        || current->caller_id == 0 || current->caller_id != ast_node_stable_id(current->caller)
        || !publication_routine_header_current(ctx, current->caller)
        || !semantic_param_publication_value(ctx, current->initializer).ok
        || (current->dependent && (!issued->present
            || row->indexed_string_source_id != UINT32_MAX))
        || (issued->present && (!issued->available
            || issued->declaration != current->declaration
            || issued->initializer != current->initializer || issued->caller != current->caller
            || issued->declaration_id != current->declaration_id
            || issued->initializer_id != current->initializer_id
            || issued->caller_id != current->caller_id
            || issued->binding_id != current->binding_id
            || issued->source_id != current->source_id
            || issued->initializer_shape != current->initializer_shape
            || issued->dependent != current->dependent))) return false;
    if (!current->dependent) { *dependency = NULL; return true; }
    if (current->initializer->type == AST_CALL) {
        if (!ast_call_semantic_callee_declared_callable(current->initializer)
            || ast_call_semantic_callee_decl_id(current->initializer) == 0) return false;
        *dependency = current->initializer;
        return true;
    }
    if (current->initializer->type != AST_IDENTIFIER) return false;
    IndexedStringLineageVisit visit = {value, parent};
    return indexed_string_dependency_at(ctx, (ASTNode *)current->initializer, dependency, &visit);
}

bool semantic_indexed_string_borrow_result_dependency_current(SemanticContext *ctx,
    ASTNode *value, const ASTNode **dependency)
{
    if (dependency == NULL || ctx == NULL || ctx->has_error
        || !semantic_param_publication_inputs_ready(ctx)) return false;
    *dependency = NULL;
    return indexed_string_dependency_at(ctx, value, dependency, NULL);
}

static size_t definition_assignment_at(const IndexedStringDefinitionRecipe *recipe,
    const ASTNode *assignment)
{
    for (size_t i = 0; i < recipe->assignment_count; i++)
        if (recipe->assignments[i].assignment == assignment) return i;
    return SIZE_MAX;
}

bool indexed_string_definition_classify_uses(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe, bool *protected_use)
{
    if (ctx == NULL || recipe == NULL || protected_use == NULL ||
        !semantic_param_publication_inputs_ready(ctx)) return false;
    *protected_use = false;
    for (size_t i = 0; i < recipe->use_count; i++) {
        IndexedStringDefinitionUse *use = &recipe->uses[i];
        PublishedNode *row = publication_node(ctx->function_param_publications,
            use->value, false);
        if (row == NULL || row->node != use->value || row->id != use->value_id ||
            !row->indexed_string_source_present ||
            row->indexed_string_source_id != use->issued_source ||
            !semantic_param_publication_checked_argument(ctx, use->sink,
                use->actual_ordinal)) return false;
        if (use->protected_candidate) {
            use->protected_use = true;
        } else if (use->issued_source == 0) {
            if (semantic_param_publication_argument_borrowed_for_call(ctx,
                    use->sink, use->actual_ordinal)) continue;
            ASTNode *callee = semantic_param_publication_callee(ctx,
                use->sink, use->actual_ordinal);
            if (callee != NULL) {
                size_t formal = SIZE_MAX;
                if (!semantic_param_publication_formal_for_actual(ctx,
                        use->sink, use->actual_ordinal, &formal)) return false;
                SlotFunctionLookup lookup = {ctx, ctx->program_root};
                unsigned mask = function_param_flow_summary_for_call(&lookup,
                    use->sink, use->actual_ordinal);
                FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
                size_t at = store == NULL ? SIZE_MAX : function_param_flow_find(store,
                    ast_node_stable_id(callee), formal);
                if (at == SIZE_MAX || store->entries[at].function_decl != callee ||
                    store->entries[at].state != FUNCTION_PARAM_FLOW_COMPLETE ||
                    store->entries[at].analysis_unsupported ||
                    !function_param_flow_output_current(store, at, false)) return false;
                if ((mask & ~SLOT_PARAM_SUMMARY_READ) == 0) continue;
            } else if (!semantic_param_publication_checked_call(ctx, use->sink)) return false;
            use->protected_use = true;
        }
        if (use->protected_use) *protected_use = true;
    }
    return true;
}

static size_t definition_use_at(const IndexedStringDefinitionRecipe *recipe,
    const ASTNode *value, const ASTNode *call, size_t ordinal)
{
    for (size_t i = 0; i < recipe->use_count; i++)
        if (recipe->uses[i].sink == call && recipe->uses[i].actual_ordinal == ordinal &&
            recipe->uses[i].value == value) return i;
    return SIZE_MAX;
}

static bool definition_call_current(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe, ASTNode *call, size_t actual, bool prepare)
{
    if (call == NULL || call->type != AST_CALL || actual >= ast_call_arg_count(call)) return false;
    for (size_t i = 0; i < recipe->use_call_count; i++) {
        IndexedStringFieldCall *saved = &recipe->use_calls[i];
        if (saved->call != call) continue;
        if (!indexed_string_field_call_syntax(saved) ||
            (ctx != NULL && !indexed_string_field_call_current(ctx, saved))) return false;
        return saved->builtin || (actual < saved->formal_count &&
            (saved->formals[actual].mode == PARAM_MODE_DEFAULT ||
             saved->formals[actual].mode == PARAM_MODE_REF));
    }
    if (!prepare || ctx == NULL || recipe->use_call_count == SIZE_MAX ||
        recipe->use_call_count + 1 > SIZE_MAX / sizeof(*recipe->use_calls)) return false;
    IndexedStringFieldCall captured = {0};
    if (!indexed_string_field_call_capture(ctx, call, &captured)) return false;
    if (!captured.builtin && (actual >= captured.formal_count ||
        (captured.formals[actual].mode != PARAM_MODE_DEFAULT &&
         captured.formals[actual].mode != PARAM_MODE_REF))) {
        indexed_string_field_call_destroy(&captured);
        return false;
    }
    size_t count = recipe->use_call_count + 1;
    IndexedStringFieldCall *grown = realloc(recipe->use_calls, count * sizeof(*grown));
    if (grown == NULL) { indexed_string_field_call_destroy(&captured); return false; }
    recipe->use_calls = grown;
    grown[recipe->use_call_count++] = captured;
    recipe->use_call_capacity = count;
    return true;
}

bool indexed_string_definition_claims_roles(SemanticContext *ctx,
    IndexedStringDefinitionRecipe *recipe, bool prepare)
{
    const FunctionParamFlowIndexClaims *claims = recipe->claims;
    if (claims == NULL || claims->failed || claims->rows == NULL || claims->count == 0 ||
        claims->count > SIZE_MAX / sizeof(size_t) - 1) return false;
    size_t n = claims->count, edges = 0, definition_count = 0, assignments_seen = 0;
    size_t *offset = calloc(n + 1, sizeof(*offset));
    size_t *cursor = calloc(n, sizeof(*cursor));
    size_t *queue = calloc(n, sizeof(*queue));
    unsigned char *contains = calloc(n, sizeof(*contains));
    unsigned char *seen_uses = calloc(recipe->use_count == 0 ? 1 : recipe->use_count,
        sizeof(*seen_uses));
    size_t *parents = NULL;
    bool ok = false;
    if (offset == NULL || cursor == NULL || queue == NULL || contains == NULL ||
        seen_uses == NULL) goto done;
    for (size_t p = 0; p < n; p++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[p];
        if (row->child_count != 0 && (row->children == NULL ||
            row->child_ids == NULL || row->child_claim_ordinals == NULL)) goto done;
        for (size_t c = 0; c < row->child_count; c++) if (row->children[c] != NULL) {
            size_t child = row->child_claim_ordinals[c];
            if (child >= n || claims->rows[child].node != row->children[c] ||
                row->child_ids[c] != claims->rows[child].id ||
                offset[child + 1] == SIZE_MAX || edges == SIZE_MAX) goto done;
            offset[child + 1]++; edges++;
        }
    }
    if (edges > SIZE_MAX / sizeof(*parents)) goto done;
    for (size_t i = 1; i <= n; i++) {
        if (offset[i] > SIZE_MAX - offset[i - 1]) goto done;
        offset[i] += offset[i - 1];
    }
    parents = edges == 0 ? NULL : malloc(edges * sizeof(*parents));
    if (edges != 0 && parents == NULL) goto done;
    memcpy(cursor, offset, n * sizeof(*cursor));
    for (size_t p = 0; p < n; p++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[p];
        for (size_t c = 0; c < row->child_count; c++) if (row->children[c] != NULL) {
            size_t child = row->child_claim_ordinals[c];
            if (cursor[child] >= offset[child + 1]) goto done;
            parents[cursor[child]++] = p;
        }
    }
    size_t head = 0, tail = 0;
    for (size_t i = 0; i < n; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->node == recipe->declaration) {
            if (++definition_count != 1 || row->id != recipe->declaration_id ||
                row->shape != AST_LET_DECL || row->local_alias || row->child_count != 2 ||
                row->children[0] != recipe->birth.value) goto done;
        } else if (row->shape == AST_LET_DECL && row->id == recipe->binding_id) goto done;
        if (row->shape == AST_ASSIGNMENT && ast_assignment_target(row->node) != NULL &&
            ast_assignment_target(row->node)->type == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(ast_assignment_target(row->node)) ==
                recipe->binding_id) {
            size_t a = definition_assignment_at(recipe, row->node);
            if (a == SIZE_MAX || row->binding != recipe->binding_id ||
                !row->assignment_replacement ||
                row->child_count != 2 || row->children[0] != recipe->assignments[a].target ||
                row->children[1] != recipe->assignments[a].rhs.value ||
                (ctx != NULL && !semantic_param_publication_assignment_replacement(ctx,
                    row->node))) goto done;
            assignments_seen++;
        }
        if (row->shape != AST_IDENTIFIER || row->binding != recipe->binding_id) continue;
        if (!row->semantic_promoted || !row->snapshot_semantic_complete ||
            offset[i] == offset[i + 1] || tail == n) goto done;
        contains[i] = 1; queue[tail++] = i;
        bool write_only = true;
        for (size_t e = offset[i]; e < offset[i + 1]; e++) {
            const FunctionParamFlowIndexClaim *parent = &claims->rows[parents[e]];
            size_t slot = SIZE_MAX;
            for (size_t c = 0; c < parent->child_count; c++)
                if (parent->children[c] == row->node) { slot = c; break; }
            if (slot == SIZE_MAX) goto done;
            if (parent->shape == AST_ASSIGNMENT) {
                if (slot != 0 || definition_assignment_at(recipe, parent->node) == SIZE_MAX)
                    goto done;
            } else if (parent->shape == AST_CALL) {
                write_only = false;
                if (slot == 0 || slot - 1 >= ast_call_arg_count(parent->node) ||
                    ast_call_argument(parent->node, slot - 1) != row->node ||
                    !definition_call_current(ctx, recipe, parent->node, slot - 1, prepare))
                    goto done;
                size_t u = definition_use_at(recipe, row->node, parent->node, slot - 1);
                if (u == SIZE_MAX) {
                    if (!prepare || recipe->use_count == SIZE_MAX ||
                        recipe->use_count + 1 > SIZE_MAX / sizeof(*recipe->uses)) goto done;
                    PublishedNode *value_row = publication_node(ctx->function_param_publications,
                        row->node, false);
                    if (value_row == NULL || value_row->node != row->node ||
                        value_row->id != row->id || !value_row->indexed_string_source_present)
                        goto done;
                    size_t count = recipe->use_count + 1;
                    IndexedStringDefinitionUse *grown = realloc(recipe->uses,
                        count * sizeof(*grown));
                    if (grown == NULL) goto done;
                    recipe->uses = grown;
                    grown[recipe->use_count++] = (IndexedStringDefinitionUse){
                        row->node, parent->node, row->id, parent->id,
                        recipe->binding_id, value_row->indexed_string_source_id,
                        row->shape, slot - 1, false, false};
                    recipe->use_capacity = count;
                    unsigned char *seen = realloc(seen_uses, count);
                    if (seen == NULL) goto done;
                    seen_uses = seen; seen_uses[count - 1] = 0;
                    u = count - 1;
                }
                if (seen_uses[u]) goto done;
                seen_uses[u] = 1;
            } else if (parent->shape == AST_BINARY) write_only = false;
            else goto done;
        }
        if (!write_only && (!row->value_ready || row->value_meta ||
            (ctx != NULL && semantic_param_publication_binding(ctx, row->node) !=
                recipe->binding_id))) goto done;
    }
    while (head < tail) {
        size_t child = queue[head++];
        for (size_t e = offset[child]; e < offset[child + 1]; e++) {
            size_t parent = parents[e];
            if (contains[parent]) continue;
            if (tail == n) goto done;
            contains[parent] = 1; queue[tail++] = parent;
        }
    }
    for (size_t i = 0; i < n; i++) if (contains[i]) {
        switch (claims->rows[i].shape) {
        case AST_LAMBDA_EXPR: case AST_ASYNC_BLOCK: case AST_SPAWN_EXPR:
        case AST_PARALLEL_BLOCK: case AST_DEFER_STMT: case AST_CHANNEL_SEND:
        case AST_TRANSACTION_BLOCK: case AST_UNSAFE_BLOCK: case AST_GIVE_STMT:
        case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE: case AST_EVENT_INVOKE:
            goto done;
        default: break;
        }
    }
    if (definition_count != 1 || assignments_seen != recipe->assignment_count ||
        recipe->assignment_count == 0 || recipe->use_count == 0) goto done;
    for (size_t u = 0; u < recipe->use_count; u++) if (!seen_uses[u]) goto done;
    ok = true;
done:
    free(offset); free(cursor); free(queue); free(contains);
    free(seen_uses); free(parents);
    return ok;
}
