/* Order-independent descriptor preservation for public array release.
 * See array_storage_deferred_preservation_owner.h for the contract. */
#include "array_storage_deferred_preservation_owner.h"
#include "type_checker_internal.h"
#include "slot_analyzer_internal.h"
#include "slot_summary.h"
#include "diag_codes.h"
#include "function_param_publication_owner.h"

#include <stdlib.h>

typedef struct {
    uint32_t binding_id;
    ASTNode *call;
    uint32_t call_id;
    ASTNode *argument;
    uint32_t argument_id;
    ASTNode *callee_decl;
    uint32_t callee_id;
    const Type *binding_type;
    size_t ordinal;
    size_t sequence;
    bool preserved;
} ArrayStoragePendingCall;

typedef struct {
    ASTNode *receiver;
    uint32_t receiver_id;
    uint32_t binding_id;
    size_t sequence;
    ArrayStorageReleaseOperation operation;
} ArrayStoragePendingDrop;

struct ArrayStorageDeferredPreservationStore {
    bool active;
    bool finalized;
    size_t next_sequence;
    ArrayStoragePendingCall *calls;
    size_t call_count;
    size_t call_capacity;
    ArrayStoragePendingDrop *drops;
    size_t drop_count;
    size_t drop_capacity;
};

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

bool
semantic_array_storage_record_pending_call(SemanticContext *ctx,
    Symbol *binding, ASTNode *call, ASTNode *callee_decl, size_t ordinal)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    ASTNode *argument = call != NULL && call->type == AST_CALL
        && ordinal < ast_call_arg_count(call) ? ast_call_argument(call, ordinal) : NULL;
    if (store == NULL || !store->active || binding == NULL || call == NULL
        || callee_decl == NULL || binding->decl_syntax_id == 0
        || binding->type == NULL || ast_node_stable_id(call) == 0
        || ast_node_stable_id(callee_decl) == 0 || argument == NULL
        || argument->type != AST_IDENTIFIER || ast_node_stable_id(argument) == 0
        || ast_identifier_binding_syntax_id(argument) != binding->decl_syntax_id
        || ast_call_semantic_callee_decl_id(call) != ast_node_stable_id(callee_decl)
        || ast_call_semantic_callee_value_binding_id(call) != 0
        || !deferred_reserve((void **)&store->calls, &store->call_capacity,
            store->call_count + 1, sizeof(*store->calls)))
        return false;
    store->calls[store->call_count++] = (ArrayStoragePendingCall){
        binding->decl_syntax_id, call, ast_node_stable_id(call), argument,
        ast_node_stable_id(argument), callee_decl, ast_node_stable_id(callee_decl),
        binding->type, ordinal, store->next_sequence++, false};
    binding->has_pending_inout_preservation = true;
    return true;
}

bool
semantic_array_storage_record_pending_drop(SemanticContext *ctx,
    ASTNode *receiver, const Symbol *binding, ArrayStorageReleaseOperation operation)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->active || receiver == NULL || binding == NULL
        || receiver->type != AST_IDENTIFIER || ast_node_stable_id(receiver) == 0
        || binding->decl_syntax_id == 0
        || ast_identifier_binding_syntax_id(receiver) != binding->decl_syntax_id
        || (operation != ARRAY_STORAGE_RELEASE_DROP_STORAGE
            && operation != ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
            && operation != ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS)
        || !deferred_reserve((void **)&store->drops, &store->drop_capacity,
            store->drop_count + 1, sizeof(*store->drops)))
        return false;
    store->drops[store->drop_count++] = (ArrayStoragePendingDrop){
        receiver, ast_node_stable_id(receiver), binding->decl_syntax_id,
        store->next_sequence++, operation};
    return true;
}

static bool
pending_call_current(SemanticContext *ctx, const ArrayStoragePendingCall *pending)
{
    if (ctx == NULL || pending == NULL || pending->call == NULL
        || pending->call->type != AST_CALL || pending->argument == NULL
        || pending->callee_decl == NULL || pending->callee_id == 0
        || pending->call_id != ast_node_stable_id(pending->call)
        || pending->callee_id != ast_node_stable_id(pending->callee_decl)
        || pending->ordinal >= ast_call_arg_count(pending->call)
        || pending->argument != ast_call_argument(pending->call, pending->ordinal)
        || pending->argument_id != ast_node_stable_id(pending->argument)
        || pending->argument->type != AST_IDENTIFIER
        || ast_identifier_binding_syntax_id(pending->argument) != pending->binding_id
        || !ast_call_semantic_callee_declared_callable(pending->call)
        || ast_call_semantic_callee_decl_id(pending->call) != pending->callee_id
        || ast_call_semantic_callee_value_binding_id(pending->call) != 0
        || !semantic_param_publication_checked_argument(ctx, pending->call,
            pending->ordinal)
        || !semantic_param_publication_routine_inputs_current(ctx,
            pending->callee_decl, true))
        return false;
    SemanticPublishedValue value = semantic_param_publication_value(ctx, pending->argument);
    return value.ok && value.type != NULL
        && type_equals(value.type, pending->binding_type);
}

size_t
semantic_array_storage_pending_sequence(const SemanticContext *ctx)
{
    const ArrayStorageDeferredPreservationStore *store =
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
        if (pending->binding_id == binding_id && pending->sequence < sequence
            && (!pending->preserved || !pending_call_current(ctx, pending)
                || !function_param_flow_preserves_array_storage(
                    ctx, pending->callee_decl, pending->ordinal)))
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
        pending->preserved = pending_call_current(ctx, pending)
            && function_param_flow_preserves_array_storage(
                ctx, pending->callee_decl, pending->ordinal);
    }
    store->finalized = true;
    for (size_t i = 0; i < store->drop_count; i++) {
        const ArrayStoragePendingDrop *drop = &store->drops[i];
        if (drop->receiver != NULL && drop->receiver->type == AST_IDENTIFIER
            && ast_node_stable_id(drop->receiver) == drop->receiver_id
            && ast_identifier_binding_syntax_id(drop->receiver) == drop->binding_id
            && semantic_array_storage_pending_preserved(ctx, drop->binding_id, drop->sequence))
            continue;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            drop->receiver, "%s requires preserved array storage; a prior call may retain or rebind the descriptor",
            drop->operation == ARRAY_STORAGE_RELEASE_DROP_OWNED_STRINGS
                ? "ArrayDropOwnedStrings"
                : drop->operation == ARRAY_STORAGE_RELEASE_MOVE_OWNED_STRINGS
                    ? "Array<String> move" : "ArrayDrop");
    }
    return true;
}

void
semantic_array_storage_deferral_destroy(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->array_storage_deferred_preservations == NULL)
        return;
    free(ctx->array_storage_deferred_preservations->calls);
    free(ctx->array_storage_deferred_preservations->drops);
    free(ctx->array_storage_deferred_preservations);
    ctx->array_storage_deferred_preservations = NULL;
}
