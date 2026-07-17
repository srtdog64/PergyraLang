#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_POOL_LIFECYCLE_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_POOL_LIFECYCLE_H

/* Caller holds the pool lifecycle mutex. On failure the pool is zeroed. */
static inline bool
pgy_pool_structure_init(PgyThreadPool *pool, size_t worker_count,
                        const char *op)
{
    if (!pgy_parallel_array_fits(worker_count, sizeof(pthread_t))
        || !pgy_parallel_array_fits(worker_count, sizeof(PgyTaskShard))
        || !pgy_parallel_array_fits(worker_count, sizeof(PgyPoolWorkerSlot))) {
        pgy_parallel_warn(op, "worker array size overflow");
        return false;
    }

    memset(pool, 0, sizeof(*pool));
    pool->worker_count = worker_count;
    pool->shard_count = worker_count;
    if (pthread_mutex_init(&pool->queue_mutex, NULL) != 0) {
        pgy_parallel_warn(op, "queue mutex initialization failed");
        memset(pool, 0, sizeof(*pool));
        return false;
    }
    if (pthread_cond_init(&pool->queue_cond, NULL) != 0) {
        pgy_parallel_warn(op, "queue condition initialization failed");
        pthread_mutex_destroy(&pool->queue_mutex);
        memset(pool, 0, sizeof(*pool));
        return false;
    }

    pool->shards = (PgyTaskShard *)calloc(worker_count, sizeof(PgyTaskShard));
    pool->workers = (pthread_t *)calloc(worker_count, sizeof(pthread_t));
    pool->worker_slots =
        (PgyPoolWorkerSlot *)calloc(worker_count, sizeof(PgyPoolWorkerSlot));
    if (pool->shards == NULL || pool->workers == NULL
        || pool->worker_slots == NULL) {
        pgy_parallel_warn(op, "worker array allocation failed");
        free(pool->shards);
        free(pool->workers);
        free(pool->worker_slots);
        pthread_mutex_destroy(&pool->queue_mutex);
        pthread_cond_destroy(&pool->queue_cond);
        memset(pool, 0, sizeof(*pool));
        return false;
    }
    for (size_t i = 0; i < worker_count; i++) {
        if (pthread_mutex_init(&pool->shards[i].mutex, NULL) != 0) {
            pgy_parallel_warn(op, "shard mutex initialization failed");
            for (size_t j = 0; j < i; j++)
                pthread_mutex_destroy(&pool->shards[j].mutex);
            free(pool->shards);
            free(pool->workers);
            free(pool->worker_slots);
            pthread_mutex_destroy(&pool->queue_mutex);
            pthread_cond_destroy(&pool->queue_cond);
            memset(pool, 0, sizeof(*pool));
            return false;
        }
    }

    for (size_t i = 0; i < worker_count; i++) {
        pool->worker_slots[i].pool = pool;
        pool->worker_slots[i].index = i;
        if (pthread_create(&pool->workers[i], NULL, pgy_worker_loop,
                           &pool->worker_slots[i]) != 0) {
            pgy_parallel_warn(op, "worker thread creation failed");
            pthread_mutex_lock(&pool->queue_mutex);
            pool->shutdown = true;
            pthread_cond_broadcast(&pool->queue_cond);
            pthread_mutex_unlock(&pool->queue_mutex);
            for (size_t j = 0; j < i; j++)
                pthread_join(pool->workers[j], NULL);
            for (size_t j = 0; j < worker_count; j++)
                pthread_mutex_destroy(&pool->shards[j].mutex);
            free(pool->shards);
            free(pool->workers);
            free(pool->worker_slots);
            pthread_mutex_destroy(&pool->queue_mutex);
            pthread_cond_destroy(&pool->queue_cond);
            memset(pool, 0, sizeof(*pool));
            return false;
        }
    }
    return true;
}

/* Workers are already joined; release queued tasks and pool structure. */
static inline void
pgy_pool_structure_teardown(PgyThreadPool *pool)
{
    for (size_t i = 0; i < pool->shard_count; i++) {
        PgyTask *task = pool->shards[i].head;

        while (task != NULL) {
            PgyTask *next = task->next;

            pgy_cancel_release(task->cancel_node);
            pthread_mutex_destroy(&task->mutex);
            pthread_cond_destroy(&task->cond);
            free(task);
            task = next;
        }
        pthread_mutex_destroy(&pool->shards[i].mutex);
    }
    free(pool->shards);
    free(pool->workers);
    free(pool->worker_slots);
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    memset(pool, 0, sizeof(*pool));
}

/* Default worker count for an auto-sized pool. PGY_WORKERS pins both pool and
 * chunk sizing so worker-invariance tests remain reproducible. */
static inline size_t
pgy_default_worker_count(void)
{
    const char *env = getenv("PGY_WORKERS");

    if (env != NULL && env[0] != '\0') {
        char *end = NULL;
        unsigned long v = strtoul(env, &end, 10);

        if (end != NULL && *end == '\0' && v >= 1 && v <= 4096)
            return (size_t)v;
        pgy_parallel_warn("pool-init",
            "PGY_WORKERS is not a valid worker count (want 1..4096); "
            "using the hardware default");
    }
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    long n = (long)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    if (n < 1)
        n = 1;
    return (size_t)n;
}

/* Help-first await: run one queued task instead of parking a worker while
 * runnable subtasks remain. */
static inline bool
pgy_pool_help_run_one(void)
{
    PgyTask *task;

    if (!atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire))
        return false;

    task = pgy_pool_try_pop(&g_pgy_pool,
        atomic_fetch_add_explicit(&g_pgy_pool.steal_cursor, 1,
                                  memory_order_relaxed));
    if (task == NULL)
        return false;

    pgy_pool_run_task(task);
    return true;
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
        worker_count = pgy_default_worker_count();
    if (!pgy_pool_structure_init(&g_pgy_pool, worker_count, "pool-init")) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return;
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
    (void)workers;
    pgy_pool_structure_teardown(&g_pgy_pool);
    atomic_store_explicit(&g_pgy_pool_shutting_down, false,
                          memory_order_release);
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
    pgy_blocking_pool_shutdown();
}

#endif
