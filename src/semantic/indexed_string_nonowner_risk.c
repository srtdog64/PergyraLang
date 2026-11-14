#include "indexed_string_nonowner_risk_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array_storage_element_lifetime_owner.h"
#include "array_storage_release_owner.h"
#include "default_aggregate_move_rule.h"
#include "diag_codes.h"
#include "formal_string_store_landing_owner.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "indexed_string_borrow_owner.h"
#include "loan_copy_landing_owner.h"
#include "type_checker/type_checker_builtins_stdlib_collections_internal.h"
#include "type_checker/type_checker_internal.h"
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
    /* `var` grants permission, not evidence of a replacement. The sealed
     * nonowner_history rejects every write/escape that invalidates this birth. */
    if (risk == NULL || !nonowner_id(risk->caller, risk->caller_id) ||
        risk->caller->type != AST_FUNC_DECL ||
        !nonowner_id(ast_func_body(risk->caller), risk->body_id) ||
        !nonowner_id(risk->definition, risk->binding_id) ||
        risk->definition->type != AST_LET_DECL ||
        ast_let_is_alias(risk->definition) ||
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
        ast_let_is_alias(definition) ||
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

static bool nonowner_read_append(IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *row, size_t actual)
{
    if (risk == NULL || claims == NULL || claims->rows == NULL || row == NULL ||
        row < claims->rows || row >= claims->rows + claims->count)
        return false;
    size_t claim = (size_t)(row - claims->rows);
    ASTNode *callee = NULL;
    size_t formal = SIZE_MAX;
    bool physical = false;
    if (!function_param_flow_call_argument_claim_current(claims, claim,
            row->node, actual, &callee, &formal, &physical) ||
        callee == NULL || callee->type != AST_FUNC_DECL ||
        formal >= ast_func_param_count(callee))
        return false;
    FuncParam *parameter = ast_func_param(callee, formal);
    IndexedStringNonownerRead read = {
        row->node, callee, ast_node_stable_id(row->node),
        ast_node_stable_id(callee),
        parameter != NULL ? ast_func_param_stable_id(parameter) : 0,
        actual, claim, formal, physical
    };
    if (read.call_id == 0 || read.callee_id == 0 || read.formal_id == 0)
        return false;
    for (size_t i = 0; i < risk->read_count; i++) {
        const IndexedStringNonownerRead *prior = &risk->reads[i];
        if (prior->call != read.call || prior->actual_ordinal != actual)
            continue;
        return prior->callee == read.callee &&
            prior->call_id == read.call_id &&
            prior->callee_id == read.callee_id &&
            prior->formal_id == read.formal_id &&
            prior->claim_ordinal == read.claim_ordinal &&
            prior->formal_ordinal == read.formal_ordinal &&
            prior->physical == read.physical;
    }
    if (risk->read_count == SIZE_MAX ||
        risk->read_count + 1 > SIZE_MAX / sizeof(*risk->reads))
        return false;
    size_t count = risk->read_count + 1;
    IndexedStringNonownerRead *grown = realloc(risk->reads,
        count * sizeof(*grown));
    if (grown == NULL)
        return false;
    risk->reads = grown;
    risk->reads[risk->read_count++] = read;
    risk->read_capacity = count;
    return true;
}

static bool nonowner_read_matches(const IndexedStringNonownerRisk *risk,
    const ASTNode *call, size_t actual, size_t at)
{
    return risk != NULL && at < risk->read_count && risk->reads != NULL &&
        risk->reads[at].call == call &&
        risk->reads[at].actual_ordinal == actual;
}

static bool nonowner_read_current(const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *row, size_t actual, size_t *at)
{
    if (risk == NULL || claims == NULL || row == NULL || at == NULL ||
        !nonowner_read_matches(risk, row->node, actual, *at))
        return false;
    const IndexedStringNonownerRead *saved = &risk->reads[(*at)++];
    ASTNode *callee = NULL;
    size_t formal = SIZE_MAX;
    bool physical = false;
    if (!function_param_flow_call_argument_claim_current(claims,
            saved->claim_ordinal, row->node, actual, &callee, &formal,
            &physical) || callee == NULL || callee->type != AST_FUNC_DECL ||
        formal >= ast_func_param_count(callee))
        return false;
    FuncParam *parameter = ast_func_param(callee, formal);
    return saved->call_id == ast_node_stable_id(row->node) &&
        saved->callee == callee && saved->callee_id == ast_node_stable_id(callee) &&
        saved->formal_ordinal == formal &&
        saved->formal_id != 0 && parameter != NULL &&
        saved->formal_id == ast_func_param_stable_id(parameter) &&
        saved->physical == physical;
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

/* Owned promotion may terminate when the exact nominal constructor field
 * takes the Array<String> descriptor and its owned-element receipt. Recheck
 * the transferred receipt here; the aggregate lifecycle owns the field after
 * this handoff, so the source local leaves the non-owning fact family. */
static bool nonowner_owned_constructor_transfer_current(SemanticContext *ctx,
    const FunctionParamFlowIndexClaim *call_row, size_t actual)
{
    if (ctx == NULL || call_row == NULL ||
        call_row->shape != AST_CALL || call_row->node == NULL ||
        call_row->collection_effect_present)
        return false;
    ASTNode *call = call_row->node;
    if (actual >= ast_call_arg_count(call) ||
        !semantic_param_publication_nominal_result_complete(ctx, call) ||
        !semantic_param_publication_checked_argument(ctx, call, actual))
        return false;
    PublishedNode *published = publication_result_current(ctx, call);
    ASTNode *argument = ast_call_argument(call, actual);
    const SemanticNominalResultSnapshot *snapshot = published != NULL
        ? published->nominal_result : NULL;
    const SemanticNominalResultField *field = snapshot != NULL &&
        actual < snapshot->field_count ? &snapshot->fields[actual] : NULL;
    bool depends = false, retains = true;
    return argument != NULL && field != NULL &&
        field->string_array_elements_present &&
        field->string_array_elements.owned &&
        semantic_param_publication_checked_result_relation(ctx, call,
            actual, &depends, &retains) && depends && !retains &&
        semantic_array_storage_transferred_element_use_current(argument, ctx,
            &field->string_array_elements);
}

/* The inline checker snapshots nominal result fields before whole-routine
 * flow closure. Validate the exact field and a hypothetical owned-element
 * receipt here without mutating the snapshot. A caller below still has to
 * prove whether this result stays local or transfers through a return. */
static bool nonowner_nominal_transfer_candidate_current(
    SemanticContext *ctx, const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaim *call_row, size_t actual)
{
    if (ctx == NULL || risk == NULL || call_row == NULL ||
        call_row->shape != AST_CALL || call_row->node == NULL ||
        call_row->collection_effect_present ||
        actual >= ast_call_arg_count(call_row->node) ||
        !semantic_param_publication_nominal_result_complete(ctx,
            call_row->node) ||
        !semantic_param_publication_checked_argument(ctx, call_row->node,
            actual))
        return false;

    ASTNode *argument = ast_call_argument(call_row->node, actual);
    PublishedNode *published = publication_result_current(ctx,
        call_row->node);
    const SemanticNominalResultSnapshot *snapshot = published != NULL
        ? published->nominal_result : NULL;
    const SemanticNominalResultField *field = snapshot != NULL &&
        actual < snapshot->field_count ? &snapshot->fields[actual] : NULL;
    bool depends = false, retains = true;
    bool relation = semantic_param_publication_checked_result_relation(ctx,
        call_row->node, actual, &depends, &retains);
    bool candidate = argument != NULL && argument->type == AST_IDENTIFIER &&
        ast_identifier_binding_syntax_id(argument) == risk->binding_id &&
        field != NULL && field->string_array_elements_present &&
        !field->string_array_elements.owned &&
        field->string_array_elements.function_id == risk->caller_id &&
        field->string_array_elements.binding_id == risk->binding_id &&
        field->string_array_elements.dependency_call == NULL &&
        field->string_array_elements.dependency_call_id == 0 &&
        !semantic_loan_copy_landing_selected(ctx, argument) &&
        !semantic_loan_copy_landing_selected(ctx, call_row->node) &&
        relation && depends && !retains;
    if (!candidate) return false;
    ArrayStorageReturnElementReceipt promoted =
        field->string_array_elements;
    promoted.owned = true;
    return semantic_array_storage_transferred_element_use_current(argument,
        ctx, &promoted);
}

static bool nonowner_terminal_nominal_transfer_candidate_current(
    SemanticContext *ctx, const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *call_row, size_t actual)
{
    return claims != NULL && call_row != NULL && call_row->node != NULL &&
        nonowner_terminal_statement(claims, call_row->node) != NULL &&
        nonowner_nominal_transfer_candidate_current(ctx, risk, call_row,
            actual);
}

/* A local nominal result may observe the same backing without becoming a
 * second owner. The sparse backing proof must show that the exact result
 * never leaves this routine and that mutation does not invalidate its
 * source. Its field snapshot remains non-owning. */
static bool nonowner_nominal_local_carrier_current(
    SemanticContext *ctx, const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaim *call_row, size_t actual)
{
    return nonowner_nominal_transfer_candidate_current(ctx, risk, call_row,
            actual) &&
        function_param_flow_call_result_stays(ctx,
            (ASTNode *)risk->caller, call_row->node);
}

/* A constructor stored in a local can still be the eventual returned owner.
 * Accept that transfer only when the existing sparse backing owner proves
 * RETURN_ESCAPE is the result's sole outward effect. The publication owner
 * promotes the exact field only after every use of this source was checked. */
static bool nonowner_returned_nominal_transfer_candidate_current(
    SemanticContext *ctx, const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaim *call_row, size_t actual)
{
    return nonowner_nominal_transfer_candidate_current(ctx, risk, call_row,
            actual) &&
        function_param_flow_call_result_returns_current(ctx,
            (ASTNode *)risk->caller, call_row->node);
}

/* An ordinary call owns a transfer only through the completed parameter-flow
 * summary and its typed path into the declared result. The moving-formal rule
 * establishes descriptor transfer; the path establishes that the same
 * backing reaches the result. A selected landing copy belongs to the next
 * analysis generation and therefore cannot authorize this transfer. */
static bool nonowner_declared_result_transfer_current(
    SemanticContext *ctx, const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaim *call_row, size_t actual)
{
    if (ctx == NULL || risk == NULL || call_row == NULL ||
        call_row->shape != AST_CALL || call_row->node == NULL ||
        call_row->collection_effect_present ||
        actual >= ast_call_arg_count(call_row->node) ||
        !semantic_param_publication_checked_argument(ctx, call_row->node,
            actual))
        return false;

    ASTNode *argument = ast_call_argument(call_row->node, actual);
    size_t formal = SIZE_MAX;
    ASTNode *callee = semantic_param_publication_callee(ctx,
        call_row->node, actual);
    if (argument == NULL || argument->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(argument) != risk->binding_id ||
        callee == NULL || callee->type != AST_FUNC_DECL ||
        !semantic_param_publication_formal_for_actual(ctx, call_row->node,
            actual, &formal) || formal >= ast_func_param_count(callee) ||
        !semantic_default_aggregate_formal_moves(ctx, callee, formal) ||
        semantic_loan_copy_landing_selected(ctx, argument) ||
        semantic_loan_copy_landing_selected(ctx, call_row->node))
        return false;

    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *store =
        function_param_flow_store_get(&lookup);
    FunctionParamFlowResultPath path = {0};
    return store != NULL && !store->failed &&
        function_param_flow_call_result_path_current(store,
            call_row->node, actual, &path) && path.present &&
        function_param_flow_call_result_consumed_current(store,
            (ASTNode *)risk->caller, call_row->node);
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

static bool nonowner_declared_read_preserves_elements(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, ASTNode *call, size_t actual);

static bool nonowner_checked_call(SemanticContext *ctx,
    IndexedStringNonownerRisk *risk, const FunctionParamFlowIndexClaims *claims,
    const FunctionParamFlowIndexClaim *row, size_t child, bool seal,
    size_t *terminal_at, size_t *read_at)
{
    ASTNode *call = row->node;
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
    /* A declared reader is the same observational use as the ArrayLength
     * intrinsic only when the parameter-flow owner proves that this exact
     * formal neither mutates, releases, retains nor forwards the elements.
     * Seal that verdict against the typed call claim so the moved snapshot can
     * replay it after the live publication store has been retired. */
    size_t actual = child - 1;
    bool saved_read = !seal && read_at != NULL &&
        nonowner_read_matches(risk, call, actual, *read_at);
    bool live_read = ctx != NULL &&
        nonowner_declared_read_preserves_elements(ctx, risk, call, actual);
    if (saved_read || live_read) {
        if (read_at == NULL || (ctx != NULL && !live_read))
            return false;
        if (seal) {
            if (!nonowner_read_append(risk, claims, row, actual))
                return false;
            (*read_at)++;
            return true;
        }
        return saved_read &&
            nonowner_read_current(risk, claims, row, actual, read_at);
    }
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
    return (function_param_flow_summary_for_param(ctx, callee, formal) &
        ~SLOT_PARAM_SUMMARY_READ) == 0;
}

/* An explicit own formal is the next element-cleanup owner.  Once every
 * transition that populated this exact local is already an owned String
 * store, handing the descriptor to that formal retires the caller's receipt
 * regardless of whether the callee returns a value.  The call checker owns
 * later-use rejection; this consumer only connects its admitted transfer to
 * the String-array element lifecycle. */
static bool nonowner_declared_own_handoff_current(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, ASTNode *call, size_t actual)
{
    if (ctx == NULL || risk == NULL || call == NULL ||
        call->type != AST_CALL || actual >= ast_call_arg_count(call) ||
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
    SemanticPublishedValue value = semantic_param_publication_value(ctx,
        argument);
    return parameter != NULL && parameter->mode == PARAM_MODE_OWN &&
        ast_func_param_stable_id(parameter) != 0 && value.ok && !value.meta &&
        type_equals(value.type, risk->array_type) &&
        semantic_default_aggregate_formal_moves(ctx, callee, formal);
}

/* The inventory includes every unknown String use observed while this empty
 * receiver was in scope, including uses unrelated to the receiver.  Promotion
 * needs those rows to remain structurally current, but it must not re-prove
 * their old non-owning classification after a landing round.  The effect scan
 * below separately proves that every operation touching the receiver is an
 * owned transition. */
static bool nonowner_promoted_use_current(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims,
    const IndexedStringNonownerUse *use)
{
    if (ctx == NULL || risk == NULL || claims == NULL || use == NULL ||
        !nonowner_id(use->value, use->value_id) ||
        use->value->type != use->shape ||
        (use->shape == AST_IDENTIFIER
            ? ast_identifier_binding_syntax_id(use->value) != use->binding_id
            : use->binding_id != 0) ||
        use->sink == NULL || use->sink->type != AST_CALL ||
        !nonowner_id(use->sink, use->sink_id) ||
        use->actual_ordinal >= ast_call_arg_count(use->sink) ||
        ast_call_argument(use->sink, use->actual_ordinal) != use->value)
        return false;

    const FunctionParamFlowIndexClaim *value = NULL, *sink = NULL;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->node == use->value) {
            if (value != NULL)
                return false;
            value = row;
        }
        if (row->node == use->sink) {
            if (sink != NULL)
                return false;
            sink = row;
        }
    }
    if (value == NULL || value->id != use->value_id ||
        value->shape != use->shape || value->binding != use->binding_id ||
        !value->semantic_promoted || !value->snapshot_semantic_complete ||
        !value->value_ready || value->value_meta ||
        value->value_type != TYPE_STRING || sink == NULL ||
        sink->id != use->sink_id || sink->shape != AST_CALL ||
        use->actual_ordinal + 1 >= sink->child_count ||
        sink->children[use->actual_ordinal + 1] !=
            use->value ||
        sink->child_ids[use->actual_ordinal + 1] != use->value_id ||
        (ast_call_semantic_callee_declared_callable(use->sink) &&
            !semantic_param_publication_checked_argument(ctx, use->sink,
                use->actual_ordinal)))
        return false;
    return true;
}

/* Resolve this local risk only after the current function index accounts for
 * every use of the exact receiver.  Owned-copy push/set transitions promote
 * its payload lifecycle.  A still-empty receiver may instead leave the frame
 * through one exact nominal result path; that transfers the descriptor
 * without upgrading the result field's non-owning element receipt.  The
 * latter path rejects every collection release or payload transition, so it
 * cannot turn an emptied or populated receiver into a vacuous transfer.
 *
 * The collection-effect and nominal-result owners are the authorities for
 * those classifications.  The UNKNOWN String-use inventory can include
 * unrelated values in the routine; it is checked for structural currency but
 * is not paired to receiver effects by position.  Mixed, missing or malformed
 * evidence remains fail-closed. */
static bool nonowner_resolved_by_transfer_or_owned_after_selected_edges(
    SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk)
{
    if (ctx == NULL || ctx->has_error || risk == NULL || risk->prepared ||
        risk->claims != NULL || risk->use_count == 0 ||
        risk->use_count > risk->use_capacity || risk->uses == NULL ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_routine_inputs_current(ctx,
            risk->caller, true) || !nonowner_birth_current(ctx, risk))
        return false;

    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving ||
        store->validating_current ||
        !function_param_flow_program_point_index_build(store,
            (ASTNode *)risk->caller, &index) || index == NULL ||
        index->function_decl != risk->caller ||
        index->function_id != risk->caller_id || index->claims == NULL ||
        index->claims->failed)
        return false;

    const FunctionParamFlowIndexClaims *claims = index->claims;
    if (claims->failed || claims->rows == NULL || claims->count == 0)
        return false;

    for (size_t u = 0; u < risk->use_count; u++)
        if (!nonowner_promoted_use_current(ctx, risk, claims,
                &risk->uses[u]))
            return false;

    /* This conservative inventory includes unknown String uses that are not
     * uses of the empty receiver itself.  If every such exact use already
     * has an independent Clone selected, the next generation has no
     * non-owner risk to promote.  This is discovery only; that generation
     * rebuilds the inventory and remains the proof owner. */
    bool all_uses_selected = true;
    for (size_t u = 0; u < risk->use_count; u++) {
        const ASTNode *value = risk->uses[u].value;
        if (!semantic_loan_copy_landing_selected(ctx, value)
            || !semantic_loan_copy_landing_carrier_copies(ctx, value)) {
            all_uses_selected = false;
            break;
        }
    }
    if (all_uses_selected) return true;

    size_t owned_transition_count = 0;
    size_t receiver_transition_count = 0;
    size_t owned_constructor_transfer_count = 0;
    size_t nominal_transfer_candidate_count = 0;
    size_t descriptor_transfer_count = 0;
    size_t declared_own_handoff_count = 0;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->shape != AST_CALL) continue;
        uint32_t kind = 0, receiver = 0, source = 0;
        bool effect = ast_call_semantic_collection_effect(row->node,
            &kind, &receiver, &source);
        if (effect != row->collection_effect_present ||
            kind != row->collection_effect_kind ||
            receiver != row->collection_effect_receiver ||
            source != row->collection_effect_source)
            return false;
        bool touches = effect && (receiver == risk->binding_id ||
            source == risk->binding_id);
        if (touches) {
            if (receiver_transition_count == SIZE_MAX)
                return false;
            receiver_transition_count++;
            ASTNode *effect_receiver = ast_call_arg_count(row->node) != 0
                ? ast_call_argument(row->node, 0) : NULL;
            bool push =
                semantic_collection_owned_string_push_effect_current(
                    row->node, effect_receiver, ctx);
            bool set = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_SET &&
                ast_call_arg_count(row->node) == 3;
            bool pop =
                semantic_collection_owned_string_pop_effect_current(
                    row->node, effect_receiver, ctx);
            bool drop = kind == PGY_COLLECTION_EFFECT_DROP &&
                ast_call_arg_count(row->node) == 1;
            if ((!push && !set && !pop && !drop) ||
                receiver != risk->binding_id ||
                ((set || pop || drop) && source != 0) ||
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
            /* The typed publication relation is the authority for allocating
             * readers such as StringJoin: no collection mutation, no retained
             * input and no result dependence means the owned receiver remains
             * the sole payload owner.  Do not recreate this from a callee
             * spelling in the String-array consumer. */
            bool result_depends = true, external_retains = true;
            if (!row->collection_effect_present &&
                semantic_param_publication_checked_result_relation(ctx,
                    row->node, actual, &result_depends,
                    &external_retains) &&
                !result_depends && !external_retains)
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
            if (nonowner_declared_own_handoff_current(ctx, risk,
                    row->node, actual)) {
                if (descriptor_transfer_count == SIZE_MAX ||
                    declared_own_handoff_count == SIZE_MAX)
                    return false;
                descriptor_transfer_count++;
                declared_own_handoff_count++;
                continue;
            }
            /* This prepare pass already belongs to a failed landing
             * generation.  An exact selected Clone may therefore be
             * projected only to discover later plans; the reloaded AST is
             * still the sole proof that the use became independent. */
            if (semantic_loan_copy_landing_satisfied(ctx, row->node))
                continue;
            if (nonowner_owned_constructor_transfer_current(ctx, row,
                    actual)) {
                if (descriptor_transfer_count == SIZE_MAX ||
                    owned_constructor_transfer_count == SIZE_MAX)
                    return false;
                descriptor_transfer_count++;
                owned_constructor_transfer_count++;
                continue;
            }
            if (nonowner_nominal_local_carrier_current(ctx, risk, row,
                    actual)) {
                if (descriptor_transfer_count == SIZE_MAX) return false;
                descriptor_transfer_count++;
                continue;
            }
            if (nonowner_declared_result_transfer_current(ctx, risk, row,
                    actual)) {
                if (descriptor_transfer_count == SIZE_MAX) return false;
                descriptor_transfer_count++;
                continue;
            }
            bool nominal_transfer =
                nonowner_terminal_nominal_transfer_candidate_current(
                    ctx, risk, claims, row, actual) ||
                nonowner_returned_nominal_transfer_candidate_current(
                    ctx, risk, row, actual);
            if (!nominal_transfer ||
                nominal_transfer_candidate_count == SIZE_MAX ||
                descriptor_transfer_count == SIZE_MAX)
                return false;
            nominal_transfer_candidate_count++;
            descriptor_transfer_count++;
        }
    }

    if (owned_transition_count == 0) {
        bool exact_own_handoff = receiver_transition_count == 0
            && declared_own_handoff_count == 1
            && descriptor_transfer_count == 1
            && owned_constructor_transfer_count == 0
            && nominal_transfer_candidate_count == 0;
        bool exact_owned_constructor = receiver_transition_count == 0
            && declared_own_handoff_count == 0
            && descriptor_transfer_count == 1
            && owned_constructor_transfer_count == 1
            && nominal_transfer_candidate_count == 0;
        bool exact_nominal_transfer = receiver_transition_count == 0
            && declared_own_handoff_count == 0
            && descriptor_transfer_count == 1
            && owned_constructor_transfer_count == 0
            && nominal_transfer_candidate_count == 1;
        /* With no payload transition, one proven-empty descriptor may leave
         * through exactly one cleanup owner.  The constructor path replays an
         * already-owned field receipt; the nominal path is promoted below
         * only after its complete result path is replayed.  Any prior release,
         * population, duplicate destination or detached result stays outside
         * these exact shapes. */
        if (!exact_own_handoff && !exact_owned_constructor &&
            !exact_nominal_transfer)
            return false;
    }
    /* Owned stores promote the payload lifecycle before descriptor
     * retirement.  More than one statically present `own` call can then be
     * valid on mutually exclusive exits; the call checker already owns
     * path-sensitive move and later-use rejection for each admitted handoff.
     * Do not turn that admitted control-flow fact back into a whole-routine
     * call-count restriction here. */

    size_t promoted = 0;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->shape != AST_CALL) continue;
        for (size_t actual = 0; actual < ast_call_arg_count(row->node);
             actual++) {
            bool nominal_transfer =
                nonowner_terminal_nominal_transfer_candidate_current(
                    ctx, risk, claims, row, actual) ||
                nonowner_returned_nominal_transfer_candidate_current(
                    ctx, risk, row, actual);
            if (!nominal_transfer)
                continue;
            if (!publication_promote_exact_nominal_string_array_transfer(
                    ctx, row->node, actual, risk->caller_id,
                    risk->binding_id) ||
                !nonowner_owned_constructor_transfer_current(ctx, row,
                    actual) || promoted == SIZE_MAX)
                return false;
            promoted++;
        }
    }
    return promoted == nominal_transfer_candidate_count;
}

/* When the exclusion proof itself is insufficient, upgrade every plain
 * String push into this receiver in the next landing round. The existing
 * landing owner records all pushes for the binding together, so the next
 * analysis cannot mix borrowed and owned elements. */
static bool nonowner_select_owned_pushes(SemanticContext *ctx,
    const IndexedStringNonownerRisk *risk, ASTNode **first)
{
    bool timing = getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL;
    ASTNode *selection_seed = NULL;
    bool projected_owned_push = false;
    if (first != NULL) *first = NULL;
    if (ctx == NULL || ctx->has_error || risk == NULL || first == NULL ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_param_publication_routine_inputs_current(ctx,
            risk->caller, true) || !nonowner_birth_current(ctx, risk)) {
        if (timing && risk != NULL)
            fprintf(stderr,
                "[semantic timing] nonowner_select_owned=failure stage=precondition binding=%u error=%d inputs=%d routine=%d birth=%d\n",
                risk->binding_id, ctx != NULL && ctx->has_error ? 1 : 0,
                semantic_param_publication_inputs_ready(ctx) ? 1 : 0,
                ctx != NULL && semantic_param_publication_routine_inputs_current(
                    ctx, risk->caller, true) ? 1 : 0,
                ctx != NULL && nonowner_birth_current(ctx, risk) ? 1 : 0);
        return false;
    }
    SlotFunctionLookup lookup = {ctx, ctx->program_root};
    FunctionParamFlowSummaryStore *store = function_param_flow_store_get(&lookup);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving ||
        store->validating_current ||
        !function_param_flow_program_point_index_build(store,
            (ASTNode *)risk->caller, &index) || index == NULL ||
        index->function_decl != risk->caller ||
        index->function_id != risk->caller_id || index->claims == NULL ||
        index->claims->failed) {
        if (timing)
            fprintf(stderr,
                "[semantic timing] nonowner_select_owned=failure stage=index binding=%u\n",
                risk->binding_id);
        return false;
    }

    for (size_t i = 0; i < index->claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &index->claims->rows[i];
        if (row->shape != AST_CALL) continue;
        uint32_t kind = 0, receiver = 0, source = 0;
        bool effect = ast_call_semantic_collection_effect(row->node,
            &kind, &receiver, &source);
        if (effect != row->collection_effect_present ||
            kind != row->collection_effect_kind ||
            receiver != row->collection_effect_receiver ||
            source != row->collection_effect_source) {
            if (timing)
                fprintf(stderr,
                    "[semantic timing] nonowner_select_owned=failure stage=effect binding=%u node=%u\n",
                    risk->binding_id, row->id);
            return false;
        }
        if (!effect || (receiver != risk->binding_id &&
            source != risk->binding_id)) continue;

        const ASTNode *callee = ast_call_callee(row->node);
        const char *name = callee != NULL && callee->type == AST_IDENTIFIER
            ? ast_identifier_name(callee) : NULL;
        bool selected_owned_push =
            kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
            name != NULL && strcmp(name, "ArrayPush") == 0 &&
            ast_call_arg_count(row->node) == 2 && source == 0 &&
            semantic_formal_string_store_landing_selected_owned_push(
                ctx, row->node);
        bool plain = kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
            name != NULL && strcmp(name, "ArrayPush") == 0 &&
            ast_call_arg_count(row->node) == 2 && row->private_array_push &&
            source == 0;
        size_t stored = SIZE_MAX;
        bool plain_set = kind == PGY_COLLECTION_EFFECT_SHALLOW_MUTATION &&
            name != NULL && strcmp(name, "ArraySet") == 0 &&
            ast_call_arg_count(row->node) == 3 && source == 0 &&
            semantic_param_publication_checked_array_store(
                ctx, row->node, &stored) && stored == 2;
        bool owned_push =
            semantic_collection_owned_string_push_effect_current(
                row->node, ast_call_arg_count(row->node) != 0
                    ? ast_call_argument(row->node, 0) : NULL, ctx);
        bool owned_set = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_SET &&
            ast_call_arg_count(row->node) == 3 && source == 0;
        bool owned_pop = kind == PGY_COLLECTION_EFFECT_OWNED_STRING_POP &&
            ast_call_arg_count(row->node) == 1 && source == 0 &&
            semantic_collection_owned_string_pop_effect_current(
                row->node, ast_call_argument(row->node, 0), ctx);
        bool drop = kind == PGY_COLLECTION_EFFECT_DROP &&
            ast_call_arg_count(row->node) == 1 && source == 0;
        if ((!plain && !selected_owned_push && !plain_set && !owned_push && !owned_set &&
                !owned_pop && !drop) ||
            receiver != risk->binding_id ||
            ast_call_argument(row->node, 0) == NULL ||
            ast_call_argument(row->node, 0)->type != AST_IDENTIFIER ||
            ast_identifier_binding_syntax_id(
                ast_call_argument(row->node, 0)) != risk->binding_id) {
            if (timing)
                fprintf(stderr,
                    "[semantic timing] nonowner_select_owned=failure stage=shape binding=%u node=%u kind=%u receiver=%u source=%u\n",
                    risk->binding_id, row->id, kind, receiver, source);
            return false;
        }
        if (selected_owned_push) {
            projected_owned_push = true;
            if (*first == NULL) *first = row->node;
            continue;
        }
        if (plain) {
            if (*first == NULL) *first = row->node;
            if (selection_seed == NULL) selection_seed = row->node;
        }
    }
    bool selected = selection_seed != NULL
        ? semantic_formal_string_store_landing_select_owned(
            ctx, selection_seed)
        : projected_owned_push;
    if (timing && !selected)
        fprintf(stderr,
            "[semantic timing] nonowner_select_owned=failure stage=select binding=%u first=%u\n",
            risk->binding_id,
            *first != NULL ? ast_node_stable_id(*first) : 0);
    return selected;
}

/* Use the original captured child edges, including dormant branches. The
 * transient reverse graph identifies capture and write ancestors without an
 * AST walk or a separate persistent proof ledger. */
static bool nonowner_history(SemanticContext *ctx, IndexedStringNonownerRisk *risk,
    const FunctionParamFlowIndexClaims *claims, bool seal, bool copy_rebound_uses)
{
    if (claims == NULL || claims->failed || claims->rows == NULL || claims->count == 0 ||
        claims->count > SIZE_MAX / sizeof(size_t) - 1) return false;
    size_t n = claims->count, edges = 0, terminal_at = 0, read_at = 0;
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
    size_t definition_count = 0, owned_rebindings = 0;
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
            if (row->child_count < 2 || row->child_claim_ordinals[0] >= n)
                goto done;
            if (contains[row->child_claim_ordinals[0]]) {
                /* This is a copy-plan premise only. A changed descriptor
                 * never receives the original nonowner receipt. Require an
                 * independent replacement and keep every other history
                 * check, including discarded transfers and captures. */
                if (!copy_rebound_uses || ctx == NULL || row->child_count != 2)
                    goto done;
                const ASTNode *replacement = row->children[1];
                uint32_t builtin = 0;
                bool depends = true, retains = true;
                SemanticPublishedValue value =
                    semantic_param_publication_value(ctx, replacement);
                bool independent = ctx != NULL && replacement != NULL &&
                    replacement->type == AST_CALL &&
                    ast_call_semantic_callee_builtin_kind(replacement, &builtin) &&
                    builtin == BUILTIN_CLONE &&
                    ast_call_semantic_callee_is_stdlib(replacement) &&
                    ast_call_arg_count(replacement) == 1 && value.ok && !value.meta &&
                    nonowner_array_type(value.type) &&
                    semantic_param_publication_clone_independent(ctx, value.type) &&
                    semantic_param_publication_checked_result_relation(ctx,
                        replacement, 0, &depends, &retains) && !depends && !retains;
                if (!independent ||
                    row->children[0] == NULL ||
                    row->children[0]->type != AST_IDENTIFIER ||
                    ast_identifier_binding_syntax_id(row->children[0]) !=
                        risk->binding_id)
                    goto done;
                owned_rebindings++;
                continue;
            }
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
                    seal, &terminal_at, &read_at)) goto done;
            } else goto done;
        }
    }
    if ((copy_rebound_uses && owned_rebindings == 0) ||
        definition_count != 1 || (seal ? terminal_at != 0 :
        terminal_at != risk->sealed_terminals) ||
        (!seal && risk->terminal_count != risk->sealed_terminals) ||
        (seal ? read_at != risk->read_count :
            read_at != risk->sealed_reads) ||
        (!seal && risk->read_count != risk->sealed_reads)) goto done;
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
        risk->read_count != 0 || risk->reads != NULL || risk->sealed_reads != 0 ||
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
        index->function_decl != risk->caller || index->function_id != risk->caller_id)
        return false;
    risk->claims = index->claims;
    risk->sealed_uses = risk->use_count;
    if (!nonowner_uses_current(ctx, risk, risk->claims) ||
        !nonowner_history(ctx, risk, risk->claims, true, false)) {
        free(risk->terminals); risk->terminals = NULL;
        risk->terminal_count = risk->terminal_capacity = 0;
        free(risk->reads); risk->reads = NULL;
        risk->read_count = risk->read_capacity = risk->sealed_reads = 0;
        risk->sealed_uses = 0;
        risk->claims = NULL;
        return false;
    }
    risk->sealed_terminals = risk->terminal_count;
    risk->sealed_reads = risk->read_count;
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
        risk->read_count != risk->sealed_reads ||
        risk->read_count > risk->read_capacity ||
        (risk->read_count != 0 && risk->reads == NULL) ||
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
            risk->claims, false, false);
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
        risk->read_count != risk->sealed_reads ||
        risk->read_count > risk->read_capacity ||
        (risk->read_count != 0 && risk->reads == NULL) ||
        !function_param_flow_syntax_claims_current(NULL, risk->claims)) return false;
    return nonowner_uses_current(NULL, risk, risk->claims) &&
        nonowner_history(NULL, (IndexedStringNonownerRisk *)risk,
        risk->claims, false, false);
}

void semantic_indexed_string_nonowner_destroy(IndexedStringNonownerRisk *risk)
{
    if (risk == NULL) return;
    free(risk->binding_name);
    free(risk->terminals);
    free(risk->uses);
    free(risk->reads);
    free(risk);
}

bool semantic_indexed_string_nonowner_receipts(SemanticContext *ctx, bool prepare)
{
    bool ready = ctx != NULL && semantic_param_publication_ready(ctx);
    if (ctx == NULL || ctx->has_error || !ready) {
        if (getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL)
            fprintf(stderr,
                "[semantic timing] nonowner_receipts=guard ctx=%d error=%d ready=%d copies=%zu stores=%zu\n",
                ctx != NULL ? 1 : 0,
                ctx != NULL && ctx->has_error ? 1 : 0, ready ? 1 : 0,
                ctx != NULL ? ctx->loan_copy_landing_selected_count : 0,
                ctx != NULL ? ctx->string_store_landing_selected_count : 0);
        return false;
    }
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t issued = 0;
    ASTNode *first_landing = NULL;
    for (size_t i = 0; i < store->node_capacity; i++) {
        IndexedStringNonownerRisk *risk = store->nodes[i].indexed_string_nonowner;
        if (risk == NULL) continue;
        if (prepare && store->indexed_string_nonowner_count != 0 &&
            nonowner_resolved_by_transfer_or_owned_after_selected_edges(
                ctx, risk)) {
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
                /* The deferred landing diagnostic deliberately leaves the
                 * context usable for discovery. Select every exact current
                 * risk before emitting it so one compiler analysis lands the
                 * complete publication generation, not one routine at a
                 * time. The reloaded AST remains the only proof that any
                 * selected shallow push became an owned store. */
                if (first_landing == NULL) first_landing = landing;
                continue;
            }
            /* Permission to rebind an empty Array is not a lifetime proof.
             * If its complete history cannot exclude it as a payload owner,
             * land independent Strings at every exact use instead. This
             * failed generation only selects copies; the reloaded AST and
             * ordinary source-lifetime checks must admit the next one. */
            SlotFunctionLookup lookup = {ctx, ctx->program_root};
            FunctionParamFlowSummaryStore *flow =
                function_param_flow_store_get(&lookup);
            FunctionParamFlowProgramPointIndex *index = NULL;
            bool uses_current = flow != NULL && !flow->failed &&
                !flow->solving && !flow->validating_current &&
                nonowner_birth_current(ctx, risk) &&
                semantic_param_publication_routine_inputs_current(
                    ctx, risk->caller, true) &&
                function_param_flow_program_point_index_build(flow,
                    (ASTNode *)risk->caller, &index) && index != NULL &&
                index->function_decl == risk->caller &&
                index->function_id == risk->caller_id &&
                index->claims != NULL && !index->claims->failed &&
                risk->use_count != 0 && risk->uses != NULL &&
                risk->use_count <= risk->use_capacity;
            for (size_t u = 0; uses_current && u < risk->use_count; u++)
                uses_current = nonowner_promoted_use_current(ctx, risk,
                    index->claims, &risk->uses[u]) &&
                    nonowner_raw_use_current(ctx, &risk->uses[u]) &&
                    semantic_loan_copy_landing_carrier_copies(
                        ctx, risk->uses[u].value);
            if (uses_current && nonowner_history(ctx, risk,
                    index->claims, true, true)) {
                bool selected = true;
                for (size_t u = 0; u < risk->use_count; u++)
                    if (!semantic_loan_copy_landing_select_string(
                            ctx, risk->uses[u].value)) selected = false;
                if (selected) {
                    if (first_landing == NULL)
                        first_landing = (ASTNode *)risk->uses[0].value;
                    continue;
                }
            }
        }
        if (issued == SIZE_MAX || !prepared ||
            !semantic_indexed_string_nonowner_current(ctx, risk)) {
            if (getenv("PGY_DEBUG_SEMANTIC_TIMING") != NULL) {
                fprintf(stderr,
                    "[semantic timing] nonowner_receipts=failure binding=%u name=%s line=%d prepared=%d issued=%zu uses=%zu copies=%zu stores=%zu\n",
                    risk->binding_id,
                    risk->binding_name != NULL ? risk->binding_name : "<none>",
                    risk->definition != NULL ? risk->definition->line : 0,
                    prepared ? 1 : 0, issued, risk->use_count,
                    ctx->loan_copy_landing_selected_count,
                    ctx->string_store_landing_selected_count);
                for (size_t u = 0; u < risk->use_count; u++)
                    fprintf(stderr,
                        "[semantic timing] nonowner_use binding=%u value=%u sink=%u actual=%zu selected_value=%d selected_sink=%d\n",
                        risk->binding_id, risk->uses[u].value_id,
                        risk->uses[u].sink_id,
                        risk->uses[u].actual_ordinal,
                        semantic_loan_copy_landing_selected(
                            ctx, risk->uses[u].value) ? 1 : 0,
                        semantic_loan_copy_landing_selected(
                            ctx, risk->uses[u].sink) ? 1 : 0);
            }
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
                (ASTNode *)risk->definition,
                "An excluded String payload owner has no current non-owning history proof");
            return false;
        }
        issued++;
    }
    if (first_landing != NULL) {
        semantic_loan_copy_landing_error_with_hints(ctx,
            PGY_CODE_SEM_BORROW_ESCAPE, PGY_CAUSE_BORROW_ESCAPE,
            PGY_FIX_USE_MOVE_OR_RETAIN_BINDING, first_landing,
            "A String use without a complete exclusion proof requires an independent owned copy");
        return false;
    }
    if (issued != store->indexed_string_nonowner_count) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_USE_MOVE_OR_RETAIN_BINDING,
            ctx->program_root, "A String non-owning risk receipt was withdrawn");
        return false;
    }
    return true;
}
