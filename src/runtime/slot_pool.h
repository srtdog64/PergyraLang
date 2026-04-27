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

#ifndef PERGYRA_SLOT_POOL_H
#define PERGYRA_SLOT_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Pool index type for efficient indexing
 */
typedef uint32_t PoolIndex;

#define NULL_INDEX ((PoolIndex)-1)
#define INVALID_INDEX ((PoolIndex)-2)

/*
 * Generic slot pool for homogeneous data structures
 * Level 1: High-performance pool-based allocation
 *
 * Beta-stable surface note:
 * keep this header limited to APIs that are implemented and regression-tested.
 * Tree/graph/smart-slot containers were removed from the public contract until
 * their implementation and tests are closed end-to-end.
 */
typedef struct
{
    void       *data;           /* Raw data array */
    size_t      elementSize;    /* Size of each element */
    size_t      capacity;       /* Maximum number of elements */
    size_t      count;          /* Current number of allocated elements */
    bool       *occupied;       /* Occupancy bitmap */
    PoolIndex  *freeList;       /* Free index stack */
    size_t      freeListTop;    /* Top of free list stack */
    
    /* Performance optimization */
    bool        cacheOptimized; /* Memory layout optimized for cache */
    size_t      cacheLineSize;  /* Cache line size (typically 64 bytes) */
    
    /* Statistics */
    uint64_t    totalAllocations;
    uint64_t    totalDeallocations;
    uint64_t    peakUsage;
} SlotPool;

/*
 * Pool-based linked list node
 * Optimized for cache-friendly traversal
 */
typedef struct
{
    int32_t     value;         /* Node data */
    PoolIndex   next;          /* Index to next node */
    PoolIndex   prev;          /* Index to previous node */
    uint32_t    generation;    /* For safety */
} LinkedListNode;

/*
 * SlotPool operations
 */
SlotPool   *SlotPoolCreate(size_t elementSize, size_t capacity, bool cacheOptimized);
void        SlotPoolDestroy(SlotPool *pool);
PoolIndex   SlotPoolAlloc(SlotPool *pool);
bool        SlotPoolFree(SlotPool *pool, PoolIndex index);
void       *SlotPoolGet(SlotPool *pool, PoolIndex index);
bool        SlotPoolIsValid(SlotPool *pool, PoolIndex index);
void        SlotPoolPrintStats(const SlotPool *pool);

/*
 * LinkedList operations using SlotPool
 */
typedef struct
{
    SlotPool   *nodePool;
    PoolIndex   head;
    PoolIndex   tail;
    size_t      count;
} LinkedList;

LinkedList     *LinkedListCreate(size_t capacity);
void            LinkedListDestroy(LinkedList *list);
PoolIndex       LinkedListPushBack(LinkedList *list, int32_t value);
PoolIndex       LinkedListPushFront(LinkedList *list, int32_t value);
bool            LinkedListRemove(LinkedList *list, PoolIndex nodeIndex);
void            LinkedListTraverse(LinkedList *list, void (*visitor)(int32_t value));
LinkedListNode *LinkedListGetNode(LinkedList *list, PoolIndex index);

/*
 * Performance testing and benchmarking
 */
typedef struct
{
    double      allocationTime;    /* Average allocation time (ns) */
    double      accessTime;        /* Average access time (ns) */
    double      traversalTime;     /* Average traversal time (ns) */
    size_t      cacheHits;         /* Cache hits during operations */
    size_t      cacheMisses;       /* Cache misses during operations */
    double      memoryUtilization; /* Memory utilization percentage */
} PerformanceMetrics;

PerformanceMetrics BenchmarkLinkedList(size_t nodeCount, size_t iterations);

/*
 * Utility functions
 */
uint64_t GetTimestampNs(void);
void     PrefetchMemory(const void *ptr, size_t size);
bool     IsAlignedToCache(const void *ptr);

#endif /* PERGYRA_SLOT_POOL_H */
