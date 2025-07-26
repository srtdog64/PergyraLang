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
#include <time.h>
#include <assert.h>

/*
 * Platform-specific cache line size detection
 */
#ifdef _WIN32
#include <windows.h>
#define CACHE_LINE_SIZE 64
#else
#include <unistd.h>
#define CACHE_LINE_SIZE 64
#endif

/*
 * Create a new slot pool
 */
SlotPool *
SlotPoolCreate(size_t elementSize, size_t capacity, bool cacheOptimized)
{
    SlotPool *pool;
    size_t    alignedElementSize;
    size_t    totalDataSize;
    
    pool = malloc(sizeof(SlotPool));
    if (pool == NULL)
        return NULL;
    
    /* Cache-align element size if requested */
    if (cacheOptimized) {
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
    totalDataSize = alignedElementSize * capacity;
    if (cacheOptimized) {
        pool->data = aligned_alloc(CACHE_LINE_SIZE, totalDataSize);
    } else {
        pool->data = malloc(totalDataSize);
    }
    
    if (pool->data == NULL) {
        free(pool);
        return NULL;
    }
    
    /* Initialize occupancy bitmap */
    pool->occupied = calloc(capacity, sizeof(bool));
    if (pool->occupied == NULL) {
        free(pool->data);
        free(pool);
        return NULL;
    }
    
    /* Initialize free list */
    pool->freeList = malloc(capacity * sizeof(PoolIndex));
    if (pool->freeList == NULL) {
        free(pool->occupied);
        free(pool->data);
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
    
    if (pool->data != NULL)
        free(pool->data);
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
    
    if (pool == NULL || pool->freeListTop == 0)
        return NULL_INDEX;
    
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
    if (pool == NULL || index >= pool->capacity || !pool->occupied[index])
        return false;
    
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
    if (pool == NULL || index >= pool->capacity || !pool->occupied[index])
        return NULL;
    
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
    printf("Total allocations: %lu\n", pool->totalAllocations);
    printf("Total deallocations: %lu\n", pool->totalDeallocations);
    printf("Cache optimized: %s\n", pool->cacheOptimized ? "Yes" : "No");
    if (pool->cacheOptimized) {
        printf("Cache line size: %zu bytes\n", pool->cacheLineSize);
    }
}

/*
 * Create pool-based linked list
 */
LinkedList *
LinkedListCreate(size_t capacity)
{
    LinkedList *list;
    
    list = malloc(sizeof(LinkedList));
    if (list == NULL)
        return NULL;
    
    list->nodePool = SlotPoolCreate(sizeof(LinkedListNode), capacity, true);
    if (list->nodePool == NULL) {
        free(list);
        return NULL;
    }
    
    list->head = NULL_INDEX;
    list->tail = NULL_INDEX;
    list->count = 0;
    
    return list;
}

/*
 * Destroy linked list
 */
void
LinkedListDestroy(LinkedList *list)
{
    if (list == NULL)
        return;
    
    if (list->nodePool != NULL)
        SlotPoolDestroy(list->nodePool);
    
    free(list);
}

/*
 * Add node to back of list
 */
PoolIndex
LinkedListPushBack(LinkedList *list, int32_t value)
{
    PoolIndex       newIndex;
    LinkedListNode *newNode;
    LinkedListNode *tailNode;
    
    if (list == NULL)
        return NULL_INDEX;
    
    newIndex = SlotPoolAlloc(list->nodePool);
    if (newIndex == NULL_INDEX)
        return NULL_INDEX;
    
    newNode = (LinkedListNode *)SlotPoolGet(list->nodePool, newIndex);
    newNode->value = value;
    newNode->next = NULL_INDEX;
    newNode->prev = list->tail;
    newNode->generation = 1;
    
    if (list->tail != NULL_INDEX) {
        tailNode = (LinkedListNode *)SlotPoolGet(list->nodePool, list->tail);
        tailNode->next = newIndex;
    } else {
        list->head = newIndex;
    }
    
    list->tail = newIndex;
    list->count++;
    
    return newIndex;
}

/*
 * Add node to front of list
 */
PoolIndex
LinkedListPushFront(LinkedList *list, int32_t value)
{
    PoolIndex       newIndex;
    LinkedListNode *newNode;
    LinkedListNode *headNode;
    
    if (list == NULL)
        return NULL_INDEX;
    
    newIndex = SlotPoolAlloc(list->nodePool);
    if (newIndex == NULL_INDEX)
        return NULL_INDEX;
    
    newNode = (LinkedListNode *)SlotPoolGet(list->nodePool, newIndex);
    newNode->value = value;
    newNode->next = list->head;
    newNode->prev = NULL_INDEX;
    newNode->generation = 1;
    
    if (list->head != NULL_INDEX) {
        headNode = (LinkedListNode *)SlotPoolGet(list->nodePool, list->head);
        headNode->prev = newIndex;
    } else {
        list->tail = newIndex;
    }
    
    list->head = newIndex;
    list->count++;
    
    return newIndex;
}

/*
 * Remove node from list
 */
bool
LinkedListRemove(LinkedList *list, PoolIndex nodeIndex)
{
    LinkedListNode *node;
    LinkedListNode *prevNode;
    LinkedListNode *nextNode;
    
    if (list == NULL || !SlotPoolIsValid(list->nodePool, nodeIndex))
        return false;
    
    node = (LinkedListNode *)SlotPoolGet(list->nodePool, nodeIndex);
    
    /* Update previous node */
    if (node->prev != NULL_INDEX) {
        prevNode = (LinkedListNode *)SlotPoolGet(list->nodePool, node->prev);
        prevNode->next = node->next;
    } else {
        list->head = node->next;
    }
    
    /* Update next node */
    if (node->next != NULL_INDEX) {
        nextNode = (LinkedListNode *)SlotPoolGet(list->nodePool, node->next);
        nextNode->prev = node->prev;
    } else {
        list->tail = node->prev;
    }
    
    /* Free the node */
    SlotPoolFree(list->nodePool, nodeIndex);
    list->count--;
    
    return true;
}

/*
 * Traverse list and call visitor function
 */
void
LinkedListTraverse(LinkedList *list, void (*visitor)(int32_t value))
{
    PoolIndex       current;
    LinkedListNode *node;
    
    if (list == NULL || visitor == NULL)
        return;
    
    current = list->head;
    while (current != NULL_INDEX) {
        node = (LinkedListNode *)SlotPoolGet(list->nodePool, current);
        visitor(node->value);
        current = node->next;
    }
}

/*
 * Get node by index
 */
LinkedListNode *
LinkedListGetNode(LinkedList *list, PoolIndex index)
{
    if (list == NULL || !SlotPoolIsValid(list->nodePool, index))
        return NULL;
    
    return (LinkedListNode *)SlotPoolGet(list->nodePool, index);
}

/*
 * Get current timestamp in nanoseconds
 */
uint64_t
GetTimestampNs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/*
 * Prefetch memory for better cache performance
 */
void
PrefetchMemory(const void *ptr, size_t size)
{
    const char *p = (const char *)ptr;
    const char *end = p + size;
    
    /* Prefetch cache lines */
    while (p < end) {
        __builtin_prefetch(p, 0, 3); /* Prefetch for read, high temporal locality */
        p += CACHE_LINE_SIZE;
    }
}

/*
 * Check if pointer is aligned to cache line
 */
bool
IsAlignedToCache(const void *ptr)
{
    return ((uintptr_t)ptr % CACHE_LINE_SIZE) == 0;
}

/*
 * Benchmark linked list performance
 */
PerformanceMetrics
BenchmarkLinkedList(size_t nodeCount, size_t iterations)
{
    PerformanceMetrics metrics = {0};
    LinkedList        *list;
    uint64_t           startTime, endTime;
    size_t             i, j;
    
    /* Allocation benchmark */
    startTime = GetTimestampNs();
    
    for (i = 0; i < iterations; i++) {
        list = LinkedListCreate(nodeCount);
        
        for (j = 0; j < nodeCount; j++) {
            LinkedListPushBack(list, (int32_t)j);
        }
        
        LinkedListDestroy(list);
    }
    
    endTime = GetTimestampNs();
    metrics.allocationTime = (double)(endTime - startTime) / iterations / nodeCount;
    
    /* Traversal benchmark */
    list = LinkedListCreate(nodeCount);
    for (i = 0; i < nodeCount; i++) {
        LinkedListPushBack(list, (int32_t)i);
    }
    
    startTime = GetTimestampNs();
    
    for (i = 0; i < iterations; i++) {
        PoolIndex current = list->head;
        while (current != NULL_INDEX) {
            LinkedListNode *node = LinkedListGetNode(list, current);
            volatile int32_t value = node->value; /* Prevent optimization */
            current = node->next;
        }
    }
    
    endTime = GetTimestampNs();
    metrics.traversalTime = (double)(endTime - startTime) / iterations / nodeCount;
    
    /* Calculate memory utilization */
    SlotPool *pool = list->nodePool;
    metrics.memoryUtilization = (double)pool->count / pool->capacity * 100.0;
    
    LinkedListDestroy(list);
    
    printf("LinkedList Benchmark Results:\n");
    printf("  Allocation time: %.2f ns per node\n", metrics.allocationTime);
    printf("  Traversal time: %.2f ns per node\n", metrics.traversalTime);
    printf("  Memory utilization: %.1f%%\n", metrics.memoryUtilization);
    
    return metrics;
}