/* Lifetime and allocation of the deferred Array preservation store. */
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

bool
array_storage_deferred_reserve(void **rows, size_t *capacity, size_t needed, size_t row_size)
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

void
semantic_array_storage_deferral_destroy(SemanticContext *ctx)
{
    if (ctx == NULL || ctx->array_storage_deferred_preservations == NULL)
        return;
    free(ctx->array_storage_deferred_preservations->calls);
    free(ctx->array_storage_deferred_preservations->drops);
    free(ctx->array_storage_deferred_preservations->returns);
    free(ctx->array_storage_deferred_preservations->results);
    free(ctx->array_storage_deferred_preservations->owned_string_results);
    free(ctx->array_storage_deferred_preservations);
    ctx->array_storage_deferred_preservations = NULL;
}
