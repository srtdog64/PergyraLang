/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot manager query, TTL cleanup, locking, stats, and fast wrapper surface.
 */

#include "slot_manager.h"
#include "slot_manager_internal.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

bool
SlotValidateType(SlotManager *manager, const SlotHandle *handle,
                 TypeTag expectedType)
{
    bool valid = false;
    SlotEntry *entry;

    if (manager == NULL || handle == NULL)
        return false;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry != NULL && entry->typeTag == (uint32_t)expectedType)
        valid = true;
    pthread_mutex_unlock(manager_mutex(manager));
    return valid;
}

bool
SlotIsValid(SlotManager *manager, const SlotHandle *handle)
{
    bool valid = false;
    SlotEntry *entry;

    if (manager == NULL || handle == NULL)
        return false;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry != NULL && !slot_is_expired_locked(entry))
        valid = true;
    pthread_mutex_unlock(manager_mutex(manager));
    return valid;
}

SlotError
SlotSetTtl(SlotManager *manager, const SlotHandle *handle, uint32_t ttlMs)
{
    SlotEntry *entry;

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    entry->ttl = ttlMs;
    entry->allocationTime = slot_now_us();
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotRefreshTtl(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    entry->allocationTime = slot_now_us();
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

void
SlotCleanupExpired(SlotManager *manager)
{
    size_t i;

    if (manager == NULL)
        return;

    pthread_mutex_lock(manager_mutex(manager));
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (entry->occupied && entry->pinCount == 0 && slot_is_expired_locked(entry))
            slot_release_entry_locked(manager, entry, true);
    }
    pthread_mutex_unlock(manager_mutex(manager));
}

SlotError
SlotLock(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    uint32_t tid = current_thread_id();

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    if (entry->threadAffinity != 0 && entry->threadAffinity != tid) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_THREAD_VIOLATION;
    }

    entry->threadAffinity = tid;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotUnlock(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    uint32_t tid = current_thread_id();

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    if (entry->threadAffinity != 0 && entry->threadAffinity != tid) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_THREAD_VIOLATION;
    }

    entry->threadAffinity = 0;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotTryLock(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    uint32_t tid = current_thread_id();

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (pthread_mutex_trylock(manager_mutex(manager)) == EBUSY)
        return SLOT_ERROR_THREAD_VIOLATION;

    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    if (entry->threadAffinity != 0 && entry->threadAffinity != tid) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_THREAD_VIOLATION;
    }

    entry->threadAffinity = tid;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

void
SlotManagerPrintStats(const SlotManager *manager)
{
    if (manager == NULL)
        return;

    printf("=== Pergyra Slot Manager Statistics ===\n");
    printf("Total allocations: %llu\n",
           (unsigned long long)manager->totalAllocations);
    printf("Total deallocations: %llu\n",
           (unsigned long long)manager->totalDeallocations);
    printf("Active slots: %llu\n",
           (unsigned long long)manager->activeSlots);
    printf("Cache hits: %llu\n",
           (unsigned long long)manager->cacheHits);
    printf("Cache misses: %llu\n",
           (unsigned long long)manager->cacheMisses);
    printf("Table size: %zu\n", manager->tableSize);
    printf("Utilization: %.2f%%\n", SlotManagerGetUtilization(manager) * 100.0);
}

size_t
SlotManagerGetActiveCount(const SlotManager *manager)
{
    return manager != NULL ? (size_t)manager->activeSlots : 0;
}

double
SlotManagerGetUtilization(const SlotManager *manager)
{
    if (manager == NULL || manager->tableSize == 0)
        return 0.0;

    return (double)manager->activeSlots / (double)manager->tableSize;
}

SlotError
SlotClaimFast(SlotManager *manager, TypeTag type, SlotHandle *handle)
{
    return SlotClaim(manager, type, handle);
}

SlotError
SlotWriteFast(SlotManager *manager, const SlotHandle *handle, const void *data,
              size_t dataSize)
{
    return SlotWrite(manager, handle, data, dataSize);
}

SlotError
SlotReadFast(SlotManager *manager, const SlotHandle *handle, void *buffer,
             size_t bufferSize)
{
    return SlotRead(manager, handle, buffer, bufferSize, NULL);
}
