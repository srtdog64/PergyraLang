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

    pthread_mutex_init(&state->mutex, NULL);
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

void
ConcurrentQueuePush(ConcurrentQueue *queue, void *data)
{
    ConcurrentQueueState *state = queue_state(queue);
    QueueNode *node;

    if (state == NULL) {
        concurrent_queue_warn("push", "queue state is null", queue);
        return;
    }

    node = queue_node_create(data);
    if (node == NULL) {
        concurrent_queue_warn("push", "node allocation failed", queue);
        return;
    }

    pthread_mutex_lock(&state->mutex);
    atomic_store_explicit(&state->tail->next, (intptr_t)node, memory_order_release);
    state->tail = node;
    atomic_fetch_add_explicit(&queue->size, 1, memory_order_acq_rel);
    pthread_mutex_unlock(&state->mutex);
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
    atomic_fetch_sub_explicit(&queue->size, 1, memory_order_acq_rel);
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

void
ConcurrentQueuePushBatch(ConcurrentQueue *queue, void **items, size_t count)
{
    if (queue == NULL) {
        concurrent_queue_warn("push-batch", "queue is null", queue);
        return;
    }
    if (items == NULL && count > 0) {
        concurrent_queue_warn("push-batch", "items buffer is null", queue);
        return;
    }

    for (size_t i = 0; i < count; i++)
        ConcurrentQueuePush(queue, items[i]);
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
