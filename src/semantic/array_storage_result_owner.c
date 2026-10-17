/* Owned Array result traversal and literal-element lifetime are one return owner. */
#include "array_storage_deferred_preservation_internal.h"
#include "type_checker_internal.h"
#include "slot_analyzer_internal.h"
#include "slot_summary.h"
#include "function_param_publication_owner.h"
#include "function_param_flow_summary_internal.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact.h"

typedef struct ArrayStorageResultVisit {
    const ASTNode *decl;
    const struct ArrayStorageResultVisit *parent;
} ArrayStorageResultVisit;

static bool
owned_function_result_current(SemanticContext *ctx, ASTNode *decl,
    const Type *result, const ArrayStorageResultVisit *parent, bool elements);

static bool
literal_return_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, const ArrayStorageOwnedReturn *event,
    const ASTNode **declaration_out, const ASTNode **literal_out);

static bool
owned_string_call_current(SemanticContext *ctx, const ASTNode *call,
    const ArrayStorageResultVisit *visit)
{
    if (call == NULL || call->type != AST_CALL) return false;
    const ASTNode *source = NULL;
    if (semantic_array_storage_independent_call_result(call, ctx))
        return semantic_array_storage_return_elements_owned((ASTNode *)call, ctx, &source)
            && source == NULL;
    const Type *result = NULL;
    ASTNode *callee = semantic_param_publication_declared_result(ctx, call, &result);
    return callee != NULL && type_is_constructed_named(result, "Array")
        && type_equals(type_get_constructed_arg(result, 0), TYPE_STRING)
        && owned_function_result_current(ctx, callee, result, visit, true);
}

static bool
owned_return_elements_current(SemanticContext *ctx, const ArrayStorageOwnedReturn *row,
    const ArrayStorageResultVisit *visit)
{
    const ASTNode *source = NULL;
    if (!semantic_array_storage_return_elements_current(row->value, ctx,
            &row->elements, &source))
        return false;
    if (row->source == ARRAY_RESULT_OWNED_BINDING
        && !semantic_array_storage_pending_owned_elements(ctx,
            row->binding_id, row->sequence)) return false;
    if (source == NULL || (source == row->value && row->source == ARRAY_RESULT_DECLARED_CALL))
        return true;
    return owned_string_call_current(ctx, source, visit);
}

static bool
owned_function_result_replay(SemanticContext *ctx, ASTNode *decl,
    const Type *result, const ArrayStorageResultVisit *parent, bool elements,
    bool prepare, ArrayStorageLiteralResultObserver observer, void *user)
{
    ArrayStorageDeferredPreservationStore *storage =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (storage == NULL || !storage->finalized || decl == NULL || decl->type != AST_FUNC_DECL
        || !type_is_constructed_named(result, "Array")
        || !semantic_param_publication_routine_result_current(ctx, decl, result)) return false;
    for (const ArrayStorageResultVisit *at = parent; at != NULL; at = at->parent)
        if (at->decl == decl) return false;
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *flow = prepare
        ? function_param_flow_store_get(&lookup) : ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (flow == NULL || (!prepare && flow->program_point_indexes == NULL)) return false;
    if (prepare) {
        if (!function_param_flow_program_point_index_build(flow, decl, &index)) return false;
    } else {
        size_t at = function_param_flow_program_point_index_find(flow, ast_node_stable_id(decl));
        if (at == SIZE_MAX) return false;
        index = &flow->program_point_indexes[at];
    }
    if (index == NULL || (observer != NULL && (index->function_decl != decl
            || !function_param_flow_index_roots_current(index)))
        || !function_param_flow_index_claims_current(ctx, index)) return false;
    /* Claims and projection are heap-owned; recursive callee index growth may
     * relocate the enclosing index vector, never these captured claim rows. */
    const FunctionParamFlowIndexClaims *claims = index->claims;
    if (claims == NULL) return false;
    const ArrayStorageResultVisit visit = {decl, parent};
    size_t reached = 0;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *claim = &claims->rows[i];
        if (claim->shape == AST_DEFER_STMT || claim->shape == AST_LAMBDA_EXPR ||
            claim->shape == AST_ASYNC_BLOCK || claim->shape == AST_PARALLEL_BLOCK ||
            claim->shape == AST_SPAWN_EXPR || claim->shape == AST_CHANNEL_SEND ||
            claim->shape == AST_TRANSACTION_BLOCK || claim->shape == AST_UNSAFE_BLOCK)
            return false;
        if (claim->shape != AST_RETURN) continue;
        bool reachable = false;
        if (!claim->semantic_promoted || !claim->snapshot_semantic_complete ||
            !claim->execution_present ||
            !semantic_param_publication_execution(ctx, claim->node, &reachable) ||
            claim->reachable != reachable) return false;
        if (!reachable) continue;
        const ArrayStorageOwnedReturn *event = NULL;
        for (size_t r = 0; r < storage->return_count; r++) {
            const ArrayStorageOwnedReturn *candidate = &storage->returns[r];
            if (candidate->decl != decl || candidate->statement != claim->node) continue;
            if (event != NULL) return false;
            event = candidate;
        }
        ASTNode *callee = NULL;
        const Type *callee_result = NULL;
        if (event == NULL || event->decl_id != ast_node_stable_id(decl) ||
            event->elements.function_id != event->decl_id ||
            event->statement_id != claim->id || !type_equals(event->type, result) ||
            !semantic_array_storage_return_source_current(ctx, event, &callee, &callee_result))
            return false;
        if (observer != NULL) {
            const ASTNode *declaration = NULL, *literal = NULL;
            if (elements || callee != NULL
                || !literal_return_current(ctx, claims, event, &declaration, &literal)
                || !observer(ctx, decl, claims, event, declaration, literal, user))
                return false;
        } else if ((callee != NULL && !owned_function_result_current(ctx, callee,
                callee_result, &visit, elements)) ||
            (elements && !owned_return_elements_current(ctx, event, &visit))) return false;
        reached++;
    }
    if (reached == 0) return false;
    size_t recorded = 0;
    for (size_t r = 0; r < storage->return_count; r++)
        if (storage->returns[r].decl == decl) recorded++;
    return recorded == reached;
}

static bool
owned_function_result_current(SemanticContext *ctx, ASTNode *decl,
    const Type *result, const ArrayStorageResultVisit *parent, bool elements)
{
    return owned_function_result_replay(ctx, decl, result, parent, elements,
        true, NULL, NULL);
}

bool
semantic_array_storage_function_result_owned(SemanticContext *ctx,
    ASTNode *decl, const Type *result)
{
    return owned_function_result_current(ctx, decl, result, NULL, false);
}

bool
semantic_array_storage_owned_result_current(SemanticContext *ctx, const ASTNode *call)
{
    ArrayStorageDeferredPreservationStore *storage =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (storage == NULL || !storage->finalized || call == NULL || call->type != AST_CALL)
        return false;
    if (semantic_array_storage_independent_call_result(call, ctx)) return true;
    const Type *result = NULL;
    ASTNode *callee = semantic_param_publication_declared_result(ctx, call, &result);
    return callee != NULL && owned_function_result_current(ctx, callee, result, NULL, false);
}

bool
semantic_array_storage_owned_string_result_current(SemanticContext *ctx,
    const ASTNode *call)
{
    return owned_string_call_current(ctx, call, NULL);
}

/* A literal's bytes are permanent, but its Array descriptor and element
 * cleanup rights are separate. This facet never changes elements.owned. */
static bool
literal_array_children_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, const ASTNode *literal,
    const Type *type)
{
    if (literal == NULL || literal->type != AST_ARRAY_LITERAL || claims == NULL
        || ast_array_literal_count(literal) == 0 || ast_node_stable_id(literal) == 0
        || !type_is_constructed_named(type, "Array")
        || !type_equals(type_get_constructed_arg(type, 0), TYPE_STRING)) return false;
    SemanticPublishedValue array = semantic_param_publication_value(ctx, literal);
    if (!array.ok || array.meta || !type_equals(array.type, type)) return false;
    const FunctionParamFlowIndexClaim *parent = NULL;
    for (size_t r = 0; r < claims->count; r++)
        if (claims->rows[r].node == literal) {
            if (parent != NULL) return false;
            parent = &claims->rows[r];
        }
    if (parent == NULL || parent->shape != AST_ARRAY_LITERAL
        || parent->id != ast_node_stable_id(literal)
        || parent->child_count != ast_array_literal_count(literal)) return false;
    for (size_t i = 0; i < parent->child_count; i++) {
        ASTNode *child = ast_array_literal_element(literal, i);
        SemanticPublishedValue value = semantic_param_publication_value(ctx, child);
        size_t seen = 0;
        if (child == NULL || child->type != AST_STRING
            || parent->children[i] != child || parent->child_ids[i] == 0
            || parent->child_ids[i] != ast_node_stable_id(child)
            || !value.ok || value.meta || !type_equals(value.type, TYPE_STRING)) return false;
        for (size_t r = 0; r < claims->count; r++)
            if (claims->rows[r].node == child && claims->rows[r].shape == AST_STRING
                && claims->rows[r].id == ast_node_stable_id(child)) seen++;
        if (seen != 1) return false;
    }
    return true;
}

/* Every reference to this binding must be an exact RETURN of its descriptor.
 * Calls, aliases, indexed writes, captures and replacements have other parent
 * roles and cannot silently preserve this literal-only lifetime property. */
static bool
literal_local_uses_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims,
    const ArrayStorageOwnedReturn *event)
{
    size_t uses = 0;
    bool selected_return = false;
    for (size_t r = 0; r < claims->count; r++) {
        const FunctionParamFlowIndexClaim *reference = &claims->rows[r];
        if (reference->shape != AST_IDENTIFIER
            || reference->binding != event->binding_id)
            continue;
        SemanticPublishedValue value = semantic_param_publication_value(ctx, reference->node);
        if (reference->id == 0 || reference->id != ast_node_stable_id(reference->node)
            || semantic_param_publication_binding(ctx, reference->node) != event->binding_id
            || !value.ok || value.meta || !type_is_constructed_named(value.type, "Array")
            || !type_equals(type_get_constructed_arg(value.type, 0), TYPE_STRING)) return false;
        size_t parents = 0;
        for (size_t p = 0; p < claims->count; p++) {
            const FunctionParamFlowIndexClaim *parent = &claims->rows[p];
            for (size_t c = 0; c < parent->child_count; c++)
                if (parent->children[c] == reference->node) {
                    if (parent->shape != AST_RETURN
                        || ast_return_value(parent->node) != reference->node
                        || parent->child_ids[c] != reference->id) return false;
                    if (reference->node == event->value
                        && parent->node == event->statement) selected_return = true;
                    parents++;
                }
        }
        if (parents != 1) return false;
        uses++;
    }
    return uses != 0 && selected_return;
}

static bool
literal_return_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims, const ArrayStorageOwnedReturn *event,
    const ASTNode **declaration_out, const ASTNode **literal_out)
{
    const ASTNode *declaration = NULL, *literal = NULL;
    if (event == NULL || event->elements.owned || event->elements.dependency_call != NULL
        || event->elements.dependency_call_id != 0) return false;
    if (event->source == ARRAY_RESULT_LITERAL) {
        literal = event->value;
    } else if (event->source == ARRAY_RESULT_OWNED_BINDING) {
        if (event->binding_id == 0 || event->value->type != AST_IDENTIFIER
            || ast_identifier_binding_syntax_id(event->value) != event->binding_id)
            return false;
        for (size_t r = 0; r < claims->count; r++)
            if (claims->rows[r].id == event->binding_id) {
                if (declaration != NULL || claims->rows[r].shape != AST_LET_DECL)
                    return false;
                declaration = claims->rows[r].node;
            }
        if (declaration == NULL || ast_let_is_mutable(declaration)
            || ast_let_is_alias(declaration)) return false;
        literal = ast_let_initializer(declaration);
        const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
            ctx, event->decl_id, event->binding_id);
        if (fact == NULL || fact->function_syntax_id != event->decl_id
            || fact->binding_syntax_id != event->binding_id
            || fact->origin != PGY_COLLECTION_ORIGIN_BORROWED_LITERAL
            || fact->element_ownership != PGY_STRING_ARRAY_BORROWED_ELEMENTS
            || fact->source_binding_syntax_id != 0
            || fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE
            || literal == NULL || fact->origin_syntax_id != ast_node_stable_id(literal)
            || !literal_local_uses_current(ctx, claims, event)) return false;
    } else return false;
    if (!literal_array_children_current(ctx, claims, literal, event->type)) return false;
    *declaration_out = declaration;
    *literal_out = literal;
    return true;
}

static bool
literal_result_replay(SemanticContext *ctx, const ASTNode *result_call,
    ArrayStorageLiteralResultObserver observer, void *user, bool prepare)
{
    ArrayStorageDeferredPreservationStore *storage =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (storage == NULL || !storage->finalized || ctx->has_error || observer == NULL
        || result_call == NULL || result_call->type != AST_CALL
        || ast_node_stable_id(result_call) == 0
        || !ast_call_semantic_callee_declared_callable(result_call)
        || ast_call_semantic_callee_value_binding_id(result_call) != 0)
        return false;
    const Type *result = NULL;
    ASTNode *decl = semantic_param_publication_declared_result(ctx, result_call, &result);
    if (decl == NULL || ast_node_stable_id(decl) == 0
        || ast_call_semantic_callee_decl_id(result_call) != ast_node_stable_id(decl)
        || !type_is_constructed_named(result, "Array")
        || !type_equals(type_get_constructed_arg(result, 0), TYPE_STRING)
        || (!prepare && ctx->function_param_flow_summaries == NULL)) return false;
    return owned_function_result_replay(ctx, decl, result, NULL, false,
        prepare, observer, user);
}

bool
semantic_array_storage_literal_result_prepare(SemanticContext *ctx,
    const ASTNode *result_call, ArrayStorageLiteralResultObserver observer,
    void *user)
{
    if (ctx == NULL || (ctx->function_param_flow_summaries != NULL
        && ctx->function_param_flow_summaries->validating_current)) return false;
    return literal_result_replay(ctx, result_call, observer, user, true);
}

bool
semantic_array_storage_literal_result_current(SemanticContext *ctx,
    const ASTNode *result_call, ArrayStorageLiteralResultObserver observer,
    void *user)
{
    bool previous = false;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = literal_result_replay(ctx, result_call, observer, user, false);
    function_param_flow_current_end(ctx, previous);
    return current;
}

