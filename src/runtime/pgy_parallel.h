/*
 * Copyright (c) 2025 Pergyra Language Project
 * Real concurrency runtime — pthread-based thread pool
 * BSD 3-Clause License
 */

#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* =================================================================
 * Task & Future
 * ================================================================= */

typedef enum {
    PGY_TASK_PENDING,
    PGY_TASK_RUNNING,
    PGY_TASK_DONE
} PgyTaskState;

typedef struct PgyTask {
    void *(*fn)(void *);
    void           *arg;
    void           *result;
    PgyTaskState    state;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    struct PgyTask *next;          /* intrusive queue link */
} PgyTask;

typedef struct {
    PgyTask *task;                 /* opaque handle to awaitable task */
} PgyTaskHandle;

/* =================================================================
 * Thread Pool
 * ================================================================= */

typedef struct {
    pthread_t      *workers;
    size_t          worker_count;

    /* Lock-protected task queue */
    PgyTask        *queue_head;
    PgyTask        *queue_tail;
    pthread_mutex_t queue_mutex;
    pthread_cond_t  queue_cond;

    bool            shutdown;
} PgyThreadPool;

/* Global singleton pool */
static PgyThreadPool g_pgy_pool = {0};
static bool          g_pgy_pool_active = false;

/* Worker thread main loop */
static void *
pgy_worker_loop(void *arg)
{
    PgyThreadPool *pool = (PgyThreadPool *)arg;

    for (;;) {
        pthread_mutex_lock(&pool->queue_mutex);

        /* Wait for work or shutdown */
        while (pool->queue_head == NULL && !pool->shutdown)
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);

        if (pool->shutdown && pool->queue_head == NULL) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        /* Dequeue task */
        PgyTask *task = pool->queue_head;
        if (task != NULL) {
            pool->queue_head = task->next;
            if (pool->queue_head == NULL)
                pool->queue_tail = NULL;
        }

        pthread_mutex_unlock(&pool->queue_mutex);

        if (task == NULL)
            continue;

        /* Execute */
        pthread_mutex_lock(&task->mutex);
        task->state = PGY_TASK_RUNNING;
        pthread_mutex_unlock(&task->mutex);

        void *result = task->fn(task->arg);

        pthread_mutex_lock(&task->mutex);
        task->result = result;
        task->state  = PGY_TASK_DONE;
        pthread_cond_broadcast(&task->cond);
        pthread_mutex_unlock(&task->mutex);
    }

    return NULL;
}

/* Initialize thread pool with N workers */
static inline void
pgy_pool_init(size_t worker_count)
{
    if (g_pgy_pool_active)
        return;

    if (worker_count == 0)
        worker_count = 4;         /* sensible default */

    memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
    g_pgy_pool.worker_count = worker_count;
    g_pgy_pool.shutdown     = false;
    g_pgy_pool.queue_head   = NULL;
    g_pgy_pool.queue_tail   = NULL;
    pthread_mutex_init(&g_pgy_pool.queue_mutex, NULL);
    pthread_cond_init(&g_pgy_pool.queue_cond, NULL);

    g_pgy_pool.workers = (pthread_t *)calloc(worker_count, sizeof(pthread_t));
    for (size_t i = 0; i < worker_count; i++)
        pthread_create(&g_pgy_pool.workers[i], NULL,
                       pgy_worker_loop, &g_pgy_pool);

    g_pgy_pool_active = true;
}

/* Shutdown pool, wait for all workers to finish */
static inline void
pgy_pool_shutdown(void)
{
    if (!g_pgy_pool_active)
        return;

    pthread_mutex_lock(&g_pgy_pool.queue_mutex);
    g_pgy_pool.shutdown = true;
    pthread_cond_broadcast(&g_pgy_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_pool.queue_mutex);

    for (size_t i = 0; i < g_pgy_pool.worker_count; i++)
        pthread_join(g_pgy_pool.workers[i], NULL);

    free(g_pgy_pool.workers);
    pthread_mutex_destroy(&g_pgy_pool.queue_mutex);
    pthread_cond_destroy(&g_pgy_pool.queue_cond);

    /* Free any remaining tasks (shouldn't be any) */
    PgyTask *t = g_pgy_pool.queue_head;
    while (t) {
        PgyTask *next = t->next;
        pthread_mutex_destroy(&t->mutex);
        pthread_cond_destroy(&t->cond);
        free(t);
        t = next;
    }

    g_pgy_pool_active = false;
}

/* Submit a task to the pool, returns a handle for await */
static inline PgyTaskHandle
pgy_spawn(void *(*fn)(void *), void *arg)
{
    PgyTaskHandle handle = {0};

    /* Fallback to synchronous if pool not active */
    if (!g_pgy_pool_active) {
        PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
        task->fn     = fn;
        task->arg    = arg;
        task->result = fn ? fn(arg) : NULL;
        task->state  = PGY_TASK_DONE;
        pthread_mutex_init(&task->mutex, NULL);
        pthread_cond_init(&task->cond, NULL);
        task->next   = NULL;
        handle.task  = task;
        return handle;
    }

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    task->fn    = fn;
    task->arg   = arg;
    task->state = PGY_TASK_PENDING;
    task->next  = NULL;
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->cond, NULL);

    handle.task = task;

    /* Enqueue */
    pthread_mutex_lock(&g_pgy_pool.queue_mutex);
    if (g_pgy_pool.queue_tail != NULL)
        g_pgy_pool.queue_tail->next = task;
    else
        g_pgy_pool.queue_head = task;
    g_pgy_pool.queue_tail = task;
    pthread_cond_signal(&g_pgy_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_pool.queue_mutex);

    return handle;
}

/* Wait for a spawned task to complete, returns its result */
static inline void *
pgy_await(PgyTaskHandle handle)
{
    PgyTask *task = handle.task;
    if (task == NULL)
        return NULL;

    pthread_mutex_lock(&task->mutex);
    while (task->state != PGY_TASK_DONE)
        pthread_cond_wait(&task->cond, &task->mutex);
    void *result = task->result;
    pthread_mutex_unlock(&task->mutex);

    pthread_mutex_destroy(&task->mutex);
    pthread_cond_destroy(&task->cond);
    free(task);

    return result;
}

#define pgy_await_take(handle, CType) \
    ({ \
        CType *_pgy_result_ptr = (CType *)pgy_await((handle)); \
        CType _pgy_value = _pgy_result_ptr != NULL ? *_pgy_result_ptr : (CType)0; \
        free(_pgy_result_ptr); \
        _pgy_value; \
    })

#define pgy_await_void(handle) \
    ((void)pgy_await((handle)))

/* =================================================================
 * Parallel block helpers
 *
 * PGY_PARALLEL_BEGIN / PGY_PARALLEL_TASK / PGY_PARALLEL_END
 * are defined in pgy_runtime.h using OpenMP or sequential fallback.
 *
 * Here we provide a thread-pool-based alternative that the compiler
 * can target directly.
 * ================================================================= */

/* Wrapper for void-returning parallel tasks */
typedef struct {
    void (*fn)(void);
} PgyParallelArg;

static void *
pgy_parallel_wrapper(void *raw)
{
    PgyParallelArg *parg = (PgyParallelArg *)raw;
    if (parg && parg->fn)
        parg->fn();
    return NULL;
}

/* Run N tasks in parallel on the thread pool, blocking until all done */
static inline void
pgy_parallel_run(void (**tasks)(void), size_t count)
{
    if (count == 0)
        return;

    if (count == 1) {
        /* Single task: just run directly */
        if (tasks[0]) tasks[0]();
        return;
    }

    PgyTaskHandle *handles = (PgyTaskHandle *)calloc(count, sizeof(PgyTaskHandle));
    PgyParallelArg *args   = (PgyParallelArg *)calloc(count, sizeof(PgyParallelArg));

    for (size_t i = 0; i < count; i++) {
        args[i].fn = tasks[i];
        handles[i] = pgy_spawn(pgy_parallel_wrapper, &args[i]);
    }

    /* Await all */
    for (size_t i = 0; i < count; i++)
        pgy_await(handles[i]);

    free(handles);
    free(args);
}

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_H */
