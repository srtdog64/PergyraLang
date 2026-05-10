/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot manager with secure shadow-copy support.
 */

#include "slot_manager.h"
#include "slot_manager_internal.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    size_t requestedSize;
} MemoryPool;

pthread_mutex_t *
manager_mutex(SlotManager *manager)
{
    return (pthread_mutex_t *)manager->mutex;
}

void
slot_manager_record_security_violation(SlotManager *manager, const char *event,
                                       uint32_t slotId, const char *details)
{
    if (manager != NULL && manager->mutex != NULL) {
        pthread_mutex_lock(manager_mutex(manager));
        manager->securityViolations++;
        pthread_mutex_unlock(manager_mutex(manager));
    }
    SlotManagerLogSecurityEvent(manager, event, slotId, details);
}

uint64_t
slot_now_us(void)
{
    return SecureTimestamp();
}

static const char *
slot_error_name(SlotError err)
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
    default: return "unknown";
    }
}

static void
slot_manager_warn(const char *op, const SlotHandle *handle, SlotError err)
{
    fprintf(stderr,
            "[pgy][slot] %s failed: %s (slot=%u type=%u gen=%u)\n",
            op != NULL ? op : "<op>",
            slot_error_name(err),
            handle != NULL ? handle->slotId : 0u,
            handle != NULL ? handle->typeTag : 0u,
            handle != NULL ? handle->generation : 0u);
}

uint32_t
current_thread_id(void)
{
    return (uint32_t)((uintptr_t)pthread_self() & 0xffffffffu);
}

SlotEntry *
find_slot_entry_locked(SlotManager *manager, const SlotHandle *handle)
{
    size_t i;

    if (manager == NULL || handle == NULL)
        return NULL;

    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (!entry->occupied)
            continue;
        if (entry->slotId != handle->slotId)
            continue;
        if (entry->generation != handle->generation)
            continue;
        return entry;
    }

    return NULL;
}

static SlotEntry *
find_free_entry_locked(SlotManager *manager)
{
    size_t i;

    for (i = 0; i < manager->tableSize; i++) {
        if (!manager->slotTable[i].occupied)
            return &manager->slotTable[i];
    }

    return NULL;
}

static void
slot_wipe_buffer(void *ptr, size_t size)
{
    if (ptr == NULL || size == 0)
        return;

    SecureMemoryWipe(ptr, size);
}

uint32_t
slot_checksum_bytes(const void *ptr, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)ptr;
    uint32_t checksum = 0;

    if (bytes == NULL)
        return 0;

    for (size_t i = 0; i < size; i++) {
        checksum = (checksum << 5) | (checksum >> 27);
        checksum ^= bytes[i];
        checksum += bytes[i];
    }
    return checksum;
}

void
slot_free_plain_buffer(SlotEntry *entry)
{
    if (entry->dataBlockRef != NULL) {
        slot_wipe_buffer(entry->dataBlockRef, entry->dataSize);
        free(entry->dataBlockRef);
        entry->dataBlockRef = NULL;
    }
}

static void
slot_free_buffers(SlotEntry *entry)
{
    slot_free_plain_buffer(entry);
    entry->dataSize = 0;
    SecureSealedPayloadDestroy(&entry->securePayload);
}

bool
slot_reserve_storage(SlotEntry *entry, size_t size)
{
    void *primary;

    if (size == 0) {
        slot_free_buffers(entry);
        return true;
    }

    /* 과도한 크기 요청 차단 (256MB 제한 - slot은 메모리 슬롯이므로 합리적 제한) */
    if (size > (256UL * 1024UL * 1024UL)) {
        return false;
    }

    if (entry->dataBlockRef != NULL && entry->dataSize == size) {
        return true;
    }

    primary = malloc(size);
    if (primary == NULL) {
        return false;
    }

    slot_free_plain_buffer(entry);
    entry->dataBlockRef = primary;
    entry->dataSize = size;
    return true;
}

static bool
slot_store_plain_payload(SlotEntry *entry, const void *data, size_t size)
{
    if (!slot_reserve_storage(entry, size))
        return false;

    if (size == 0) {
        return true;
    }

    memcpy(entry->dataBlockRef, data, size);
    return true;
}

bool
slot_is_expired_locked(const SlotEntry *entry)
{
    uint64_t nowUs;
    uint64_t expiryUs;

    if (entry == NULL || !entry->occupied || entry->ttl == 0)
        return false;

    nowUs = slot_now_us();
    expiryUs = entry->allocationTime + ((uint64_t)entry->ttl * 1000ULL);
    return nowUs > expiryUs;
}

static void
slot_reset_entry_locked(SlotEntry *entry)
{
    slot_free_buffers(entry);
    SecureMemoryWipe(&entry->writeToken, sizeof(entry->writeToken));
    memset(entry, 0, sizeof(*entry));
}

SlotError
slot_release_entry_locked(SlotManager *manager, SlotEntry *entry,
                          bool allowSecure)
{
    if (entry == NULL || !entry->occupied)
        return SLOT_ERROR_SLOT_NOT_FOUND;

    if (entry->securityEnabled && !allowSecure)
        return SLOT_ERROR_PERMISSION_DENIED;

    if (entry->pinCount > 0)
        return SLOT_ERROR_PINNED;

    slot_reset_entry_locked(entry);
    manager->totalDeallocations++;
    if (manager->activeSlots > 0)
        manager->activeSlots--;
    return SLOT_SUCCESS;
}

static SlotError
slot_claim_common(SlotManager *manager, TypeTag type, uint32_t scopeId,
                  SlotHandle *handle)
{
    SlotEntry *entry;
    SlotError err;

    if (manager == NULL || handle == NULL) {
        err = SLOT_ERROR_INVALID_HANDLE;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    pthread_mutex_lock(manager_mutex(manager));

    if (manager->nextSlotId == 0 || manager->nextSlotId == UINT32_MAX) {
        memset(handle, 0, sizeof(*handle));
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_OUT_OF_MEMORY;
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

    entry = find_free_entry_locked(manager);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_OUT_OF_MEMORY;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    memset(entry, 0, sizeof(*entry));
    entry->occupied = true;
    entry->slotId = manager->nextSlotId++;
    entry->generation = 1;
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

SlotManager *
SlotManagerCreate(size_t maxSlots, size_t memoryPoolSize)
{
    SlotManager *manager;
    MemoryPool *pool;
    pthread_mutex_t *mutex;

    manager = calloc(1, sizeof(*manager));
    if (manager == NULL)
        return NULL;

    manager->slotTable = calloc(maxSlots, sizeof(SlotEntry));
    if (manager->slotTable == NULL) {
        free(manager);
        return NULL;
    }

    pool = calloc(1, sizeof(*pool));
    mutex = malloc(sizeof(*mutex));
    if (pool == NULL || mutex == NULL) {
        free(pool);
        free(mutex);
        free(manager->slotTable);
        free(manager);
        return NULL;
    }

    pool->requestedSize = memoryPoolSize;
    pthread_mutex_init(mutex, NULL);

    manager->tableSize = maxSlots;
    manager->maxSlots = maxSlots;
    manager->nextSlotId = 1;
    manager->memoryPool = pool;
    manager->mutex = mutex;
    manager->defaultSecurityLevel = SECURITY_LEVEL_BASIC;
    return manager;
}

void
SlotManagerDestroy(SlotManager *manager)
{
    size_t i;

    if (manager == NULL)
        return;

    if (manager->securityContext != NULL)
        SlotManagerDisableSecurity(manager);

    if (manager->slotTable != NULL) {
        for (i = 0; i < manager->tableSize; i++) {
            if (manager->slotTable[i].occupied)
                slot_reset_entry_locked(&manager->slotTable[i]);
        }
        free(manager->slotTable);
    }

    if (manager->mutex != NULL) {
        pthread_mutex_destroy(manager_mutex(manager));
        free(manager->mutex);
    }

    free(manager->memoryPool);
    free(manager);
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
SlotRelease(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    SlotError result;

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    result = slot_release_entry_locked(manager, entry, false);
    pthread_mutex_unlock(manager_mutex(manager));
    return result;
}

SlotError
SlotClaimScoped(SlotManager *manager, TypeTag type, uint32_t scopeId,
                SlotHandle *handle)
{
    return slot_claim_common(manager, type, scopeId, handle);
}

SlotError
SlotReleaseScope(SlotManager *manager, uint32_t scopeId)
{
    size_t i;
    SlotError result = SLOT_SUCCESS;

    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (entry->occupied && entry->scopeId == scopeId) {
            SlotError releaseResult = slot_release_entry_locked(manager, entry, false);
            if (releaseResult != SLOT_SUCCESS)
                result = releaseResult;
        }
    }
    pthread_mutex_unlock(manager_mutex(manager));
    return result;
}
