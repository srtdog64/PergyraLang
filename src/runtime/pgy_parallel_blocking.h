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
static atomic_bool   g_pgy_blocking_pool_active = false;
static atomic_bool   g_pgy_blocking_pool_shutting_down = false;
static pthread_mutex_t g_pgy_blocking_pool_lifecycle_mutex =
    PTHREAD_MUTEX_INITIALIZER;

static inline void
pgy_blocking_pool_init(size_t worker_count)
{
    pthread_mutex_lock(&g_pgy_blocking_pool_lifecycle_mutex);
    if (atomic_load_explicit(&g_pgy_blocking_pool_active,
                             memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        return;
    }
    if (atomic_load_explicit(&g_pgy_blocking_pool_shutting_down,
                             memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        return;
    }

    if (worker_count == 0)
        worker_count = 4;  /* sensible default for I/O-bound work */
    if (!pgy_parallel_array_fits(worker_count, sizeof(pthread_t))) {
        pgy_parallel_warn("blocking-pool-init", "worker array size overflow");
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        return;
    }

    memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
    g_pgy_blocking_pool.worker_count = worker_count;
    if (pthread_mutex_init(&g_pgy_blocking_pool.queue_mutex, NULL) != 0) {
        pgy_parallel_warn("blocking-pool-init",
                          "queue mutex initialization failed");
        memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        return;
    }
    if (pthread_cond_init(&g_pgy_blocking_pool.queue_cond, NULL) != 0) {
        pgy_parallel_warn("blocking-pool-init",
                          "queue condition initialization failed");
        pthread_mutex_destroy(&g_pgy_blocking_pool.queue_mutex);
        memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        return;
    }

    g_pgy_blocking_pool.workers =
        (pthread_t *)calloc(worker_count, sizeof(pthread_t));
    if (g_pgy_blocking_pool.workers == NULL) {
        pgy_parallel_warn("blocking-pool-init", "worker array allocation failed");
        pthread_mutex_destroy(&g_pgy_blocking_pool.queue_mutex);
        pthread_cond_destroy(&g_pgy_blocking_pool.queue_cond);
        memset(&g_pgy_blocking_pool, 0, sizeof(g_pgy_blocking_pool));
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
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
            pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
            return;
        }
    }

    atomic_store_explicit(&g_pgy_blocking_pool_active, true,
                          memory_order_release);
    pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
}

static inline void
pgy_blocking_pool_shutdown(void)
{
    pthread_mutex_lock(&g_pgy_blocking_pool_lifecycle_mutex);
    if (!atomic_load_explicit(&g_pgy_blocking_pool_active,
                              memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        return;
    }
    atomic_store_explicit(&g_pgy_blocking_pool_active, false,
                          memory_order_release);
    atomic_store_explicit(&g_pgy_blocking_pool_shutting_down, true,
                          memory_order_release);

    pthread_mutex_lock(&g_pgy_blocking_pool.queue_mutex);
    g_pgy_blocking_pool.shutdown = true;
    pthread_cond_broadcast(&g_pgy_blocking_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_blocking_pool.queue_mutex);

    pthread_t *workers = g_pgy_blocking_pool.workers;
    size_t worker_count = g_pgy_blocking_pool.worker_count;
    pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);

    for (size_t i = 0; i < worker_count; i++)
        pthread_join(workers[i], NULL);

    pthread_mutex_lock(&g_pgy_blocking_pool_lifecycle_mutex);

    free(workers);
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
    atomic_store_explicit(&g_pgy_blocking_pool_shutting_down, false,
                          memory_order_release);
    pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
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
    if (atomic_load_explicit(&g_pgy_blocking_pool_shutting_down,
                             memory_order_acquire)) {
        pgy_parallel_warn("spawn-blocking", "blocking pool is shutting down");
        return handle;
    }

    /* Same SPAWN_COUNT budget charge as pgy_spawn -- blocking tasks count toward
     * the same fork-bomb ceiling (see pgy_parallel.h). */
    if (pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");

    /* Lazy-init blocking pool on first use */
    if (!atomic_load_explicit(&g_pgy_blocking_pool_active,
                              memory_order_acquire))
        pgy_blocking_pool_init(0);
    pthread_mutex_lock(&g_pgy_blocking_pool_lifecycle_mutex);
    if (atomic_load_explicit(&g_pgy_blocking_pool_shutting_down,
                             memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        pgy_parallel_warn("spawn-blocking", "blocking pool is shutting down");
        return handle;
    }
    if (!atomic_load_explicit(&g_pgy_blocking_pool_active,
                              memory_order_acquire)) {
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        pgy_parallel_warn("spawn-blocking", "blocking pool initialization failed");
        return handle;
    }

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL) {
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        pgy_parallel_warn("spawn-blocking", "task allocation failed");
        return handle;
    }

    task->model = PGY_TASK_MODEL_THREAD;
    task->lane = PGY_LANE_BLOCKING_POOL;
    task->fn = fn;
    task->arg = arg;
    task->state = PGY_TASK_PENDING;
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    if (!pgy_task_sync_init(task, "spawn-blocking")) {
        pgy_cancel_release(task->cancel_node);
        free(task);
        pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);
        return handle;
    }
    handle.task = task;

    pthread_mutex_lock(&g_pgy_blocking_pool.queue_mutex);
    if (g_pgy_blocking_pool.queue_tail != NULL)
        g_pgy_blocking_pool.queue_tail->next = task;
    else
        g_pgy_blocking_pool.queue_head = task;
    g_pgy_blocking_pool.queue_tail = task;
    pthread_cond_signal(&g_pgy_blocking_pool.queue_cond);
    pthread_mutex_unlock(&g_pgy_blocking_pool.queue_mutex);
    pthread_mutex_unlock(&g_pgy_blocking_pool_lifecycle_mutex);

    return handle;
}

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_BLOCKING_H */
