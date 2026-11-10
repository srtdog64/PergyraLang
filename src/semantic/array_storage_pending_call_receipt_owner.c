/* Immutable Array call and payload handoff receipts. */
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
#include "collection_ownership_fact.h"
#include "collection_owned_element_requirement_owner.h"
#include "formal_string_store_landing_owner.h"
#include "loan_copy_landing_owner.h"
#include "builtin_kind.h"

#include <stdlib.h>
#include <string.h>

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
        || ctx->current_function_decl == NULL
        || ast_node_stable_id(ctx->current_function_decl) == 0
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
        || !array_storage_deferred_reserve((void **)&store->calls, &store->call_capacity,
            store->call_count + 1, sizeof(*store->calls)))
        return false;
    ArrayStorageReturnElementReceipt elements = {0};
    bool elements_captured = type_is_constructed_named(binding->type, "Array")
        && type_equals(type_get_constructed_arg(binding->type, 0), TYPE_STRING)
        && semantic_array_storage_capture_element_use(argument, ctx, &elements);
    store->calls[store->call_count++] = (ArrayStoragePendingCall){
        .source = {
            .binding_id = binding->decl_syntax_id,
            .call = call,
            .call_id = ast_node_stable_id(call),
            .caller_decl = ctx->current_function_decl,
            .caller_id = ast_node_stable_id(ctx->current_function_decl),
            .argument = argument,
            .argument_id = ast_node_stable_id(argument),
            .callee_decl = callee_decl,
            .callee_id = ast_node_stable_id(callee_decl),
            .binding_type = binding->type,
            .ordinal = ordinal,
            .formal_ordinal = formal,
            .formal_id = ast_func_param_stable_id(ast_func_param(callee_decl, formal)),
            .sequence = store->next_sequence++,
            .payload = payload,
            .source_id = source_id,
            .source_exclusive = binding->has_exclusive_array_storage,
            .elements_captured = elements_captured,
            .elements = elements,
        }};
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
semantic_array_storage_exact_pending_call_identity(SemanticContext *ctx,
    const Symbol *binding, const ASTNode *call, const ASTNode *callee_decl,
    size_t ordinal, size_t formal_ordinal, size_t *sequence_out)
{
    const ArrayStorageDeferredPreservationStore *store = ctx != NULL
        ? ctx->array_storage_deferred_preservations : NULL;
    const ArrayStoragePendingCallReceipt *matched = NULL;
    if (sequence_out != NULL) *sequence_out = SIZE_MAX;
    if (store == NULL || !store->active || binding == NULL
        || binding->decl_syntax_id == 0 || !binding->has_pending_inout_preservation
        || call == NULL || call->type != AST_CALL
        || callee_decl == NULL || callee_decl->type != AST_FUNC_DECL
        || ordinal >= ast_call_arg_count(call)
        || formal_ordinal >= ast_func_param_count(callee_decl))
        return false;
    const ASTNode *argument = ast_call_argument(call, ordinal);
    const FuncParam *formal = ast_func_param(callee_decl, formal_ordinal);
    for (size_t i = 0; i < store->call_count; i++) {
        const ArrayStoragePendingCallReceipt *source =
            &store->calls[i].source;
        if (source->call != call || source->binding_id != binding->decl_syntax_id)
            continue;
        if (source->payload || source->call_id == 0
            || source->call_id != ast_node_stable_id(call)
            || source->caller_decl != ctx->current_function_decl
            || source->caller_id == 0
            || source->caller_id != ast_node_stable_id(source->caller_decl)
            || source->argument != argument || source->argument_id == 0
            || source->argument_id != ast_node_stable_id(argument)
            || argument->type != AST_IDENTIFIER
            || ast_identifier_binding_syntax_id(argument)
                != binding->decl_syntax_id
            || source->callee_decl != callee_decl || source->callee_id == 0
            || source->callee_id != ast_node_stable_id(callee_decl)
            || source->ordinal != ordinal
            || source->formal_ordinal != formal_ordinal
            || formal == NULL || source->formal_id == 0
            || source->formal_id != ast_func_param_stable_id(formal)
            || !type_equals(source->binding_type, binding->type)
            || source->sequence >= store->next_sequence
            || (matched != NULL
                && matched->sequence == source->sequence))
            return false;
        /* A loop fixed-point replay records the same syntax edge again.  It
         * is not ambiguous when every identity above is byte-for-byte the
         * same: the newest row names this replay, and its sequence makes the
         * finalizer prove every earlier handoff before admitting it.  Do not
         * collapse the rows; each remains an independently checked call. */
        if (matched == NULL || matched->sequence < source->sequence)
            matched = source;
    }
    if (matched == NULL)
        return false;
    if (sequence_out != NULL) *sequence_out = matched->sequence;
    return true;
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
        !array_storage_deferred_reserve((void **)&store->drops, &store->drop_capacity,
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
    if (semantic_array_storage_has_pending_owned_string_result(
            ctx, binding_id))
        return true;
    return false;
}
