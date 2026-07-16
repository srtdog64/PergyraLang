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
#ifndef _WIN32
#include <unistd.h>   /* sysconf(_SC_NPROCESSORS_ONLN) for pgy_default_worker_count */
#endif

#include "../common/execution_lane_kind.h"
#include "pgy_runtime_panic_contract.h"
#include "pgy_runtime_cancel_probe.h"

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
    PgyExecutionLane lane;
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

/* Probe for the channel-wait cancellation hook (docs/182 SS2.2): reads
 * the current task's cancel-node chain. Installed at every task
 * creation path, so a channel wait can only ever observe a probe once
 * a task that could be cancelled exists. */
static bool
pgy_parallel_cancel_probe(void)
{
    return pgy_cancel_is_requested(pgy_current_cancel_node());
}

typedef struct PgyTask {
    PgyTaskModel    model;
    PgyExecutionLane lane;
    void *(*fn)(void *);
    void           *arg;
    void           *result;
    /* Atomic so the runner can publish PENDING->RUNNING without taking the
     * task mutex (docs/186 P-B2). DONE is still stored under the mutex,
     * paired with the cond broadcast, so an awaiter that saw !DONE under
     * the mutex cannot miss the wakeup; `result` is ordered by that same
     * mutex plus the release store. */
    _Atomic PgyTaskState state;
    PgyCancelNode  *cancel_node;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    struct PgyTask *next;
} PgyTask;

typedef struct {
    void *task;                 /* PgyTask* or coroutine task header */
} PgyTaskHandle;

static inline PgyExecutionLane
pgy_task_handle_lane(PgyTaskHandle handle)
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    return header != NULL ? header->lane : PGY_LANE_REJECT;
}

static inline void
pgy_task_handle_set_lane(PgyTaskHandle handle, PgyExecutionLane lane)
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    if (header != NULL)
        header->lane = lane;
}

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
                           bool charge_spawn_budget, PgyExecutionLane lane)
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
    task->lane = lane;
    task->fn = fn;
    task->arg = arg;
    if (!pgy_task_sync_init(task, op_name)) {
        free(task);
        return handle;
    }
    pgy_cancel_probe_install(pgy_parallel_cancel_probe);
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    task->result = pgy_cancel_is_requested(task->cancel_node)
        ? NULL
        : fn(arg);
    atomic_store_explicit(&task->state, PGY_TASK_DONE, memory_order_release);
    handle.task = task;
    return handle;
}

/* =================================================================
 * Thread pool runtime for `parallel`
 * ================================================================= */

/* One mutex-protected FIFO shard per worker (docs/186 P-B2). The measured
 * fine-grain cost (~9 of 10.7 us/task, 20k-task micro) was contention on the
 * old SINGLE queue mutex: every spawner, every worker pop and every help
 * attempt serialized on one lock. Sharding by worker count spreads producers
 * round-robin and lets each worker pop its own shard first, stealing from the
 * others only when it runs dry -- same task set, same run protocol, ~1/Nth
 * the contenders per lock. The trailing pad keeps two shards' hot state off
 * one cache line. */
typedef struct {
    pthread_mutex_t mutex;
    PgyTask        *head;
    PgyTask        *tail;
    char            _pad[64];
} PgyTaskShard;

typedef struct PgyThreadPool PgyThreadPool;

typedef struct {
    PgyThreadPool *pool;
    size_t         index;   /* this worker's home shard */
} PgyPoolWorkerSlot;

struct PgyThreadPool {
    pthread_t         *workers;
    PgyPoolWorkerSlot *worker_slots;
    size_t             worker_count;
    PgyTaskShard      *shards;
    size_t             shard_count;
    atomic_size_t      push_cursor;   /* producer round-robin over shards */
    atomic_size_t      steal_cursor;  /* helper sweep start rotation */
    /* Queued-but-not-yet-popped count; the park predicate. Producers bump it
     * AFTER enqueue, sleepers re-check it AFTER registering in `sleepers`
     * (both seq_cst) so the pair cannot miss each other (store-buffer case). */
    atomic_size_t      pending;
    atomic_size_t      sleepers;
    pthread_mutex_t    queue_mutex;   /* park/wake handshake only */
    pthread_cond_t     queue_cond;
    bool               shutdown;      /* written under queue_mutex */
};

static PgyThreadPool g_pgy_pool = {0};
static atomic_bool   g_pgy_pool_active = false;
static atomic_bool   g_pgy_pool_shutting_down = false;
static pthread_mutex_t g_pgy_pool_lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static __thread PgyTask *g_pgy_thread_current = NULL;

/* Forward declaration — blocking pool shutdown called from pgy_pool_shutdown */
static inline void pgy_blocking_pool_shutdown(void);

static inline void
pgy_pool_enqueue(PgyThreadPool *pool, PgyTask *task)
{
    size_t k = atomic_fetch_add_explicit(&pool->push_cursor, 1,
                                         memory_order_relaxed)
               % pool->shard_count;
    PgyTaskShard *shard = &pool->shards[k];

    task->next = NULL;
    pthread_mutex_lock(&shard->mutex);
    if (shard->tail != NULL)
        shard->tail->next = task;
    else
        shard->head = task;
    shard->tail = task;
    pthread_mutex_unlock(&shard->mutex);

    atomic_fetch_add_explicit(&pool->pending, 1, memory_order_seq_cst);
    if (atomic_load_explicit(&pool->sleepers, memory_order_seq_cst) != 0) {
        pthread_mutex_lock(&pool->queue_mutex);
        pthread_cond_signal(&pool->queue_cond);
        pthread_mutex_unlock(&pool->queue_mutex);
    }
}

/* Pop one task, own shard first, then sweep the others (the steal). NULL
 * means every shard was empty at the moment it was inspected. */
static inline PgyTask *
pgy_pool_try_pop(PgyThreadPool *pool, size_t start)
{
    size_t count = pool->shard_count;

    for (size_t i = 0; i < count; i++) {
        PgyTaskShard *shard = &pool->shards[(start + i) % count];
        PgyTask *task;

        pthread_mutex_lock(&shard->mutex);
        task = shard->head;
        if (task != NULL) {
            shard->head = task->next;
            if (shard->head == NULL)
                shard->tail = NULL;
        }
        pthread_mutex_unlock(&shard->mutex);
        if (task != NULL) {
            atomic_fetch_sub_explicit(&pool->pending, 1,
                                      memory_order_relaxed);
            return task;
        }
    }
    return NULL;
}

/* Park until work may be available (pending != 0) or shutdown. Returns the
 * shutdown flag observed under the queue mutex. The sleepers++ BEFORE the
 * pending re-check pairs with the producer's pending++ BEFORE its sleepers
 * check: whichever runs second sees the other side, so no lost wakeup. */
static inline bool
pgy_pool_park_until_signal(PgyThreadPool *pool)
{
    bool shutting;

    pthread_mutex_lock(&pool->queue_mutex);
    for (;;) {
        if (pool->shutdown)
            break;
        atomic_fetch_add_explicit(&pool->sleepers, 1, memory_order_seq_cst);
        if (atomic_load_explicit(&pool->pending, memory_order_seq_cst) != 0) {
            atomic_fetch_sub_explicit(&pool->sleepers, 1,
                                      memory_order_relaxed);
            break;
        }
        if (pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex) != 0) {
            atomic_fetch_sub_explicit(&pool->sleepers, 1,
                                      memory_order_relaxed);
            pgy_parallel_warn("worker-loop",
                              "worker condition wait failed");
            pool->shutdown = true;
            pthread_cond_broadcast(&pool->queue_cond);
            break;
        }
        atomic_fetch_sub_explicit(&pool->sleepers, 1, memory_order_relaxed);
    }
    shutting = pool->shutdown;
    pthread_mutex_unlock(&pool->queue_mutex);
    return shutting;
}

/* The one run protocol, shared by workers and help-first awaiters. The
 * caller's task identity is saved/restored so cancellation scoping follows
 * the task actually running. */
static inline void
pgy_pool_run_task(PgyTask *task)
{
    PgyTask *prev;
    void    *result;

    if (pgy_cancel_is_requested(task->cancel_node)) {
        pthread_mutex_lock(&task->mutex);
        task->result = NULL;
        atomic_store_explicit(&task->state, PGY_TASK_DONE,
                              memory_order_release);
        pthread_cond_broadcast(&task->cond);
        pthread_mutex_unlock(&task->mutex);
        return;
    }
    atomic_store_explicit(&task->state, PGY_TASK_RUNNING,
                          memory_order_relaxed);

    prev = g_pgy_thread_current;
    g_pgy_thread_current = task;
    result = task->fn(task->arg);
    g_pgy_thread_current = prev;

    pthread_mutex_lock(&task->mutex);
    task->result = result;
    atomic_store_explicit(&task->state, PGY_TASK_DONE, memory_order_release);
    pthread_cond_broadcast(&task->cond);
    pthread_mutex_unlock(&task->mutex);
}

/* NOTE (docs/186 P-B2, measured 2026-07-17): a bounded pre-park spin
 * (sched_yield loop so sleepers stays 0 and producers skip the cond_signal
 * futex wake) was tried here and REVERTED -- it showed a 2.2x best case on
 * the 200k fine-grain micro but wild variance (981..2425ms interleaved
 * rounds): sixteen yielding workers preempt the one producing thread this
 * whole workload is bound on. The structural fix for fine-grain fan-out is
 * chunking (P-B3), not park tuning. */
static void *
pgy_worker_loop(void *arg)
{
    PgyPoolWorkerSlot *slot = (PgyPoolWorkerSlot *)arg;
    PgyThreadPool *pool = slot->pool;

    for (;;) {
        PgyTask *task = pgy_pool_try_pop(pool, slot->index);

        if (task == NULL) {
            bool shutting = pgy_pool_park_until_signal(pool);

            task = pgy_pool_try_pop(pool, slot->index);
            if (task == NULL) {
                if (shutting)
                    break;   /* drained: every shard empty at shutdown */
                continue;
            }
        }
        pgy_pool_run_task(task);
    }
    return NULL;
}

/* Allocate shards/slots and start the workers. Caller holds the pool's
 * lifecycle mutex. On failure everything is unwound and *pool is zeroed. */
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

/* Free queued (never-run) tasks, shard mutexes and the worker arrays after
 * the workers have been joined. Caller holds the pool's lifecycle mutex. */
static inline void
pgy_pool_structure_teardown(PgyThreadPool *pool)
{
    for (size_t i = 0; i < pool->shard_count; i++) {
        PgyTask *t = pool->shards[i].head;

        while (t != NULL) {
            PgyTask *next = t->next;

            pgy_cancel_release(t->cancel_node);
            pthread_mutex_destroy(&t->mutex);
            pthread_cond_destroy(&t->cond);
            free(t);
            t = next;
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

/* Default worker count for an auto-sized pool (pgy_pool_init(0)): one worker
 * per hardware thread, so `parallel`/`spawn` uses the whole machine instead of
 * a fixed 4. Both backends funnel through pgy_pool_init (C emits
 * pgy_pool_init(0); LLVM calls pgy_pool_init_export(0)), so this one detector
 * sizes the pool for both. Same idiom as runtime/async/scheduler.c. */
static inline size_t
pgy_default_worker_count(void)
{
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

/* Pop and run ONE queued task on the calling thread. Returns false if the
 * queue was empty. This is the help-first half of await (docs/186 P-A1): a
 * pool worker that awaits a subtask must never park in cond_wait while
 * runnable work is queued -- with every worker parked, subtasks queued behind
 * remaining outer tasks can never run (pool-starvation deadlock; witnessed
 * deterministically by the nested-parallel fixture before this landed).
 * Mirrors pgy_worker_loop's execution protocol exactly, except the caller's
 * task identity is saved and restored so cancellation scoping follows the
 * task actually running. */
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

    (void)workers;   /* joined above; freed by the structure teardown */
    pgy_pool_structure_teardown(&g_pgy_pool);
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
        /* No silent serialization (docs/177 F1, CLAUDE No.1.1): a spawn with
         * no live pool runs inline/serially. Announce it so "my parallel code
         * isn't parallel" is observable instead of a hidden control-flow branch.
         * With the thread-pool surface fact fixed, main() emits pgy_pool_init
         * whenever spawn/parallel is used, so reaching here means pool init was
         * skipped or failed -- a condition worth a warning, not a silent fall. */
        pgy_parallel_warn("spawn",
            "worker pool inactive; task runs inline (serial). "
            "parallel/spawn does not run concurrently without a live pool "
            "(pgy_pool_init).");
        return pgy_spawn_inline_completed(fn, arg, "spawn", false,
                                          PGY_LANE_WORKER_POOL);
    }

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL) {
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        pgy_parallel_warn("spawn", "task allocation failed");
        return handle;
    }

    task->model = PGY_TASK_MODEL_THREAD;
    task->lane = PGY_LANE_WORKER_POOL;
    task->fn = fn;
    task->arg = arg;
    atomic_store_explicit(&task->state, PGY_TASK_PENDING,
                          memory_order_relaxed);
    pgy_cancel_probe_install(pgy_parallel_cancel_probe);
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    if (!pgy_task_sync_init(task, "spawn")) {
        pgy_cancel_release(task->cancel_node);
        free(task);
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        return handle;
    }
    handle.task = task;

    pgy_pool_enqueue(&g_pgy_pool, task);
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);

    return handle;
}

#include "runtime/pgy_parallel_blocking.h"

#if defined(__APPLE__) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "runtime/pgy_parallel_coroutine.h"
#include "runtime/pgy_parallel_task_ops.h"
#if defined(__APPLE__) && defined(__clang__)
#pragma clang diagnostic pop
#endif

#include "runtime/pgy_parallel_run.h"

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_H */
