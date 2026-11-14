#include "indexed_string_option_payload.h"
#include "function_param_flow/function_param_publication_internal.h"
#include "indexed_string_borrow_owner.h"
#include "indexed_string_permanent_element.h"
#include "type_checker/type_checker_internal.h"
#include "../common/match_variant_policy.h"
#include <stdlib.h>
#include <string.h>

static bool option_type(const Type *type, const Type *payload)
{ return type_is_constructed_named(type, "Option") &&
    type_constructed_arg_count(type) == 1 && type_constructed_arg(type, 0) == payload; }

static bool option_intrinsic(SemanticContext *ctx, const ASTNode *call, const char *name)
{
    if (ctx == NULL || call == NULL || call->type != AST_CALL || ast_call_arg_count(call) != 1 ||
        ast_call_generic_arg_count(call) != 0 || ast_call_argument_name(call, 0) != NULL) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    const ASTNode *target = ast_call_callee(call);
    uint32_t kind = 0;
    return row != NULL && row->node == call && row->id != 0 && row->id == ast_node_stable_id(call) &&
        row->call_present && row->kind_present && row->builtin == BUILTIN_NOT_BUILTIN &&
        row->stdlib && !row->declared_callable && row->declaration == 0 && row->value_binding == 0 &&
        row->argument_count == 1 && row->target == target && target != NULL && target->type == AST_IDENTIFIER &&
        row->target_id != 0 && row->target_id == ast_node_stable_id(target) &&
        ast_identifier_name(target) != NULL && strcmp(ast_identifier_name(target), name) == 0 &&
        ast_call_semantic_callee_is_stdlib(call) && !ast_call_semantic_callee_declared_callable(call) &&
        ast_call_semantic_callee_decl_id(call) == 0 && ast_call_semantic_callee_value_binding_id(call) == 0 &&
        ast_call_semantic_callee_builtin_kind(call, &kind) && kind == BUILTIN_NOT_BUILTIN;
}

static bool option_relation(SemanticContext *ctx, const ASTNode *call, bool unwrap)
{
    if (!option_intrinsic(ctx, call, unwrap ? "UnwrapOption" : "Some")) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    const ASTNode *actual = ast_call_argument(call, 0);
    PublishedNode *arg = publication_node(ctx->function_param_publications, actual, false);
    if (arg == NULL || !arg->value_present || arg->meta || arg->id == 0 ||
        arg->id != ast_node_stable_id(actual) || arg->value_shape != actual->type ||
        !row->result_relation || !row->value_present || row->meta ||
        row->relation_argument != actual || row->relation_argument_id != arg->id ||
        row->relation_argument_count != 1 || row->relation_input_type != arg->value_type ||
        row->relation_output_type != row->value_type || row->relation_argument_type == NULL ||
        row->relation_result_type == NULL || arg->value_type_name == NULL || row->value_type_name == NULL ||
        ast_call_semantic_value_type_name(call) == NULL ||
        strcmp(row->relation_argument_type, arg->value_type_name) != 0 ||
        strcmp(row->relation_result_type, row->value_type_name) != 0 ||
        strcmp(row->relation_result_type, ast_call_semantic_value_type_name(call)) != 0) return false;
    return unwrap ? row->value_type == TYPE_STRING && option_type(arg->value_type, TYPE_STRING) :
        arg->value_type == TYPE_STRING && option_type(row->value_type, TYPE_STRING);
}

bool semantic_indexed_string_option_unwrap_candidate(SemanticContext *ctx, const ASTNode *call)
{ return option_relation(ctx, call, true); }

static FunctionParamFlowIndexClaims *option_claims(SemanticContext *ctx, const ASTNode *caller)
{
    FunctionParamFlowSummaryStore *store = ctx == NULL ? NULL : ctx->function_param_flow_summaries;
    if (store == NULL || store->failed || caller == NULL || caller->type != AST_FUNC_DECL ||
        caller->is_async_decl || !publication_routine_header_current(ctx, caller)) return NULL;
    FunctionParamFlowProgramPointIndex *found = NULL;
    for (size_t i = 0; i < store->program_point_index_count; i++)
        if (store->program_point_indexes[i].function_decl == caller) {
            if (found != NULL) return NULL;
            found = &store->program_point_indexes[i];
        }
    return found != NULL && function_param_flow_index_claims_current(ctx, found) ? found->claims : NULL;
}

static bool option_payload_member_independent_current(SemanticContext *ctx,
    IndexedStringResultInputs *inputs, const ASTNode *caller,
    const ASTNode *value, bool prepare)
{
    FunctionParamFlowSummaryStore *store = ctx == NULL
        ? NULL : ctx->function_param_flow_summaries;
    FunctionParamFlowIndexClaims *claims = option_claims(ctx, caller);
    FunctionParamFlowProgramPointIndex *index = NULL;
    if (store == NULL || store->failed || store->solving || claims == NULL ||
        value == NULL || value->type != AST_MEMBER_ACCESS ||
        !indexed_string_result_capture_dependency(ctx, (ASTNode *)caller,
            claims, inputs, prepare) ||
        !function_param_flow_program_point_index_build(store,
            (ASTNode *)caller, &index) ||
        index == NULL || index->claims != claims ||
        index->declared_result == NULL ||
        !function_param_flow_declared_result_rows_current(store,
            (ASTNode *)caller))
        return false;
    bool has_origin = true;
    return function_param_flow_backing_value_origin_current(store,
            index->declared_result, value, &has_origin) &&
        !has_origin;
}

bool indexed_string_option_relation_record(SemanticContext *ctx, IndexedStringResultInputs *inputs,
    const ASTNode *call, bool unwrap, bool prepare)
{
    IndexedStringOptionInputs *option = inputs->option;
    bool depends = false, retains = true;
    if (!option_relation(ctx, call, unwrap) || !semantic_param_publication_checked_call(ctx, call) ||
        !semantic_param_publication_checked_argument(ctx, call, 0) ||
        !semantic_param_publication_checked_result_relation(ctx, call, 0, &depends, &retains) || !depends || retains ||
        !semantic_param_publication_checked_backing_relation(ctx, call, 0, &depends, &retains) || !depends || retains ||
        !indexed_string_result_capture_call(ctx, call, inputs, prepare))
        return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    for (size_t i = 0; i < option->relation_count; i++) if (option->relations[i].call == call) {
        const IndexedStringOptionRelation *saved = &option->relations[i];
        return saved->call_id == ast_node_stable_id(call) && saved->argument == row->relation_argument &&
            saved->argument_id == row->relation_argument_id && saved->unwrap == unwrap &&
            saved->argument_shape == row->relation_argument->type && saved->argument_binding ==
                (row->relation_argument->type == AST_IDENTIFIER ? ast_identifier_binding_syntax_id(row->relation_argument) : 0) &&
            strcmp(saved->input_name, row->relation_argument_type) == 0 &&
            strcmp(saved->output_name, row->relation_result_type) == 0;
    }
    if (!prepare || option->relation_count >= SIZE_MAX / sizeof(*option->relations)) return false;
    IndexedStringOptionRelation saved = {.call = call, .argument = row->relation_argument,
        .call_id = ast_node_stable_id(call), .argument_id = row->relation_argument_id,
        .argument_binding = row->relation_argument->type == AST_IDENTIFIER ?
            ast_identifier_binding_syntax_id(row->relation_argument) : 0,
        .argument_shape = row->relation_argument->type, .unwrap = unwrap};
    size_t a = strlen(row->relation_argument_type) + 1, r = strlen(row->relation_result_type) + 1;
    saved.input_name = malloc(a); saved.output_name = malloc(r);
    if (saved.input_name == NULL || saved.output_name == NULL) {
        free(saved.input_name); free(saved.output_name); return false;
    }
    memcpy(saved.input_name, row->relation_argument_type, a);
    memcpy(saved.output_name, row->relation_result_type, r);
    IndexedStringOptionRelation *grown = realloc(option->relations,
        (option->relation_count + 1) * sizeof(*grown));
    if (grown == NULL) { free(saved.input_name); free(saved.output_name); return false; }
    option->relations = grown; grown[option->relation_count++] = saved;
    option->relation_capacity = option->relation_count;
    return true;
}

static bool birth_record(SemanticContext *ctx, IndexedStringResultInputs *inputs,
    const ASTNode *caller, const ASTNode *declaration, bool carrier, bool prepare)
{
    FunctionParamFlowIndexClaims *claims = option_claims(ctx, caller);
    if (claims == NULL || declaration == NULL || declaration->type != AST_LET_DECL ||
        ast_let_is_mutable(declaration) || ast_let_is_alias(declaration) || ast_node_stable_id(declaration) == 0 ||
        ast_let_initializer(declaration) == NULL ||
        !indexed_string_result_capture_dependency(ctx, (ASTNode *)caller, claims, inputs, prepare)) return false;
    IndexedStringOptionBirth saved = {declaration, ast_let_initializer(declaration), caller,
        ast_node_stable_id(declaration), ast_node_stable_id(ast_let_initializer(declaration)),
        ast_node_stable_id(caller), claims, carrier};
    if (saved.initializer_id == 0) return false;
    IndexedStringOptionInputs *option = inputs->option;
    for (size_t i = 0; i < option->birth_count; i++) if (option->births[i].binding == saved.binding) {
        const IndexedStringOptionBirth *prior = &option->births[i];
        return prior->declaration == declaration && prior->initializer == saved.initializer &&
            prior->caller == caller && prior->claims == claims && prior->carrier == carrier &&
            prior->initializer_id == saved.initializer_id && prior->caller_id == saved.caller_id &&
            indexed_string_option_birth_uses(ctx, inputs, prior, prepare);
    }
    if (!prepare || option->birth_count >= SIZE_MAX / sizeof(*option->births)) return false;
    IndexedStringOptionBirth *grown = realloc(option->births, (option->birth_count + 1) * sizeof(*grown));
    if (grown == NULL) return false;
    option->births = grown; grown[option->birth_count++] = saved;
    option->birth_capacity = option->birth_count;
    return indexed_string_option_birth_uses(ctx, inputs, &saved, prepare);
}

typedef struct OptionPayloadVisit {
    const ASTNode *value;
    const struct OptionPayloadVisit *parent;
} OptionPayloadVisit;

static bool payload_source(SemanticContext *ctx, IndexedStringResultInputs *inputs,
    const ASTNode *caller, const ASTNode *value, const ASTNode *some, bool prepare, const OptionPayloadVisit *parent)
{
    for (const OptionPayloadVisit *p = parent; p != NULL; p = p->parent)
        if (p->value == value) return false;
    SemanticPublishedValue typed = semantic_param_publication_value(ctx, value);
    if (!typed.ok || typed.meta || typed.type != TYPE_STRING) return false;
    OptionPayloadVisit visit = {value, parent};
    uint32_t source = UINT32_MAX;
    if (value->type == AST_ARRAY_ACCESS) {
        const IndexedStringPermanentElementRecipe *recipe = semantic_indexed_string_permanent_for_use(ctx, some, 0, value);
        return recipe != NULL && indexed_string_result_capture_permanent(ctx, recipe, inputs, prepare);
    }
    if (value->type == AST_IDENTIFIER) {
        uint32_t binding = ast_identifier_binding_syntax_id(value);
        FunctionParamFlowIndexClaims *claims = option_claims(ctx, caller);
        const ASTNode *declaration = NULL;
        if (claims == NULL || binding == 0) return false;
        for (size_t i = 0; i < claims->count; i++) if (claims->rows[i].shape == AST_LET_DECL &&
            claims->rows[i].id == binding) {
            if (declaration != NULL) return false;
            declaration = claims->rows[i].node;
        }
        if (declaration != NULL)
            return birth_record(ctx, inputs, caller, declaration, false, prepare) &&
                payload_source(ctx, inputs, caller, ast_let_initializer(declaration), some, prepare, &visit);
        bool formal = false;
        for (size_t p = 0; p < ast_func_param_count(caller); p++)
            if (ast_func_param_stable_id(ast_func_param(caller, p)) == binding) formal = true;
        if (!formal) return false;
    } else if (value->type == AST_CALL && ast_call_semantic_callee_declared_callable(value)) {
        return indexed_string_result_derive(ctx, value, &source, prepare, false, inputs) && source == 0;
    } else if (value->type == AST_MEMBER_ACCESS) {
        /* The declared-result frame already owns this exact projection's
         * backing origin.  An input-free member is independent result storage;
         * an input-derived member remains refused. */
        return option_payload_member_independent_current(ctx, inputs, caller,
            value, prepare);
    } else if (value->type != AST_CALL && value->type != AST_STRING) return false;
    return (value->type != AST_CALL || indexed_string_result_capture_call(ctx, value, inputs, prepare)) &&
        semantic_indexed_string_borrow_source_current((ASTNode *)value, ctx, &source) && source == 0;
}

static bool returns_current(SemanticContext *ctx, IndexedStringResultInputs *inputs,
    const ASTNode *producer, FunctionParamFlowIndexClaims *claims, bool prepare)
{
    IndexedStringOptionInputs *option = inputs->option;
    size_t found = 0;
    for (size_t i = 0; i < claims->count; i++) {
        const FunctionParamFlowIndexClaim *row = &claims->rows[i];
        if (row->shape != AST_RETURN || (row->execution_present && !row->reachable)) continue;
        const ASTNode *value = ast_return_value(row->node), *payload = NULL;
        if (row->child_count != 1 || row->children == NULL || row->child_ids == NULL ||
            row->child_claim_ordinals == NULL || row->children[0] != value || value == NULL ||
            row->child_claim_ordinals[0] >= claims->count ||
            claims->rows[row->child_claim_ordinals[0]].node != value) {
            return false;
        }
        bool none = value->type == AST_IDENTIFIER &&
            pgy_match_variant_lookup(ast_identifier_name(value)) == PGY_MATCH_VARIANT_NONE_CTOR;
        SemanticPublishedValue typed = semantic_param_publication_value(ctx, value);
        if (!typed.ok) {
            return false;
        }
        if (none) {
            const FunctionParamFlowIndexClaim *constant = &claims->rows[row->child_claim_ordinals[0]];
            /* None is the existing meta constant issuer, not a String value
             * or an origin-ready independent payload. */
            if (!typed.meta || !option_type(typed.type, TYPE_UNKNOWN) ||
                ast_identifier_binding_syntax_id(value) != 0 || constant->binding != 0 ||
                !constant->semantic_promoted || !constant->snapshot_semantic_complete ||
                !constant->value_ready || !constant->value_meta) {
                return false;
            }
        } else {
            if (typed.meta || !indexed_string_option_relation_record(
                    ctx, inputs, value, false, prepare)) {
                return false;
            }
            payload = ast_call_argument(value, 0);
            if (!payload_source(ctx, inputs, producer, payload, value,
                    prepare, NULL)) {
                return false;
            }
        }
        IndexedStringOptionReturn saved = {producer, row->node, value, payload, ast_node_stable_id(producer),
            row->id, ast_node_stable_id(value), ast_node_stable_id(payload), none};
        bool existing = false;
        for (size_t r = 0; r < option->return_count; r++) if (option->returns[r].statement == row->node) {
            const IndexedStringOptionReturn *old = &option->returns[r];
            if (old->caller != producer || old->value != value || old->payload != payload || old->none != none ||
                old->caller_id != saved.caller_id || old->statement_id != saved.statement_id ||
                old->value_id != saved.value_id || old->payload_id != saved.payload_id) return false;
            existing = true;
        }
        if (!existing) {
            if (!prepare || option->return_count >= SIZE_MAX / sizeof(*option->returns)) return false;
            IndexedStringOptionReturn *grown = realloc(option->returns, (option->return_count + 1) * sizeof(*grown));
            if (grown == NULL) return false;
            option->returns = grown; grown[option->return_count++] = saved;
            option->return_capacity = option->return_count;
        }
        found++;
    }
    return found != 0 && found == option->return_count;
}

bool indexed_string_option_source(SemanticContext *ctx, const ASTNode *call,
    uint32_t *source, IndexedStringResultInputs *inputs, bool prepare)
{
    if (source == NULL || inputs == NULL || ctx == NULL || ctx->has_error ||
        !semantic_param_publication_inputs_ready(ctx) ||
        !semantic_indexed_string_option_unwrap_candidate(ctx, call)) return false;
    if (prepare) {
        if (inputs->option != NULL) return false;
        inputs->option = calloc(1, sizeof(*inputs->option));
        if (inputs->option == NULL) return false;
        inputs->option->unwrap = call; inputs->option->unwrap_id = ast_node_stable_id(call);
    }
    IndexedStringOptionInputs *option = inputs->option;
    if (option == NULL || option->unwrap != call ||
        option->unwrap_id != ast_node_stable_id(call) ||
        !indexed_string_option_relation_record(
            ctx, inputs, call, true, prepare)) return false;
    PublishedNode *published = publication_node(ctx->function_param_publications, call, false);
    const ASTNode *value = ast_call_argument(call, 0), *caller = published->caller;
    if (value == NULL || value->type != AST_IDENTIFIER || caller == NULL ||
        published->caller_id != ast_node_stable_id(caller)) return false;
    FunctionParamFlowIndexClaims *claims = option_claims(ctx, caller);
    const ASTNode *declaration = NULL;
    uint32_t binding = ast_identifier_binding_syntax_id(value);
    if (claims == NULL || binding == 0) return false;
    for (size_t i = 0; i < claims->count; i++) if (claims->rows[i].shape == AST_LET_DECL && claims->rows[i].id == binding) {
        if (declaration != NULL) return false;
        declaration = claims->rows[i].node;
    }
    if (declaration == NULL) return false;
    const ASTNode *producer_call = ast_let_initializer(declaration);
    const Type *result = NULL;
    ASTNode *producer = semantic_param_publication_declared_result(ctx, producer_call, &result);
    uint32_t incoming = UINT32_MAX;
    if (producer == NULL || !option_type(result, TYPE_STRING) ||
        !indexed_string_result_derive(
            ctx, producer_call, &incoming, prepare, true, inputs) ||
        incoming != 0 ||
        !birth_record(ctx, inputs, caller, declaration, true, prepare)) return false;
    claims = option_claims(ctx, producer);
    if (claims == NULL ||
        !returns_current(ctx, inputs, producer, claims, prepare)) return false;
    if (prepare) {
        option->sealed_births = option->birth_count; option->sealed_returns = option->return_count;
        option->sealed_relations = option->relation_count; option->sealed_uses = option->use_count;
        option->sealed = true;
    }
    *source = 0;
    return true;
}

void indexed_string_option_destroy(IndexedStringOptionInputs *option)
{
    if (option == NULL) return;
    for (size_t i = 0; i < option->relation_count; i++) {
        free(option->relations[i].input_name); free(option->relations[i].output_name);
    }
    free(option->relations); free(option->births); free(option->returns); free(option->uses); free(option);
}
