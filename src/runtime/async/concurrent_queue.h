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

/* Concurrent FIFO queue abstraction used by the async scheduler.
 * Current implementation is a mutex-backed FIFO queue. */
typedef struct ConcurrentQueue {
    atomic_intptr_t head;
    atomic_intptr_t tail;
    atomic_size_t size;
} ConcurrentQueue;

/* Queue operations - BSD style with PascalCase */
ConcurrentQueue* ConcurrentQueueCreate(void);
/* Destroy is quiescent-only: callers must stop producers/consumers first. */
void ConcurrentQueueDestroy(ConcurrentQueue* queue);

/* Enqueue/Dequeue.
 * NULL payloads are rejected: Pop uses NULL as the empty/failure sentinel. */
bool ConcurrentQueuePush(ConcurrentQueue* queue, void* data);
void* ConcurrentQueuePop(ConcurrentQueue* queue);
void* ConcurrentQueueTryPop(ConcurrentQueue* queue);

/* Queue state */
size_t ConcurrentQueueSize(ConcurrentQueue* queue);
bool ConcurrentQueueIsEmpty(ConcurrentQueue* queue);

/* Batch operations for efficiency.
 * PushBatch is all-or-nothing: allocation failure enqueues no items. */
bool ConcurrentQueuePushBatch(ConcurrentQueue* queue, void** items, size_t count);
size_t ConcurrentQueuePopBatch(ConcurrentQueue* queue, void** buffer, size_t maxCount);

#endif /* PERGYRA_CONCURRENT_QUEUE_H */
