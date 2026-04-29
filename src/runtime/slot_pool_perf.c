/*
 * Copyright (c) 2025 Pergyra Language Project
 * Slot pool performance and cache utility owner.
 * BSD 3-Clause License
 */

#include "slot_pool.h"
#include <stdio.h>
#include <time.h>
#include <stdint.h>

#ifdef _WIN32
#define CACHE_LINE_SIZE 64
#else
#define CACHE_LINE_SIZE 64
#endif

/*
 * Get current timestamp in nanoseconds
 */
uint64_t
GetTimestampNs(void)
{
    struct timespec ts;

#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
#endif

    if (timespec_get(&ts, TIME_UTC) == TIME_UTC) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }

    return (uint64_t)clock() * (1000000000ULL / CLOCKS_PER_SEC);
}

/*
 * Prefetch memory for better cache performance
 */
void
PrefetchMemory(const void *ptr, size_t size)
{
    const char *p = (const char *)ptr;
    const char *end = p + size;

    while (p < end) {
        __builtin_prefetch(p, 0, 3);
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
            (void)value;
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
