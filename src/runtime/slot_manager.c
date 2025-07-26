/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Pergyra Language Project nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "slot_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

/*
 * External assembly function declarations
 */
extern uint32_t SlotHashFunctionAsm(uint32_t slotId);
extern bool     SlotCompareAndSwapAsm(volatile uint32_t *ptr, 
                                      uint32_t expected, uint32_t newVal);
extern void     SlotMemoryBarrierAsm(void);

/*
 * Simple memory pool implementation
 */
typedef struct
{
    void           *poolStart;
    size_t          poolSize;
    size_t          blockSize;
    size_t          totalBlocks;
    bool           *freeBlocks;
    pthread_mutex_t mutex;
} MemoryPool;

/*
 * Create a new slot manager instance
 */
SlotManager *
SlotManagerCreate(size_t maxSlots, size_t memoryPoolSize)
{
    SlotManager *manager;
    MemoryPool  *pool;
    
    manager = malloc(sizeof(SlotManager));
    if (manager == NULL)
        return NULL;
    
    /* Allocate slot table */
    manager->slotTable = calloc(maxSlots, sizeof(SlotEntry));
    if (manager->slotTable == NULL) {
        free(manager);
        return NULL;
    }
    
    manager->tableSize = maxSlots;
    manager->maxSlots = maxSlots;
    manager->nextSlotId = 1; /* 0 is reserved as invalid ID */
    
    /* Create memory pool */
    pool = malloc(sizeof(MemoryPool));
    if (pool == NULL) {
        free(manager->slotTable);
        free(manager);
        return NULL;
    }
    
    pool->poolSize = memoryPoolSize;
    pool->blockSize = 64; /* 64-byte blocks */
    pool->totalBlocks = memoryPoolSize / pool->blockSize;
    pool->poolStart = aligned_alloc(64, memoryPoolSize); /* 64-byte aligned */
    pool->freeBlocks = calloc(pool->totalBlocks, sizeof(bool));
    
    if (pool->poolStart == NULL || pool->freeBlocks == NULL) {
        if (pool->poolStart != NULL)
            free(pool->poolStart);
        if (pool->freeBlocks != NULL)
            free(pool->freeBlocks);
        free(pool);
        free(manager->slotTable);
        free(manager);
        return NULL;
    }
    
    pthread_mutex_init(&pool->mutex, NULL);
    manager->memoryPool = pool;
    
    /* Initialize mutex */
    manager->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init((pthread_mutex_t *)manager->mutex, NULL);
    
    /* Initialize statistics */
    manager->totalAllocations = 0;
    manager->totalDeallocations = 0;
    manager->activeSlots = 0;
    manager->cacheHits = 0;
    manager->cacheMisses = 0;
    
    return manager;
}

/*
 * Destroy slot manager and free all resources
 */
void
SlotManagerDestroy(SlotManager *manager)
{
    MemoryPool *pool;
    
    if (manager == NULL)
        return;
    
    /* Free memory pool */
    if (manager->memoryPool != NULL) {
        pool = (MemoryPool *)manager->memoryPool;
        pthread_mutex_destroy(&pool->mutex);
        if (pool->poolStart != NULL)
            free(pool->poolStart);
        if (pool->freeBlocks != NULL)
            free(pool->freeBlocks);
        free(pool);
    }
    
    /* Free mutex */
    if (manager->mutex != NULL) {
        pthread_mutex_destroy((pthread_mutex_t *)manager->mutex);
        free(manager->mutex);
    }
    
    /* Free slot table */
    if (manager->slotTable != NULL)
        free(manager->slotTable);
    
    free(manager);
}

/*
 * Allocate memory block from pool (internal function)
 */
static void *
AllocateMemoryBlock(SlotManager *manager, size_t size)
{
    MemoryPool *pool;
    size_t      blocksNeeded;
    size_t      i, j;
    bool        found;
    void       *result;
    
    if (manager == NULL || manager->memoryPool == NULL)
        return NULL;
    
    pool = (MemoryPool *)manager->memoryPool;
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Calculate required blocks */
    blocksNeeded = (size + pool->blockSize - 1) / pool->blockSize;
    
    /* Find contiguous free blocks */
    for (i = 0; i <= pool->totalBlocks - blocksNeeded; i++) {
        found = true;
        for (j = 0; j < blocksNeeded; j++) {
            if (pool->freeBlocks[i + j]) {
                found = false;
                break;
            }
        }
        
        if (found) {
            /* Mark blocks as used */
            for (j = 0; j < blocksNeeded; j++)
                pool->freeBlocks[i + j] = true;
            
            result = (char *)pool->poolStart + (i * pool->blockSize);
            pthread_mutex_unlock(&pool->mutex);
            return result;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return NULL; /* Allocation failed */
}

/*
 * Deallocate memory block (internal function)
 */
static void
DeallocateMemoryBlock(SlotManager *manager, void *ptr, size_t size)
{
    MemoryPool *pool;
    size_t      blockIndex;
    size_t      blocksToFree;
    size_t      i;
    
    if (manager == NULL || manager->memoryPool == NULL || ptr == NULL)
        return;
    
    pool = (MemoryPool *)manager->memoryPool;
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Calculate block index */
    blockIndex = ((char *)ptr - (char *)pool->poolStart) / pool->blockSize;
    blocksToFree = (size + pool->blockSize - 1) / pool->blockSize;
    
    /* Mark blocks as free */
    for (i = 0; i < blocksToFree; i++) {
        if (blockIndex + i < pool->totalBlocks)
            pool->freeBlocks[blockIndex + i] = false;
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

/*
 * Claim a new slot (C implementation for general case)
 */
SlotError
SlotClaim(SlotManager *manager, TypeTag type, SlotHandle *handle)
{
    SlotEntry *entry;
    size_t     i;
    
    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    /* Use assembly optimized version for simple types */
    if (type <= TYPE_CUSTOM)
        return SlotClaimFast(manager, type, handle);
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find empty slot */
    for (i = 0; i < manager->tableSize; i++) {
        entry = &manager->slotTable[i];
        
        if (!entry->occupied) {
            /* Initialize slot */
            entry->slotId = manager->nextSlotId++;
            entry->typeTag = type;
            entry->occupied = true;
            entry->dataBlockRef = NULL; /* Allocated later */
            entry->ttl = 0; /* Unlimited */
            entry->threadAffinity = 0; /* Current thread */
            entry->allocationTime = time(NULL);
            
            /* Set handle */
            handle->slotId = entry->slotId;
            handle->typeTag = type;
            handle->generation = 1;
            
            /* Update statistics */
            manager->totalAllocations++;
            manager->activeSlots++;
            
            pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
            return SLOT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_ERROR_OUT_OF_MEMORY;
}

/*
 * Write data to slot
 */
SlotError
SlotWrite(SlotManager *manager, const SlotHandle *handle, 
          const void *data, size_t dataSize)
{
    SlotEntry *entry = NULL;
    size_t     i;
    
    if (manager == NULL || handle == NULL || data == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    /* Use assembly version for simple types */
    if (dataSize <= 256 && handle->typeTag <= TYPE_CUSTOM)
        return SlotWriteFast(manager, handle, data, dataSize);
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find slot */
    for (i = 0; i < manager->tableSize; i++) {
        if (manager->slotTable[i].slotId == handle->slotId &&
            manager->slotTable[i].occupied) {
            entry = &manager->slotTable[i];
            break;
        }
    }
    
    if (entry == NULL) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    /* Validate type */
    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_TYPE_MISMATCH;
    }
    
    /* Allocate memory block if needed */
    if (entry->dataBlockRef == NULL) {
        entry->dataBlockRef = AllocateMemoryBlock(manager, dataSize);
        if (entry->dataBlockRef == NULL) {
            pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
            return SLOT_ERROR_OUT_OF_MEMORY;
        }
    }
    
    /* Copy data */
    memcpy(entry->dataBlockRef, data, dataSize);
    
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_SUCCESS;
}

/*
 * Read data from slot
 */
SlotError
SlotRead(SlotManager *manager, const SlotHandle *handle, 
         void *buffer, size_t bufferSize, size_t *bytesRead)
{
    SlotEntry *entry = NULL;
    size_t     i;
    size_t     copySize;
    
    if (manager == NULL || handle == NULL || buffer == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find slot */
    for (i = 0; i < manager->tableSize; i++) {
        if (manager->slotTable[i].slotId == handle->slotId &&
            manager->slotTable[i].occupied) {
            entry = &manager->slotTable[i];
            break;
        }
    }
    
    if (entry == NULL) {
        manager->cacheMisses++;
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    /* Validate type */
    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_TYPE_MISMATCH;
    }
    
    /* Check data block */
    if (entry->dataBlockRef == NULL) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    /* Copy data (size determined by type) */
    copySize = TypeGetSize(entry->typeTag);
    if (copySize > bufferSize)
        copySize = bufferSize;
    
    memcpy(buffer, entry->dataBlockRef, copySize);
    if (bytesRead != NULL)
        *bytesRead = copySize;
    
    manager->cacheHits++;
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_SUCCESS;
}

/*
 * Release slot and free resources
 */
SlotError
SlotRelease(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    size_t     i;
    size_t     blockSize;
    
    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find slot */
    for (i = 0; i < manager->tableSize; i++) {
        entry = &manager->slotTable[i];
        
        if (entry->slotId == handle->slotId && entry->occupied) {
            /* Free memory block */
            if (entry->dataBlockRef != NULL) {
                blockSize = TypeGetSize(entry->typeTag);
                DeallocateMemoryBlock(manager, entry->dataBlockRef, blockSize);
            }
            
            /* Clear slot entry */
            memset(entry, 0, sizeof(SlotEntry));
            
            /* Update statistics */
            manager->totalDeallocations++;
            manager->activeSlots--;
            
            pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
            return SLOT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_ERROR_SLOT_NOT_FOUND;
}

/*
 * Print slot manager statistics
 */
void
SlotManagerPrintStats(const SlotManager *manager)
{
    if (manager == NULL)
        return;
    
    printf("=== Pergyra Slot Manager Statistics ===\n");
    printf("Total allocations: %lu\n", manager->totalAllocations);
    printf("Total deallocations: %lu\n", manager->totalDeallocations);
    printf("Active slots: %lu\n", manager->activeSlots);
    printf("Cache hits: %lu\n", manager->cacheHits);
    printf("Cache misses: %lu\n", manager->cacheMisses);
    printf("Table size: %zu\n", manager->tableSize);
    printf("Utilization: %.2f%%\n", SlotManagerGetUtilization(manager) * 100.0);
}

/*
 * Get active slot count
 */
size_t
SlotManagerGetActiveCount(const SlotManager *manager)
{
    return manager != NULL ? manager->activeSlots : 0;
}

/*
 * Get slot table utilization ratio
 */
double
SlotManagerGetUtilization(const SlotManager *manager)
{
    if (manager == NULL || manager->tableSize == 0)
        return 0.0;
    return (double)manager->activeSlots / (double)manager->tableSize;
}

/*
 * Get type size in bytes
 */
size_t
TypeGetSize(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:    return sizeof(int32_t);
    case TYPE_LONG:   return sizeof(int64_t);
    case TYPE_FLOAT:  return sizeof(float);
    case TYPE_DOUBLE: return sizeof(double);
    case TYPE_BOOL:   return sizeof(bool);
    case TYPE_STRING: return 256; /* Maximum string size */
    case TYPE_VECTOR: return 1024; /* Vector size */
    default:          return 64; /* Default size */
    }
}

/*
 * Convert type tag to string
 */
const char *
TypeTagToString(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:    return "Int";
    case TYPE_LONG:   return "Long";
    case TYPE_FLOAT:  return "Float";
    case TYPE_DOUBLE: return "Double";
    case TYPE_BOOL:   return "Bool";
    case TYPE_STRING: return "String";
    case TYPE_VECTOR: return "Vector";
    default:          return "Custom";
    }
}

/*
 * Check if type is primitive
 */
bool
TypeIsPrimitive(TypeTag tag)
{
    return tag >= TYPE_INT && tag <= TYPE_BOOL;
}

/*
 * Generate hash for type name
 */
uint32_t
TypeTagHash(const char *typeName)
{
    uint32_t hash = 5381;
    const char *c;
    
    if (typeName == NULL)
        return 0;
    
    /* Simple djb2 hash function */
    for (c = typeName; *c; c++)
        hash = ((hash << 5) + hash) + *c;
    
    return hash;
}

/*
 * Inline function implementations
 */
static inline uint32_t
SlotHashFunction(uint32_t slotId)
{
    return SlotHashFunctionAsm(slotId);
}

static inline bool
SlotCompareAndSwap(volatile uint32_t *ptr, uint32_t expected, uint32_t newVal)
{
    return SlotCompareAndSwapAsm(ptr, expected, newVal);
}

static inline void
SlotMemoryBarrier(void)
{
    SlotMemoryBarrierAsm();
}