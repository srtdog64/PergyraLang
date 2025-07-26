/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Concurrent queue implementation for scheduler
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_CONCURRENT_QUEUE_H
#define PERGYRA_CONCURRENT_QUEUE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

/* Queue node */
typedef struct QueueNode {
    void* data;
    atomic_intptr_t next;
} QueueNode;

/* Lock-free concurrent queue using Michael & Scott algorithm */
typedef struct ConcurrentQueue {
    atomic_intptr_t head;
    atomic_intptr_t tail;
    atomic_size_t size;
} ConcurrentQueue;

/* Queue operations - BSD style with PascalCase */
ConcurrentQueue* ConcurrentQueueCreate(void);
void ConcurrentQueueDestroy(ConcurrentQueue* queue);

/* Enqueue/Dequeue */
void ConcurrentQueuePush(ConcurrentQueue* queue, void* data);
void* ConcurrentQueuePop(ConcurrentQueue* queue);
void* ConcurrentQueueTryPop(ConcurrentQueue* queue);

/* Queue state */
size_t ConcurrentQueueSize(ConcurrentQueue* queue);
bool ConcurrentQueueIsEmpty(ConcurrentQueue* queue);

/* Batch operations for efficiency */
void ConcurrentQueuePushBatch(ConcurrentQueue* queue, void** items, size_t count);
size_t ConcurrentQueuePopBatch(ConcurrentQueue* queue, void** buffer, size_t maxCount);

#endif /* PERGYRA_CONCURRENT_QUEUE_H */
