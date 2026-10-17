#include "indexed_string_permanent_internal.h"
#include "indexed_string_option_payload.h"
#include "indexed_string_borrow_owner.h"
#include "function_param_publication_internal.h"
#include "type_checker_internal.h"
#include "diag_codes.h"
#include "../common/match_variant_policy.h"
#include <stdlib.h>

bool semantic_indexed_string_permanent_some_boundary(const ASTNode *some)
{
    const ASTNode *target = some != NULL && some->type == AST_CALL ? ast_call_callee(some) : NULL;
    uint32_t kind = 0;
    return target != NULL && target->type == AST_IDENTIFIER &&
        pgy_match_variant_lookup(ast_identifier_name(target)) == PGY_MATCH_VARIANT_SOME &&
        ast_call_arg_count(some) == 1 && ast_call_generic_arg_count(some) == 0 &&
        ast_call_semantic_callee_decl_id(some) == 0 && !ast_call_semantic_callee_declared_callable(some) &&
        ast_call_semantic_callee_value_binding_id(some) == 0 &&
        ast_call_semantic_callee_builtin_kind(some, &kind) && kind == BUILTIN_NOT_BUILTIN;
}

static IndexedStringPermanentRecord permanent_record(SemanticContext *ctx,
    ASTNode *value, ASTNode *some, size_t ordinal)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_pending(ctx) || ordinal != 0 ||
        !semantic_indexed_string_permanent_some_boundary(some) || ast_call_argument(some, 0) != value ||
        value == NULL || value->type != AST_ARRAY_ACCESS || ctx->current_function_decl == NULL)
        return INDEXED_STRING_PERMANENT_NOT_APPLICABLE;
    ASTNode *receiver = ast_array_access_array(value), *index = ast_array_access_index(value);
    if (receiver == NULL || receiver->type != AST_IDENTIFIER || index == NULL)
        return INDEXED_STRING_PERMANENT_NOT_APPLICABLE;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    uint32_t binding = ast_identifier_binding_syntax_id(receiver);
    PublishedNode *let = publication_node_by_id(store, binding);
    const ASTNode *declaration = let == NULL ? NULL : let->node;
    const ASTNode *producer = declaration == NULL ? NULL : ast_let_initializer(declaration);
    if (declaration == NULL || declaration->type != AST_LET_DECL || ast_let_is_mutable(declaration) ||
        ast_let_is_alias(declaration) || producer == NULL || producer->type != AST_CALL ||
        !ast_call_semantic_callee_declared_callable(producer) || ast_call_semantic_callee_decl_id(producer) == 0 ||
        ast_call_semantic_callee_value_binding_id(producer) != 0)
        return INDEXED_STRING_PERMANENT_NOT_APPLICABLE;
    PublishedNode *source = publication_node(store, value, false);
    PublishedNode *array = publication_node(store, receiver, false);
    if (source == NULL || array == NULL || !source->value_present || source->meta || source->value_type != TYPE_STRING ||
        !source->indexed_string_source_present || source->indexed_string_source_id == 0 ||
        source->indexed_string_source_id == UINT32_MAX || !array->indexed_array_source_present ||
        array->indexed_array_source_id != source->indexed_string_source_id || binding == 0 ||
        ast_node_stable_id(declaration) != binding || ast_node_stable_id(value) == 0 ||
        ast_node_stable_id(receiver) == 0 || ast_node_stable_id(index) == 0 || ast_node_stable_id(some) == 0 ||
        ast_node_stable_id(producer) == 0 || ast_node_stable_id(ctx->current_function_decl) == 0)
        return INDEXED_STRING_PERMANENT_FAILED;
    IndexedStringPermanentElementRecipe issued = {.caller = ctx->current_function_decl, .some = some,
        .value = value, .receiver = receiver, .index = index, .declaration = declaration, .producer = producer,
        .caller_id = ast_node_stable_id(ctx->current_function_decl), .some_id = ast_node_stable_id(some),
        .value_id = ast_node_stable_id(value), .receiver_id = ast_node_stable_id(receiver), .index_id = ast_node_stable_id(index),
        .binding_id = binding, .source_id = source->indexed_string_source_id, .producer_id = ast_node_stable_id(producer)};
    PublishedNode *owner = publication_node(store, some, true);
    if (owner == NULL) return INDEXED_STRING_PERMANENT_FAILED;
    if (owner->indexed_string_permanent != NULL) {
        const IndexedStringPermanentElementRecipe *old = owner->indexed_string_permanent;
        return !old->prepared && old->caller == issued.caller && old->some == some && old->value == value &&
            old->receiver == receiver && old->index == index && old->declaration == declaration && old->producer == producer &&
            old->caller_id == issued.caller_id && old->some_id == issued.some_id && old->value_id == issued.value_id &&
            old->receiver_id == issued.receiver_id && old->index_id == issued.index_id && old->binding_id == binding &&
            old->source_id == issued.source_id && old->producer_id == issued.producer_id ?
            INDEXED_STRING_PERMANENT_RECORDED : INDEXED_STRING_PERMANENT_FAILED;
    }
    if (store->indexed_string_permanent_count == SIZE_MAX) return INDEXED_STRING_PERMANENT_FAILED;
    IndexedStringPermanentElementRecipe *recipe = malloc(sizeof(*recipe));
    if (recipe == NULL) return INDEXED_STRING_PERMANENT_FAILED;
    *recipe = issued;
    owner->indexed_string_permanent = recipe;
    store->indexed_string_permanent_count++;
    return INDEXED_STRING_PERMANENT_RECORDED;
}

IndexedStringPermanentRecord semantic_indexed_string_permanent_record(SemanticContext *ctx,
    ASTNode *value, ASTNode *some, size_t ordinal)
{
    IndexedStringPermanentRecord recorded = permanent_record(ctx, value, some, ordinal);
    if (recorded == INDEXED_STRING_PERMANENT_FAILED)
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, value, "A Some payload lacks an exact permanent-element candidate");
    return recorded;
}

static FunctionParamFlowIndexClaims *permanent_claims(SemanticContext *ctx, const ASTNode *decl)
{
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    FunctionParamFlowProgramPointIndex *found = NULL;
    if (store == NULL || store->failed || !publication_routine_header_current(ctx, decl)) return NULL;
    for (size_t i = 0; i < store->program_point_index_count; i++) if (store->program_point_indexes[i].function_decl == decl) {
        if (found != NULL) return NULL;
        found = &store->program_point_indexes[i];
    }
    return found != NULL && function_param_flow_index_claims_current(ctx, found) ? found->claims : NULL;
}

bool indexed_string_permanent_birth(IndexedStringPermanentElementRecipe *recipe,
    SemanticContext *ctx, const ASTNode *caller, const ASTNode *declaration,
    const FunctionParamFlowIndexClaims *claims, bool prepare)
{
    if (declaration == NULL || declaration->type != AST_LET_DECL || ast_let_is_mutable(declaration) ||
        ast_let_is_alias(declaration) || claims == NULL || ast_let_initializer(declaration) == NULL ||
        !indexed_string_result_capture_dependency(ctx, (ASTNode *)caller, (FunctionParamFlowIndexClaims *)claims,
            &recipe->inputs, prepare)) return false;
    IndexedStringPermanentBirth row = {caller, declaration, ast_let_initializer(declaration), ast_node_stable_id(caller),
        ast_node_stable_id(declaration), ast_node_stable_id(ast_let_initializer(declaration)), (FunctionParamFlowIndexClaims *)claims};
    if (row.caller_id == 0 || row.binding_id == 0 || row.initializer_id == 0) return false;
    for (size_t i = 0; i < recipe->birth_count; i++) if (recipe->births[i].binding_id == row.binding_id) {
        const IndexedStringPermanentBirth *old = &recipe->births[i];
        return old->caller == caller && old->declaration == declaration && old->initializer == row.initializer &&
            old->caller_id == row.caller_id && old->initializer_id == row.initializer_id && old->claims == claims &&
            indexed_string_permanent_history(ctx, recipe, old, prepare);
    }
    if (!prepare || recipe->birth_count >= SIZE_MAX / sizeof(*recipe->births)) return false;
    IndexedStringPermanentBirth *grown = realloc(recipe->births, (recipe->birth_count + 1) * sizeof(*grown));
    if (grown == NULL) return false;
    recipe->births = grown; grown[recipe->birth_count++] = row; recipe->birth_capacity = recipe->birth_count;
    return indexed_string_permanent_history(ctx, recipe, &row, prepare);
}

typedef struct { IndexedStringPermanentElementRecipe *recipe; bool prepare; size_t visits; } PermanentCapture;
static bool permanent_return_capture(SemanticContext *ctx, ASTNode *decl,
    const FunctionParamFlowIndexClaims *claims, const ArrayStorageOwnedReturnReceipt *event,
    const ASTNode *literal_declaration, const ASTNode *literal, void *user)
{
    PermanentCapture *capture = user;
    IndexedStringPermanentElementRecipe *recipe = capture->recipe;
    if (event == NULL || event->decl != decl || literal == NULL || literal->type != AST_ARRAY_LITERAL ||
        claims == NULL || !indexed_string_result_capture_dependency(ctx, decl, (FunctionParamFlowIndexClaims *)claims,
            &recipe->inputs, capture->prepare) || (literal_declaration != NULL &&
            !indexed_string_permanent_birth(recipe, ctx, decl, literal_declaration, claims, capture->prepare))) return false;
    IndexedStringPermanentReturn row = {decl, event->statement, event->value, literal, event->decl_id,
        event->statement_id, event->value_id, ast_node_stable_id(literal), event->binding_id, event->callee_id, event->source};
    if (row.literal_id == 0) return false;
    for (size_t i = 0; i < recipe->return_count; i++) if (recipe->returns[i].statement == row.statement) {
        const IndexedStringPermanentReturn *old = &recipe->returns[i];
        bool same = old->caller == row.caller && old->value == row.value && old->literal == row.literal &&
            old->caller_id == row.caller_id && old->statement_id == row.statement_id && old->value_id == row.value_id &&
            old->literal_id == row.literal_id && old->binding_id == row.binding_id && old->callee_id == row.callee_id &&
            old->source == row.source;
        if (same) capture->visits++;
        return same;
    }
    if (!capture->prepare || recipe->return_count >= SIZE_MAX / sizeof(*recipe->returns)) return false;
    IndexedStringPermanentReturn *grown = realloc(recipe->returns, (recipe->return_count + 1) * sizeof(*grown));
    if (grown == NULL) return false;
    recipe->returns = grown; grown[recipe->return_count++] = row; recipe->return_capacity = recipe->return_count;
    capture->visits++;
    return true;
}

static bool permanent_semantic(IndexedStringPermanentElementRecipe *recipe, SemanticContext *ctx, bool prepare)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx) || recipe == NULL ||
        !semantic_indexed_string_permanent_some_boundary(recipe->some) ||
        !semantic_param_publication_checked_call(ctx, recipe->some) ||
        !semantic_param_publication_checked_argument(ctx, recipe->some, 0) ||
        ast_call_argument(recipe->some, 0) != recipe->value) return false;
    PublishedNode *owner = publication_node(ctx->function_param_publications, recipe->some, false);
    if (owner == NULL || owner->node != recipe->some || owner->id != recipe->some_id ||
        owner->indexed_string_permanent != recipe || owner->caller != recipe->caller ||
        owner->caller_id != recipe->caller_id) return false;
    bool depends = false, retains = true;
    if (!semantic_param_publication_checked_result_relation(ctx, recipe->some, 0, &depends, &retains) || !depends || retains ||
        !semantic_param_publication_checked_backing_relation(ctx, recipe->some, 0, &depends, &retains) || !depends || retains)
        return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, recipe->value);
    PublishedNode *raw = publication_node(ctx->function_param_publications, recipe->value, false);
    uint32_t source = 0;
    if (!value.ok || value.meta || value.type != TYPE_STRING || raw == NULL || !raw->indexed_string_source_present ||
        raw->indexed_string_source_id != recipe->source_id ||
        !semantic_indexed_string_borrow_storage_source_current((ASTNode *)recipe->receiver, ctx, &source) ||
        source != recipe->source_id) return false;
    FunctionParamFlowIndexClaims *claims = permanent_claims(ctx, recipe->caller);
    if (claims == NULL || !indexed_string_permanent_birth(recipe, ctx, recipe->caller, recipe->declaration, claims, prepare) ||
        !indexed_string_result_capture_call(ctx, recipe->some, &recipe->inputs, prepare) ||
        !indexed_string_result_capture_call(ctx, recipe->producer, &recipe->inputs, prepare)) return false;
    PermanentCapture capture = {recipe, prepare, 0};
    bool proved = prepare ? semantic_array_storage_literal_result_prepare(ctx, recipe->producer, permanent_return_capture, &capture) :
        semantic_array_storage_literal_result_current(ctx, recipe->producer, permanent_return_capture, &capture);
    return proved && capture.visits != 0 && capture.visits == recipe->return_count;
}

bool semantic_indexed_string_permanent_current(SemanticContext *ctx,
    const IndexedStringPermanentElementRecipe *recipe)
{
    if (!semantic_indexed_string_permanent_syntax_current(recipe)) return false;
    bool previous;
    if (!function_param_flow_current_begin(ctx, &previous)) return false;
    bool current = permanent_semantic((IndexedStringPermanentElementRecipe *)recipe, ctx, false);
    function_param_flow_current_end(ctx, previous);
    return current;
}

bool semantic_indexed_string_permanent_receipts(SemanticContext *ctx, bool prepare)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t found = 0;
    for (size_t i = 0; i < store->node_capacity; i++) {
        IndexedStringPermanentElementRecipe *recipe = store->nodes[i].indexed_string_permanent;
        if (recipe == NULL) continue;
        bool proved = store->nodes[i].node == recipe->some && store->nodes[i].id == recipe->some_id;
        if (proved && prepare && !recipe->prepared) {
            proved = permanent_semantic(recipe, ctx, true);
            if (proved) {
                recipe->sealed_births = recipe->birth_count; recipe->sealed_returns = recipe->return_count;
                recipe->inputs.sealed_calls = recipe->inputs.call_count;
                recipe->inputs.sealed_dependencies = recipe->inputs.dependency_count;
                recipe->inputs.prepared = true; recipe->prepared = true;
            }
        }
        if (!proved || !semantic_indexed_string_permanent_current(ctx, recipe) || found == SIZE_MAX) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, (ASTNode *)recipe->value,
                "A retained String element has no current permanent-literal source proof");
            return false;
        }
        found++;
    }
    if (found != store->indexed_string_permanent_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, ctx->program_root, "A permanent-literal String source receipt was withdrawn");
        return false;
    }
    return true;
}

const IndexedStringPermanentElementRecipe *semantic_indexed_string_permanent_for_use(
    SemanticContext *ctx, const ASTNode *some, size_t ordinal, const ASTNode *value)
{
    PublishedNode *row = ctx == NULL ? NULL : publication_node(ctx->function_param_publications, some, false);
    const IndexedStringPermanentElementRecipe *recipe = row == NULL ? NULL : row->indexed_string_permanent;
    return recipe != NULL && ordinal == 0 && recipe->some == some && recipe->value == value &&
        semantic_indexed_string_permanent_current(ctx, recipe) ? recipe : NULL;
}

const IndexedStringFieldDependency *semantic_indexed_string_permanent_dependencies(
    const IndexedStringPermanentElementRecipe *recipe, size_t *count)
{
    if (count == NULL) return NULL;
    *count = 0;
    return semantic_indexed_string_permanent_syntax_current(recipe) ?
        semantic_indexed_string_result_inputs_dependencies(&recipe->inputs, count) : NULL;
}

const IndexedStringResultInputs *semantic_indexed_string_permanent_inputs(const IndexedStringPermanentElementRecipe *recipe)
{ return semantic_indexed_string_permanent_syntax_current(recipe) ? &recipe->inputs : NULL; }

void semantic_indexed_string_permanent_destroy(IndexedStringPermanentElementRecipe *recipe)
{
    if (recipe == NULL) return;
    semantic_indexed_string_result_inputs_destroy(&recipe->inputs);
    free(recipe->births); free(recipe->returns); free(recipe);
}
