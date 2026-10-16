/* One all-typed publication boundary. Pending rows own diagnostic strings,
 * borrow compilation-owned AST/Type identity, and never retain scope Symbols. */
#include "function_param_publication_internal.h"
#include "typed_map_query_fact_owner.h"
#include "type_checker_internal.h"
#include "type_checker_resolution_internal.h"
#include "type_checker_ownership_internal.h"
#include "slot_analyzer_internal.h"
#include "function_param_flow_summary_internal.h"
#include "callable_capability_inference.h"
#include "region_retention_summary.h"
#include "indexed_string_borrow_owner.h"
#include "collection_ownership_fact.h"
#include "../common/string_compat.h"
#include <stdlib.h>
#include <string.h>
static bool publication_channel_shape(const PublishedNode *row)
{
    if (row == NULL || !row->channel_binding || row->node == NULL || row->node->type != AST_LET_DECL ||
        row->id == 0 || row->id != ast_node_stable_id(row->node) || ast_let_is_mutable(row->node) ||
        ast_let_is_alias(row->node) || row->target_name == NULL || ast_let_name(row->node) == NULL ||
        strcmp(row->target_name, ast_let_name(row->node)) != 0 ||
        row->channel_type_node == NULL || row->channel_type_node != ast_let_type(row->node) ||
        row->channel_type_id == 0 || row->channel_type_id != ast_node_stable_id(row->channel_type_node) ||
        row->value_shape != row->channel_type_node->type || row->value_type == NULL ||
        row->value_type->kind != TYPE_KIND_CONSTRUCTED || type_constructed_constructor(row->value_type) != TYPE_CHANNEL ||
        type_constructed_arg_count(row->value_type) != 1 || row->channel_element_type == NULL ||
        row->channel_element_type == TYPE_UNKNOWN || type_constructed_arg(row->value_type, 0) != row->channel_element_type ||
        row->value_type_name == NULL || row->value_type->name == NULL ||
        strcmp(row->value_type_name, row->value_type->name) != 0 ||
        row->target == NULL || row->target != ast_let_initializer(row->node) || row->target->type != AST_CALL ||
        row->target_id == 0 || row->target_id != ast_node_stable_id(row->target) ||
        ast_call_arg_count(row->target) != 1 || row->argument_count != 1 || row->arguments == NULL ||
        row->call_receiver == NULL || row->call_receiver != ast_call_callee(row->target) ||
        row->call_receiver->type != AST_IDENTIFIER || row->call_receiver_id == 0 ||
        row->call_receiver_id != ast_node_stable_id(row->call_receiver) ||
        ast_identifier_name(row->call_receiver) == NULL || strcmp(ast_identifier_name(row->call_receiver), "Channel") != 0 ||
        ast_call_semantic_callee_decl_id(row->target) != 0 ||
        ast_call_semantic_callee_value_binding_id(row->target) != 0 || ast_call_semantic_callee_is_stdlib(row->target))
        return false;
    uint32_t kind;
    const PublishedCallArgument *arg = &row->arguments[0];
    return ast_call_semantic_callee_builtin_kind(row->target, &kind) && kind == BUILTIN_NOT_BUILTIN &&
        arg->node != NULL && arg->node == ast_call_argument(row->target, 0) && arg->id != 0 &&
        arg->id == ast_node_stable_id(arg->node) && arg->shape == arg->node->type &&
        arg->binding == ast_identifier_binding_syntax_id(arg->node) && arg->type == TYPE_INT;
}
bool semantic_param_publication_record_channel_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *channel_type)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error || declaration == NULL ||
        declaration->type != AST_LET_DECL || channel_type == NULL || channel_type->name == NULL ||
        type_constructed_constructor(channel_type) != TYPE_CHANNEL ||
        type_constructed_arg_count(channel_type) != 1) return false;
    ASTNode *init = ast_let_initializer(declaration);
    ASTNode *capacity = init != NULL && init->type == AST_CALL && ast_call_arg_count(init) == 1
        ? ast_call_argument(init, 0) : NULL;
    if (init == NULL || capacity == NULL) return false;
    PublishedNode *value = publication_node(ctx->function_param_publications, capacity, false);
    if (value == NULL || !value->value_present || value->value_type != TYPE_INT ||
        !value->physical_scalar || value->value_shape != capacity->type) return false;
    PublishedCallArgument argument = {capacity, ast_node_stable_id(capacity), capacity->type,
        ast_identifier_binding_syntax_id(capacity), TYPE_INT};
    if (!semantic_param_publication_record_value(ctx, init, channel_type, false) ||
        !semantic_param_publication_record_checked_call(ctx, init)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, true);
    if (row == NULL) return false;
    row->channel_binding = false;
    free(row->arguments); free(row->target_name); free(row->value_type_name);
    row->arguments = malloc(sizeof(argument)); row->target_name = pergyra_strdup(ast_let_name(declaration));
    row->value_type_name = pergyra_strdup(channel_type->name);
    if (row->arguments == NULL || row->target_name == NULL || row->value_type_name == NULL) return false;
    *row->arguments = argument; row->argument_count = 1;
    row->value_type = channel_type; row->channel_element_type = type_constructed_arg(channel_type, 0);
    row->channel_type_node = ast_let_type(declaration); row->channel_type_id = ast_node_stable_id(row->channel_type_node);
    row->value_shape = row->channel_type_node == NULL ? AST_LET_DECL : row->channel_type_node->type;
    row->target = init; row->target_id = ast_node_stable_id(init);
    row->call_receiver = ast_call_callee(init); row->call_receiver_id = ast_node_stable_id(row->call_receiver);
    row->channel_binding = true;
    if (!publication_channel_shape(row)) return false;
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, declaration)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, declaration, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax);
    row->deferred_syntax = syntax;
    return true;
}
bool semantic_param_publication_channel_binding(const SemanticContext *ctx, const ASTNode *declaration)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, false);
    if (!publication_channel_shape(row) || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax) ||
        semantic_type_resolution_lookup_metadata_type_ref((SemanticContext *)ctx, (ASTNode *)row->channel_type_node) != row->value_type)
        return false;
    PublishedNode *init = publication_current_value(ctx, row->target);
    PublishedNode *capacity = publication_current_value(ctx, row->arguments[0].node);
    return init != NULL && init->value_type == row->value_type && capacity != NULL &&
        capacity->value_type == TYPE_INT && capacity->physical_scalar;
}

bool semantic_param_publication_record_slot_token_operand(SemanticContext *ctx, ASTNode *call,
    size_t ordinal, const Symbol *slot, const Symbol *token, const Type *slot_type)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error || call == NULL || call->type != AST_CALL ||
        slot == NULL || token == NULL || slot->name == NULL || token->name == NULL || token->type == NULL ||
        slot->slot_info.paired_token_name == NULL || token->slot_info.paired_slot_name == NULL ||
        strcmp(slot->slot_info.paired_token_name, token->name) != 0 || strcmp(token->slot_info.paired_slot_name, slot->name) != 0 ||
        slot_type == NULL || slot->type != slot_type || !type_slot_is_secure(slot_type) || ordinal >= ast_call_arg_count(call)) return false;
    ASTNode *operand = ast_call_argument(call, ordinal), *receiver = ast_call_argument(call, 0);
    if (operand == NULL || receiver == NULL || operand->type != AST_IDENTIFIER || receiver->type != AST_IDENTIFIER ||
        ast_identifier_binding_syntax_id(operand) != token->decl_syntax_id ||
        ast_identifier_binding_syntax_id(receiver) != slot->decl_syntax_id) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, operand, false);
    if (row == NULL || !row->value_present || row->value_type != token->type) return false;
    row->slot_token_operand = false;
    free(row->target_name); free(row->relation_argument_type);
    row->target_name = pergyra_strdup(slot->name); row->relation_argument_type = pergyra_strdup(token->name);
    if (row->target_name == NULL || row->relation_argument_type == NULL) return false;
    row->target = call; row->target_id = ast_node_stable_id(call); row->slot_token_ordinal = ordinal;
    row->relation_argument = receiver; row->relation_argument_id = ast_node_stable_id(receiver);
    row->target_binding = ast_identifier_binding_syntax_id(receiver); row->relation_argument_count = ast_call_arg_count(call);
    row->relation_input_type = slot_type; row->relation_output_type = token->type;
    row->write_access_mode = type_slot_access_mode(slot_type); row->write_inner_type = type_slot_inner_type(slot_type);
    row->kind_present = ast_call_semantic_callee_builtin_kind(call, &row->builtin);
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, call)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, operand, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax); row->deferred_syntax = syntax;
    row->slot_token_operand = true;
    return true;
}
bool semantic_param_publication_slot_token_operand(const SemanticContext *ctx, const ASTNode *operand)
{
    PublishedNode *row = publication_current_value(ctx, operand);
    if (row == NULL || !row->slot_token_operand || operand->type != AST_IDENTIFIER || row->target == NULL ||
        row->target->type != AST_CALL || row->target_id == 0 || row->target_id != ast_node_stable_id(row->target) ||
        row->slot_token_ordinal >= ast_call_arg_count(row->target) || ast_call_argument(row->target, row->slot_token_ordinal) != operand ||
        row->relation_argument_count != ast_call_arg_count(row->target) || row->relation_argument == NULL ||
        row->relation_argument != ast_call_argument(row->target, 0) || row->relation_argument->type != AST_IDENTIFIER ||
        row->relation_argument_id == 0 || row->relation_argument_id != ast_node_stable_id(row->relation_argument) ||
        row->target_binding != ast_identifier_binding_syntax_id(row->relation_argument) || row->relation_input_type == NULL ||
        row->relation_input_type->kind != TYPE_KIND_SLOT || !type_slot_is_secure(row->relation_input_type) ||
        type_slot_access_mode(row->relation_input_type) != row->write_access_mode ||
        type_slot_inner_type(row->relation_input_type) != row->write_inner_type ||
        row->relation_output_type != row->value_type || type_constructed_constructor(row->value_type) != TYPE_TOKEN ||
        type_constructed_arg_count(row->value_type) != 1 || type_constructed_arg(row->value_type, 0) != type_slot_inner_type(row->relation_input_type) ||
        row->target_name == NULL || ast_identifier_name(row->relation_argument) == NULL ||
        strcmp(row->target_name, ast_identifier_name(row->relation_argument)) != 0 || row->relation_argument_type == NULL ||
        ast_identifier_name(operand) == NULL || strcmp(row->relation_argument_type, ast_identifier_name(operand)) != 0 ||
        row->deferred_syntax == NULL || !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
    PublishedNode *call = publication_current_value(ctx, row->target);
    PublishedNode *receiver = publication_current_value(ctx, row->relation_argument);
    if (call == NULL || receiver == NULL || receiver->value_type != row->relation_input_type ||
        !row->kind_present || !call->kind_present || call->builtin != row->builtin || call->stdlib ||
        call->declaration != 0 || call->value_binding != 0 || call->target->type != AST_IDENTIFIER) return false;
    for (size_t p = 0; p < call->argument_count; p++) {
        PublishedNode *actual = publication_current_value(ctx, call->arguments[p].node);
        if (actual == NULL || call->arguments[p].type == NULL || call->arguments[p].type != actual->value_type) return false;
    }
    const char *name = ast_identifier_name(call->target);
    if (name == NULL) return false;
    if (row->builtin == BUILTIN_WRITE)
        return row->slot_token_ordinal == 2 && call->argument_count == 3 && strcmp(name, "Write") == 0;
    if (row->builtin == BUILTIN_READ || row->builtin == BUILTIN_RELEASE)
        return row->slot_token_ordinal == 1 && call->argument_count == 2 &&
            strcmp(name, row->builtin == BUILTIN_READ ? "Read" : "Release") == 0;
    return false;
}
static bool publication_slot_birth_shape(const PublishedNode *row)
{
    if (row == NULL || !row->owned_slot_binding || row->node == NULL || row->id == 0 ||
        row->id != ast_node_stable_id(row->node) || row->value_shape != row->node->type ||
        row->slot_birth.slot_type == NULL || row->slot_birth.slot_type->kind != TYPE_KIND_SLOT ||
        type_slot_access_mode(row->slot_birth.slot_type) != SLOT_ACCESS_OWNED ||
        type_slot_is_secure(row->slot_birth.slot_type) != row->slot_birth_secure || row->slot_birth_inner_type == NULL ||
        row->slot_birth_inner_type == TYPE_UNKNOWN || type_slot_inner_type(row->slot_birth.slot_type) != row->slot_birth_inner_type ||
        row->target == NULL || row->target->type != AST_CALL || row->target_id == 0 ||
        row->target_id != ast_node_stable_id(row->target) || ast_call_arg_count(row->target) != 0 ||
        ast_call_semantic_callee_declared_callable(row->target) || ast_call_semantic_callee_is_stdlib(row->target) ||
        ast_call_semantic_callee_decl_id(row->target) != 0 || ast_call_semantic_callee_value_binding_id(row->target) != 0 ||
        row->call_receiver != ast_call_callee(row->target) || row->call_receiver == NULL || row->call_receiver->type != AST_IDENTIFIER ||
        row->call_receiver_id == 0 || row->call_receiver_id != ast_node_stable_id(row->call_receiver)) return false;
    uint32_t builtin;
    if (!ast_call_semantic_callee_builtin_kind(row->target, &builtin) ||
        builtin != (uint32_t)(row->slot_birth_secure ? BUILTIN_CLAIM_SECURE_SLOT : BUILTIN_CLAIM_SLOT)) return false;
    const char *name = ast_identifier_name(row->call_receiver);
    if (name == NULL || strcmp(name, row->slot_birth_secure ? "ClaimSecureSlot" : "ClaimSlot") != 0) return false;
    if (row->slot_birth_secure) {
        if (row->slot_birth.token_type == NULL || row->relation_argument_type == NULL ||
            type_constructed_constructor(row->slot_birth.token_type) != TYPE_TOKEN ||
            type_constructed_arg_count(row->slot_birth.token_type) != 1 ||
            type_constructed_arg(row->slot_birth.token_type, 0) != row->slot_birth_inner_type) return false;
    } else if (row->slot_birth.token_type != NULL || row->slot_birth.token_binding_id != 0 || row->relation_argument_type != NULL) return false;
    if (row->node->type == AST_LET_DECL) {
        if (ast_let_is_alias(row->node) || row->target != ast_let_initializer(row->node) ||
            row->slot_birth.slot_binding_id != row->id || row->slot_birth.token_binding_id != 0 ||
            row->slot_birth_annotation != ast_let_type(row->node) ||
            row->slot_birth_annotation_id != ast_node_stable_id(row->slot_birth_annotation) ||
            (row->slot_birth_annotation != NULL && row->slot_birth_annotation_id == 0) ||
            row->target_name == NULL || ast_let_name(row->node) == NULL || strcmp(row->target_name, ast_let_name(row->node)) != 0) return false;
    } else if (row->node->type == AST_LET_DESTRUCTURE) {
        if (row->target != ast_let_destructure_initializer(row->node) ||
            ast_let_destructure_name_count(row->node) != (row->slot_birth_secure ? 2u : 1u) ||
            row->slot_birth.slot_binding_id == 0 || row->slot_birth.slot_binding_id != ast_let_destructure_binding_stable_id(row->node, 0) ||
            row->target_name == NULL || ast_let_destructure_name(row->node, 0) == NULL ||
            strcmp(row->target_name, ast_let_destructure_name(row->node, 0)) != 0) return false;
        if (row->slot_birth_secure && (row->slot_birth.token_binding_id == 0 ||
            row->slot_birth.token_binding_id != ast_let_destructure_binding_stable_id(row->node, 1) ||
            ast_let_destructure_name(row->node, 1) == NULL ||
            strcmp(row->relation_argument_type, ast_let_destructure_name(row->node, 1)) != 0)) return false;
    } else return false;
    return row->slot_birth_annotation != NULL || ast_call_generic_arg_count(row->target) == 1;
}
bool semantic_param_publication_record_owned_slot_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *slot_type, uint32_t slot_binding_id,
    const Type *token_type, uint32_t token_binding_id, const char *token_name)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error || declaration == NULL ||
        (declaration->type != AST_LET_DECL && declaration->type != AST_LET_DESTRUCTURE) ||
        slot_type == NULL || slot_type->kind != TYPE_KIND_SLOT) return false;
    ASTNode *init = declaration->type == AST_LET_DECL ? ast_let_initializer(declaration) : ast_let_destructure_initializer(declaration);
    if (init == NULL || !semantic_param_publication_record_value(ctx, init, slot_type, false) ||
        !semantic_param_publication_record_checked_call(ctx, init)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, true);
    if (row == NULL) return false;
    row->owned_slot_binding = false;
    free(row->target_name); free(row->relation_argument_type);
    row->target_name = pergyra_strdup(declaration->type == AST_LET_DECL ? ast_let_name(declaration) : ast_let_destructure_name(declaration, 0));
    row->relation_argument_type = pergyra_strdup(token_name);
    if (row->target_name == NULL || (token_name != NULL && row->relation_argument_type == NULL)) return false;
    row->slot_birth = (SemanticPublishedSlotBirth){slot_binding_id, token_binding_id, slot_type, token_type};
    row->slot_birth_inner_type = type_slot_inner_type(slot_type); row->slot_birth_secure = type_slot_is_secure(slot_type);
    row->slot_birth_annotation = declaration->type == AST_LET_DECL ? ast_let_type(declaration) : NULL;
    row->slot_birth_annotation_id = ast_node_stable_id(row->slot_birth_annotation);
    row->slot_birth_annotation_type = row->slot_birth_annotation == NULL ? NULL :
        semantic_type_resolution_lookup_metadata_type_ref(ctx, (ASTNode *)row->slot_birth_annotation);
    row->target = init; row->target_id = ast_node_stable_id(init); row->value_shape = declaration->type;
    row->call_receiver = ast_call_callee(init); row->call_receiver_id = ast_node_stable_id(row->call_receiver);
    row->owned_slot_binding = true;
    if (!publication_slot_birth_shape(row)) { row->owned_slot_binding = false; return false; }
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, declaration)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, declaration, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax); row->deferred_syntax = syntax;
    PublishedNode *value = publication_node(ctx->function_param_publications, init, false);
    if (value == NULL) return false;
    value->owned_slot_declaration = declaration; value->owned_slot_declaration_id = ast_node_stable_id(declaration);
    return true;
}
bool semantic_param_publication_owned_slot_binding(const SemanticContext *ctx,
    const ASTNode *declaration, SemanticPublishedSlotBirth *birth)
{
    if (!semantic_param_publication_inputs_ready(ctx) || birth == NULL) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, false);
    if (!publication_slot_birth_shape(row) || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
    if (row->slot_birth_annotation != NULL &&
        (row->slot_birth_annotation_type == NULL || row->slot_birth_annotation_type == TYPE_UNKNOWN ||
         semantic_type_resolution_lookup_metadata_type_ref((SemanticContext *)ctx, (ASTNode *)row->slot_birth_annotation) != row->slot_birth_annotation_type ||
         row->slot_birth_annotation_type != (row->slot_birth_annotation_type->kind == TYPE_KIND_SLOT ? row->slot_birth.slot_type : row->slot_birth_inner_type))) return false;
    PublishedNode *value = publication_node(ctx->function_param_publications, row->target, false);
    if (value == NULL || !value->value_present || value->value_shape != AST_CALL || !publication_call_ready(value) ||
        value->value_type != row->slot_birth.slot_type || value->owned_slot_declaration != declaration ||
        value->owned_slot_declaration_id != row->id) return false;
    *birth = row->slot_birth;
    return true;
}
bool publication_record_read_payload_loan(SemanticContext *ctx, ASTNode *call)
{
    if (!semantic_param_publication_pending(ctx)) return false;
    FunctionParamPublicationStore *store = ctx->function_param_publications;
    PublishedNode *value = publication_node(store, call, false);
    if (value == NULL || !value->value_present || value->value_type == NULL || value->physical_scalar ||
        publication_physical_scalar(value->value_type) || value->value_type == TYPE_UNKNOWN ||
        !value->kind_present || value->builtin != BUILTIN_READ || !publication_call_ready(value)) return false;
    size_t at = 0;
    while (at < store->read_loan_count && store->read_loans[at].call != call) at++;
    if (at == store->read_loan_count && !semantic_param_publication_reserve_storage((void **)&store->read_loans,
        &store->read_loan_capacity, store->read_loan_count, sizeof(*store->read_loans))) return false;
    char *module = pergyra_strdup(ctx->current_module_path);
    if (ctx->current_module_path != NULL && module == NULL) return false;
    PendingReadPayloadLoan loan = {0};
    loan.call = call; loan.call_id = ast_node_stable_id(call);
    loan.caller = value->caller; loan.caller_id = value->caller_id;
    if (loan.caller != NULL) loan.caller_shape = loan.caller->type;
    loan.result_type = value->value_type; loan.result_kind = value->value_type->kind; loan.module = module;
    if (at == store->read_loan_count) store->read_loan_count++;
    else free(store->read_loans[at].module);
    store->read_loans[at] = loan; value->read_payload_loan_index = at + 1;
    return true;
}
bool semantic_param_publication_read_payload_loan_current(const SemanticContext *ctx,
    const PendingReadPayloadLoan *loan, uint32_t *owned_receiver_binding)
{
    if (!semantic_param_publication_inputs_ready(ctx) || loan == NULL || owned_receiver_binding == NULL) return false;
    const FunctionParamPublicationStore *store = ctx->function_param_publications;
    size_t at = 0;
    while (at < store->read_loan_count && &store->read_loans[at] != loan) at++;
    if (at == store->read_loan_count || loan->call == NULL || loan->call->type != AST_CALL || loan->call_id == 0 ||
        loan->call_id != ast_node_stable_id(loan->call) || loan->caller == NULL || loan->caller_shape != AST_FUNC_DECL ||
        loan->caller->type != loan->caller_shape || loan->caller_id == 0 || loan->caller_id != ast_node_stable_id(loan->caller) ||
        !publication_routine_header_current(ctx, loan->caller)) return false;
    PublishedNode *value = publication_current_value(ctx, loan->call);
    if (value == NULL || value->read_payload_loan_index != at + 1 || value->caller != loan->caller ||
        value->caller_id != loan->caller_id || value->value_type != loan->result_type ||
        loan->result_type == NULL || loan->result_type == TYPE_UNKNOWN || loan->result_type->kind != loan->result_kind ||
        value->physical_scalar || publication_physical_scalar(loan->result_type) ||
        !value->kind_present || value->builtin != BUILTIN_READ) return false;
    bool identity, backing, retains;
    if (!semantic_param_publication_checked_result_relation(ctx, loan->call, 0, &identity, &retains) || identity || retains ||
        !semantic_param_publication_checked_backing_relation(ctx, loan->call, 0, &backing, &retains) || !backing || retains) return false;
    const PublishedCallArgument *receiver = &value->arguments[0];
    *owned_receiver_binding = type_slot_access_mode(value->relation_input_type) == SLOT_ACCESS_OWNED &&
        receiver->node->type == AST_IDENTIFIER ? receiver->binding : 0;
    return true;
}
static bool publication_empty_map_shape(const PublishedNode *row)
{
    if (row == NULL || !row->empty_map_binding || row->node == NULL || row->node->type != AST_LET_DECL ||
        row->id == 0 || row->id != ast_node_stable_id(row->node) || ast_let_is_alias(row->node) ||
        row->target_name == NULL || ast_let_name(row->node) == NULL || strcmp(row->target_name, ast_let_name(row->node)) != 0 ||
        row->map_type_node == NULL || row->map_type_node != ast_let_type(row->node) || row->map_type_id == 0 ||
        row->map_type_id != ast_node_stable_id(row->map_type_node) || row->map_type_node->type != row->value_shape ||
        row->value_type == NULL || type_constructed_constructor(row->value_type) != TYPE_HASHMAP ||
        type_constructed_arg_count(row->value_type) != 2 || row->map_key_type == NULL || row->map_value_type == NULL ||
        row->map_key_type == TYPE_UNKNOWN || row->map_value_type == TYPE_UNKNOWN ||
        type_constructed_arg(row->value_type, 0) != row->map_key_type || type_constructed_arg(row->value_type, 1) != row->map_value_type ||
        row->value_type_name == NULL || row->value_type->name == NULL || strcmp(row->value_type_name, row->value_type->name) != 0 ||
        row->target == NULL || row->target != ast_let_initializer(row->node) || row->target->type != AST_CALL ||
        row->target_id == 0 || row->target_id != ast_node_stable_id(row->target) || ast_call_arg_count(row->target) != 0 ||
        row->call_receiver == NULL || row->call_receiver != ast_call_callee(row->target) || row->call_receiver->type != AST_IDENTIFIER ||
        row->call_receiver_id == 0 || row->call_receiver_id != ast_node_stable_id(row->call_receiver) ||
        ast_identifier_name(row->call_receiver) == NULL || strcmp(ast_identifier_name(row->call_receiver), "MapNew") != 0 ||
        ast_call_semantic_callee_decl_id(row->target) != 0 || ast_call_semantic_callee_value_binding_id(row->target) != 0 ||
        !ast_call_semantic_callee_is_stdlib(row->target)) return false;
    uint32_t kind;
    return ast_call_semantic_callee_builtin_kind(row->target, &kind) && kind == BUILTIN_NOT_BUILTIN;
}
bool semantic_param_publication_record_empty_map_binding(SemanticContext *ctx,
    ASTNode *declaration, const Type *map_type)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error || declaration == NULL ||
        declaration->type != AST_LET_DECL || ast_let_type(declaration) == NULL || map_type == NULL || map_type->name == NULL ||
        type_constructed_constructor(map_type) != TYPE_HASHMAP || type_constructed_arg_count(map_type) != 2) return false;
    ASTNode *init = ast_let_initializer(declaration);
    if (init == NULL || init->type != AST_CALL || ast_call_arg_count(init) != 0 ||
        !semantic_param_publication_record_value(ctx, init, map_type, false) ||
        !semantic_param_publication_record_checked_call(ctx, init)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, true);
    if (row == NULL) return false;
    row->empty_map_binding = false;
    free(row->target_name); free(row->value_type_name);
    row->target_name = pergyra_strdup(ast_let_name(declaration)); row->value_type_name = pergyra_strdup(map_type->name);
    row->value_type = map_type; row->value_shape = ast_let_type(declaration)->type;
    row->map_type_node = ast_let_type(declaration); row->map_type_id = ast_node_stable_id(row->map_type_node);
    row->map_key_type = type_constructed_arg(map_type, 0); row->map_value_type = type_constructed_arg(map_type, 1);
    row->target = init; row->target_id = ast_node_stable_id(init);
    row->call_receiver = ast_call_callee(init); row->call_receiver_id = ast_node_stable_id(row->call_receiver);
    row->empty_map_binding = true;
    if (!publication_empty_map_shape(row)) { row->empty_map_binding = false; return false; }
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, declaration)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, declaration, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax); row->deferred_syntax = syntax;
    PublishedNode *initializer = publication_node(ctx->function_param_publications, init, false);
    if (initializer == NULL) return false;
    initializer->empty_map_declaration = declaration;
    initializer->empty_map_declaration_id = ast_node_stable_id(declaration);
    return true;
}
bool semantic_param_publication_empty_map_binding(const SemanticContext *ctx, const ASTNode *declaration)
{
    if (!semantic_param_publication_inputs_ready(ctx)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, declaration, false);
    if (!publication_empty_map_shape(row) || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax) ||
        semantic_type_resolution_lookup_metadata_type_ref((SemanticContext *)ctx, (ASTNode *)row->map_type_node) != row->value_type) return false;
    PublishedNode *init = publication_node(ctx->function_param_publications, row->target, false);
    return init != NULL && init->value_present && init->value_shape == AST_CALL && publication_call_ready(init) &&
        init->value_type == row->value_type && init->empty_map_declaration == declaration &&
        init->empty_map_declaration_id == row->id;
}
static bool publication_slot_write_shape(const PublishedNode *row)
{
    if (row == NULL || !row->slot_write || !publication_call_ready(row) || !row->kind_present ||
        row->builtin != BUILTIN_WRITE || row->stdlib || row->declaration != 0 || row->value_binding != 0 ||
        row->target->type != AST_IDENTIFIER || ast_identifier_name(row->target) == NULL ||
        strcmp(ast_identifier_name(row->target), "Write") != 0 || row->write_receiver_type == NULL ||
        row->write_receiver_type->kind != TYPE_KIND_SLOT || type_is_read_view(row->write_receiver_type) ||
        type_slot_access_mode(row->write_receiver_type) != row->write_access_mode ||
        type_slot_is_secure(row->write_receiver_type) != row->write_secure ||
        type_slot_inner_type(row->write_receiver_type) != row->write_inner_type ||
        type_is_move_token(row->write_receiver_type) || row->write_payload_type == NULL ||
        row->write_payload_type == TYPE_UNKNOWN || row->write_payload_type == TYPE_VOID ||
        row->argument_count != (type_slot_is_secure(row->write_receiver_type) ? 3u : 2u) ||
        row->arguments[0].type != row->write_receiver_type || row->arguments[1].type != row->write_payload_type) return false;
    return type_is_assignable((Type *)row->write_payload_type, type_slot_inner_type(row->write_receiver_type));
}
bool semantic_param_publication_record_slot_write(SemanticContext *ctx, ASTNode *call,
    const Type *receiver_type, const Type *payload_type)
{
    if (!semantic_param_publication_pending(ctx) || ctx->has_error || ast_node_stable_id(call) == 0 ||
        !semantic_param_publication_record_value(ctx, call, TYPE_VOID, false) ||
        !semantic_param_publication_record_checked_call(ctx, call)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL) return false;
    row->write_receiver_type = receiver_type; row->write_payload_type = payload_type; row->slot_write = true;
    row->write_inner_type = type_slot_inner_type(receiver_type);
    row->write_access_mode = type_slot_access_mode(receiver_type); row->write_secure = type_slot_is_secure(receiver_type);
    if (!publication_slot_write_shape(row)) { row->slot_write = false; return false; }
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, call)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, call, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax); row->deferred_syntax = syntax;
    return true;
}
bool semantic_param_publication_checked_slot_write(const SemanticContext *ctx, const ASTNode *call)
{
    PublishedNode *row = publication_current_value(ctx, call);
    if (!publication_slot_write_shape(row) || row->value_type != TYPE_VOID || row->deferred_syntax == NULL ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
    for (size_t p = 0; p < row->argument_count; p++) {
        PublishedNode *arg = publication_current_value(ctx, row->arguments[p].node);
        if (arg == NULL || row->arguments[p].type == NULL || row->arguments[p].type != arg->value_type) return false;
    }
    return true;
}
void semantic_param_publication_retire_deferred(PublishedNode *row)
{
    if (row == NULL) return;
    for (size_t c = 0; c < row->deferred_capture_count; c++) {
        free(row->deferred_captures[c].name); free(row->deferred_captures[c].type_name);
    }
    free(row->deferred_captures);
    function_param_flow_claims_destroy(row->deferred_syntax);
    row->deferred_captures = NULL; row->deferred_capture_count = 0;
    row->deferred_syntax = NULL; row->deferred_scan = row->deferred_checked = false;
}
static bool publication_deferred_kind(ASTNodeType kind)
{
    return kind == AST_LAMBDA_EXPR || kind == AST_ASYNC_BLOCK || kind == AST_SPAWN_EXPR ||
        kind == AST_EVENT_SUBSCRIBE || kind == AST_EVENT_UNSUBSCRIBE || kind == AST_EVENT_INVOKE;
}
bool semantic_param_publication_record_deferred_capture(SemanticContext *ctx, ASTNode *lambda,
    ASTNode *source, uint32_t binding_id, const Type *type, LambdaCaptureMode mode)
{
    if (!semantic_param_publication_pending(ctx)) return true;
    if (ctx->has_error || lambda == NULL || lambda->type != AST_LAMBDA_EXPR ||
        source == NULL || source->type != AST_IDENTIFIER || ast_node_stable_id(source) == 0 ||
        binding_id == 0 || type == NULL || type->kind != TYPE_KIND_PRIMITIVE ||
        type->name == NULL || ast_identifier_name(source) == NULL || mode != LAMBDA_CAPTURE_COPY) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, lambda, true);
    if (row == NULL) return false;
    for (size_t c = 0; c < row->deferred_capture_count; c++)
        if (row->deferred_captures[c].binding_id == binding_id)
            return row->deferred_captures[c].mode == mode && row->deferred_captures[c].type == type;
    size_t count = row->deferred_capture_count;
    if (count == SIZE_MAX / sizeof(*row->deferred_captures)) return false;
    PublishedDeferredCapture *captures = realloc(row->deferred_captures, (count + 1) * sizeof(*captures));
    if (captures == NULL) return false;
    row->deferred_captures = captures;
    captures[count] = (PublishedDeferredCapture){.source = source, .source_id = ast_node_stable_id(source),
        .binding_id = binding_id, .category = semantic_classify_ownership_type(type, ctx),
        .type = type, .mode = mode, .copy_escape = publication_physical_scalar(type),
        .name = pergyra_strdup(ast_identifier_name(source)), .type_name = pergyra_strdup(type->name)};
    row->deferred_capture_count++;
    return captures[count].name != NULL && captures[count].type_name != NULL;
}
bool semantic_param_publication_record_deferred_scan(SemanticContext *ctx, ASTNode *boundary)
{
    if (!semantic_param_publication_pending(ctx)) return true;
    if (ctx->has_error || boundary == NULL || !publication_deferred_kind(boundary->type)) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, boundary, true);
    if (row == NULL) return false;
    row->deferred_scan = true; row->deferred_checked = false; row->deferred_shape = boundary->type;
    return true;
}
static bool publication_capture_metadata_current(const ASTNode *lambda, const PublishedNode *row)
{
    if (row->deferred_capture_count != ast_lambda_capture_count(lambda) ||
        (row->deferred_capture_count != 0 && row->deferred_captures == NULL)) return false;
    for (size_t c = 0; c < row->deferred_capture_count; c++) {
        const PublishedDeferredCapture *capture = &row->deferred_captures[c];
        const char *name = ast_lambda_capture_name(lambda, c), *type = ast_lambda_capture_type_name(lambda, c);
        if (capture->source == NULL || capture->source->type != AST_IDENTIFIER ||
            capture->source_id == 0 || capture->source_id != ast_node_stable_id(capture->source) ||
            capture->binding_id == 0 || capture->type == NULL || capture->type->kind != TYPE_KIND_PRIMITIVE ||
            capture->type->name == NULL || capture->name == NULL || capture->type_name == NULL ||
            ast_identifier_name(capture->source) == NULL ||
            name == NULL || type == NULL || capture->mode != LAMBDA_CAPTURE_COPY ||
            capture->mode != ast_lambda_capture_mode(lambda, c) ||
            strcmp(name, capture->name) != 0 || strcmp(type, capture->type_name) != 0 ||
            strcmp(ast_identifier_name(capture->source), capture->name) != 0 ||
            strcmp(capture->type->name, capture->type_name) != 0 ||
            capture->copy_escape != publication_physical_scalar(capture->type)) return false;
    }
    return true;
}
bool semantic_param_publication_record_checked_deferred(SemanticContext *ctx, ASTNode *boundary)
{
    if (!semantic_param_publication_pending(ctx)) return true;
    if (ctx->has_error || boundary == NULL) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, boundary, false);
    if (row == NULL || !row->deferred_scan || row->deferred_shape != boundary->type ||
        !publication_deferred_kind(boundary->type) ||
        (boundary->type == AST_LAMBDA_EXPR && !publication_capture_metadata_current(boundary, row))) return false;
    if (boundary->type == AST_SPAWN_EXPR) {
        ASTNode *call = ast_spawn_function(boundary);
        PublishedNode *checked = publication_node(ctx->function_param_publications, call, false);
        if (ast_spawn_arg_count(boundary) != 0 || call == NULL || call->type != AST_CALL ||
            !publication_call_ready(checked) || checked->value_binding != 0) return false;
    }
    FunctionParamFlowIndexClaims *syntax = NULL;
    if (!function_param_flow_syntax_claims_snapshot(ctx, &syntax, boundary)) {
        function_param_flow_claims_destroy(syntax); return false;
    }
    row = publication_node(ctx->function_param_publications, boundary, false);
    if (row == NULL) { function_param_flow_claims_destroy(syntax); return false; }
    function_param_flow_claims_destroy(row->deferred_syntax);
    row->deferred_syntax = syntax; row->deferred_checked = true;
    return true;
}
bool semantic_param_publication_query_deferred(const SemanticContext *ctx,
    const ASTNode *boundary, SemanticPublishedDeferred *view)
{
    if (view == NULL) return false;
    *view = (SemanticPublishedDeferred){0};
    if (!semantic_param_publication_inputs_ready(ctx) || boundary == NULL) return false;
    PublishedNode *row = publication_node(ctx->function_param_publications, boundary, false);
    if (row == NULL || !row->deferred_scan || !row->deferred_checked ||
        row->deferred_shape != boundary->type || !publication_deferred_kind(boundary->type) ||
        !function_param_flow_syntax_claims_current(ctx, row->deferred_syntax)) return false;
    if ((boundary->type == AST_LAMBDA_EXPR || boundary->type == AST_SPAWN_EXPR) &&
        publication_current_value(ctx, boundary) == NULL) return false;
    if (boundary->type == AST_LAMBDA_EXPR) {
        if (!publication_capture_metadata_current(boundary, row)) return false;
        for (size_t c = 0; c < row->deferred_capture_count; c++) {
            const PublishedDeferredCapture *capture = &row->deferred_captures[c];
            PublishedNode *source = publication_current_value(ctx, capture->source);
            if (source == NULL || source->binding_id != capture->binding_id ||
                capture->binding_id != ast_identifier_binding_syntax_id(capture->source) ||
                capture->category != source->category || capture->copy_escape != source->copy_escape ||
                capture->category != OWNERSHIP_TYPE_COPY_ONLY) return false;
        }
    } else if (row->deferred_capture_count != 0) return false;
    if (boundary->type == AST_SPAWN_EXPR &&
        (ast_spawn_arg_count(boundary) != 0 || ast_spawn_function(boundary) == NULL ||
         ast_spawn_function(boundary)->type != AST_CALL ||
         !semantic_param_publication_checked_call(ctx, ast_spawn_function(boundary)))) return false;
    if (boundary->type == AST_EVENT_SUBSCRIBE || boundary->type == AST_EVENT_UNSUBSCRIBE ||
        boundary->type == AST_EVENT_INVOKE) {
        const ASTNode *event = boundary->type == AST_EVENT_INVOKE ?
            ast_event_invoke_event(boundary) : ast_event_op_event(boundary);
        if (publication_current_value(ctx, event) == NULL) return false;
        size_t count = boundary->type == AST_EVENT_INVOKE ? ast_event_invoke_arg_count(boundary) : 1;
        for (size_t a = 0; a < count; a++) {
            const ASTNode *arg = boundary->type == AST_EVENT_INVOKE ?
                ast_event_invoke_argument(boundary, a) : ast_event_op_handler(boundary);
            if (publication_current_value(ctx, arg) == NULL) return false;
            if (arg->type == AST_LAMBDA_EXPR) {
                SemanticPublishedDeferred handler;
                if (!semantic_param_publication_query_deferred(ctx, arg, &handler)) return false;
            }
        }
    }
    view->captures = row->deferred_captures; view->capture_count = row->deferred_capture_count;
    return true;
}
