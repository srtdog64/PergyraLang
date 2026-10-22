/* Order-independent descriptor preservation for public array release.
 * See array_storage_deferred_preservation_owner.h for the contract. */
#include "array_storage_deferred_preservation_owner.h"
#include "array_storage_deferred_preservation_internal.h"
#include "type_checker_internal.h"
#include "slot_analyzer_internal.h"
#include "slot_summary.h"
#include "diag_codes.h"
#include "function_param_publication_owner.h"
#include "function_param_flow_summary_internal.h"
#include "array_storage_release_owner.h"
#include "array_storage_element_lifetime_owner.h"
#include "loan_copy_landing_owner.h"

#include <stdlib.h>

static bool
deferred_reserve(void **rows, size_t *capacity, size_t needed, size_t row_size)
{
    if (needed <= *capacity)
        return true;
    size_t next = *capacity == 0 ? 16 : *capacity;
    while (next < needed) {
        if (next > SIZE_MAX / 2)
            return false;
        next *= 2;
    }
    if (next > SIZE_MAX / row_size)
        return false;
    void *grown = realloc(*rows, next * row_size);
    if (grown == NULL)
        return false;
    *rows = grown;
    *capacity = next;
    return true;
}

bool
semantic_array_storage_deferral_begin(SemanticContext *ctx)
{
    if (ctx == NULL)
        return false;
    semantic_array_storage_deferral_destroy(ctx);
    ArrayStorageDeferredPreservationStore *store = calloc(1, sizeof(*store));
    if (store == NULL)
        return false;
    store->active = true;
    ctx->array_storage_deferred_preservations = store;
    return true;
}

bool
semantic_array_storage_deferral_active(const SemanticContext *ctx)
{
    return ctx != NULL && ctx->array_storage_deferred_preservations != NULL
        && ctx->array_storage_deferred_preservations->active;
}

static bool
record_pending_call_kind(SemanticContext *ctx, Symbol *binding,
    ASTNode *call, ASTNode *callee_decl, size_t ordinal, bool payload)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    ASTNode *argument = call != NULL && call->type == AST_CALL
        && ordinal < ast_call_arg_count(call) ? ast_call_argument(call, ordinal) : NULL;
    uint32_t source_id = 0;
    size_t formal = 0;
    if (store == NULL || !store->active || binding == NULL || call == NULL
        || callee_decl == NULL || binding->decl_syntax_id == 0
        || binding->type == NULL || ast_node_stable_id(call) == 0
        || ast_node_stable_id(callee_decl) == 0 || argument == NULL
        || ast_node_stable_id(argument) == 0
        || ast_call_semantic_callee_decl_id(call) != ast_node_stable_id(callee_decl)
        || ast_call_semantic_callee_value_binding_id(call) != 0
        || !semantic_param_publication_selected_formal_for_actual(ctx,
            call, callee_decl, ordinal, &formal)
        || (payload ? (!ast_call_semantic_callee_declared_callable(call)
            || !semantic_array_storage_payload_call_capture(ctx, binding, call, argument,
                callee_decl, ordinal, &source_id))
            : (argument->type != AST_IDENTIFIER
                || ast_identifier_binding_syntax_id(argument) != binding->decl_syntax_id))
        || store->next_sequence == SIZE_MAX
        || !deferred_reserve((void **)&store->calls, &store->call_capacity,
            store->call_count + 1, sizeof(*store->calls)))
        return false;
    store->calls[store->call_count++] = (ArrayStoragePendingCall){
        .source = {binding->decl_syntax_id, call, ast_node_stable_id(call), argument,
            ast_node_stable_id(argument), callee_decl, ast_node_stable_id(callee_decl),
            binding->type, ordinal, formal,
            ast_func_param_stable_id(ast_func_param(callee_decl, formal)),
            store->next_sequence++, payload, source_id}};
    if (!payload) binding->has_pending_inout_preservation = true;
    return true;
}

bool
semantic_array_storage_record_pending_call(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *callee_decl, size_t ordinal)
{
    return record_pending_call_kind(ctx, binding, call, callee_decl, ordinal, false);
}

bool
semantic_array_storage_record_pending_payload(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *callee_decl, size_t ordinal)
{
    return record_pending_call_kind(ctx, binding, call, callee_decl, ordinal, true);
}

size_t
semantic_array_storage_return_value_begin(const SemanticContext *ctx)
{
    const ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    return store != NULL && store->active ? store->next_sequence : SIZE_MAX;
}

void
semantic_array_storage_return_value_checked(SemanticContext *ctx, size_t first)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->active || first == SIZE_MAX) return;
    for (size_t i = 0; i < store->call_count; i++)
        if (store->calls[i].source.sequence >= first && store->calls[i].terminal_end == 0)
            store->calls[i].terminal_end = store->next_sequence;
}

bool
semantic_array_storage_record_pending_call_consumer(SemanticContext *ctx,
    const Symbol *binding, ASTNode *call, size_t actual_ordinal)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->active || store->call_count == 0 ||
        binding == NULL || call == NULL || call->type != AST_CALL ||
        binding->decl_syntax_id == 0 || store->next_sequence == SIZE_MAX) return false;
    size_t index = store->call_count - 1;
    const ArrayStoragePendingCallReceipt *source = &store->calls[index].source;
    ASTNode *argument = actual_ordinal < ast_call_arg_count(call)
        ? ast_call_argument(call, actual_ordinal) : NULL;
    if (argument == NULL || source->binding_id != binding->decl_syntax_id ||
        !type_equals(source->binding_type, binding->type) ||
        source->call != call || source->call_id != ast_node_stable_id(call) ||
        source->ordinal != actual_ordinal || source->argument != argument ||
        source->argument_id != ast_node_stable_id(argument) ||
        source->sequence + 1 != store->next_sequence ||
        !deferred_reserve((void **)&store->drops, &store->drop_capacity,
            store->drop_count + 1, sizeof(*store->drops))) return false;
    store->drops[store->drop_count++] = (ArrayStoragePendingDrop){
        call, source->call_id, source->binding_id, store->next_sequence++,
        ARRAY_STORAGE_CALL_AFTER_PENDING, {0}, index};
    return true;
}

bool
semantic_array_storage_has_pending_elements(SemanticContext *ctx, uint32_t binding_id)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || binding_id == 0) return false;
    for (size_t i = 0; i < store->call_count; i++) {
        const ArrayStoragePendingCallReceipt *source = &store->calls[i].source;
        if (source->binding_id == binding_id
            && semantic_array_storage_pending_call_elements_required(source))
            return true;
    }
    return false;
}

bool
semantic_array_storage_record_pending_result(SemanticContext *ctx,
    Symbol *binding, ASTNode *call)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || binding == NULL || call == NULL || call->type != AST_CALL
        || binding->decl_syntax_id == 0
        || !type_is_constructed_named(binding->type, "Array")
        || ast_node_stable_id(call) == 0
        || !(semantic_array_storage_independent_call_result(call, ctx) ||
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
        || !deferred_reserve((void **)&store->results, &store->result_capacity,
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
            type_equals(binding->type, type) && binding->has_exclusive_array_storage &&
            !binding->has_escaped_array_storage && !binding->is_consumed &&
            !binding->is_host_field && (!binding->is_parameter || binding->param_mode == PARAM_MODE_OWN)) {
            source = ARRAY_RESULT_OWNED_BINDING;
            binding_id = binding->decl_syntax_id;
        }
    }
    ArrayStorageReturnElementReceipt elements = {
        ast_node_stable_id(decl), binding_id, elements_owned, element_source,
        element_source != NULL ? ast_node_stable_id(element_source) : 0};
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
    if (store->next_sequence == SIZE_MAX || !deferred_reserve((void **)&store->returns,
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
        || !deferred_reserve((void **)&store->drops, &store->drop_capacity,
            store->drop_count + 1, sizeof(*store->drops)))
        return false;
    store->drops[store->drop_count++] = (ArrayStoragePendingDrop){
        receiver, ast_node_stable_id(receiver), binding->decl_syntax_id,
        store->next_sequence++, operation, elements, SIZE_MAX};
    return true;
}

static bool
pending_result_proof(SemanticContext *ctx, ArrayStoragePendingResult *row)
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

size_t
semantic_array_storage_pending_sequence(const SemanticContext *ctx)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    return store != NULL ? store->next_sequence : 0;
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
            return false;
    }
    for (size_t i = 0; i < store->result_count; i++) {
        ArrayStoragePendingResult *result = &store->results[i];
        if (result->birth.binding_id == binding_id && result->birth.sequence < sequence &&
            !pending_result_proof(ctx, result)) return false;
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
            || !pending_result_proof(ctx, result)
            || !semantic_array_storage_owned_string_result_current(ctx, result->birth.call))
            return false;
    }
    return true;
}

bool
semantic_array_storage_deferral_finalize(SemanticContext *ctx)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->active || !semantic_param_publication_ready(ctx))
        return false;
    store->active = false;
    /* The final owner supplies one all-typed store to every consumer. */
    for (size_t i = 0; i < store->call_count; i++) {
        ArrayStoragePendingCall *pending = &store->calls[i];
        unsigned failures = semantic_array_storage_pending_call_failures(ctx, &pending->source);
        pending->preserved = (failures & FUNCTION_PARAM_ARRAY_DESCRIPTOR_UNPROVED) == 0;
        pending->elements_preserved = (failures & FUNCTION_PARAM_ARRAY_ELEMENTS_UNPROVED) == 0;
    }
    store->finalized = true;
    store->sealing = true;
    for (size_t i = 0; i < store->result_count; i++) {
        ArrayStoragePendingResult *result = &store->results[i];
        if (!pending_result_proof(ctx, result))
            semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
                PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_CHANGE_REF_TO_OWN_OR_STOP_ESCAPE,
                result->birth.call, "Array call result storage has no current ownership transfer proof");
    }
    store->sealing = false;
    /* Every call was checked above in this same typed generation. Recording
     * is disabled, and this synchronous loop cannot mutate those inputs.
     * Reuse its verdicts only here; later public queries must replay currency. */
    store->consuming_checked_calls = true;
    for (size_t i = 0; i < store->drop_count; i++) {
        const ArrayStoragePendingDrop *drop = &store->drops[i];
        if (drop->operation == ARRAY_STORAGE_CALL_AFTER_PENDING &&
            drop->receiver != NULL && drop->receiver->type == AST_CALL &&
            drop->call_index < store->call_count) {
            const ArrayStoragePendingCallReceipt *source = &store->calls[drop->call_index].source;
            if (semantic_array_storage_pending_call_consumer_current(ctx, source,
                    drop->receiver, drop->receiver_id, drop->binding_id, drop->sequence))
                continue;
        }
        if (drop->receiver != NULL && drop->receiver->type == AST_IDENTIFIER
            && ast_node_stable_id(drop->receiver) == drop->receiver_id
            && ast_identifier_binding_syntax_id(drop->receiver) == drop->binding_id
            && semantic_array_storage_pending_preserved(ctx, drop->binding_id, drop->sequence)
            && ((drop->operation != ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                    && drop->operation != ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
                    && drop->operation != ARRAY_STORAGE_MUTATE_OWNED_STRINGS)
                || ((drop->operation != ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                    || drop->elements.dependency_call == NULL
                    || semantic_array_storage_element_use_current(drop->receiver,
                        ctx, &drop->elements))
                    && semantic_array_storage_pending_owned_elements(ctx,
                        drop->binding_id, drop->sequence))))
            continue;
        /* A String element handed to a callee that may keep it, with no proof
         * that the loan stays, is copied where it is passed, as any String
         * argument no proof admits is (2026-10-21_string_argument_no_proof_admits_is_copied). */
        if (drop->operation == ARRAY_STORAGE_CALL_AFTER_PENDING && drop->call_index < store->call_count &&
            store->calls[drop->call_index].source.payload &&
            !semantic_loan_copy_landing_select_carrier(ctx, store->calls[drop->call_index].source.argument))
            semantic_error(ctx, drop->receiver, "String element copy landing selection failed");
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            drop->receiver, "%s requires preserved array storage; a prior call may retain or rebind the descriptor",
            drop->operation == ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                ? "ArrayDropOwnedStrings"
                : drop->operation == ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
                    ? "Array<String> move"
                    : drop->operation == ARRAY_STORAGE_MUTATE_STORAGE
                        ? "Array mutation"
                    : drop->operation == ARRAY_STORAGE_MUTATE_OWNED_STRINGS
                        ? "ArrayPushOwnedString"
                        : drop->operation == ARRAY_STORAGE_CALL_AFTER_PENDING
                            ? "Array argument call" : "ArrayDrop");
    }
    store->consuming_checked_calls = false;
    return true;
}

void
semantic_array_storage_deferral_destroy(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->array_storage_deferred_preservations == NULL)
        return;
    free(ctx->array_storage_deferred_preservations->calls);
    free(ctx->array_storage_deferred_preservations->drops);
    free(ctx->array_storage_deferred_preservations->returns);
    free(ctx->array_storage_deferred_preservations->results);
    free(ctx->array_storage_deferred_preservations);
    ctx->array_storage_deferred_preservations = NULL;
}
