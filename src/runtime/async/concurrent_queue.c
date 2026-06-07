/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Concurrent queue implementation for scheduler
 */

#include "concurrent_queue.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ConcurrentQueueState {
    pthread_mutex_t mutex;
    QueueNode *head;
    QueueNode *tail;
} ConcurrentQueueState;

static void
concurrent_queue_warn(const char *op, const char *reason, ConcurrentQueue *queue)
{
    fprintf(stderr,
            "[pgy][concurrent-queue] %s failed: %s (queue=%p)\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown",
            (void *)queue);
}

static ConcurrentQueueState *
queue_state(ConcurrentQueue *queue)
{
    if (queue == NULL)
        return NULL;
    return (ConcurrentQueueState *)(intptr_t)
        atomic_load_explicit(&queue->head, memory_order_acquire);
}

static QueueNode *
queue_node_create(void *data)
{
    QueueNode *node = (QueueNode *)calloc(1, sizeof(QueueNode));
    if (node == NULL)
        return NULL;
    node->data = data;
    atomic_init(&node->next, (intptr_t)NULL);
    return node;
}

static void
queue_size_increment(ConcurrentQueue *queue)
{
    size_t current;

    if (queue == NULL)
        return;
    current = atomic_load_explicit(&queue->size, memory_order_acquire);
    while (current != SIZE_MAX) {
        if (atomic_compare_exchange_weak_explicit(
                &queue->size, &current, current + 1,
                memory_order_acq_rel, memory_order_acquire)) {
            return;
        }
    }
}

static void
queue_size_increment_by(ConcurrentQueue *queue, size_t count)
{
    size_t current;
    size_t next;

    if (queue == NULL || count == 0)
        return;

    current = atomic_load_explicit(&queue->size, memory_order_acquire);
    do {
        next = (SIZE_MAX - current < count) ? SIZE_MAX : current + count;
    } while (!atomic_compare_exchange_weak_explicit(
        &queue->size, &current, next,
        memory_order_acq_rel, memory_order_acquire));
}

static void
queue_size_decrement(ConcurrentQueue *queue)
{
    size_t current;

    if (queue == NULL)
        return;
    current = atomic_load_explicit(&queue->size, memory_order_acquire);
    while (current > 0) {
        if (atomic_compare_exchange_weak_explicit(
                &queue->size, &current, current - 1,
                memory_order_acq_rel, memory_order_acquire)) {
            return;
        }
    }
}

static void
queue_node_destroy_chain(QueueNode *node)
{
    while (node != NULL) {
        QueueNode *next = (QueueNode *)(intptr_t)
            atomic_load_explicit(&node->next, memory_order_acquire);
        free(node);
        node = next;
    }
}

ConcurrentQueue *
ConcurrentQueueCreate(void)
{
    ConcurrentQueue *queue = (ConcurrentQueue *)calloc(1, sizeof(ConcurrentQueue));
    ConcurrentQueueState *state;
    QueueNode *sentinel;

    if (queue == NULL) {
        concurrent_queue_warn("create", "queue allocation failed", NULL);
        return NULL;
    }

    state = (ConcurrentQueueState *)calloc(1, sizeof(ConcurrentQueueState));
    if (state == NULL) {
        concurrent_queue_warn("create", "state allocation failed", queue);
        free(queue);
        return NULL;
    }

    sentinel = queue_node_create(NULL);
    if (sentinel == NULL) {
        concurrent_queue_warn("create", "sentinel allocation failed", queue);
        free(state);
        free(queue);
        return NULL;
    }

    if (pthread_mutex_init(&state->mutex, NULL) != 0) {
        concurrent_queue_warn("create", "mutex initialization failed", queue);
        free(sentinel);
        free(state);
        free(queue);
        return NULL;
    }
    state->head = sentinel;
    state->tail = sentinel;

    atomic_init(&queue->head, (intptr_t)state);
    atomic_init(&queue->tail, (intptr_t)state);
    atomic_init(&queue->size, 0);
    return queue;
}

void
ConcurrentQueueDestroy(ConcurrentQueue *queue)
{
    ConcurrentQueueState *state = queue_state(queue);
    QueueNode *node;

    if (queue == NULL)
        return;

    if (state == NULL) {
        concurrent_queue_warn("destroy", "queue state is null", queue);
        free(queue);
        return;
    }

    pthread_mutex_lock(&state->mutex);
    node = state->head;
    while (node != NULL) {
        QueueNode *next = (QueueNode *)(intptr_t)
            atomic_load_explicit(&node->next, memory_order_acquire);
        free(node);
        node = next;
    }
    state->head = NULL;
    state->tail = NULL;
    pthread_mutex_unlock(&state->mutex);

    pthread_mutex_destroy(&state->mutex);
    atomic_store_explicit(&queue->head, (intptr_t)NULL, memory_order_release);
    atomic_store_explicit(&queue->tail, (intptr_t)NULL, memory_order_release);
    free(state);
    free(queue);
}

bool
ConcurrentQueuePush(ConcurrentQueue *queue, void *data)
{
    ConcurrentQueueState *state = queue_state(queue);
    QueueNode *node;

    if (state == NULL) {
        concurrent_queue_warn("push", "queue state is null", queue);
        return false;
    }
    if (data == NULL) {
        concurrent_queue_warn("push", "NULL payload is reserved as the empty sentinel", queue);
        return false;
    }

    node = queue_node_create(data);
    if (node == NULL) {
        concurrent_queue_warn("push", "node allocation failed", queue);
        return false;
    }

    pthread_mutex_lock(&state->mutex);
    if (state->tail == NULL) {
        pthread_mutex_unlock(&state->mutex);
        free(node);
        concurrent_queue_warn("push", "queue tail is null", queue);
        return false;
    }
    atomic_store_explicit(&state->tail->next, (intptr_t)node, memory_order_release);
    state->tail = node;
    queue_size_increment(queue);
    pthread_mutex_unlock(&state->mutex);
    return true;
}

void *
ConcurrentQueuePop(ConcurrentQueue *queue)
{
    ConcurrentQueueState *state = queue_state(queue);
    QueueNode *sentinel;
    QueueNode *next;
    void *data;

    if (state == NULL) {
        concurrent_queue_warn("pop", "queue state is null", queue);
        return NULL;
    }

    pthread_mutex_lock(&state->mutex);
    sentinel = state->head;
    if (sentinel == NULL) {
        pthread_mutex_unlock(&state->mutex);
        concurrent_queue_warn("pop", "queue head is null", queue);
        return NULL;
    }
    next = sentinel != NULL
        ? (QueueNode *)(intptr_t)atomic_load_explicit(&sentinel->next, memory_order_acquire)
        : NULL;
    if (next == NULL) {
        pthread_mutex_unlock(&state->mutex);
        return NULL;
    }

    data = next->data;
    state->head = next;
    if (state->tail == next)
        state->tail = next;
    queue_size_decrement(queue);
    pthread_mutex_unlock(&state->mutex);

    free(sentinel);
    return data;
}

void *
ConcurrentQueueTryPop(ConcurrentQueue *queue)
{
    return ConcurrentQueuePop(queue);
}

size_t
ConcurrentQueueSize(ConcurrentQueue *queue)
{
    if (queue == NULL) {
        concurrent_queue_warn("size", "queue is null", queue);
        return 0;
    }
    return atomic_load_explicit(&queue->size, memory_order_acquire);
}

bool
ConcurrentQueueIsEmpty(ConcurrentQueue *queue)
{
    return ConcurrentQueueSize(queue) == 0;
}

bool
ConcurrentQueuePushBatch(ConcurrentQueue *queue, void **items, size_t count)
{
    ConcurrentQueueState *state = queue_state(queue);
    QueueNode *first = NULL;
    QueueNode *last = NULL;

    if (queue == NULL) {
        concurrent_queue_warn("push-batch", "queue is null", queue);
        return false;
    }
    if (state == NULL) {
        concurrent_queue_warn("push-batch", "queue state is null", queue);
        return false;
    }
    if (items == NULL && count > 0) {
        concurrent_queue_warn("push-batch", "items buffer is null", queue);
        return false;
    }
    if (count == 0)
        return true;

    for (size_t i = 0; i < count; i++) {
        QueueNode *node;

        if (items[i] == NULL) {
            queue_node_destroy_chain(first);
            concurrent_queue_warn("push-batch", "NULL payload is reserved as the empty sentinel", queue);
            return false;
        }
        node = queue_node_create(items[i]);
        if (node == NULL) {
            queue_node_destroy_chain(first);
            concurrent_queue_warn("push-batch", "node allocation failed", queue);
            return false;
        }
        if (last != NULL)
            atomic_store_explicit(&last->next, (intptr_t)node,
                                  memory_order_release);
        else
            first = node;
        last = node;
    }

    pthread_mutex_lock(&state->mutex);
    if (state->tail == NULL) {
        pthread_mutex_unlock(&state->mutex);
        queue_node_destroy_chain(first);
        concurrent_queue_warn("push-batch", "queue tail is null", queue);
        return false;
    }
    atomic_store_explicit(&state->tail->next, (intptr_t)first,
                          memory_order_release);
    state->tail = last;
    queue_size_increment_by(queue, count);
    pthread_mutex_unlock(&state->mutex);
    return true;
}

size_t
ConcurrentQueuePopBatch(ConcurrentQueue *queue, void **buffer, size_t maxCount)
{
    size_t count = 0;

    if (queue == NULL) {
        concurrent_queue_warn("pop-batch", "queue is null", queue);
        return 0;
    }
    if (buffer == NULL && maxCount > 0) {
        concurrent_queue_warn("pop-batch", "output buffer is null", queue);
        return 0;
    }

    while (count < maxCount) {
        void *item = ConcurrentQueuePop(queue);
        if (item == NULL)
            break;
        buffer[count++] = item;
    }

    return count;
}
