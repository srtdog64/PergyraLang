/* Array result, return, drop, and preservation replay receipts. */
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_deferred_preservation_internal.h"
#include "type_checker/type_checker_internal.h"
#include "slot_analyzer_internal.h"
#include "slot_summary.h"
#include "diag_codes.h"
#include "function_param_flow/function_param_publication_owner.h"
#include "function_param_flow/function_param_flow_summary_internal.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "collection_ownership_fact.h"
#include "collection_owned_element_requirement_owner.h"
#include "formal_string_store_landing_owner.h"
#include "loan_copy_landing_owner.h"
#include "builtin_kind.h"

#include <stdlib.h>
#include <string.h>

bool
semantic_array_storage_record_pending_owned_string_result(
    SemanticContext *ctx, ASTNode *push_call, ASTNode *receiver,
    ASTNode *result_call,
    uint32_t producer_syntax_id)
{
    ArrayStorageDeferredPreservationStore *store = ctx != NULL
        ? ctx->array_storage_deferred_preservations : NULL;
    Symbol *binding = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? scope_lookup(ctx->scope, ast_identifier_name(receiver)) : NULL;
    uint32_t binding_id = receiver != NULL && receiver->type == AST_IDENTIFIER
        ? ast_identifier_binding_syntax_id(receiver) : 0;
    uint32_t current_producer = 0;
    ASTNode *function_decl = ctx != NULL ? ctx->current_function_decl : NULL;
    size_t formal_index = SIZE_MAX;
    ParamMode formal_mode = PARAM_MODE_DEFAULT;
    bool parameter_receiver = false;
    if (store == NULL || !store->active || receiver == NULL
        || receiver->type != AST_IDENTIFIER || result_call == NULL
        || result_call->type != AST_CALL || push_call == NULL
        || push_call->type != AST_CALL || ast_call_arg_count(push_call) != 2
        || ast_call_argument(push_call, 0) != receiver
        || ast_call_argument(push_call, 1) != result_call
        || ast_node_stable_id(push_call) == 0 || binding == NULL
        || binding_id == 0 || binding->decl_syntax_id != binding_id
        || binding->is_consumed
        || binding->is_host_field || binding->type == NULL
        || !type_is_constructed_named(binding->type, "Array")
        || !type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)
        || ast_node_stable_id(result_call) == 0 || producer_syntax_id == 0
        || !semantic_collection_owned_string_result_candidate(
            result_call, ctx, &current_producer)
        || current_producer != producer_syntax_id)
        return false;
    if (binding->is_parameter) {
        bool carrier_current =
            semantic_collection_owned_element_parameter_carrier_current(
                receiver, function_decl, ctx, &formal_index, &formal_mode,
                &parameter_receiver);
        if (!carrier_current || !parameter_receiver || formal_index == SIZE_MAX
            || (formal_mode != PARAM_MODE_OWN
                && formal_mode != PARAM_MODE_MUT_REF
                && formal_mode != PARAM_MODE_DEFAULT))
            return false;
    }
    FuncParam *formal = parameter_receiver
        && formal_index < ast_func_param_count(function_decl)
        ? ast_func_param(function_decl, formal_index) : NULL;
    uint32_t function_id = parameter_receiver
        ? ast_node_stable_id(function_decl) : 0;
    uint32_t formal_id = formal != NULL
        ? ast_func_param_stable_id(formal) : 0;
    if (parameter_receiver && (function_id == 0 || formal_id == 0
        || formal->mode != formal_mode))
        return false;
    for (size_t i = 0; i < store->owned_string_result_count; i++) {
        const ArrayStoragePendingOwnedStringResult *row =
            &store->owned_string_results[i];
        if (row->result_call != result_call)
            continue;
        return row->push_call == push_call
            && row->push_call_id == ast_node_stable_id(push_call)
            && row->receiver == receiver
            && row->receiver_id == ast_node_stable_id(receiver)
            && row->result_call_id == ast_node_stable_id(result_call)
            && row->binding_id == binding_id
            && row->producer_id == producer_syntax_id
            && row->parameter_receiver == parameter_receiver
            && row->function_decl == (parameter_receiver
                ? function_decl : NULL)
            && row->function_id == function_id
            && row->formal_index == (parameter_receiver
                ? formal_index : SIZE_MAX)
            && row->formal_id == formal_id
            && row->formal_mode == formal_mode;
    }
    if (store->next_sequence == SIZE_MAX
        || !array_storage_deferred_reserve((void **)&store->owned_string_results,
            &store->owned_string_result_capacity,
            store->owned_string_result_count + 1,
            sizeof(*store->owned_string_results)))
        return false;
    store->owned_string_results[store->owned_string_result_count++] =
        (ArrayStoragePendingOwnedStringResult){
            .push_call = push_call,
            .push_call_id = ast_node_stable_id(push_call),
            .receiver = receiver,
            .receiver_id = ast_node_stable_id(receiver),
            .result_call = result_call,
            .result_call_id = ast_node_stable_id(result_call),
            .binding_id = binding_id,
            .producer_id = producer_syntax_id,
            .function_decl = parameter_receiver ? function_decl : NULL,
            .function_id = function_id,
            .formal_index = parameter_receiver ? formal_index : SIZE_MAX,
            .formal_id = formal_id,
            .formal_mode = formal_mode,
            .sequence = store->next_sequence++,
            .parameter_receiver = parameter_receiver,
        };
    return true;
}

bool
semantic_array_storage_has_pending_owned_string_result(
    const SemanticContext *ctx, uint32_t binding_id)
{
    const ArrayStorageDeferredPreservationStore *store = ctx != NULL
        ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || binding_id == 0)
        return false;
    for (size_t i = 0; i < store->owned_string_result_count; i++)
        if (store->owned_string_results[i].binding_id == binding_id)
            return true;
    return false;
}

bool
semantic_array_storage_pending_owned_string_parameter_push(
    SemanticContext *ctx, const ASTNode *push_call,
    const ASTNode *function_decl, size_t formal_index)
{
    ArrayStorageDeferredPreservationStore *store = ctx != NULL
        ? ctx->array_storage_deferred_preservations : NULL;
    /* Finalization closes the recording phase before it replays parameter
     * summaries.  The exact deferred row remains the authority during that
     * replay; only a completed finalization retires it. */
    if (store == NULL || store->finalized ||
        push_call == NULL || push_call->type != AST_CALL ||
        function_decl == NULL || function_decl->type != AST_FUNC_DECL ||
        formal_index >= ast_func_param_count(function_decl))
        return false;
    FuncParam *formal = ast_func_param(function_decl, formal_index);
    for (size_t i = 0; i < store->owned_string_result_count; i++) {
        const ArrayStoragePendingOwnedStringResult *row =
            &store->owned_string_results[i];
        if (row->push_call != push_call || row->push_call_id == 0 ||
            row->push_call_id != ast_node_stable_id(push_call) ||
            row->receiver == NULL || row->receiver->type != AST_IDENTIFIER ||
            row->receiver_id == 0 ||
            row->receiver_id != ast_node_stable_id(row->receiver) ||
            ast_call_arg_count(push_call) != 2 ||
            ast_call_argument(push_call, 0) != row->receiver ||
            ast_call_argument(push_call, 1) != row->result_call ||
            row->binding_id == 0 ||
            ast_identifier_binding_syntax_id(row->receiver) != row->binding_id ||
            row->result_call == NULL || row->result_call->type != AST_CALL ||
            row->result_call_id == 0 ||
            row->result_call_id != ast_node_stable_id(row->result_call) ||
            row->producer_id == 0 || !row->parameter_receiver ||
            row->function_decl != function_decl || row->function_id == 0 ||
            row->function_id != ast_node_stable_id(function_decl) ||
            row->formal_index != formal_index || formal == NULL ||
            row->formal_id == 0 ||
            row->formal_id != ast_func_param_stable_id(formal) ||
            row->formal_mode != formal->mode)
            continue;
        return true;
    }
    return false;
}

bool
array_storage_pending_owned_string_result_current(SemanticContext *ctx,
    ArrayStoragePendingOwnedStringResult *row, bool record_requirement)
{
    uint32_t current_producer = 0;
    SemanticPublishedValue receiver_value;
    if (ctx == NULL || row == NULL || row->push_call == NULL ||
        row->push_call->type != AST_CALL || row->push_call_id == 0 ||
        row->push_call_id != ast_node_stable_id(row->push_call) ||
        !semantic_param_publication_checked_call(ctx, row->push_call) ||
        ast_call_arg_count(row->push_call) != 2 || row->receiver == NULL ||
        row->receiver->type != AST_IDENTIFIER || row->receiver_id == 0 ||
        row->receiver_id != ast_node_stable_id(row->receiver) ||
        ast_call_argument(row->push_call, 0) != row->receiver ||
        row->result_call == NULL || row->result_call->type != AST_CALL ||
        row->result_call_id == 0 ||
        row->result_call_id != ast_node_stable_id(row->result_call) ||
        ast_call_argument(row->push_call, 1) != row->result_call ||
        row->binding_id == 0 ||
        semantic_param_publication_binding(ctx, row->receiver) !=
            row->binding_id || row->producer_id == 0 ||
        !semantic_collection_owned_string_call_result(
            row->result_call, ctx, &current_producer) ||
        current_producer != row->producer_id) {
        return false;
    }
    receiver_value = semantic_param_publication_value(ctx, row->receiver);
    if (!receiver_value.ok || receiver_value.meta ||
        !type_is_constructed_named(receiver_value.type, "Array") ||
        !type_equals(type_get_constructed_arg(receiver_value.type, 0),
            TYPE_STRING)) {
        return false;
    }
    if (!row->parameter_receiver)
        return row->function_decl == NULL && row->function_id == 0 &&
            row->formal_index == SIZE_MAX && row->formal_id == 0 &&
            !row->parameter_requirement_recorded;
    if (row->function_decl == NULL ||
        row->function_decl->type != AST_FUNC_DECL || row->function_id == 0 ||
        row->function_id != ast_node_stable_id(row->function_decl) ||
        row->formal_index >= ast_func_param_count(row->function_decl)) {
        return false;
    }
    FuncParam *formal = ast_func_param(row->function_decl, row->formal_index);
    if (formal == NULL || row->formal_id == 0 ||
        row->formal_id != ast_func_param_stable_id(formal) ||
        row->formal_mode != formal->mode ||
        !semantic_param_publication_array_parameter_value_current(
            ctx, row->receiver, row->function_decl, row->formal_index,
            row->formal_mode)) {
        return false;
    }
    if (record_requirement && !row->parameter_requirement_recorded) {
        if (!semantic_collection_owned_element_requirement_record_parameter_mutation(
                row->push_call, row->receiver, row->function_decl,
                row->formal_index, ctx))
            return false;
        row->parameter_requirement_recorded = true;
    }
    return row->parameter_requirement_recorded;
}

bool
semantic_array_storage_record_pending_result(SemanticContext *ctx,
    Symbol *binding, ASTNode *call)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    uint32_t builtin = 0;
    const char *checked_result = call != NULL
        ? ast_call_semantic_value_type_name(call) : NULL;
    bool provisional_clone = binding != NULL && call != NULL
        && call->type == AST_CALL && ast_call_arg_count(call) == 1
        && ast_call_semantic_callee_builtin_kind(call, &builtin)
        && builtin == BUILTIN_CLONE
        && ast_call_semantic_callee_is_stdlib(call)
        && ast_call_semantic_callee_decl_id(call) == 0
        && ast_call_semantic_callee_value_binding_id(call) == 0
        && checked_result != NULL && binding->type != NULL
        && binding->type->name != NULL
        && strcmp(checked_result, binding->type->name) == 0;
    if (store == NULL || binding == NULL || call == NULL || call->type != AST_CALL
        || binding->decl_syntax_id == 0
        || !type_is_constructed_named(binding->type, "Array")
        || ast_node_stable_id(call) == 0
        || !(semantic_array_storage_independent_call_result(call, ctx)
            || provisional_clone ||
            (ast_call_semantic_callee_declared_callable(call) &&
             ast_call_semantic_callee_value_binding_id(call) == 0 &&
             ast_call_semantic_callee_decl_id(call) != 0))) return false;
    for (size_t i = 0; i < store->result_count; i++) {
        const ArrayStoragePendingResult *row = &store->results[i];
        if (row->birth.binding_id != binding->decl_syntax_id) continue;
        return (store->active ? row->birth.caller == ctx->current_function_decl
                : store->finalized && row->proved)
            && row->birth.call == call
            && row->birth.call_id == ast_node_stable_id(call)
            && row->birth.callee_id == ast_call_semantic_callee_decl_id(call)
            && type_equals(row->birth.type, binding->type)
            && (store->active || (semantic_array_storage_result_birth_current(ctx, &row->birth)
                && semantic_array_storage_owned_result_current(ctx, call)));
    }
    if (!store->active || ctx->current_function_decl == NULL
        || ast_node_stable_id(ctx->current_function_decl) == 0
        || store->next_sequence == SIZE_MAX
        || !array_storage_deferred_reserve((void **)&store->results, &store->result_capacity,
            store->result_count + 1, sizeof(*store->results))) return false;
    store->results[store->result_count++] = (ArrayStoragePendingResult){
        .birth = {ctx->current_function_decl, call,
            ast_node_stable_id(ctx->current_function_decl), ast_node_stable_id(call),
            binding->decl_syntax_id, ast_call_semantic_callee_decl_id(call),
            binding->type, store->next_sequence++}};
    return true;
}

bool
semantic_array_storage_record_owned_return(SemanticContext *ctx,
    ASTNode *ret, ASTNode *value, const Type *type)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    ASTNode *decl = ctx != NULL ? ctx->current_function_decl : NULL;
    if (store == NULL || !store->active || ctx->has_error || decl == NULL
        || decl->type != AST_FUNC_DECL || ret == NULL || ret->type != AST_RETURN
        || value == NULL || ast_return_value(ret) != value
        || !type_is_constructed_named(type, "Array")
        || ast_node_stable_id(decl) == 0 || ast_node_stable_id(ret) == 0
        || ast_node_stable_id(value) == 0) return false;
    ArrayStorageReturnSource source = ARRAY_RESULT_UNPROVED;
    uint32_t binding_id = 0, callee_id = 0;
    const ASTNode *element_source = NULL;
    bool elements_owned = !type_equals(type_get_constructed_arg(type, 0), TYPE_STRING)
        || semantic_array_storage_return_elements_owned(value, ctx, &element_source);
    Symbol *binding = NULL;
    if (value->type == AST_ARRAY_LITERAL) source = ARRAY_RESULT_LITERAL;
    else if (value->type == AST_CALL) {
        if (semantic_array_storage_independent_call_result(value, ctx))
            source = ARRAY_RESULT_INDEPENDENT_CALL;
        else if (ast_call_semantic_callee_declared_callable(value) &&
            ast_call_semantic_callee_value_binding_id(value) == 0 &&
            (callee_id = ast_call_semantic_callee_decl_id(value)) != 0)
            source = ARRAY_RESULT_DECLARED_CALL;
    } else if (value->type == AST_IDENTIFIER) {
        binding = lookup_identifier_symbol(value, ctx);
        if (binding != NULL && binding->decl_syntax_id != 0 &&
            ast_identifier_binding_syntax_id(value) == binding->decl_syntax_id &&
            type_equals(binding->type, type) && binding->is_parameter &&
            binding->param_mode == PARAM_MODE_DEFAULT && !binding->is_host_field) {
            source = ARRAY_RESULT_DEFAULT_FORMAL;
            binding_id = binding->decl_syntax_id;
        } else if (binding != NULL && binding->decl_syntax_id != 0 &&
            ast_identifier_binding_syntax_id(value) == binding->decl_syntax_id &&
            type_equals(binding->type, type) && binding->has_exclusive_array_storage &&
            !binding->has_escaped_array_storage && !binding->is_consumed &&
            !binding->is_host_field && (!binding->is_parameter || binding->param_mode == PARAM_MODE_OWN)) {
            source = ARRAY_RESULT_OWNED_BINDING;
            binding_id = binding->decl_syntax_id;
        }
    }
    ArrayStorageReturnElementReceipt elements = {
        .function_id = ast_node_stable_id(decl),
        .binding_id = binding_id,
        .owned = elements_owned,
        .dependency_call = element_source,
        .dependency_call_id = element_source != NULL
            ? ast_node_stable_id(element_source) : 0,
    };
    for (size_t i = 0; i < store->return_count; i++) {
        const ArrayStorageOwnedReturn *row = &store->returns[i];
        if (row->statement != ret && row->statement_id != ast_node_stable_id(ret)) continue;
        bool same = row->decl == decl && row->statement == ret && row->value == value
            && row->source == source && row->binding_id == binding_id
            && row->callee_id == callee_id && type_equals(row->type, type)
            && row->elements.owned == elements.owned
            && row->elements.dependency_call == elements.dependency_call
            && row->elements.dependency_call_id == elements.dependency_call_id;
        return same && (source != ARRAY_RESULT_OWNED_BINDING
            || consume_array_storage_binding(value, ctx));
    }
    if (store->next_sequence == SIZE_MAX || !array_storage_deferred_reserve((void **)&store->returns,
        &store->return_capacity, store->return_count + 1, sizeof(*store->returns))) return false;
    if (source == ARRAY_RESULT_OWNED_BINDING && !consume_array_storage_binding(value, ctx)) return false;
    store->returns[store->return_count++] = (ArrayStorageOwnedReturn){
        .decl = decl, .statement = ret, .value = value,
        .decl_id = ast_node_stable_id(decl), .statement_id = ast_node_stable_id(ret),
        .value_id = ast_node_stable_id(value), .binding_id = binding_id,
        .callee_id = callee_id, .elements = elements,
        .type = type, .sequence = store->next_sequence++, .source = source,
    };
    return true;
}

bool
semantic_array_storage_record_pending_drop(SemanticContext *ctx,
    ASTNode *receiver, const Symbol *binding, ArrayStorageReleaseOperation operation)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    ArrayStorageReturnElementReceipt elements = {0};
    if (store == NULL || !store->active || receiver == NULL || binding == NULL
        || receiver->type != AST_IDENTIFIER || ast_node_stable_id(receiver) == 0
        || binding->decl_syntax_id == 0
        || ast_identifier_binding_syntax_id(receiver) != binding->decl_syntax_id
        || (operation != ARRAY_STORAGE_RELEASE_DROP_STORAGE
            && operation != ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
            && operation != ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
            && operation != ARRAY_STORAGE_MUTATE_STORAGE
            && operation != ARRAY_STORAGE_MUTATE_OWNED_STRINGS
            && operation != ARRAY_STORAGE_CALL_AFTER_PENDING)
        || (operation == ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
            && !semantic_array_storage_capture_element_use(receiver, ctx, &elements))
        || !array_storage_deferred_reserve((void **)&store->drops, &store->drop_capacity,
            store->drop_count + 1, sizeof(*store->drops)))
        return false;
    store->drops[store->drop_count++] = (ArrayStoragePendingDrop){
        receiver, ast_node_stable_id(receiver), binding->decl_syntax_id,
        store->next_sequence++, operation, elements, SIZE_MAX};
    return true;
}

bool
array_storage_pending_result_proof(SemanticContext *ctx, ArrayStoragePendingResult *row)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || row == NULL || row->checking
        || (!store->sealing && !row->proved)) return false;
    row->checking = true;
    bool current = semantic_array_storage_result_birth_current(ctx, &row->birth)
        && semantic_array_storage_owned_result_current(ctx, row->birth.call);
    row->checking = false;
    if (store->sealing && current) row->proved = true;
    return current;
}

/* A deep-drop receipt may be issued before a declared Array<String> result's
 * element proof is sealed.  The provisional receipt still names the exact
 * result birth; finalization may complete that same proof, but it must not
 * replace a failed owned receipt with a different source. */
bool
array_storage_pending_drop_element_source_current(SemanticContext *ctx,
    ArrayStorageDeferredPreservationStore *store,
    const ArrayStoragePendingDrop *drop)
{
    const ArrayStorageReturnElementReceipt *receipt = &drop->elements;
    if (receipt->dependency_call == NULL)
        return receipt->dependency_call_id == 0;
    if (semantic_array_storage_element_use_current(
            drop->receiver, ctx, receipt))
        return true;
    if (receipt->owned || receipt->function_id == 0
        || receipt->binding_id != drop->binding_id
        || receipt->dependency_call->type != AST_CALL
        || receipt->dependency_call_id == 0
        || receipt->dependency_call_id
            != ast_node_stable_id(receipt->dependency_call))
        return false;
    ArrayStoragePendingResult *matched = NULL;
    for (size_t i = 0; i < store->result_count; i++) {
        ArrayStoragePendingResult *result = &store->results[i];
        if (result->birth.binding_id != drop->binding_id
            || result->birth.call != receipt->dependency_call)
            continue;
        if (matched != NULL || result->birth.sequence >= drop->sequence
            || result->birth.caller_id != receipt->function_id
            || result->birth.call_id != receipt->dependency_call_id
            || !type_is_constructed_named(result->birth.type, "Array")
            || !type_equals(type_get_constructed_arg(
                    result->birth.type, 0), TYPE_STRING))
            return false;
        matched = result;
    }
    return matched != NULL && array_storage_pending_result_proof(ctx, matched)
        && semantic_array_storage_owned_string_result_current(
            ctx, matched->birth.call);
}

size_t
semantic_array_storage_pending_sequence(const SemanticContext *ctx)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    return store != NULL ? store->next_sequence : 0;
}

bool
semantic_array_storage_pending_call_entry_current(SemanticContext *ctx,
    uint32_t binding_id, const ASTNode *call, const ASTNode *callee_decl,
    size_t ordinal, size_t formal_ordinal, size_t sequence)
{
    const ArrayStorageDeferredPreservationStore *store = ctx != NULL
        ? ctx->array_storage_deferred_preservations : NULL;
    const ArrayStoragePendingCallReceipt *matched = NULL;
    if (store == NULL || !store->finalized || binding_id == 0
        || call == NULL || call->type != AST_CALL
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || sequence == SIZE_MAX)
        return false;
    for (size_t i = 0; i < store->call_count; i++) {
        const ArrayStoragePendingCallReceipt *source =
            &store->calls[i].source;
        if (source->sequence != sequence)
            continue;
        if (matched != NULL)
            return false;
        matched = source;
    }
    return matched != NULL && !matched->payload
        && matched->binding_id == binding_id
        && matched->call == call
        && matched->callee_decl == callee_decl
        && matched->ordinal == ordinal
        && matched->formal_ordinal == formal_ordinal
        && semantic_array_storage_pending_call_current(ctx, matched);
}

bool
semantic_array_storage_pending_preserved(SemanticContext *ctx,
    uint32_t binding_id, size_t sequence)
{
    const ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->finalized)
        return false;
    for (size_t i = 0; i < store->call_count; i++) {
        const ArrayStoragePendingCall *pending = &store->calls[i];
        if (!pending->source.payload && pending->source.binding_id == binding_id
            && pending->source.sequence < sequence
            && (pending->terminal_end == 0 || sequence < pending->terminal_end)
            && (!pending->preserved || (!store->consuming_checked_calls &&
                (semantic_array_storage_pending_call_failures(ctx, &pending->source)
                 & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) != 0)))
        {
            return false;
        }
    }
    for (size_t i = 0; i < store->result_count; i++) {
        ArrayStoragePendingResult *result = &store->results[i];
        if (result->birth.binding_id == binding_id && result->birth.sequence < sequence &&
            !array_storage_pending_result_proof(ctx, result)) return false;
    }
    return true;
}

bool
semantic_array_storage_pending_owned_elements(SemanticContext *ctx,
    uint32_t binding_id, size_t sequence)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->finalized || binding_id == 0) return false;
    for (size_t i = 0; i < store->call_count; i++) {
        const ArrayStoragePendingCall *pending = &store->calls[i];
        if (pending->source.binding_id == binding_id
            && pending->source.sequence < sequence
            && (pending->terminal_end == 0 || sequence < pending->terminal_end)
            && semantic_array_storage_pending_call_elements_required(&pending->source)
            && (!pending->elements_preserved || (!store->consuming_checked_calls &&
                (semantic_array_storage_pending_call_failures(ctx, &pending->source)
                 & FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED) != 0)))
            return false;
    }
    for (size_t i = 0; i < store->result_count; i++) {
        ArrayStoragePendingResult *result = &store->results[i];
        if (result->birth.binding_id != binding_id || result->birth.sequence >= sequence) continue;
        if (!type_equals(type_get_constructed_arg(result->birth.type, 0), TYPE_STRING)
            || !array_storage_pending_result_proof(ctx, result)
            || !semantic_array_storage_owned_string_result_current(ctx, result->birth.call))
            return false;
    }
    for (size_t i = 0; i < store->owned_string_result_count; i++) {
        ArrayStoragePendingOwnedStringResult *result =
            &store->owned_string_results[i];
        if (result->binding_id != binding_id
            || result->sequence >= sequence)
            continue;
        if (!array_storage_pending_owned_string_result_current(ctx, result, false))
            return false;
    }
    return true;
}

/* An Array handed to two by-value formals of one call, an earlier of which
 * may keep or change it, is a moved argument used again. When every earlier
 * handoff that leaves the descriptor unpreserved is such an argument of the
 * consumer's own call, each is copied where the call takes it
 * (2026-10-22_moved_argument_used_again_is_copied); otherwise nothing is
 * selected. False only when a selection could not be recorded. */
bool
array_storage_same_call_arguments_copied(SemanticContext *ctx, const ArrayStorageDeferredPreservationStore *store,
    const ArrayStoragePendingDrop *drop)
{
    const ArrayStoragePendingCallReceipt *consumer = &store->calls[drop->call_index].source;
    for (size_t i = 0; i < store->result_count; i++) {
        ArrayStoragePendingResult *result = &store->results[i];
        if (result->birth.binding_id == drop->binding_id && result->birth.sequence < consumer->sequence &&
            !array_storage_pending_result_proof(ctx, result)) return true;
    }
    size_t earlier = 0;
    for (bool select = false;; select = true) {
        for (size_t i = 0; i < store->call_count; i++) {
            const ArrayStoragePendingCall *pending = &store->calls[i];
            const ArrayStoragePendingCallReceipt *source = &pending->source;
            if (source->payload || source->binding_id != drop->binding_id || source->sequence >= consumer->sequence ||
                (pending->terminal_end != 0 && consumer->sequence >= pending->terminal_end) || pending->preserved)
                continue;
            if (select) {
                if (!semantic_loan_copy_landing_select_carrier(ctx, source->argument)) return false;
                continue;
            }
            FuncParam *formal = source->callee_decl != NULL &&
                source->formal_ordinal < ast_func_param_count(source->callee_decl)
                ? ast_func_param(source->callee_decl, source->formal_ordinal) : NULL;
            if (source->call != consumer->call || formal == NULL || formal->mode != PARAM_MODE_DEFAULT) return true;
            earlier++;
        }
        if (select || earlier == 0) return true;
    }
}
