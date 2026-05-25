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

uintptr_t
current_thread_id(void)
{
    return (uintptr_t)pthread_self();
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

SlotManager *
SlotManagerCreate(size_t maxSlots, size_t memoryPoolSize)
{
    SlotManager *manager;
    MemoryPool *pool;
    pthread_mutex_t *mutex;

    if (maxSlots == 0)
        return NULL;
    if (maxSlots > SIZE_MAX / sizeof(SlotEntry))
        return NULL;

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
    if (pthread_mutex_init(mutex, NULL) != 0) {
        free(pool);
        free(mutex);
        free(manager->slotTable);
        free(manager);
        return NULL;
    }

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
