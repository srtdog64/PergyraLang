#ifndef PERGYRA_ARRAY_STORAGE_DEFERRED_PRESERVATION_INTERNAL_H
#define PERGYRA_ARRAY_STORAGE_DEFERRED_PRESERVATION_INTERNAL_H

#include "array_storage_deferred_preservation_owner.h"

typedef struct {
    ArrayStoragePendingCallReceipt source;
    bool preserved;
    bool elements_preserved;
    /* Nonzero for a call made by a return value: nothing sequenced from
     * here on runs after it, since the routine has returned. */
    size_t terminal_end;
} ArrayStoragePendingCall;

typedef struct {
    ASTNode *receiver;
    uint32_t receiver_id;
    uint32_t binding_id;
    size_t sequence;
    ArrayStorageReleaseOperation operation;
    ArrayStorageReturnElementReceipt elements;
    size_t call_index;
} ArrayStoragePendingDrop;

typedef ArrayStorageOwnedReturnReceipt ArrayStorageOwnedReturn;

typedef struct {
    ArrayStorageResultBirthReceipt birth;
    bool proved, checking;
} ArrayStoragePendingResult;

typedef struct {
    ASTNode *push_call, *receiver, *result_call, *function_decl;
    uint32_t push_call_id, receiver_id, result_call_id;
    uint32_t binding_id, producer_id, function_id, formal_id;
    size_t formal_index;
    ParamMode formal_mode;
    size_t sequence;
    bool parameter_receiver, parameter_requirement_recorded;
} ArrayStoragePendingOwnedStringResult;

/* Mutable ordering remains private to this physical array-return owner. */
struct ArrayStorageDeferredPreservationStore {
    bool active, finalized, sealing, consuming_checked_calls;
    size_t next_sequence;
    ArrayStoragePendingCall *calls;
    size_t call_count, call_capacity;
    ArrayStoragePendingDrop *drops;
    size_t drop_count, drop_capacity;
    ArrayStorageOwnedReturn *returns;
    size_t return_count, return_capacity;
    ArrayStoragePendingResult *results;
    size_t result_count, result_capacity;
    ArrayStoragePendingOwnedStringResult *owned_string_results;
    size_t owned_string_result_count, owned_string_result_capacity;
};

bool array_storage_deferred_reserve(void **rows, size_t *capacity,
    size_t needed, size_t row_size);
bool array_storage_pending_owned_string_result_current(SemanticContext *ctx,
    ArrayStoragePendingOwnedStringResult *row, bool record_requirement);
bool array_storage_pending_result_proof(SemanticContext *ctx,
    ArrayStoragePendingResult *row);
bool array_storage_pending_drop_element_source_current(SemanticContext *ctx,
    struct ArrayStorageDeferredPreservationStore *store,
    const ArrayStoragePendingDrop *drop);
bool array_storage_same_call_arguments_copied(SemanticContext *ctx,
    const struct ArrayStorageDeferredPreservationStore *store,
    const ArrayStoragePendingDrop *drop);

#endif
