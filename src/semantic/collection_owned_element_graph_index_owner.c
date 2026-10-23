/* Indexes the sealed definition and call-edge facts used by aggregate field
 * ownership validation.  Their original stores remain authoritative; this
 * generation-local view removes repeated whole-program scans. */
#include "collection_owned_element_requirement_internal.h"

#include "function_param_publication_internal.h"
#include "../parser/ast_api.h"

#include <stdlib.h>
#include <string.h>

static size_t
aggregate_graph_bucket_slot(size_t capacity,
                            uint32_t function_id,
                            uint32_t second_id)
{
    uint64_t mixed = (uint64_t)function_id * UINT64_C(11400714819323198485)
        ^ (uint64_t)second_id * UINT64_C(14029467366897019727);
    return (size_t)mixed & (capacity - 1u);
}

static bool
aggregate_graph_bucket_capacity(size_t count, size_t *capacity_out)
{
    size_t capacity = 8;
    if (capacity_out == NULL || count > SIZE_MAX / 2u)
        return false;
    while (capacity < count * 2u) {
        if (capacity > SIZE_MAX / 2u)
            return false;
        capacity *= 2u;
    }
    *capacity_out = capacity;
    return true;
}

static size_t
aggregate_proof_memo_hash(size_t capacity,
                          OwnedElementAggregateProofKind kind,
                          uint32_t function_id, uint32_t value_id,
                          uint32_t call_context_id,
                          const uint32_t *field_ids, size_t field_count)
{
    uint64_t mixed = (uint64_t)(kind + 1u) * UINT64_C(11400714819323198485)
        ^ (uint64_t)function_id * UINT64_C(14029467366897019727)
        ^ (uint64_t)value_id * UINT64_C(1609587929392839161)
        ^ (uint64_t)call_context_id * UINT64_C(9650029242287828579);
    for (size_t i = 0; i < field_count; i++)
        mixed = (mixed ^ field_ids[i]) * UINT64_C(1099511628211);
    return (size_t)mixed & (capacity - 1u);
}

static bool
aggregate_proof_memo_same(
    const OwnedElementAggregateProofMemo *row,
    OwnedElementAggregateProofKind kind,
    uint32_t function_id, uint32_t value_id, uint32_t call_context_id,
    const uint32_t *field_ids, size_t field_count)
{
    return row->kind == kind && row->function_id == function_id
        && row->value_id == value_id
        && row->call_context_id == call_context_id
        && row->field_count == field_count
        && memcmp(row->field_ids, field_ids,
            field_count * sizeof(*field_ids)) == 0;
}

static bool
aggregate_proof_memo_buckets_resize(
    CollectionOwnedElementRequirementStore *store, size_t capacity)
{
    size_t *buckets = calloc(capacity, sizeof(*buckets));
    if (buckets == NULL)
        return false;
    for (size_t i = 0; i < store->aggregate_proof_memo_count; i++) {
        OwnedElementAggregateProofMemo *row =
            &store->aggregate_proof_memos[i];
        size_t slot = aggregate_proof_memo_hash(capacity,
            row->kind, row->function_id, row->value_id,
            row->call_context_id, row->field_ids, row->field_count);
        row->hash_next_plus_one = buckets[slot];
        buckets[slot] = i + 1u;
    }
    free(store->aggregate_proof_memo_buckets);
    store->aggregate_proof_memo_buckets = buckets;
    store->aggregate_proof_memo_bucket_capacity = capacity;
    return true;
}

static bool
aggregate_proof_memo_reserve(
    CollectionOwnedElementRequirementStore *store, size_t needed)
{
    OwnedElementAggregateProofMemo *grown =
        collection_owned_requirement_rows_reserve(
            store->aggregate_proof_memos,
            &store->aggregate_proof_memo_capacity,
            needed, sizeof(*store->aggregate_proof_memos));
    if (grown == NULL)
        return false;
    store->aggregate_proof_memos = grown;
    size_t buckets = store->aggregate_proof_memo_bucket_capacity;
    if (buckets == 0)
        buckets = 64;
    while (needed > buckets / 2u) {
        if (buckets > SIZE_MAX / 2u)
            return false;
        buckets *= 2u;
    }
    return buckets == store->aggregate_proof_memo_bucket_capacity
        || aggregate_proof_memo_buckets_resize(store, buckets);
}

OwnedElementAggregateMemoLookup
collection_owned_proof_memo_begin(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    ASTNode *site, OwnedElementAggregateProofKind kind,
    uint32_t function_id, uint32_t value_id, uint32_t call_context_id,
    const uint32_t *field_ids, size_t field_count,
    size_t *memo_index_out)
{
    if (store == NULL || function_id == 0 || value_id == 0
        || field_ids == NULL || field_count == 0
        || field_count > PGY_COLLECTION_MEMBER_PATH_LIMIT
        || memo_index_out == NULL) {
        collection_owned_requirement_failure(ctx, site,
            "aggregate proof memo identity is incomplete");
        return OWNED_ELEMENT_AGGREGATE_MEMO_ERROR;
    }
    size_t capacity = store->aggregate_proof_memo_bucket_capacity;
    if (capacity != 0) {
        size_t slot = aggregate_proof_memo_hash(capacity, kind,
            function_id, value_id, call_context_id,
            field_ids, field_count);
        for (size_t link = store->aggregate_proof_memo_buckets[slot];
             link != 0;) {
            const OwnedElementAggregateProofMemo *row =
                &store->aggregate_proof_memos[link - 1u];
            if (aggregate_proof_memo_same(row, kind, function_id,
                    value_id, call_context_id, field_ids, field_count)) {
                *memo_index_out = link - 1u;
                if (row->state == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_PROVED)
                    return OWNED_ELEMENT_AGGREGATE_MEMO_PROVED;
                if (row->state == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED)
                    return OWNED_ELEMENT_AGGREGATE_MEMO_REFUTED;
                if (row->state
                    == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_UNCACHED) {
                    store->aggregate_proof_memos[link - 1u].state =
                        OWNED_ELEMENT_AGGREGATE_REQUIREMENT_VISITING;
                    return OWNED_ELEMENT_AGGREGATE_MEMO_NEW;
                }
                return OWNED_ELEMENT_AGGREGATE_MEMO_VISITING;
            }
            link = row->hash_next_plus_one;
        }
    }
    if (store->aggregate_proof_memo_count == SIZE_MAX
        || !aggregate_proof_memo_reserve(store,
            store->aggregate_proof_memo_count + 1u)) {
        collection_owned_requirement_failure(ctx, site,
            "aggregate proof memo allocation failed");
        return OWNED_ELEMENT_AGGREGATE_MEMO_ERROR;
    }
    capacity = store->aggregate_proof_memo_bucket_capacity;
    size_t slot = aggregate_proof_memo_hash(capacity, kind,
        function_id, value_id, call_context_id, field_ids, field_count);
    size_t at = store->aggregate_proof_memo_count++;
    store->aggregate_proof_memos[at] = (OwnedElementAggregateProofMemo){
        .kind = kind,
        .function_id = function_id,
        .value_id = value_id,
        .call_context_id = call_context_id,
        .field_count = field_count,
        .state = OWNED_ELEMENT_AGGREGATE_REQUIREMENT_VISITING,
        .hash_next_plus_one = store->aggregate_proof_memo_buckets[slot]};
    memcpy(store->aggregate_proof_memos[at].field_ids,
        field_ids, field_count * sizeof(*field_ids));
    store->aggregate_proof_memo_buckets[slot] = at + 1u;
    *memo_index_out = at;
    return OWNED_ELEMENT_AGGREGATE_MEMO_NEW;
}

bool
collection_owned_proof_memo_resolve(
    CollectionOwnedElementRequirementStore *store,
    size_t memo_index, bool proved, bool cache_proved)
{
    if (store == NULL || memo_index >= store->aggregate_proof_memo_count)
        return false;
    OwnedElementAggregateProofMemo *row =
        &store->aggregate_proof_memos[memo_index];
    if (row->state == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_PROVED) {
        if (!proved)
            row->state = OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED;
        return proved;
    }
    if (row->state == OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED)
        return false;
    if (row->state != OWNED_ELEMENT_AGGREGATE_REQUIREMENT_VISITING)
        return false;
    if (row->cycle_root_plus_one != 0) {
        size_t root = row->cycle_root_plus_one - 1u;
        if (root >= store->aggregate_proof_memo_count)
            return false;
        if (memo_index != root) {
            if (!proved)
                row->state = OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED;
            return proved;
        }
        for (size_t i = 0; i < store->aggregate_proof_memo_count; i++) {
            OwnedElementAggregateProofMemo *member =
                &store->aggregate_proof_memos[i];
            if (member->cycle_root_plus_one != root + 1u)
                continue;
            member->state = proved
                ? OWNED_ELEMENT_AGGREGATE_REQUIREMENT_PROVED
                : OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED;
            member->cycle_root_plus_one = 0;
        }
        return proved;
    }
    row->state = !proved
        ? OWNED_ELEMENT_AGGREGATE_REQUIREMENT_REFUTED
        : cache_proved
            ? OWNED_ELEMENT_AGGREGATE_REQUIREMENT_PROVED
            : OWNED_ELEMENT_AGGREGATE_REQUIREMENT_UNCACHED;
    return proved;
}

static bool
aggregate_assignment_publication_current(
    SemanticContext *ctx, const PublishedNode *row)
{
    return row != NULL && row->assignment_replacement
        && row->node != NULL && row->node->type == AST_ASSIGNMENT
        && row->caller != NULL && row->caller->type == AST_FUNC_DECL
        && row->caller_id != 0
        && ast_node_stable_id(row->caller) == row->caller_id
        && row->target_binding != 0
        && semantic_param_publication_assignment_replacement(ctx, row->node);
}

static bool
aggregate_assignment_index_build(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store,
    FunctionParamPublicationStore *publication)
{
    size_t count = 0;
    size_t capacity;
    for (size_t i = 0; i < publication->node_capacity; i++) {
        const PublishedNode *row = &publication->nodes[i];
        if (!row->assignment_replacement)
            continue;
        if (!aggregate_assignment_publication_current(ctx, row))
            return collection_owned_requirement_failure(ctx,
                (ASTNode *)row->node,
                "aggregate assignment publication changed before finalization");
        if (count == SIZE_MAX)
            return collection_owned_requirement_failure(ctx,
                (ASTNode *)row->node,
                "aggregate assignment index bound overflow");
        count++;
    }
    if (!aggregate_graph_bucket_capacity(count, &capacity)
        || count > SIZE_MAX / sizeof(*store->aggregate_assignment_rows)
        || capacity > SIZE_MAX
            / sizeof(*store->aggregate_assignment_buckets))
        return collection_owned_requirement_failure(ctx, store->program,
            "aggregate assignment index allocation bound overflow");
    store->aggregate_assignment_rows = count != 0
        ? calloc(count, sizeof(*store->aggregate_assignment_rows)) : NULL;
    store->aggregate_assignment_buckets = calloc(capacity,
        sizeof(*store->aggregate_assignment_buckets));
    if ((count != 0 && store->aggregate_assignment_rows == NULL)
        || store->aggregate_assignment_buckets == NULL)
        return collection_owned_requirement_failure(ctx, store->program,
            "aggregate assignment index allocation failed");
    store->aggregate_assignment_bucket_capacity = capacity;
    for (size_t i = 0; i < publication->node_capacity; i++) {
        const PublishedNode *row = &publication->nodes[i];
        if (!row->assignment_replacement)
            continue;
        size_t slot = aggregate_graph_bucket_slot(capacity,
            row->caller_id, row->target_binding);
        while (store->aggregate_assignment_buckets[slot].function_id != 0
            && (store->aggregate_assignment_buckets[slot].function_id
                    != row->caller_id
                || store->aggregate_assignment_buckets[slot].binding_id
                    != row->target_binding))
            slot = (slot + 1u) & (capacity - 1u);
        OwnedElementAggregateAssignmentIndexBucket *bucket =
            &store->aggregate_assignment_buckets[slot];
        if (bucket->function_id == 0) {
            bucket->function_id = row->caller_id;
            bucket->binding_id = row->target_binding;
        }
        size_t at = store->aggregate_assignment_count++;
        store->aggregate_assignment_rows[at] =
            (OwnedElementAggregateAssignmentIndexRow){
                .publication = row,
                .next_plus_one = bucket->head_plus_one};
        bucket->head_plus_one = at + 1u;
    }
    return true;
}

static bool
aggregate_return_index_build(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store)
{
    size_t count = store->aggregate_return_count;
    size_t capacity;
    if (!aggregate_graph_bucket_capacity(count, &capacity)
        || count > SIZE_MAX / sizeof(*store->aggregate_return_next_plus_one)
        || capacity > SIZE_MAX / sizeof(*store->aggregate_return_buckets))
        return collection_owned_requirement_failure(ctx, store->program,
            "aggregate return index allocation bound overflow");
    store->aggregate_return_next_plus_one = count != 0
        ? calloc(count, sizeof(*store->aggregate_return_next_plus_one)) : NULL;
    store->aggregate_return_buckets = calloc(capacity,
        sizeof(*store->aggregate_return_buckets));
    if ((count != 0 && store->aggregate_return_next_plus_one == NULL)
        || store->aggregate_return_buckets == NULL)
        return collection_owned_requirement_failure(ctx, store->program,
            "aggregate return index allocation failed");
    store->aggregate_return_bucket_capacity = capacity;
    for (size_t i = 0; i < count; i++) {
        const OwnedElementAggregateReturn *row = &store->aggregate_returns[i];
        if (row->function == NULL || row->function->type != AST_FUNC_DECL
            || row->function_id == 0
            || ast_node_stable_id(row->function) != row->function_id)
            return collection_owned_requirement_failure(ctx, row->statement,
                "aggregate return identity changed before indexing");
        size_t slot = aggregate_graph_bucket_slot(
            capacity, row->function_id, 0);
        while (store->aggregate_return_buckets[slot].function_id != 0
            && store->aggregate_return_buckets[slot].function_id
                != row->function_id)
            slot = (slot + 1u) & (capacity - 1u);
        OwnedElementAggregateReturnIndexBucket *bucket =
            &store->aggregate_return_buckets[slot];
        if (bucket->function_id == 0)
            bucket->function_id = row->function_id;
        store->aggregate_return_next_plus_one[i] = bucket->head_plus_one;
        bucket->head_plus_one = i + 1u;
    }
    return true;
}

static bool
aggregate_actual_index_build(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store)
{
    size_t count = store->aggregate_actual_count;
    size_t capacity;
    if (!aggregate_graph_bucket_capacity(count, &capacity)
        || count > SIZE_MAX / sizeof(*store->aggregate_actual_next_plus_one)
        || capacity > SIZE_MAX / sizeof(*store->aggregate_actual_buckets))
        return collection_owned_requirement_failure(ctx, store->program,
            "aggregate actual index allocation bound overflow");
    store->aggregate_actual_next_plus_one = count != 0
        ? calloc(count, sizeof(*store->aggregate_actual_next_plus_one)) : NULL;
    store->aggregate_actual_buckets = calloc(capacity,
        sizeof(*store->aggregate_actual_buckets));
    if ((count != 0 && store->aggregate_actual_next_plus_one == NULL)
        || store->aggregate_actual_buckets == NULL)
        return collection_owned_requirement_failure(ctx, store->program,
            "aggregate actual index allocation failed");
    store->aggregate_actual_bucket_capacity = capacity;
    for (size_t i = 0; i < count; i++) {
        const OwnedElementAggregateActual *row = &store->aggregate_actuals[i];
        if (row->callee_id == 0 || row->formal_id == 0)
            return collection_owned_requirement_failure(ctx, row->call,
                "aggregate actual identity changed before indexing");
        size_t slot = aggregate_graph_bucket_slot(
            capacity, row->callee_id, row->formal_id);
        while (store->aggregate_actual_buckets[slot].function_id != 0
            && (store->aggregate_actual_buckets[slot].function_id
                    != row->callee_id
                || store->aggregate_actual_buckets[slot].formal_id
                    != row->formal_id))
            slot = (slot + 1u) & (capacity - 1u);
        OwnedElementAggregateActualIndexBucket *bucket =
            &store->aggregate_actual_buckets[slot];
        if (bucket->function_id == 0) {
            bucket->function_id = row->callee_id;
            bucket->formal_id = row->formal_id;
        }
        store->aggregate_actual_next_plus_one[i] = bucket->head_plus_one;
        bucket->head_plus_one = i + 1u;
    }
    return true;
}

bool
collection_owned_element_graph_index_build(
    SemanticContext *ctx, CollectionOwnedElementRequirementStore *store)
{
    FunctionParamPublicationStore *publication = ctx != NULL
        ? ctx->function_param_publications : NULL;
    if (store == NULL || publication == NULL
        || store->program != ctx->program_root)
        return collection_owned_requirement_failure(ctx,
            store != NULL ? store->program : NULL,
            "missing publication owner for aggregate definition index");
    if (store->aggregate_graph_index_ready)
        return true;
    if (!aggregate_assignment_index_build(ctx, store, publication)
        || !aggregate_return_index_build(ctx, store)
        || !aggregate_actual_index_build(ctx, store))
        return false;
    store->aggregate_graph_index_ready = true;
    return true;
}

const OwnedElementAggregateAssignmentIndexBucket *
collection_owned_assignment_index_find(
    const CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t binding_id)
{
    if (store == NULL || !store->aggregate_graph_index_ready
        || store->aggregate_assignment_bucket_capacity == 0
        || function_id == 0 || binding_id == 0)
        return NULL;
    size_t capacity = store->aggregate_assignment_bucket_capacity;
    size_t slot = aggregate_graph_bucket_slot(
        capacity, function_id, binding_id);
    while (store->aggregate_assignment_buckets[slot].function_id != 0) {
        const OwnedElementAggregateAssignmentIndexBucket *bucket =
            &store->aggregate_assignment_buckets[slot];
        if (bucket->function_id == function_id
            && bucket->binding_id == binding_id)
            return bucket;
        slot = (slot + 1u) & (capacity - 1u);
    }
    return NULL;
}

const OwnedElementAggregateReturnIndexBucket *
collection_owned_return_index_find(
    const CollectionOwnedElementRequirementStore *store,
    uint32_t function_id)
{
    if (store == NULL || !store->aggregate_graph_index_ready
        || store->aggregate_return_bucket_capacity == 0
        || function_id == 0)
        return NULL;
    size_t capacity = store->aggregate_return_bucket_capacity;
    size_t slot = aggregate_graph_bucket_slot(capacity, function_id, 0);
    while (store->aggregate_return_buckets[slot].function_id != 0) {
        const OwnedElementAggregateReturnIndexBucket *bucket =
            &store->aggregate_return_buckets[slot];
        if (bucket->function_id == function_id)
            return bucket;
        slot = (slot + 1u) & (capacity - 1u);
    }
    return NULL;
}

const OwnedElementAggregateActualIndexBucket *
collection_owned_actual_index_find(
    const CollectionOwnedElementRequirementStore *store,
    uint32_t function_id, uint32_t formal_id)
{
    if (store == NULL || !store->aggregate_graph_index_ready
        || store->aggregate_actual_bucket_capacity == 0
        || function_id == 0 || formal_id == 0)
        return NULL;
    size_t capacity = store->aggregate_actual_bucket_capacity;
    size_t slot = aggregate_graph_bucket_slot(
        capacity, function_id, formal_id);
    while (store->aggregate_actual_buckets[slot].function_id != 0) {
        const OwnedElementAggregateActualIndexBucket *bucket =
            &store->aggregate_actual_buckets[slot];
        if (bucket->function_id == function_id
            && bucket->formal_id == formal_id)
            return bucket;
        slot = (slot + 1u) & (capacity - 1u);
    }
    return NULL;
}
