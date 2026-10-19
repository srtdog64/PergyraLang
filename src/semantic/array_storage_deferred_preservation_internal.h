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
};

#endif
