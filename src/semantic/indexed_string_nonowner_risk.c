#include "indexed_string_nonowner_risk_internal.h"

#include <stdlib.h>
#include <string.h>

#include "array_storage_release_owner.h"
#include "diag_codes.h"
#include "formal_string_store_landing_owner.h"
#include "function_param_publication_internal.h"
#include "indexed_string_borrow_owner.h"
#include "loan_copy_landing_owner.h"
#include "type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker_internal.h"
#include "../common/string_compat.h"

/* UNKNOWN is never a source or a cleanup policy. This receipt only removes
 * one exact empty local from an otherwise independent risk inventory. */
static bool nonowner_id(const ASTNode *node, uint32_t id)
{ return node != NULL && id != 0 && ast_node_stable_id(node) == id; }

static bool nonowner_array_type(const Type *type)
{
    return type_is_constructed_named(type, "Array") &&
        type_equals(type_get_constructed_arg(type, 0), TYPE_STRING);
}

static bool nonowner_birth_syntax(const IndexedStringNonownerRisk *risk)
{
    if (risk == NULL || !nonowner_id(risk->caller, risk->caller_id) ||
        risk->caller->type != AST_FUNC_DECL ||
        !nonowner_id(ast_func_body(risk->caller), risk->body_id) ||
        !nonowner_id(risk->definition, risk->binding_id) ||
        risk->definition->type != AST_LET_DECL ||
        ast_let_is_alias(risk->definition) || ast_let_is_mutable(risk->definition) ||
        risk->binding_name == NULL || ast_let_name(risk->definition) == NULL ||
        strcmp(risk->binding_name, ast_let_name(risk->definition)) != 0 ||
        ast_let_initializer(risk->definition) != risk->initializer ||
        !nonowner_id(risk->initializer, risk->initializer_id) ||
        risk->initializer->type != AST_ARRAY_LITERAL ||
        ast_array_literal_count(risk->initializer) != 0 ||
        risk->array_type == NULL) return false;
    return true;
}

static bool nonowner_birth_current(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk)
{
    if (ctx == NULL || ctx->has_error || !nonowner_birth_syntax(risk) ||
        !publication_generation_ready(ctx) ||
        !nonowner_array_type(risk->array_type)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *birth = publication_node_by_id(store, risk->binding_id);
    PublishedNode *literal = publication_node(store, risk->initializer, false);
    const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
        ctx, risk->caller_id, risk->binding_id);
    if (birth == NULL || birth->node != risk->definition ||
        birth->id != risk->binding_id || birth->indexed_string_nonowner != risk ||
        !birth->execution_present || !birth->reachable || literal == NULL ||
        literal->caller != risk->caller || literal->caller_id != risk->caller_id ||
        !literal->value_present || literal->meta ||
        literal->value_shape != AST_ARRAY_LITERAL ||
        !nonowner_array_type(literal->value_type) ||
        !type_equals(literal->value_type, risk->array_type) ||
        fact == NULL || fact->function_syntax_id != risk->caller_id ||
        fact->binding_syntax_id != risk->binding_id ||
        fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL ||
        fact->origin_syntax_id != risk->initializer_id ||
        fact->source_binding_syntax_id != 0 ||
        fact->element_ownership != PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN ||
        fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE ||
        fact->return_elements_source != NULL ||
        fact->return_elements_source_id != 0) return false;
    return true;
}

static bool nonowner_use_append(IndexedStringNonownerRisk *risk,
    IndexedStringNonownerUse use)
{
    for (size_t i = 0; i < risk->use_count; i++) {
        const IndexedStringNonownerUse *prior = &risk->uses[i];
        bool same_slot = (prior->sink == use.sink || prior->sink_id == use.sink_id) &&
            prior->actual_ordinal == use.actual_ordinal;
        if (!same_slot) continue;
        return prior->value == use.value && prior->sink == use.sink &&
            prior->value_id == use.value_id &&
            prior->sink_id == use.sink_id && prior->binding_id == use.binding_id &&
            prior->actual_ordinal == use.actual_ordinal && prior->shape == use.shape;
    }
    if (risk->use_count == SIZE_MAX ||
        risk->use_count + 1 > SIZE_MAX / sizeof(*risk->uses)) return false;
    size_t count = risk->use_count + 1;
    IndexedStringNonownerUse *grown = realloc(risk->uses, count * sizeof(*grown));
    if (grown == NULL) return false;
    risk->uses = grown;
    risk->uses[risk->use_count++] = use;
    risk->use_capacity = count;
    return true;
}

static bool nonowner_raw_use_current(SemanticContext *ctx,
    const IndexedStringNonownerUse *use)
{
    if (ctx == NULL || use == NULL || !nonowner_id(use->value, use->value_id) ||
        use->value->type != use->shape ||
        (use->shape == AST_IDENTIFIER &&
            ast_identifier_binding_syntax_id(use->value) != use->binding_id) ||
        (use->shape != AST_IDENTIFIER && use->binding_id != 0) ||
        use->sink == NULL || use->sink->type != AST_CALL ||
        !nonowner_id(use->sink, use->sink_id) ||
        use->actual_ordinal >= ast_call_arg_count(use->sink) ||
        ast_call_argument(use->sink, use->actual_ordinal) != use->value) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications,
        use->value, false);
    uint32_t source = 0;
    return row != NULL && row->id == use->value_id && row->node == use->value &&
        row->value_present && !row->meta && row->value_shape == use->shape &&
        row->value_type == TYPE_STRING && row->string_value &&
        row->binding_id == use->binding_id && row->indexed_string_source_present &&
        row->indexed_string_source_id == UINT32_MAX &&
        semantic_indexed_string_borrow_source_current((ASTNode *)use->value,
            ctx, &source) && source == UINT32_MAX;
}

bool semantic_indexed_string_nonowner_record(SemanticContext *ctx,
    const Symbol *candidate, ASTNode *value, ASTNode *sink, size_t actual_ordinal)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_pending(ctx) ||
        ctx->current_function_decl == NULL ||
        ctx->current_function_decl->type != AST_FUNC_DECL ||
        candidate == NULL || candidate->name == NULL || candidate->decl_syntax_id == 0 ||
        candidate->is_parameter || candidate->is_host_field || candidate->is_consumed ||
        !candidate->has_exclusive_array_storage || candidate->has_escaped_array_storage ||
        semantic_array_storage_has_pending(candidate) ||
        candidate->collection_storage_source_syntax_id != candidate->decl_syntax_id ||
        !nonowner_array_type(candidate->type) || value == NULL || sink == NULL ||
        ast_node_stable_id(value) == 0 ||
        sink->type != AST_CALL || ast_node_stable_id(sink) == 0 ||
        actual_ordinal >= ast_call_arg_count(sink) ||
        ast_call_argument(sink, actual_ordinal) != value) return false;
    IndexedStringNonownerUse use = {value, sink,
        ast_node_stable_id(value), ast_node_stable_id(sink),
        value->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(value) : 0,
        actual_ordinal, value->type};
    if (!nonowner_raw_use_current(ctx, &use)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *birth = publication_node_by_id(store, candidate->decl_syntax_id);
    ASTNode *definition = birth == NULL ? NULL : (ASTNode *)birth->node;
    ASTNode *initializer = definition != NULL && definition->type == AST_LET_DECL ?
        ast_let_initializer(definition) : NULL;
    if (birth == NULL || definition == NULL ||
        definition->type != AST_LET_DECL ||
        ast_node_stable_id(definition) != candidate->decl_syntax_id ||
        !birth->execution_present || !birth->reachable ||
        ast_let_is_alias(definition) || ast_let_is_mutable(definition) ||
        ast_let_name(definition) == NULL ||
        strcmp(ast_let_name(definition), candidate->name) != 0 ||
        initializer == NULL || initializer->type != AST_ARRAY_LITERAL ||
        ast_array_literal_count(initializer) != 0 ||
        ast_node_stable_id(initializer) == 0 ||
        ast_node_stable_id(ast_func_body(ctx->current_function_decl)) == 0)
        return false;
    PublishedNode *literal = publication_node(store, initializer, false);
    uint32_t caller_id = ast_node_stable_id(ctx->current_function_decl);
    const PgyCollectionOwnershipFact *fact = semantic_collection_ownership_fact_find(
        ctx, caller_id, candidate->decl_syntax_id);
    if (caller_id == 0 || literal == NULL ||
        literal->caller != ctx->current_function_decl ||
        literal->caller_id != caller_id ||
        !literal->value_present || literal->meta ||
        literal->value_shape != AST_ARRAY_LITERAL ||
        !nonowner_array_type(literal->value_type) ||
        !type_equals(literal->value_type, candidate->type) || fact == NULL ||
        fact->function_syntax_id != caller_id ||
        fact->binding_syntax_id != candidate->decl_syntax_id ||
        fact->origin != PGY_COLLECTION_ORIGIN_EMPTY_LITERAL ||
        fact->origin_syntax_id != ast_node_stable_id(initializer) ||
        fact->source_binding_syntax_id != 0 ||
        fact->element_ownership != PGY_STRING_ARRAY_OWNERSHIP_UNKNOWN ||
        fact->disposition != PGY_COLLECTION_DISPOSITION_LIVE ||
        fact->return_elements_source != NULL ||
        fact->return_elements_source_id != 0) return false;
    if (birth->indexed_string_nonowner != NULL) {
        const IndexedStringNonownerRisk *prior = birth->indexed_string_nonowner;
        return prior->caller == ctx->current_function_decl && prior->definition == definition &&
            prior->initializer == initializer && prior->caller_id == caller_id &&
            prior->body_id == ast_node_stable_id(ast_func_body(ctx->current_function_decl)) &&
            prior->binding_id == candidate->decl_syntax_id &&
            prior->initializer_id == ast_node_stable_id(initializer) &&
            prior->binding_name != NULL &&
            strcmp(prior->binding_name, candidate->name) == 0 &&
            type_equals(prior->array_type, candidate->type) && !prior->prepared &&
            nonowner_use_append((IndexedStringNonownerRisk *)prior, use);
    }
    if (store->indexed_string_nonowner_count == SIZE_MAX) return false;
    IndexedStringNonownerRisk *risk = calloc(1, sizeof(*risk));
    if (risk == NULL) return false;
    risk->binding_name = pergyra_strdup(candidate->name);
    if (risk->binding_name == NULL) { free(risk); return false; }
    risk->caller = ctx->current_function_decl;
    risk->caller_id = caller_id;
    risk->body_id = ast_node_stable_id(ast_func_body(ctx->current_function_decl));
    risk->definition = definition;
    risk->binding_id = candidate->decl_syntax_id;
    risk->initializer = initializer;
    risk->initializer_id = ast_node_stable_id(initializer);
    risk->array_type = candidate->type;
    if (!nonowner_use_append(risk, use)) {
        semantic_indexed_string_nonowner_destroy(risk);
        return false;
    }
    birth->indexed_string_nonowner = risk;
    store->indexed_string_nonowner_count++;
    return true;
}

static bool nonowner_terminal_append(IndexedStringNonownerRisk *risk,
    IndexedStringNonownerTerminal row)
{
    if (risk->terminal_count == SIZE_MAX ||
        risk->terminal_count + 1 > SIZE_MAX / sizeof(*risk->terminals)) return false;
    size_t count = risk->terminal_count + 1;
    IndexedStringNonownerTerminal *grown = realloc(risk->terminals,
        count * sizeof(*grown));
    if (grown == NULL) return false;
    risk->terminals = grown;
    risk->terminals[risk->terminal_count++] = row;
    risk->terminal_capacity = count;
    return true;
}

static const ASTNode *nonowner_terminal_statement(
    const FunctionParamFlowIndexClaims *claims, const ASTNode *call)
{
    const ASTNode *statement = NULL;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        for (size_t c = 0; c < row->child_count; c++) if (row->children[c] == call) {
            if (statement != NULL || row->shape != AST_RETURN || c != 0 ||
                ast_return_value(row->node) != call) return NULL;
            statement = row->node;
        }
    }
    return statement;
}

/* Owned promotion may terminate at the same exact nominal-result handoff as
 * the non-owning receipt.  Recheck it without appending a receipt because the
 * promoted local is retired from this fact family immediately afterwards. */
static bool nonowner_owned_terminal_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *call_row, size_t actual)
{
    if (ctx == NULL || claims == NULL || call_row == NULL ||
        call_row->shape != AST_CALL || call_row->node == NULL ||
        call_row->collection_effect_present)
        return false;
    ASTNode *call = call_row->node;
    const ASTNode *statement = nonowner_terminal_statement(claims, call);
    if (statement == NULL || actual >= ast_call_arg_count(call) ||
        !semantic_param_publication_nominal_result_complete(ctx, call) ||
        !semantic_param_publication_checked_argument(ctx, call, actual))
        return false;
    PublishedNode *published = publication_result_current(ctx, call);
    bool depends = false, retains = true;
    return published != NULL && published->nominal_result != NULL &&
        semantic_param_publication_checked_result_relation(ctx, call, actual,
            &depends, &retains) && depends && !retains;
}

static bool nonowner_terminal_current(SemanticContext *ctx,
    IndexedStringNonownerRisk *risk, const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *call_row, size_t actual, bool seal,
    size_t *terminal_at)
{
    const ASTNode *call = call_row->node;
    const ASTNode *statement = nonowner_terminal_statement(claims, call);
    if (statement == NULL || actual >= ast_call_arg_count(call) ||
        ast_call_argument(call, actual) == NULL ||
        call_row->collection_effect_present ||
        (ctx != NULL && (!semantic_param_publication_nominal_result_complete(ctx, call) ||
        !semantic_param_publication_checked_argument(ctx, call, actual)))) return false;
    SemanticNominalResultSnapshot *mapping = NULL;
    if (ctx != NULL) {
        PublishedNode *published = publication_node(ctx->function_param_publications, call, false);
        bool depends = false, retains = true;
        if (published == NULL || published->nominal_result == NULL ||
            !semantic_param_publication_checked_result_relation(ctx, call, actual,
                &depends, &retains) || !depends || retains) return false;
        mapping = published->nominal_result;
    }
    IndexedStringNonownerTerminal row = {call, statement,
        ast_node_stable_id(call), ast_node_stable_id(statement), actual, mapping};
    if (row.call_id == 0 || row.statement_id == 0) return false;
    if (seal) return nonowner_terminal_append(risk, row);
    if (*terminal_at >= risk->terminal_count || risk->terminals == NULL) return false;
    const IndexedStringNonownerTerminal *saved = &risk->terminals[(*terminal_at)++];
    return saved->call == row.call && saved->statement == row.statement &&
        saved->call_id == row.call_id && saved->statement_id == row.statement_id &&
        saved->actual_ordinal == actual && saved->mapping != NULL &&
        semantic_nominal_result_snapshot_syntax_current(call, saved->mapping) &&
        (ctx == NULL || saved->mapping == mapping);
}

static bool nonowner_checked_array_read(SemanticContext *ctx,
    const FunctionParamFlowIndexClaim *row)
{
    if (row->shape != AST_ARRAY_ACCESS || !row->value_ready || row->value_meta ||
        row->value_type != TYPE_STRING) return false;
    if (ctx == NULL) return true;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, row->node);
    return value.ok && !value.meta && value.type == TYPE_STRING;
}

static bool nonowner_checked_call(SemanticContext *ctx,
    IndexedStringNonownerRisk *risk, const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *row, size_t child, bool seal,
    size_t *terminal_at)
{
    const ASTNode *call = row->node;
    if (child == 0 || child > ast_call_arg_count(call) ||
        row->children[child] != ast_call_argument(call, child - 1) ||
        (ctx != NULL && !semantic_param_publication_checked_argument(ctx, call, child - 1)))
        return false;
    if (child == 1 && ast_call_arg_count(call) == 1 &&
        !row->collection_effect_present &&
        semantic_stdlib_array_argument_preserves_storage(call, 0) &&
        ast_call_callee(call) != NULL &&
        ast_call_callee(call)->type == AST_IDENTIFIER &&
        ast_identifier_name(ast_call_callee(call)) != NULL &&
        strcmp(ast_identifier_name(ast_call_callee(call)), "ArrayLength") == 0 &&
        !ast_call_semantic_callee_declared_callable(call) &&
        (ctx == NULL || semantic_stdlib_array_argument_preserves_elements(call, 0, ctx)))
        return row->value_ready && !row->value_meta && row->value_type == TYPE_INT;
    if (child == 1 && ast_call_arg_count(call) == 2 &&
        row->private_array_push && row->collection_effect_present &&
        row->collection_effect_kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
        row->collection_effect_receiver == risk->binding_id &&
        row->collection_effect_source == 0 &&
        ast_call_callee(call) != NULL && ast_call_callee(call)->type == AST_IDENTIFIER &&
        ast_identifier_name(ast_call_callee(call)) != NULL &&
        strcmp(ast_identifier_name(ast_call_callee(call)), "ArrayPush") == 0 &&
        semantic_stdlib_array_argument_preserves_storage(call, 0) &&
        !ast_call_semantic_callee_declared_callable(call) &&
        (ctx == NULL || semantic_param_publication_private_array_push(
            ctx, call, risk->caller))) return true;
    return nonowner_terminal_current(ctx, risk, claims, row,
        child - 1, seal, terminal_at);
}

static bool nonowner_uses_current(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims)
{
    if (risk->use_count == 0 || risk->use_count != risk->sealed_uses ||
        risk->use_count > risk->use_capacity || risk->uses == NULL) return false;
    for (size_t u = 0; u < risk->use_count; u++) {
        const IndexedStringNonownerUse *use = &risk->uses[u];
        if (!nonowner_id(use->value, use->value_id) ||
            use->value->type != use->shape ||
            (use->shape == AST_IDENTIFIER ?
                ast_identifier_binding_syntax_id(use->value) != use->binding_id :
                use->binding_id != 0)) return false;
        const FunctionParamFlowIndexClaim *value = NULL, *sink = NULL;
        for (size_t i = 0; i < claims->count; i++) {
            const FunctionParamFlowIndexClaim *row = &claims->rows[i];
            if (row->node == use->value) {
                if (value != NULL) return false;
                value = row;
            }
            if (use->sink != NULL && row->node == use->sink) {
                if (sink != NULL) return false;
                sink = row;
            }
        }
        if (value == NULL || value->id != use->value_id ||
            value->shape != use->shape || value->binding != use->binding_id ||
            !value->semantic_promoted || !value->snapshot_semantic_complete ||
            !value->value_ready || value->value_meta || value->value_type != TYPE_STRING ||
            (ctx != NULL && !nonowner_raw_use_current(ctx, use))) return false;
        if (sink == NULL || sink->id != use->sink_id ||
            sink->shape != AST_CALL ||
            use->actual_ordinal >= ast_call_arg_count(use->sink) ||
            ast_call_argument(use->sink, use->actual_ordinal) != use->value ||
            use->actual_ordinal + 1 >= sink->child_count ||
            sink->children[use->actual_ordinal + 1] != use->value ||
            sink->child_ids[use->actual_ordinal + 1] != use->value_id ||
            (ctx != NULL && ast_call_semantic_callee_declared_callable(use->sink) &&
                !semantic_param_publication_checked_argument(ctx, use->sink,
                    use->actual_ordinal))) return false;
    }
    return true;
}

/* An empty local Array<String> can become an owned-element collection through
 * an `inout` call without a local collection-effect row.  The existing array
 * preservation owner already walks the exact callee/formal chain and refuses
 * shallow stores, unsupported aliases and callees that can lose or borrow an
 * element.  Starting from an empty array, that complete preservation proof
 * means every element present after the call was produced by an admitted
 * owned/fresh store in that chain.  This query only connects that proof to the
 * caller's exact actual; it does not infer ownership from `inout` alone. */
static bool nonowner_inout_owned_elements(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, ASTNode *call, size_t actual)
{
    if (ctx == NULL || risk == NULL || call == NULL || call->type != AST_CALL ||
        actual >= ast_call_arg_count(call) ||
        !semantic_param_publication_checked_argument(ctx, call, actual))
        return false;
    ASTNode *argument = ast_call_argument(call, actual);
    if (argument == NULL || argument->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(argument) != risk->binding_id)
        return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx,
        argument);
    ASTNode *callee = semantic_param_publication_callee(ctx, call, actual);
    size_t formal = SIZE_MAX;
    if (!value.ok || value.meta || !type_equals(value.type, risk->array_type) ||
        callee == NULL || callee->type != AST_FUNC_DECL ||
        !semantic_param_publication_formal_for_actual(ctx, call, actual,
            &formal) || formal >= ast_func_param_count(callee) ||
        !semantic_param_publication_routine_inputs_current(ctx, callee, true))
        return false;
    FuncParam *parameter = ast_func_param(callee, formal);
    if (parameter == NULL || parameter->mode != PARAM_MODE_MUT_REF ||
        ast_func_param_stable_id(parameter) == 0)
        return false;
    return function_param_flow_array_preservation_failures(ctx, callee,
        formal) == 0;
}

/* A declared read-only call can observe the collection between owned stores
 * without changing the postcondition.  Default aggregate parameters share
 * storage, so their summary must prove no write/release/escape; `ref` is held
 * to the same condition here. */
static bool nonowner_declared_read_preserves_elements(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, ASTNode *call, size_t actual)
{
    if (ctx == NULL || risk == NULL || call == NULL || call->type != AST_CALL ||
        actual >= ast_call_arg_count(call) ||
        !semantic_param_publication_checked_argument(ctx, call, actual))
        return false;
    ASTNode *argument = ast_call_argument(call, actual);
    ASTNode *callee = semantic_param_publication_callee(ctx, call, actual);
    size_t formal = SIZE_MAX;
    if (argument == NULL || argument->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(argument) != risk->binding_id ||
        callee == NULL || callee->type != AST_FUNC_DECL ||
        !semantic_param_publication_formal_for_actual(ctx, call, actual,
            &formal) || formal >= ast_func_param_count(callee) ||
        !semantic_param_publication_routine_inputs_current(ctx, callee, true))
        return false;
    FuncParam *parameter = ast_func_param(callee, formal);
    if (parameter == NULL ||
        (parameter->mode != PARAM_MODE_DEFAULT &&
            parameter->mode != PARAM_MODE_REF) ||
        ast_func_param_stable_id(parameter) == 0 ||
        function_param_flow_array_preservation_failures(ctx, callee,
            formal) != 0)
        return false;
    unsigned summary = function_param_flow_summary_for_param(ctx, callee,
        formal);
    return (summary & ~SLOT_PARAM_SUMMARY_READ) == 0;
}

/* The landing owner may replace every borrowed String push recorded by this
 * risk with an owned-copy push. In that case the non-owning proof is no longer
 * the right fact family: the collection transition owns the stored payloads.
 * Retire the old receipt only after the current function index proves that
 * every effect touching this receiver belongs to the owned push/set/drop
 * lifecycle, with at least one owned store. The use
 * inventory spans all unknown String uses in the routine, not just stores into
 * this receiver, so it is checked for currency but is not paired by position.
 * A mixed or malformed effect remains fail-closed. */
static bool nonowner_promoted_to_owned(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk)
{
    if (ctx == NULL || ctx->has_error || risk == NULL || risk->prepared ||
        risk->claims != NULL || risk->use_count == 0 ||
        risk->use_count > risk->use_capacity || risk->uses == NULL ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_routine_inputs_current(ctx,
            risk->caller, true) || !nonowner_birth_current(ctx, risk)) return false;

    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving ||
        store->validating_current ||
        !function_param_flow_program_point_index_build(store,
            (ASTNode *)risk->caller, &index) || index == NULL ||
        index->function_decl != risk->caller ||
        index->function_id != risk->caller_id || index->claims == NULL ||
        !function_param_flow_index_roots_current(index) ||
        !function_param_flow_index_claims_current(ctx, index)) return false;

    const FunctionParamFlowIndexClaims *claims = index->claims;
    if (claims->failed || claims->rows == NULL || claims->count == 0)
        return false;

    for (size_t u = 0; u < risk->use_count; u++) {
        const IndexedStringNonownerUse *use = &risk->uses[u];
        const FunctionParamFlowIndexClaim *value = NULL, *sink = NULL;
        if (!nonowner_raw_use_current(ctx, use)) return false;
        for (size_t i = 0; i < claims->count; i++) {
            const FunctionParamFlowIndexClaim *row = &claims->rows[i];
            if (row->node == use->value) {
                if (value != NULL) return false;
                value = row;
            }
            if (row->node == use->sink) {
                if (sink != NULL) return false;
                sink = row;
            }
        }
        if (value == NULL || value->id != use->value_id ||
            value->shape != use->shape || value->binding != use->binding_id ||
            !value->semantic_promoted || !value->snapshot_semantic_complete ||
            !value->value_ready || value->value_meta ||
            value->value_type != TYPE_STRING || sink == NULL ||
            sink->id != use->sink_id || sink->shape != AST_CALL ||
            use->actual_ordinal >= ast_call_arg_count(use->sink) ||
            ast_call_argument(use->sink, use->actual_ordinal) != use->value ||
            use->actual_ordinal + 1 >= sink->child_count ||
            sink->children[use->actual_ordinal + 1] != use->value ||
            sink->child_ids[use->actual_ordinal + 1] != use->value_id ||
            (ast_call_semantic_callee_declared_callable(use->sink) &&
                !semantic_param_publication_checked_argument(ctx, use->sink,
                    use->actual_ordinal))) return false;
    }

    size_t owned_transition_count = 0;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->shape != AST_CALL) continue;
        uint32_t kind = 0, receiver = 0, source = 0;
        bool effect = ast_call_semantic_collection_effect(row->node,
            &kind, &receiver, &source);
        if (effect != row->collection_effect_present ||
            kind != row->collection_effect_kind ||
            receiver != row->collection_effect_receiver ||
            source != row->collection_effect_source) return false;
        bool touches = effect && (receiver == risk->binding_id ||
            source == risk->binding_id);
        if (touches) {
            const ASTNode *callee = ast_call_callee(row->node);
            const char *name = callee != NULL &&
                callee->type == AST_IDENTIFIER
                ? ast_identifier_name(callee) : NULL;
            bool push = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH &&
                name != NULL && strcmp(name, "ArrayPushOwnedString") == 0 &&
                ast_call_arg_count(row->node) == 2;
            bool set = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_SET &&
                name != NULL && strcmp(name, "ArraySetOwnedString") == 0 &&
                ast_call_arg_count(row->node) == 3;
            bool drop = kind == PGY_COLLECTION_EFFECT_DROP && name != NULL &&
                strcmp(name, "ArrayDropOwnedStrings") == 0 &&
                ast_call_arg_count(row->node) == 1;
            if ((!push && !set && !drop) ||
                receiver != risk->binding_id || source != 0 ||
                callee == NULL || callee->type != AST_IDENTIFIER ||
                ast_identifier_binding_syntax_id(callee) != 0 ||
                ast_call_semantic_callee_declared_callable(row->node) ||
                ast_call_argument(row->node, 0) == NULL ||
                ast_call_argument(row->node, 0)->type != AST_IDENTIFIER ||
                ast_identifier_binding_syntax_id(
                    ast_call_argument(row->node, 0)) != risk->binding_id)
                return false;
            if (push || set) {
                if (owned_transition_count == SIZE_MAX) return false;
                owned_transition_count++;
            }
        }

        for (size_t actual = 0; actual < ast_call_arg_count(row->node);
             actual++) {
            ASTNode *argument = ast_call_argument(row->node, actual);
            if (argument == NULL || argument->type != AST_IDENTIFIER ||
                ast_identifier_binding_syntax_id(argument) !=
                    risk->binding_id)
                continue;
            if (touches && actual == 0) continue;
            if (semantic_stdlib_array_argument_preserves_storage(
                    row->node, actual) &&
                semantic_stdlib_array_argument_preserves_elements(
                    row->node, actual, ctx))
                continue;
            if (nonowner_declared_read_preserves_elements(ctx, risk,
                    row->node, actual))
                continue;
            if (nonowner_inout_owned_elements(ctx, risk, row->node,
                    actual)) {
                if (owned_transition_count == SIZE_MAX) return false;
                owned_transition_count++;
                continue;
            }
            if (!nonowner_owned_terminal_current(ctx, claims, row, actual))
                return false;
        }
    }

    return owned_transition_count != 0;
}

/* When the exclusion proof itself is insufficient, upgrade every plain
 * String push into this receiver in the next landing round. The existing
 * landing owner records all pushes for the binding together, so the next
 * analysis cannot mix borrowed and owned elements. */
static bool nonowner_select_owned_pushes(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, ASTNode **first)
{
    if (first != NULL) *first = NULL;
    if (ctx == NULL || ctx->has_error || risk == NULL || first == NULL ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_routine_inputs_current(ctx,
            risk->caller, true) || !nonowner_birth_current(ctx, risk)) return false;
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving ||
        store->validating_current ||
        !function_param_flow_program_point_index_build(store,
            (ASTNode *)risk->caller, &index) || index == NULL ||
        index->function_decl != risk->caller ||
        index->function_id != risk->caller_id || index->claims == NULL ||
        !function_param_flow_index_roots_current(index) ||
        !function_param_flow_index_claims_current(ctx, index)) return false;

    for (size_t i = 0; i < index->claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &index->claims->rows[i];
        if (row->shape != AST_CALL) continue;
        uint32_t kind = 0, receiver = 0, source = 0;
        bool effect = ast_call_semantic_collection_effect(row->node,
            &kind, &receiver, &source);
        if (effect != row->collection_effect_present ||
            kind != row->collection_effect_kind ||
            receiver != row->collection_effect_receiver ||
            source != row->collection_effect_source) return false;
        if (!effect || (receiver != risk->binding_id &&
            source != risk->binding_id)) continue;

        const ASTNode *callee = ast_call_callee(row->node);
        const char *name = callee != NULL && callee->type == AST_IDENTIFIER
            ? ast_identifier_name(callee) : NULL;
        bool plain = kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
            name != NULL && strcmp(name, "ArrayPush") == 0 &&
            ast_call_arg_count(row->node) == 2 && row->private_array_push;
        bool owned_push = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_PUSH &&
            name != NULL && strcmp(name, "ArrayPushOwnedString") == 0 &&
            ast_call_arg_count(row->node) == 2;
        bool owned_set = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_SET &&
            name != NULL && strcmp(name, "ArraySetOwnedString") == 0 &&
            ast_call_arg_count(row->node) == 3;
        bool drop = kind == PGY_COLLECTION_EFFECT_DROP && name != NULL &&
            strcmp(name, "ArrayDropOwnedStrings") == 0 &&
            ast_call_arg_count(row->node) == 1;
        if ((!plain && !owned_push && !owned_set && !drop) ||
            receiver != risk->binding_id || source != 0 || callee == NULL ||
            callee->type != AST_IDENTIFIER ||
            ast_identifier_binding_syntax_id(callee) != 0 ||
            ast_call_semantic_callee_declared_callable(row->node) ||
            ast_call_argument(row->node, 0) == NULL ||
            ast_call_argument(row->node, 0)->type != AST_IDENTIFIER ||
            ast_identifier_binding_syntax_id(
                ast_call_argument(row->node, 0)) != risk->binding_id)
            return false;
        if (plain) {
            if (*first != NULL) continue;
            *first = row->node;
        }
    }
    return *first != NULL &&
        semantic_formal_string_store_landing_select_owned(ctx, *first);
}

/* Use the original captured child edges, including dormant branches. The
 * transient reverse graph identifies capture and write ancestors without an
 * AST walk or a separate persistent proof ledger. */
static bool nonowner_history(SemanticContext *ctx, IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims, bool seal)
{
    if (claims == NULL || claims->failed || claims->rows == NULL || claims->count == 0 ||
        claims->count > SIZE_MAX / sizeof(size_t) - 1) return false;
    size_t n = claims->count, edges = 0, terminal_at = 0;
    size_t *offset = calloc(n + 1, sizeof(*offset));
    size_t *cursor = calloc(n, sizeof(*cursor));
    size_t *queue = calloc(n, sizeof(*queue));
    unsigned char *contains = calloc(n, sizeof(*contains));
    size_t *parents = NULL;
    bool ok = false;
    if (offset == NULL || cursor == NULL || queue == NULL || contains == NULL) goto done;
    for (size_t p = 0; p < n; p++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[p];
        if (row->child_count != 0 && (row->children == NULL ||
            row->child_claim_ordinals == NULL)) goto done;
        for (size_t c = 0; c < row->child_count; c++) if (row->children[c] != NULL) {
            size_t child = row->child_claim_ordinals[c];
            if (child >= n || claims->rows[child].node != row->children[c] ||
                offset[child + 1] == SIZE_MAX || edges == SIZE_MAX) goto done;
            offset[child + 1]++; edges++;
        }
    }
    if (edges > SIZE_MAX / sizeof(*parents)) goto done;
    for (size_t i = 1; i <= n; i++) {
        if (offset[i] > SIZE_MAX - offset[i - 1]) goto done;
        offset[i] += offset[i - 1];
    }
    if (offset[n] != edges) goto done;
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
        if (row->shape != AST_IDENTIFIER || row->binding != risk->binding_id) continue;
        if (offset[i] == offset[i + 1] || tail == n || !row->semantic_promoted ||
            !row->snapshot_semantic_complete || !row->value_ready || row->value_meta)
            goto done;
        contains[i] = 1; queue[tail++] = i;
        if (ctx != NULL && semantic_param_publication_binding(ctx, row->node) !=
            risk->binding_id) goto done;
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
    size_t definition_count = 0;
    for (size_t i = 0; i < n; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        const ASTNode *node = row->node;
        if (node == risk->definition) {
            if (++definition_count != 1 || row->id != risk->binding_id ||
                row->shape != AST_LET_DECL || row->child_count != 2 ||
                row->children[0] != risk->initializer || row->local_alias) goto done;
        } else if (row->id == risk->binding_id && row->shape == AST_LET_DECL) goto done;
        if (node == ast_func_body(risk->caller) && row->id != risk->body_id) goto done;
        if (row->shape == AST_CALL) {
            uint32_t kind = 0, receiver = 0, source = 0;
            bool effect = ast_call_semantic_collection_effect(node, &kind, &receiver, &source);
            if (effect != row->collection_effect_present ||
                kind != row->collection_effect_kind ||
                receiver != row->collection_effect_receiver ||
                source != row->collection_effect_source) goto done;
            if (effect && (receiver == risk->binding_id || source == risk->binding_id) &&
                (kind != PGY_COLLECTION_EFFECT_SHALLOW_MUTATION ||
                    receiver != risk->binding_id || source != 0 ||
                    ast_call_arg_count(node) != 2 ||
                    ast_call_argument(node, 0) == NULL ||
                    ast_call_argument(node, 0)->type != AST_IDENTIFIER ||
                    ast_identifier_binding_syntax_id(ast_call_argument(node, 0)) !=
                        risk->binding_id || !row->private_array_push)) goto done;
        }
        if (!contains[i]) continue;
        switch (row->shape) {
        case AST_FUNC_DECL: case AST_LAMBDA_EXPR: case AST_ASYNC_BLOCK:
        case AST_SPAWN_EXPR: case AST_PARALLEL_BLOCK: case AST_DEFER_STMT:
        case AST_EVENT_SUBSCRIBE: case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE: case AST_TRANSACTION_BLOCK:
        case AST_CHANNEL_SEND: case AST_GIVE_STMT: case AST_UNSAFE_BLOCK:
            goto done;
        case AST_ASSIGNMENT:
            if (row->child_count < 2 || row->child_claim_ordinals[0] >= n ||
                contains[row->child_claim_ordinals[0]]) goto done;
            break;
        default: break;
        }
        for (size_t c = 0; c < row->child_count; c++) {
            const ASTNode *child = row->children[c];
            if (child == NULL || child->type != AST_IDENTIFIER ||
                ast_identifier_binding_syntax_id(child) != risk->binding_id) continue;
            if (row->shape == AST_ARRAY_ACCESS && c == 0) {
                if (!nonowner_checked_array_read(ctx, row)) goto done;
            } else if (row->shape == AST_CALL) {
                if (!nonowner_checked_call(ctx, risk, claims, row, c,
                    seal, &terminal_at)) goto done;
            } else goto done;
        }
    }
    if (definition_count != 1 || (seal ? terminal_at != 0 :
        terminal_at != risk->sealed_terminals) ||
        (!seal && risk->terminal_count != risk->sealed_terminals)) goto done;
    ok = true;
done:
    free(offset); free(cursor); free(queue); free(contains); free(parents);
    return ok;
}

bool semantic_indexed_string_nonowner_prepare(SemanticContext *ctx,
    IndexedStringNonownerRisk *risk)
{
    if (ctx == NULL || ctx->has_error || risk == NULL || risk->prepared ||
        risk->claims != NULL || risk->terminal_count != 0 || risk->terminals != NULL ||
        risk->use_count == 0 || risk->use_count > risk->use_capacity ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_routine_inputs_current(ctx, risk->caller, true) ||
        !nonowner_birth_current(ctx, risk)) return false;
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving || store->validating_current ||
        !function_param_flow_program_point_index_build(store,
            (ASTNode *)risk->caller, &index) || index == NULL ||
        index->function_decl != risk->caller || index->function_id != risk->caller_id ||
        !function_param_flow_index_roots_current(index) ||
        !function_param_flow_index_claims_current(ctx, index)) return false;
    risk->claims = index->claims;
    risk->sealed_uses = risk->use_count;
    if (!nonowner_uses_current(ctx, risk, risk->claims) ||
        !nonowner_history(ctx, risk, risk->claims, true)) {
        free(risk->terminals); risk->terminals = NULL;
        risk->terminal_count = risk->terminal_capacity = 0;
        risk->sealed_uses = 0;
        risk->claims = NULL;
        return false;
    }
    risk->sealed_terminals = risk->terminal_count;
    risk->prepared = true;
    return true;
}

bool semantic_indexed_string_nonowner_current(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk)
{
    bool previous;
    if (risk == NULL || !risk->prepared || risk->claims == NULL ||
        risk->sealed_terminals != risk->terminal_count ||
        risk->sealed_uses != risk->use_count ||
        risk->terminal_count > risk->terminal_capacity ||
        (risk->terminal_count != 0 && risk->terminals == NULL) ||
        !function_param_flow_current_begin(ctx, &previous)) return false;
    bool ok = false;
    FunctionParamFlowSummaryStore *store = ctx->function_param_flow_summaries;
    if (!nonowner_birth_current(ctx, risk) ||
        !semantic_param_publication_routine_inputs_current(ctx, risk->caller, true)) goto done;
    for (size_t i = 0; i < store->program_point_index_count; i++) {
        FunctionParamFlowProgramPointIndex *index = &store->program_point_indexes[i];
        if (index->function_id != risk->caller_id) continue;
        if (index->function_decl != risk->caller || index->claims != risk->claims ||
            !function_param_flow_index_roots_current(index) ||
            !function_param_flow_index_claims_current(ctx, index)) break;
        ok = nonowner_uses_current(ctx, risk, risk->claims) &&
            nonowner_history(ctx, (IndexedStringNonownerRisk *)risk,
            risk->claims, false);
        break;
    }
done:
    function_param_flow_current_end(ctx, previous);
    return ok;
}

bool semantic_indexed_string_nonowner_syntax_current(
    const IndexedStringNonownerRisk *risk)
{
    if (risk == NULL || !risk->prepared || !nonowner_birth_syntax(risk) ||
        risk->claims == NULL ||
        risk->terminal_count != risk->sealed_terminals ||
        risk->terminal_count > risk->terminal_capacity ||
        (risk->terminal_count != 0 && risk->terminals == NULL) ||
        !function_param_flow_syntax_claims_current(NULL, risk->claims)) return false;
    return nonowner_uses_current(NULL, risk, risk->claims) &&
        nonowner_history(NULL, (IndexedStringNonownerRisk *)risk,
        risk->claims, false);
}

void semantic_indexed_string_nonowner_destroy(IndexedStringNonownerRisk *risk)
{
    if (risk == NULL) return;
    free(risk->binding_name);
    free(risk->terminals);
    free(risk->uses);
    free(risk);
}

bool semantic_indexed_string_nonowner_receipts(SemanticContext *ctx, bool prepare)
{
    if (ctx == NULL || ctx->has_error || !semantic_param_publication_ready(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t issued = 0;
    for (size_t i = 0; i < store->node_capacity; i++) {
        IndexedStringNonownerRisk *risk = store->nodes[i].indexed_string_nonowner;
        if (risk == NULL) continue;
        if (prepare && store->indexed_string_nonowner_count != 0 &&
            nonowner_promoted_to_owned(ctx, risk)) {
            store->nodes[i].indexed_string_nonowner = NULL;
            store->indexed_string_nonowner_count--;
            semantic_indexed_string_nonowner_destroy(risk);
            continue;
        }
        bool prepared = !prepare ||
            semantic_indexed_string_nonowner_prepare(ctx, risk);
        if (!prepared && prepare) {
            ASTNode *landing = NULL;
            if (nonowner_select_owned_pushes(ctx, risk, &landing)) {
                semantic_loan_copy_landing_error_with_hints(ctx,
                    PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
                    PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, landing,
                    "A borrowed String array without a complete exclusion proof stores owned copies");
                return false;
            }
        }
        if (issued == SIZE_MAX || !prepared ||
            !semantic_indexed_string_nonowner_current(ctx, risk)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)risk->definition,
                "An excluded String payload owner has no current non-owning history proof");
            return false;
        }
        issued++;
    }
    if (issued != store->indexed_string_nonowner_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            ctx->program_root, "A String non-owning risk receipt was withdrawn");
        return false;
    }
    return true;
}
