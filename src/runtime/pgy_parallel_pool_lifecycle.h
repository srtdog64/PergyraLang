#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_POOL_LIFECYCLE_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_POOL_LIFECYCLE_H

/* Spare-worker headroom per pool (WO-RT-5 compensation): up to 4 extra
 * runners per configured worker may be spawned by blocked channel waits.
 * Beyond the cap a blocked wait degrades to the plain quantum park -- that
 * residue is recorded on the board; the fiber lane is its candidate. */
#ifndef PGY_POOL_SPARE_FACTOR
#define PGY_POOL_SPARE_FACTOR 4
#endif

/* Caller holds the pool lifecycle mutex. On failure the pool is zeroed. */
static inline bool
pgy_pool_structure_init(PgyThreadPool *pool, size_t worker_count,
                        const char *op)
{
    size_t slot_headroom = worker_count * (PGY_POOL_SPARE_FACTOR + 1);

    if (slot_headroom / (PGY_POOL_SPARE_FACTOR + 1) != worker_count
        || !pgy_parallel_array_fits(slot_headroom, sizeof(pthread_t))
        || !pgy_parallel_array_fits(worker_count, sizeof(PgyTaskShard))
        || !pgy_parallel_array_fits(slot_headroom, sizeof(PgyPoolWorkerSlot))) {
        pgy_parallel_warn(op, "worker array size overflow");
        return false;
    }

    memset(pool, 0, sizeof(*pool));
    atomic_init(&pool->spare_count, 0);
    atomic_init(&pool->push_cursor, 0);
    atomic_init(&pool->steal_cursor, 0);
    atomic_init(&pool->pending, 0);
    atomic_init(&pool->sleepers, 0);
    pool->worker_count = worker_count;
    pool->max_spares = worker_count * PGY_POOL_SPARE_FACTOR;
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
    /* Headroom for compensation spares so they join the same teardown. */
    pool->workers = (pthread_t *)calloc(slot_headroom, sizeof(pthread_t));
    pool->worker_slots =
        (PgyPoolWorkerSlot *)calloc(slot_headroom, sizeof(PgyPoolWorkerSlot));
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

/* Compensation for UNBOUNDED channel waits (WO-RT-5, ForkJoin managedBlock
 * shape): a pool task parked on a channel occupies its thread, and with
 * every pool thread so parked, a queued task that would unblock them can
 * never run -- the channel edition of the WO-RT-3 starvation class,
 * witnessed RED by tests/channel_pool_starvation_probe.sh. Running a queued
 * task INLINE on the parked thread was tried first and REFUTED by the
 * backpressure gate: channel dependencies are cyclic, so the helped consumer
 * can nest above the parked producer it depends on and self-deadlock the
 * thread. Instead, each park quantum of a blocked pool task checks whether
 * the pool still has an idle runner; if not (and work is queued), it spawns
 * one capacity-bounded spare worker. No nesting, so no cyclic-dependency
 * trap; the 10ms quantum makes it self-healing for work that arrives while
 * everyone is blocked. Beyond the spare cap the wait degrades to the plain
 * quantum park -- that residue is on the board; the fiber lane (docs/187
 * memo 1) remains its candidate. */
static inline void
pgy_pool_spawn_spare_locked(PgyThreadPool *pool)
{
    size_t spare = atomic_load_explicit(&pool->spare_count,
                                        memory_order_relaxed);
    size_t idx;

    if (spare >= pool->max_spares)
        return;
    idx = pool->worker_count + spare;
    pool->worker_slots[idx].pool = pool;
    pool->worker_slots[idx].index = idx % pool->shard_count;
    if (pthread_create(&pool->workers[idx], NULL, pgy_worker_loop,
                       &pool->worker_slots[idx]) != 0) {
        pgy_parallel_warn("channel-blocked",
            "compensation worker creation failed; wait degrades to park");
        return;
    }
    atomic_store_explicit(&pool->spare_count, spare + 1,
                          memory_order_release);
}

static inline void
pgy_pool_channel_blocked_tick(void)
{
    if (g_pgy_pool_task_depth <= 0)
        return;   /* not inside a pool task; pool parallelism unaffected */
    if (!atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire))
        return;
    if (atomic_load_explicit(&g_pgy_pool.sleepers, memory_order_seq_cst) != 0)
        return;   /* an idle runner exists; enqueue signals reach it */
    if (atomic_load_explicit(&g_pgy_pool.pending, memory_order_seq_cst) == 0)
        return;   /* nothing queued for a spare to run */
    if (atomic_load_explicit(&g_pgy_pool.spare_count, memory_order_acquire)
        >= g_pgy_pool.max_spares)
        return;

    pthread_mutex_lock(&g_pgy_pool_lifecycle_mutex);
    if (atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire)
        && !atomic_load_explicit(&g_pgy_pool_shutting_down,
                                 memory_order_acquire))
        pgy_pool_spawn_spare_locked(&g_pgy_pool);
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
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
    /* Spares join the same teardown; no new spare can spawn once the
     * lifecycle flags above are set (the blocked tick re-checks them under
     * this same mutex), so this count is final. */
    size_t worker_count = g_pgy_pool.worker_count
        + atomic_load_explicit(&g_pgy_pool.spare_count, memory_order_acquire);
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
