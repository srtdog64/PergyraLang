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

typedef struct PgyMnQueueState {
    pthread_mutex_t mutex;
    PgyMnQueueNode *head;
    PgyMnQueueNode *tail;
} PgyMnQueueState;

static void
concurrent_queue_warn(const char *op, const char *reason, PgyMnQueue *queue)
{
    fprintf(stderr,
            "[pgy][concurrent-queue] %s failed: %s (queue=%p)\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown",
            (void *)queue);
}

static PgyMnQueueState *
queue_state(PgyMnQueue *queue)
{
    if (queue == NULL)
        return NULL;
    return (PgyMnQueueState *)(intptr_t)
        atomic_load_explicit(&queue->head, memory_order_acquire);
}

static PgyMnQueueNode *
queue_node_create(void *data)
{
    PgyMnQueueNode *node = (PgyMnQueueNode *)calloc(1, sizeof(PgyMnQueueNode));
    if (node == NULL)
        return NULL;
    node->data = data;
    atomic_init(&node->next, (intptr_t)NULL);
    return node;
}

static void
queue_size_increment(PgyMnQueue *queue)
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
queue_size_increment_by(PgyMnQueue *queue, size_t count)
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
queue_size_decrement(PgyMnQueue *queue)
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
queue_node_destroy_chain(PgyMnQueueNode *node)
{
    while (node != NULL) {
        PgyMnQueueNode *next = (PgyMnQueueNode *)(intptr_t)
            atomic_load_explicit(&node->next, memory_order_acquire);
        free(node);
        node = next;
    }
}

PgyMnQueue *
pgy_mn_queue_create(void)
{
    PgyMnQueue *queue = (PgyMnQueue *)calloc(1, sizeof(PgyMnQueue));
    PgyMnQueueState *state;
    PgyMnQueueNode *sentinel;

    if (queue == NULL) {
        concurrent_queue_warn("create", "queue allocation failed", NULL);
        return NULL;
    }

    state = (PgyMnQueueState *)calloc(1, sizeof(PgyMnQueueState));
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
pgy_mn_queue_destroy(PgyMnQueue *queue)
{
    PgyMnQueueState *state = queue_state(queue);
    PgyMnQueueNode *node;

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
        PgyMnQueueNode *next = (PgyMnQueueNode *)(intptr_t)
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
pgy_mn_queue_push(PgyMnQueue *queue, void *data)
{
    PgyMnQueueState *state = queue_state(queue);
    PgyMnQueueNode *node;

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
pgy_mn_queue_pop(PgyMnQueue *queue)
{
    PgyMnQueueState *state = queue_state(queue);
    PgyMnQueueNode *sentinel;
    PgyMnQueueNode *next;
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
        ? (PgyMnQueueNode *)(intptr_t)atomic_load_explicit(&sentinel->next, memory_order_acquire)
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
pgy_mn_queue_try_pop(PgyMnQueue *queue)
{
    return pgy_mn_queue_pop(queue);
}

size_t
pgy_mn_queue_size(PgyMnQueue *queue)
{
    if (queue == NULL) {
        concurrent_queue_warn("size", "queue is null", queue);
        return 0;
    }
    return atomic_load_explicit(&queue->size, memory_order_acquire);
}

bool
pgy_mn_queue_is_empty(PgyMnQueue *queue)
{
    return pgy_mn_queue_size(queue) == 0;
}

bool
pgy_mn_queue_push_batch(PgyMnQueue *queue, void **items, size_t count)
{
    PgyMnQueueState *state = queue_state(queue);
    PgyMnQueueNode *first = NULL;
    PgyMnQueueNode *last = NULL;

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
        PgyMnQueueNode *node;

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
pgy_mn_queue_pop_batch(PgyMnQueue *queue, void **buffer, size_t maxCount)
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
        void *item = pgy_mn_queue_pop(queue);
        if (item == NULL)
            break;
        buffer[count++] = item;
    }

    return count;
}
