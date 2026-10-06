/* Order-independent inout descriptor preservation for public ArrayDrop.
 * See array_storage_deferred_preservation_owner.h for the contract. */
#include "array_storage_deferred_preservation_owner.h"
#include "type_checker_internal.h"
#include "slot_analyzer_internal.h"
#include "slot_summary.h"
#include "diag_codes.h"

#include <stdlib.h>

typedef struct {
    uint32_t binding_id;
    ASTNode *callee_decl;
    size_t ordinal;
    size_t sequence;
    bool preserved;
} ArrayStoragePendingCall;

typedef struct {
    ASTNode *receiver;
    uint32_t binding_id;
    size_t sequence;
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
    if (store == NULL || !store->active || binding == NULL || call == NULL
        || callee_decl == NULL || binding->decl_syntax_id == 0
        || !deferred_reserve((void **)&store->calls, &store->call_capacity,
            store->call_count + 1, sizeof(*store->calls)))
        return false;
    store->calls[store->call_count++] = (ArrayStoragePendingCall){
        binding->decl_syntax_id, callee_decl, ordinal, store->next_sequence++, false};
    binding->has_pending_inout_preservation = true;
    return true;
}

bool
semantic_array_storage_record_pending_drop(SemanticContext *ctx,
    ASTNode *receiver, const Symbol *binding)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->active || receiver == NULL || binding == NULL
        || binding->decl_syntax_id == 0
        || !deferred_reserve((void **)&store->drops, &store->drop_capacity,
            store->drop_count + 1, sizeof(*store->drops)))
        return false;
    store->drops[store->drop_count++] = (ArrayStoragePendingDrop){
        receiver, binding->decl_syntax_id, store->next_sequence++};
    return true;
}

size_t
semantic_array_storage_pending_sequence(const SemanticContext *ctx)
{
    const ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    return store != NULL ? store->next_sequence : 0;
}

bool
semantic_array_storage_pending_preserved(const SemanticContext *ctx,
    uint32_t binding_id, size_t sequence)
{
    const ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->finalized)
        return false;
    for (size_t i = 0; i < store->call_count; i++) {
        const ArrayStoragePendingCall *pending = &store->calls[i];
        if (pending->binding_id == binding_id && pending->sequence < sequence
            && !pending->preserved)
            return false;
    }
    return true;
}

bool
semantic_array_storage_deferral_finalize(SemanticContext *ctx)
{
    ArrayStorageDeferredPreservationStore *store =
        ctx != NULL ? ctx->array_storage_deferred_preservations : NULL;
    if (store == NULL || !store->active)
        return false;
    store->active = false;
    /* Summaries taken during Pass 2 may have read bodies that were not yet
     * checked. Decide over a fresh store and keep the Pass 2 store, whose
     * masks other owners snapshot, unchanged. */
    FunctionParamFlowSummaryStore *pass2_store = ctx->function_param_flow_summaries;
    ctx->function_param_flow_summaries = NULL;
    for (size_t i = 0; i < store->call_count; i++) {
        ArrayStoragePendingCall *pending = &store->calls[i];
        pending->preserved = function_param_flow_preserves_array_storage(
            ctx, pending->callee_decl, pending->ordinal);
    }
    function_param_flow_summary_store_destroy(ctx);
    ctx->function_param_flow_summaries = pass2_store;
    store->finalized = true;
    for (size_t i = 0; i < store->drop_count; i++) {
        const ArrayStoragePendingDrop *drop = &store->drops[i];
        if (semantic_array_storage_pending_preserved(ctx, drop->binding_id, drop->sequence))
            continue;
        semantic_error_with_hints(ctx, PGY_CODE_SEM_BORROW_ESCAPE,
            PGY_CAUSE_BORROW_ESCAPE, PGY_FIX_BIND_TO_NAMED_VARIABLE_BEFORE_MOVE,
            drop->receiver, "ArrayDrop requires one named, exclusive owned Array<T> binding; borrowed or aliased storage cannot be released");
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
