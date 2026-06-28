/*
 * Copyright (c) 2025 Pergyra Language Project
 * Real concurrency + coroutine runtime
 * BSD 3-Clause License
 */

#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>

#include "pgy_runtime_panic_contract.h"

#ifndef PGY_COROUTINES_AVAILABLE
#ifdef _WIN32
#define PGY_COROUTINES_AVAILABLE 1
#elif defined(__APPLE__)
#if defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700
#define PGY_COROUTINES_AVAILABLE 1
#else
#define PGY_COROUTINES_AVAILABLE 0
#endif
#else
#define PGY_COROUTINES_AVAILABLE 1
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#elif PGY_COROUTINES_AVAILABLE
#include <ucontext.h>
#endif

/* =================================================================
 * Shared task handle
 * ================================================================= */

typedef enum {
    PGY_TASK_PENDING,
    PGY_TASK_RUNNING,
    PGY_TASK_DONE
} PgyTaskState;

typedef enum {
    PGY_TASK_MODEL_THREAD = 1,
    PGY_TASK_MODEL_COROUTINE = 2
} PgyTaskModel;

typedef struct PgyCancelNode {
    struct PgyCancelNode *parent;
    atomic_size_t        refcount;
    atomic_bool          cancelled;
} PgyCancelNode;

typedef struct {
    PgyTaskModel model;
} PgyTaskHeader;

static inline PgyCancelNode *
pgy_current_cancel_node(void);

static inline void
pgy_parallel_warn(const char *op, const char *reason)
{
    fprintf(stderr,
            "[pgy][parallel] %s failed: %s\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown");
}

static inline bool
pgy_parallel_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

static inline void
pgy_cancel_retain(PgyCancelNode *node)
{
    if (node != NULL)
        (void)atomic_fetch_add_explicit(&node->refcount, 1, memory_order_relaxed);
}

static inline PgyCancelNode *
pgy_cancel_node_create(PgyCancelNode *parent)
{
    PgyCancelNode *node = (PgyCancelNode *)calloc(1, sizeof(PgyCancelNode));
    if (node == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    node->parent = parent;
    atomic_init(&node->refcount, 1);
    atomic_init(&node->cancelled, false);
    pgy_cancel_retain(parent);
    return node;
}

static inline void
pgy_cancel_release(PgyCancelNode *node)
{
    if (node == NULL)
        return;

    if (atomic_fetch_sub_explicit(&node->refcount, 1, memory_order_acq_rel) != 1)
        return;

    PgyCancelNode *parent = node->parent;
    free(node);
    pgy_cancel_release(parent);
}

static inline void
pgy_cancel_request(PgyCancelNode *node)
{
    if (node != NULL)
        atomic_store_explicit(&node->cancelled, true, memory_order_release);
}

static inline bool
pgy_cancel_is_requested(PgyCancelNode *node)
{
    while (node != NULL) {
        if (atomic_load_explicit(&node->cancelled, memory_order_acquire))
            return true;
        node = node->parent;
    }
    return false;
}

typedef struct PgyTask {
    PgyTaskModel    model;
    void *(*fn)(void *);
    void           *arg;
    void           *result;
    PgyTaskState    state;
    PgyCancelNode  *cancel_node;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    struct PgyTask *next;
} PgyTask;

typedef struct {
    void *task;                 /* PgyTask* or coroutine task header */
} PgyTaskHandle;

static inline bool
pgy_task_sync_init(PgyTask *task, const char *op)
{
    if (task == NULL) {
        pgy_parallel_warn(op, "task sync init target is null");
        return false;
    }
    if (pthread_mutex_init(&task->mutex, NULL) != 0) {
        pgy_parallel_warn(op, "task mutex initialization failed");
        return false;
    }
    if (pthread_cond_init(&task->cond, NULL) != 0) {
        pgy_parallel_warn(op, "task condition initialization failed");
        pthread_mutex_destroy(&task->mutex);
        return false;
    }
    return true;
}

static inline PgyTaskHandle
pgy_spawn_inline_completed(void *(*fn)(void *), void *arg, const char *op,
                           bool charge_spawn_budget)
{
    PgyTaskHandle handle = {0};
    const char *op_name = op != NULL ? op : "spawn-inline";

    if (fn == NULL) {
        pgy_parallel_warn(op_name, "task function is null");
        return handle;
    }
    if (charge_spawn_budget && pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL) {
        pgy_parallel_warn(op_name, "inline task allocation failed");
        return handle;
    }
    task->model = PGY_TASK_MODEL_THREAD;
    task->fn = fn;
    task->arg = arg;
    if (!pgy_task_sync_init(task, op_name)) {
        free(task);
        return handle;
    }
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    task->result = pgy_cancel_is_requested(task->cancel_node)
        ? NULL
        : fn(arg);
    task->state = PGY_TASK_DONE;
    handle.task = task;
    return handle;
}

/* =================================================================
 * Thread pool runtime for `parallel`
 * ================================================================= */

typedef struct {
    pthread_t      *workers;
    size_t          worker_count;
    PgyTask        *queue_head;
    PgyTask        *queue_tail;
    pthread_mutex_t queue_mutex;
    pthread_cond_t  queue_cond;
    bool            shutdown;
} PgyThreadPool;

static PgyThreadPool g_pgy_pool = {0};
static atomic_bool   g_pgy_pool_active = false;
static atomic_bool   g_pgy_pool_shutting_down = false;
static pthread_mutex_t g_pgy_pool_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static __thread PgyTask *g_pgy_thread_current = NULL;

/* Forward declaration — blocking pool shutdown called from pgy_pool_shutdown */
static inline void pgy_blocking_pool_shutdown(void);

static void *
pgy_worker_loop(void *arg)
{
    PgyThreadPool *pool = (PgyThreadPool *)arg;

    for (;;) {
        pthread_mutex_lock(&pool->queue_mutex);
        while (pool->queue_head == NULL && !pool->shutdown) {
            if (pthread_cond_wait(&pool->queue_cond,
                                  &pool->queue_mutex) != 0) {
                pgy_parallel_warn("worker-loop",
                                  "worker condition wait failed");
                pool->shutdown = true;
                pthread_cond_broadcast(&pool->queue_cond);
                pthread_mutex_unlock(&pool->queue_mutex);
                return NULL;
            }
        }

        if (pool->shutdown && pool->queue_head == NULL) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        PgyTask *task = pool->queue_head;
        if (task != NULL) {
            pool->queue_head = task->next;
            if (pool->queue_head == NULL)
                pool->queue_tail = NULL;
        }
        pthread_mutex_unlock(&pool->queue_mutex);

        if (task == NULL)
            continue;

        pthread_mutex_lock(&task->mutex);
        if (pgy_cancel_is_requested(task->cancel_node)) {
            task->state = PGY_TASK_DONE;
            task->result = NULL;
            pthread_cond_broadcast(&task->cond);
            pthread_mutex_unlock(&task->mutex);
            continue;
        }
        task->state = PGY_TASK_RUNNING;
        pthread_mutex_unlock(&task->mutex);

        g_pgy_thread_current = task;
        void *result = task->fn(task->arg);
        g_pgy_thread_current = NULL;

        pthread_mutex_lock(&task->mutex);
        task->result = result;
        task->state = PGY_TASK_DONE;
        pthread_cond_broadcast(&task->cond);
        pthread_mutex_unlock(&task->mutex);
    }

    return NULL;
}

static inline void
pgy_pool_init(size_t worker_count)
{
    pthread_mutex_lock(&g_pgy_pool_lifecycle_mutex);
    if (atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
    }
    if (atomic_load_explicit(&g_pgy_pool_shutting_down, memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
    }

    if (worker_count == 0)
        worker_count = 4;
    if (!pgy_parallel_array_fits(worker_count, sizeof(pthread_t))) {
        pgy_parallel_warn("pool-init", "worker array size overflow");
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
    }

    memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
    g_pgy_pool.worker_count = worker_count;
    if (pthread_mutex_init(&g_pgy_pool.queue_mutex, NULL) != 0) {
        pgy_parallel_warn("pool-init", "queue mutex initialization failed");
        memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
    }
    if (pthread_cond_init(&g_pgy_pool.queue_cond, NULL) != 0) {
        pgy_parallel_warn("pool-init", "queue condition initialization failed");
        pthread_mutex_destroy(&g_pgy_pool.queue_mutex);
        memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
    }

    g_pgy_pool.workers = (pthread_t *)calloc(worker_count, sizeof(pthread_t));
    if (g_pgy_pool.workers == NULL) {
        pgy_parallel_warn("pool-init", "worker array allocation failed");
        pthread_mutex_destroy(&g_pgy_pool.queue_mutex);
        pthread_cond_destroy(&g_pgy_pool.queue_cond);
        memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
    }
    for (size_t i = 0; i < worker_count; i++) {
        if (pthread_create(&g_pgy_pool.workers[i], NULL,
                           pgy_worker_loop, &g_pgy_pool) != 0) {
            pgy_parallel_warn("pool-init", "worker thread creation failed");
            g_pgy_pool.shutdown = true;
            pthread_cond_broadcast(&g_pgy_pool.queue_cond);
            for (size_t j = 0; j < i; j++)
                pthread_join(g_pgy_pool.workers[j], NULL);
            free(g_pgy_pool.workers);
            pthread_mutex_destroy(&g_pgy_pool.queue_mutex);
            pthread_cond_destroy(&g_pgy_pool.queue_cond);
            memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
            pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
            return;
        }
    }

    atomic_store_explicit(&g_pgy_pool_active, true, memory_order_release);
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
}

static inline void
pgy_pool_shutdown(void)
{
    pthread_mutex_lock(&g_pgy_pool_lifecycle_mutex);
    if (!atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
    }
    atomic_store_explicit(&g_pgy_pool_active, false, memory_order_release);
    atomic_store_explicit(&g_pgy_pool_shutting_down, true, memory_order_release);

    pthread_mutex_lock(&g_pgy_pool.queue_mutex);
    g_pgy_pool.shutdown = true;
    pthread_cond_broadcast(&g_pgy_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_pool.queue_mutex);

    pthread_t *workers = g_pgy_pool.workers;
    size_t worker_count = g_pgy_pool.worker_count;
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);

    for (size_t i = 0; i < worker_count; i++)
        pthread_join(workers[i], NULL);

    pthread_mutex_lock(&g_pgy_pool_lifecycle_mutex);

    free(workers);
    pthread_mutex_destroy(&g_pgy_pool.queue_mutex);
    pthread_cond_destroy(&g_pgy_pool.queue_cond);

    PgyTask *t = g_pgy_pool.queue_head;
    while (t != NULL) {
        PgyTask *next = t->next;
        pgy_cancel_release(t->cancel_node);
        pthread_mutex_destroy(&t->mutex);
        pthread_cond_destroy(&t->cond);
        free(t);
        t = next;
    }

    memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
    atomic_store_explicit(&g_pgy_pool_shutting_down, false,
                          memory_order_release);
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);

    /* Also shut down the blocking pool if it was started */
    pgy_blocking_pool_shutdown();
}

static inline PgyTaskHandle
pgy_spawn(void *(*fn)(void *), void *arg)
{
    PgyTaskHandle handle = {0};

    if (fn == NULL) {
        pgy_parallel_warn("spawn", "task function is null");
        return handle;
    }
    if (atomic_load_explicit(&g_pgy_pool_shutting_down,
                             memory_order_acquire)) {
        pgy_parallel_warn("spawn", "pool is shutting down");
        return handle;
    }

    /* Quantitative sandbox gate (R6): charge SPAWN_COUNT before spawning so a
     * fork-bomb of tasks fail-closes on the charge that crosses the host's
     * ceiling. Behind the imposed fast-path so trusted programs pay nothing.
     * Both backends funnel here (C emits pgy_spawn; LLVM's *_spawn_export call
     * it), so this one charge covers the spawn DoS surface on both. */
    if (pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");

    pthread_mutex_lock(&g_pgy_pool_lifecycle_mutex);
    if (atomic_load_explicit(&g_pgy_pool_shutting_down,
                             memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        pgy_parallel_warn("spawn", "pool is shutting down");
        return handle;
    }
    if (!atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return pgy_spawn_inline_completed(fn, arg, "spawn", false);
    }

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        pgy_parallel_warn("spawn", "task allocation failed");
        return handle;
    }

    task->model = PGY_TASK_MODEL_THREAD;
    task->fn = fn;
    task->arg = arg;
    task->state = PGY_TASK_PENDING;
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    if (!pgy_task_sync_init(task, "spawn")) {
        pgy_cancel_release(task->cancel_node);
        free(task);
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return handle;
    }
    handle.task = task;

    pthread_mutex_lock(&g_pgy_pool.queue_mutex);
    if (g_pgy_pool.queue_tail != NULL)
        g_pgy_pool.queue_tail->next = task;
    else
        g_pgy_pool.queue_head = task;
    g_pgy_pool.queue_tail = task;
    pthread_cond_signal(&g_pgy_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_pool.queue_mutex);
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);

    return handle;
}

#include "runtime/pgy_parallel_blocking.h"
#include "runtime/pgy_parallel_coroutine.h"

/* =================================================================
 * Task cancellation / cancellation query
 * ================================================================= */

static inline bool
pgy_task_cancel(PgyTaskHandle handle)
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    if (header == NULL)
        return false;

#if PGY_COROUTINES_AVAILABLE
    if (header->model == PGY_TASK_MODEL_COROUTINE) {
        PgyCoroTask *task = (PgyCoroTask *)handle.task;
        pgy_cancel_request(task->cancel_node);
        if (!pgy_async_in_coroutine())
            (void)pgy_async_progress_one();
        return true;
    }
#endif

    if (header->model == PGY_TASK_MODEL_THREAD) {
        PgyTask *task = (PgyTask *)handle.task;
        pthread_mutex_lock(&task->mutex);
        pgy_cancel_request(task->cancel_node);
        pthread_mutex_unlock(&task->mutex);
        return true;
    }

    return false;
}

static inline bool
pgy_task_is_cancelled(void)
{
    return pgy_cancel_is_requested(pgy_current_cancel_node());
}

/* =================================================================
 * Unified await
 * ================================================================= */

static inline void *
pgy_await(PgyTaskHandle handle)
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    if (header == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "await task handle is null");
    }

#if PGY_COROUTINES_AVAILABLE
    if (header->model == PGY_TASK_MODEL_COROUTINE) {
        PgyCoroTask *task = (PgyCoroTask *)handle.task;
        if (pgy_async_in_coroutine()) {
            while (!task->done) {
                PgyCoroTask *current = g_pgy_coro.current;
                task->waiter = current;
                g_pgy_coro.current = NULL;
#ifdef _WIN32
                SwitchToFiber(g_pgy_coro.scheduler_fiber);
#else
                swapcontext(&current->ctx, &g_pgy_coro.scheduler_ctx);
#endif
                g_pgy_coro.current = current;
            }
        } else {
            pgy_async_progress_until(pgy_async_task_done, task);
            while (!task->done && pgy_async_progress_one()) {
            }
        }

        void *result = task->result;
        pgy_coro_destroy(task);
        return result;
    }
#endif

    PgyTask *task = (PgyTask *)handle.task;
    pthread_mutex_lock(&task->mutex);
    while (task->state != PGY_TASK_DONE) {
        if (pgy_async_in_coroutine()) {
            pthread_mutex_unlock(&task->mutex);
            pgy_async_yield();
            pthread_mutex_lock(&task->mutex);
        } else {
            if (pthread_cond_wait(&task->cond, &task->mutex) != 0) {
                pthread_mutex_unlock(&task->mutex);
                PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                                  "await condition wait failed");
            }
        }
    }
    void *result = task->result;
    pthread_mutex_unlock(&task->mutex);

    pgy_cancel_release(task->cancel_node);
    pthread_mutex_destroy(&task->mutex);
    pthread_cond_destroy(&task->cond);
    free(task);

    return result;
}

#define pgy_await_take(handle, CType) \
    ({ \
        CType *_pgy_result_ptr = (CType *)pgy_await((handle)); \
        if (_pgy_result_ptr == NULL) { \
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                              "Future await returned null result"); \
        } \
        CType _pgy_value = *_pgy_result_ptr; \
        free(_pgy_result_ptr); \
        _pgy_value; \
    })

#define pgy_await_void(handle) \
    ((void)pgy_await((handle)))

#include "runtime/pgy_parallel_run.h"

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_H */
