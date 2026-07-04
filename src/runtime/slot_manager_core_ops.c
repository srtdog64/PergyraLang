/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Core Slot claim/read/write operations.
 */

#include "slot_manager_internal.h"
#include "pgy_runtime_security_log.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char *
SlotErrorName(SlotError err)
{
    switch (err) {
    case SLOT_SUCCESS: return "success";
    case SLOT_ERROR_OUT_OF_MEMORY: return "out-of-memory";
    case SLOT_ERROR_INVALID_HANDLE: return "invalid-handle";
    case SLOT_ERROR_TYPE_MISMATCH: return "type-mismatch";
    case SLOT_ERROR_SLOT_NOT_FOUND: return "slot-not-found";
    case SLOT_ERROR_PERMISSION_DENIED: return "permission-denied";
    case SLOT_ERROR_TTL_EXPIRED: return "ttl-expired";
    case SLOT_ERROR_THREAD_VIOLATION: return "thread-violation";
    case SLOT_ERROR_PINNED: return "pinned";
    case SLOT_ERROR_INVALID_PIN: return "invalid-pin";
    case SLOT_ERROR_ID_EXHAUSTED: return "id-exhausted";
    default: return "unknown";
    }
}

PgyRuntimeSlotStatus
SlotRuntimeStatusFromError(SlotError err)
{
    switch (err) {
    case SLOT_SUCCESS:
        return PGY_RUNTIME_SLOT_STATUS_OK;
    case SLOT_ERROR_OUT_OF_MEMORY:
        return PGY_RUNTIME_SLOT_STATUS_OUT_OF_MEMORY;
    case SLOT_ERROR_INVALID_HANDLE:
        return PGY_RUNTIME_SLOT_STATUS_INVALID_HANDLE;
    case SLOT_ERROR_TYPE_MISMATCH:
        return PGY_RUNTIME_SLOT_STATUS_TYPE_MISMATCH;
    case SLOT_ERROR_SLOT_NOT_FOUND:
        return PGY_RUNTIME_SLOT_STATUS_SLOT_NOT_FOUND;
    case SLOT_ERROR_PERMISSION_DENIED:
        return PGY_RUNTIME_SLOT_STATUS_PERMISSION_DENIED;
    case SLOT_ERROR_TTL_EXPIRED:
        return PGY_RUNTIME_SLOT_STATUS_TTL_EXPIRED;
    case SLOT_ERROR_THREAD_VIOLATION:
        return PGY_RUNTIME_SLOT_STATUS_THREAD_VIOLATION;
    case SLOT_ERROR_PINNED:
        return PGY_RUNTIME_SLOT_STATUS_PINNED;
    case SLOT_ERROR_INVALID_PIN:
        return PGY_RUNTIME_SLOT_STATUS_INVALID_PIN;
    case SLOT_ERROR_ID_EXHAUSTED:
        return PGY_RUNTIME_SLOT_STATUS_ID_EXHAUSTED;
    default:
        return PGY_RUNTIME_SLOT_STATUS_INVALID_HANDLE;
    }
}

bool
SlotErrorBoundaryRecoverable(SlotError err)
{
    return pgy_runtime_slot_status_boundary_recoverable(
        SlotRuntimeStatusFromError(err));
}

SlotFailure
SlotFailureFromError(SlotError err, const char *operation,
                     const SlotHandle *handle)
{
    SlotFailure failure;

    failure.error = err;
    failure.runtimeStatus = SlotRuntimeStatusFromError(err);
    failure.name = SlotErrorName(err);
    failure.operation = operation != NULL ? operation : "<op>";
    failure.recoverable = SlotErrorBoundaryRecoverable(err);
    failure.slotId = handle != NULL ? handle->slotId : 0u;
    failure.typeTag = handle != NULL ? handle->typeTag : 0u;
    failure.generation = handle != NULL ? handle->generation : 0u;
    return failure;
}

static void
slot_manager_warn(const char *op, const SlotHandle *handle, SlotError err)
{
    SlotFailure failure = SlotFailureFromError(err, op, handle);

    fputs("{\"component\":\"slot\",\"operation\":", stderr);
    pgy_runtime_fprint_json_string(stderr, failure.operation);
    fputs(",\"event\":\"operation_failed\",\"error\":", stderr);
    pgy_runtime_fprint_json_string(stderr, failure.name);
    fprintf(stderr,
            ",\"runtimeStatus\":%d,\"recoverable\":%s,"
            "\"slot\":%u,\"type\":%u,\"generation\":%u}\n",
            (int)failure.runtimeStatus,
            failure.recoverable ? "true" : "false",
            failure.slotId,
            failure.typeTag,
            failure.generation);
}

static SlotEntry *
find_free_entry_locked(SlotManager *manager, bool *sawGenerationExhausted)
{
    size_t i;

    if (sawGenerationExhausted != NULL)
        *sawGenerationExhausted = false;

    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (entry->occupied)
            continue;
        if (entry->slotId == 0 || entry->generation < UINT32_MAX)
            return entry;
        if (sawGenerationExhausted != NULL)
            *sawGenerationExhausted = true;
    }

    return NULL;
}

static SlotError
slot_claim_common(SlotManager *manager, TypeTag type, uint32_t scopeId,
                  SlotHandle *handle)
{
    SlotEntry *entry;
    SlotError err;
    bool sawGenerationExhausted;

    if (manager == NULL || handle == NULL) {
        err = SLOT_ERROR_INVALID_HANDLE;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    pthread_mutex_lock(manager_mutex(manager));

    entry = find_free_entry_locked(manager, &sawGenerationExhausted);
    if (entry == NULL) {
        memset(handle, 0, sizeof(*handle));
        pthread_mutex_unlock(manager_mutex(manager));
        err = sawGenerationExhausted ? SLOT_ERROR_ID_EXHAUSTED
                                     : SLOT_ERROR_OUT_OF_MEMORY;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    if (entry->slotId == 0
        && (manager->nextSlotId == 0 || manager->nextSlotId == UINT32_MAX)) {
        memset(handle, 0, sizeof(*handle));
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_ID_EXHAUSTED;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    if (manager->activeSlots >= manager->tableSize) {
        memset(handle, 0, sizeof(*handle));
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_OUT_OF_MEMORY;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    {
        uint32_t recycledSlotId = entry->slotId;
        uint32_t recycledGeneration = entry->generation;

        memset(entry, 0, sizeof(*entry));
        if (recycledSlotId != 0) {
            entry->slotId = recycledSlotId;
            entry->generation = recycledGeneration + 1u;
        } else {
            entry->slotId = manager->nextSlotId++;
            entry->generation = 1;
        }
    }
    entry->occupied = true;
    entry->typeTag = (uint32_t)type;
    entry->scopeId = scopeId;
    entry->threadAffinity = 0;
    entry->allocationTime = slot_now_us();
    entry->lastAccessTime = entry->allocationTime;
    entry->securityLevel = manager->defaultSecurityLevel;
    SecureSealedPayloadInit(&entry->securePayload);

    handle->slotId = entry->slotId;
    handle->typeTag = entry->typeTag;
    handle->generation = entry->generation;

    manager->totalAllocations++;
    manager->activeSlots++;

    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

static SlotError
slot_write_common(SlotManager *manager, const SlotHandle *handle,
                  const void *data, size_t dataSize)
{
    SlotEntry *entry;
    SlotError err;

    if (manager == NULL || handle == NULL || data == NULL) {
        err = SLOT_ERROR_INVALID_HANDLE;
        slot_manager_warn("write", handle, err);
        return err;
    }

    pthread_mutex_lock(manager_mutex(manager));

    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        manager->cacheMisses++;
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_SLOT_NOT_FOUND;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TYPE_MISMATCH;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (entry->securityEnabled) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_PERMISSION_DENIED;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (entry->pinCount > 0) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_PINNED;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (slot_is_expired_locked(entry)) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TTL_EXPIRED;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (!slot_store_plain_payload(entry, data, dataSize)) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_OUT_OF_MEMORY;
        slot_manager_warn("write", handle, err);
        return err;
    }

    entry->lastAccessTime = slot_now_us();
    entry->accessCount++;
    manager->cacheHits++;

    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

static SlotError
slot_read_common(SlotManager *manager, const SlotHandle *handle, void *buffer,
                 size_t bufferSize, size_t *bytesRead)
{
    SlotEntry *entry;
    size_t copySize;
    SlotError err;
    if (manager == NULL || handle == NULL || buffer == NULL) {
        err = SLOT_ERROR_INVALID_HANDLE;
        slot_manager_warn("read", handle, err);
        return err;
    }

    pthread_mutex_lock(manager_mutex(manager));

    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        manager->cacheMisses++;
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_SLOT_NOT_FOUND;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TYPE_MISMATCH;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (entry->securityEnabled) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_PERMISSION_DENIED;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (entry->pinCount > 0 && entry->pinMode == (uint32_t)PGY_SLOT_PIN_WRITE) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_PINNED;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (slot_is_expired_locked(entry)) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TTL_EXPIRED;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (entry->dataBlockRef == NULL || entry->dataSize == 0) {
        if (bytesRead != NULL)
            *bytesRead = 0;
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_SUCCESS;
    }

    copySize = entry->dataSize < bufferSize ? entry->dataSize : bufferSize;
    memcpy(buffer, entry->dataBlockRef, copySize);
    if (bytesRead != NULL)
        *bytesRead = copySize;

    entry->lastAccessTime = slot_now_us();
    entry->accessCount++;
    manager->cacheHits++;

    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotClaim(SlotManager *manager, TypeTag type, SlotHandle *handle)
{
    return slot_claim_common(manager, type, 0, handle);
}

SlotError
SlotWrite(SlotManager *manager, const SlotHandle *handle, const void *data,
          size_t dataSize)
{
    return slot_write_common(manager, handle, data, dataSize);
}

SlotError
SlotRead(SlotManager *manager, const SlotHandle *handle, void *buffer,
         size_t bufferSize, size_t *bytesRead)
{
    return slot_read_common(manager, handle, buffer, bufferSize, bytesRead);
}

SlotError
SlotClaimScoped(SlotManager *manager, TypeTag type, uint32_t scopeId,
                SlotHandle *handle)
{
    return slot_claim_common(manager, type, scopeId, handle);
}
