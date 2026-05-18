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

#include "slot_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

/*
 * Platform-specific cache line size detection
 */
#ifdef _WIN32
#include <malloc.h>
#include <windows.h>
#define CACHE_LINE_SIZE 64
#else
#include <unistd.h>
#define CACHE_LINE_SIZE 64
#endif

static void *
slot_pool_alloc_data(size_t alignment, size_t size, bool cacheOptimized)
{
    if (!cacheOptimized)
        return malloc(size);
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    return aligned_alloc(alignment, size);
#endif
}
static void
slot_pool_free_data(void *data, bool cacheOptimized)
{
    if (data == NULL)
        return;
#ifdef _WIN32
    if (cacheOptimized) {
        _aligned_free(data);
        return;
    }
#else
    (void)cacheOptimized;
#endif
    free(data);
}

static void
slot_pool_warn(const char *op, const char *reason, size_t capacity,
               PoolIndex index)
{
    fprintf(stderr,
            "[pgy][slot-pool] %s failed: %s (capacity=%zu index=%u)\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "unknown",
            capacity,
            (unsigned)index);
}

/*
 * Create a new slot pool
 */
SlotPool *
SlotPoolCreate(size_t elementSize, size_t capacity, bool cacheOptimized)
{
    SlotPool *pool;
    size_t    alignedElementSize;
    size_t    totalDataSize;

    if (elementSize == 0 || capacity == 0) {
        slot_pool_warn("create", "elementSize and capacity must be non-zero",
                       capacity, NULL_INDEX);
        return NULL;
    }
    if (capacity > UINT32_MAX) {
        slot_pool_warn("create", "capacity exceeds PoolIndex range",
                       capacity, NULL_INDEX);
        return NULL;
    }
    
    pool = malloc(sizeof(SlotPool));
    if (pool == NULL)
        return NULL;
    
    /* Cache-align element size if requested */
    if (cacheOptimized) {
        if (elementSize > SIZE_MAX - (CACHE_LINE_SIZE - 1)) {
            free(pool);
            slot_pool_warn("create", "element size alignment overflow",
                           capacity, NULL_INDEX);
            return NULL;
        }
        alignedElementSize = ((elementSize + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE) * CACHE_LINE_SIZE;
        pool->cacheLineSize = CACHE_LINE_SIZE;
    } else {
        alignedElementSize = elementSize;
        pool->cacheLineSize = 0;
    }
    
    pool->elementSize = alignedElementSize;
    pool->capacity = capacity;
    pool->count = 0;
    pool->cacheOptimized = cacheOptimized;
    
    /* Allocate aligned data array */
    if (alignedElementSize > SIZE_MAX / capacity
        || capacity > SIZE_MAX / sizeof(bool)
        || capacity > SIZE_MAX / sizeof(PoolIndex)) {
        free(pool);
        slot_pool_warn("create", "allocation size overflow",
                       capacity, NULL_INDEX);
        return NULL;
    }
    totalDataSize = alignedElementSize * capacity;
    pool->data = slot_pool_alloc_data(CACHE_LINE_SIZE, totalDataSize,
                                      cacheOptimized);
    
    if (pool->data == NULL) {
        free(pool);
        return NULL;
    }
    
    /* Initialize occupancy bitmap */
    pool->occupied = calloc(capacity, sizeof(bool));
    if (pool->occupied == NULL) {
        slot_pool_free_data(pool->data, pool->cacheOptimized);
        free(pool);
        return NULL;
    }
    
    /* Initialize free list */
    pool->freeList = malloc(capacity * sizeof(PoolIndex));
    if (pool->freeList == NULL) {
        free(pool->occupied);
        slot_pool_free_data(pool->data, pool->cacheOptimized);
        free(pool);
        return NULL;
    }
    
    /* Fill free list with all indices */
    for (size_t i = 0; i < capacity; i++) {
        pool->freeList[i] = (PoolIndex)i;
    }
    pool->freeListTop = capacity;
    
    /* Initialize statistics */
    pool->totalAllocations = 0;
    pool->totalDeallocations = 0;
    pool->peakUsage = 0;
    
    /* Clear data array */
    memset(pool->data, 0, totalDataSize);
    
    return pool;
}

/*
 * Destroy slot pool and free resources
 */
void
SlotPoolDestroy(SlotPool *pool)
{
    if (pool == NULL)
        return;
    
    slot_pool_free_data(pool->data, pool->cacheOptimized);
    if (pool->occupied != NULL)
        free(pool->occupied);
    if (pool->freeList != NULL)
        free(pool->freeList);
    
    free(pool);
}

/*
 * Allocate a new slot from the pool
 */
PoolIndex
SlotPoolAlloc(SlotPool *pool)
{
    PoolIndex index;
    
    if (pool == NULL) {
        slot_pool_warn("alloc", "pool is null", 0, NULL_INDEX);
        return NULL_INDEX;
    }
    if (pool->freeListTop == 0) {
        slot_pool_warn("alloc", "pool exhausted", pool->capacity, NULL_INDEX);
        return NULL_INDEX;
    }
    
    /* Pop from free list */
    pool->freeListTop--;
    index = pool->freeList[pool->freeListTop];
    
    /* Mark as occupied */
    pool->occupied[index] = true;
    pool->count++;
    
    /* Update statistics */
    pool->totalAllocations++;
    if (pool->count > pool->peakUsage)
        pool->peakUsage = pool->count;
    
    return index;
}

/*
 * Free a slot back to the pool
 */
bool
SlotPoolFree(SlotPool *pool, PoolIndex index)
{
    if (pool == NULL) {
        slot_pool_warn("free", "pool is null", 0, index);
        return false;
    }
    if (index >= pool->capacity) {
        slot_pool_warn("free", "index out of range", pool->capacity, index);
        return false;
    }
    if (!pool->occupied[index]) {
        slot_pool_warn("free", "slot already free or never allocated",
                       pool->capacity, index);
        return false;
    }
    
    /* Clear the slot data */
    void *slotData = (char *)pool->data + (index * pool->elementSize);
    memset(slotData, 0, pool->elementSize);
    
    /* Mark as free */
    pool->occupied[index] = false;
    pool->count--;
    
    /* Push back to free list */
    pool->freeList[pool->freeListTop] = index;
    pool->freeListTop++;
    
    /* Update statistics */
    pool->totalDeallocations++;
    
    return true;
}

/*
 * Get pointer to slot data
 */
void *
SlotPoolGet(SlotPool *pool, PoolIndex index)
{
    if (pool == NULL) {
        slot_pool_warn("get", "pool is null", 0, index);
        return NULL;
    }
    if (index >= pool->capacity) {
        slot_pool_warn("get", "index out of range", pool->capacity, index);
        return NULL;
    }
    if (!pool->occupied[index]) {
        slot_pool_warn("get", "slot is not occupied", pool->capacity, index);
        return NULL;
    }
    
    return (char *)pool->data + (index * pool->elementSize);
}

/*
 * Check if slot index is valid and occupied
 */
bool
SlotPoolIsValid(SlotPool *pool, PoolIndex index)
{
    if (pool == NULL || index >= pool->capacity)
        return false;
    
    return pool->occupied[index];
}

/*
 * Print pool statistics
 */
void
SlotPoolPrintStats(const SlotPool *pool)
{
    if (pool == NULL)
        return;
    
    printf("=== SlotPool Statistics ===\n");
    printf("Capacity: %zu elements\n", pool->capacity);
    printf("Element size: %zu bytes\n", pool->elementSize);
    printf("Current usage: %zu/%zu (%.1f%%)\n", 
           pool->count, pool->capacity, 
           (double)pool->count / pool->capacity * 100.0);
    printf("Peak usage: %zu elements\n", pool->peakUsage);
    printf("Total allocations: %" PRIu64 "\n", pool->totalAllocations);
    printf("Total deallocations: %" PRIu64 "\n", pool->totalDeallocations);
    printf("Cache optimized: %s\n", pool->cacheOptimized ? "Yes" : "No");
    if (pool->cacheOptimized) {
        printf("Cache line size: %zu bytes\n", pool->cacheLineSize);
    }
}
