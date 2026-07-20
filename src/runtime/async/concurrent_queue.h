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
typedef struct PgyMnQueueNode {
    void* data;
    atomic_intptr_t next;
} PgyMnQueueNode;

/* Concurrent FIFO queue abstraction used by the async scheduler.
 * Current implementation is a mutex-backed FIFO queue. */
typedef struct PgyMnQueue {
    atomic_intptr_t head;
    atomic_intptr_t tail;
    atomic_size_t size;
} PgyMnQueue;

/* Queue operations - BSD style with PascalCase */
PgyMnQueue* pgy_mn_queue_create(void);
/* Destroy is quiescent-only: callers must stop producers/consumers first. */
void pgy_mn_queue_destroy(PgyMnQueue* queue);

/* Enqueue/Dequeue.
 * NULL payloads are rejected: Pop uses NULL as the empty/failure sentinel. */
bool pgy_mn_queue_push(PgyMnQueue* queue, void* data);
void* pgy_mn_queue_pop(PgyMnQueue* queue);
void* pgy_mn_queue_try_pop(PgyMnQueue* queue);

/* Queue state */
size_t pgy_mn_queue_size(PgyMnQueue* queue);
bool pgy_mn_queue_is_empty(PgyMnQueue* queue);

/* Batch operations for efficiency.
 * PushBatch is all-or-nothing: allocation failure enqueues no items. */
bool pgy_mn_queue_push_batch(PgyMnQueue* queue, void** items, size_t count);
size_t pgy_mn_queue_pop_batch(PgyMnQueue* queue, void** buffer, size_t maxCount);

#endif /* PERGYRA_CONCURRENT_QUEUE_H */
