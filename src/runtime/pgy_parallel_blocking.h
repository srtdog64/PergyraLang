/*
 * Copyright (c) 2025 Pergyra Language Project
 * Blocking pool runtime for Pergyra parallel tasks.
 * BSD 3-Clause License
 */

#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_BLOCKING_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_BLOCKING_H

/* =================================================================
 * Blocking pool - dedicated thread pool for FFI / system calls
 *
 * Fiber workers must never block on OS-level I/O; doing so stalls
 * every fiber mapped to that worker. spawn_blocking() offloads the
 * work to a separate, expandable thread pool so the fiber scheduler
 * stays responsive.
 * ================================================================= */

static PgyThreadPool g_pgy_blocking_pool = {0};
static bool          g_pgy_blocking_pool_active = false;

static inline void
pgy_blocking_pool_init(size_t worker_count)
{
    if (g_pgy_blocking_pool_active)
        return;

    if (worker_count == 0)
        worker_count = 4;  /* sensible default for I/O-bound work */

    memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
    g_pgy_blocking_pool.worker_count = worker_count;
    pthread_mutex_init(&g_pgy_blocking_pool.queue_mutex, NULL);
    pthread_cond_init(&g_pgy_blocking_pool.queue_cond, NULL);

    g_pgy_blocking_pool.workers =
        (pthread_t *)calloc(worker_count, sizeof(pthread_t));
    if (g_pgy_blocking_pool.workers == NULL) {
        pgy_parallel_warn("blocking-pool-init", "worker array allocation failed");
        pthread_mutex_destroy(&g_pgy_blocking_pool.queue_mutex);
        pthread_cond_destroy(&g_pgy_blocking_pool.queue_cond);
        memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
        return;
    }
    for (size_t i = 0; i < worker_count; i++) {
        if (pthread_create(&g_pgy_blocking_pool.workers[i], NULL,
                           pgy_worker_loop, &g_pgy_blocking_pool) != 0) {
            pgy_parallel_warn("blocking-pool-init", "worker thread creation failed");
            g_pgy_blocking_pool.shutdown = true;
            pthread_cond_broadcast(&g_pgy_blocking_pool.queue_cond);
            for (size_t j = 0; j < i; j++)
                pthread_join(g_pgy_blocking_pool.workers[j], NULL);
            free(g_pgy_blocking_pool.workers);
            pthread_mutex_destroy(&g_pgy_blocking_pool.queue_mutex);
            pthread_cond_destroy(&g_pgy_blocking_pool.queue_cond);
            memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
            return;
        }
    }

    g_pgy_blocking_pool_active = true;
}

static inline void
pgy_blocking_pool_shutdown(void)
{
    if (!g_pgy_blocking_pool_active)
        return;

    pthread_mutex_lock(&g_pgy_blocking_pool.queue_mutex);
    g_pgy_blocking_pool.shutdown = true;
    pthread_cond_broadcast(&g_pgy_blocking_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_blocking_pool.queue_mutex);

    for (size_t i = 0; i < g_pgy_blocking_pool.worker_count; i++)
        pthread_join(g_pgy_blocking_pool.workers[i], NULL);

    free(g_pgy_blocking_pool.workers);
    pthread_mutex_destroy(&g_pgy_blocking_pool.queue_mutex);
    pthread_cond_destroy(&g_pgy_blocking_pool.queue_cond);

    PgyTask *t = g_pgy_blocking_pool.queue_head;
    while (t != NULL) {
        PgyTask *next = t->next;
        pgy_cancel_release(t->cancel_node);
        pthread_mutex_destroy(&t->mutex);
        pthread_cond_destroy(&t->cond);
        free(t);
        t = next;
    }

    memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
    g_pgy_blocking_pool_active = false;
}

/* Offload a blocking function (FFI, system call) to the blocking pool.
 * Returns a Future<T> that can be awaited from fiber context without
 * stalling the fiber scheduler. */
static inline PgyTaskHandle
pgy_spawn_blocking(void *(*fn)(void *), void *arg)
{
    PgyTaskHandle handle = {0};

    if (fn == NULL) {
        pgy_parallel_warn("spawn-blocking", "task function is null");
        return handle;
    }

    /* Lazy-init blocking pool on first use */
    if (!g_pgy_blocking_pool_active)
        pgy_blocking_pool_init(0);
    if (!g_pgy_blocking_pool_active) {
        pgy_parallel_warn("spawn-blocking", "blocking pool initialization failed");
        return handle;
    }

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL) {
        pgy_parallel_warn("spawn-blocking", "task allocation failed");
        return handle;
    }

    task->model = PGY_TASK_MODEL_THREAD;
    task->fn = fn;
    task->arg = arg;
    task->state = PGY_TASK_PENDING;
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    if (task->cancel_node == NULL)
        pgy_parallel_warn("spawn-blocking", "cancellation disabled because cancel node allocation failed");
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->cond, NULL);
    handle.task = task;

    pthread_mutex_lock(&g_pgy_blocking_pool.queue_mutex);
    if (g_pgy_blocking_pool.queue_tail != NULL)
        g_pgy_blocking_pool.queue_tail->next = task;
    else
        g_pgy_blocking_pool.queue_head = task;
    g_pgy_blocking_pool.queue_tail = task;
    pthread_cond_signal(&g_pgy_blocking_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_blocking_pool.queue_mutex);

    return handle;
}

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_BLOCKING_H */
