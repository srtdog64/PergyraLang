#include "pgy_runtime_linkage.h"
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
#include <unistd.h>
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

PGY_RT_DECL PgyCancelNode *
pgy_current_cancel_node(void);

#include "runtime/pgy_parallel_task_foundation.h"

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

PGY_RT_DECL PgyExecutionLane
pgy_task_handle_lane(PgyTaskHandle handle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    return header != NULL ? header->lane : PGY_LANE_REJECT;
}
#else
;
#endif


PGY_RT_DECL void
pgy_task_handle_set_lane(PgyTaskHandle handle, PgyExecutionLane lane)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    if (header != NULL)
        header->lane = lane;
}
#else
;
#endif


PGY_RT_DECL bool
pgy_task_sync_init(PgyTask *task, const char *op)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL PgyTaskHandle
pgy_spawn_inline_completed(void *(*fn)(void *), void *arg, const char *op,
                           bool charge_spawn_budget, PgyExecutionLane lane)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


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
    /* Compensation workers (WO-RT-5, the ForkJoin managedBlock idea): when a
     * pool task parks in an UNBOUNDED channel wait it occupies its thread,
     * and with every thread so parked a queued task that would unblock them
     * can never run. Running that task inline on the parked thread was tried
     * and REFUTED (producer/consumer channel deps are cyclic: the helped
     * consumer nests above the parked producer it depends on -- witnessed by
     * the backpressure gate). Instead the blocked thread spawns one spare
     * worker, capacity-bounded below, so the pool keeps draining without
     * stack nesting. Spares run the normal worker loop, park when idle, and
     * retire at shutdown. workers/worker_slots are allocated with headroom
     * for the cap so spares join the same teardown path. */
    atomic_size_t      spare_count;   /* spawned spares (<= max_spares) */
    size_t             max_spares;
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
    /* Per-pool lifecycle wiring so pool-generic code (the channel-blocked
     * compensation tick) can act on the pool the CURRENT thread serves
     * without naming pool globals that are defined later in the include
     * chain (docs/189 C13-④). Each pool's init fills these right after
     * pgy_pool_structure_init; NULL means "not wired" and the tick
     * refuses rather than guessing. */
    pthread_mutex_t   *lifecycle_mutex;
    atomic_bool       *active_flag;
    atomic_bool       *shutting_flag;
};

PGY_RT_GLOBAL PgyThreadPool g_pgy_pool
#ifndef PGY_RUNTIME_DECLS_ONLY
    = {0}
#endif
;
PGY_RT_GLOBAL atomic_bool   g_pgy_pool_active
#ifndef PGY_RUNTIME_DECLS_ONLY
    = false
#endif
;
PGY_RT_GLOBAL atomic_bool   g_pgy_pool_shutting_down
#ifndef PGY_RUNTIME_DECLS_ONLY
    = false
#endif
;
PGY_RT_GLOBAL pthread_mutex_t g_pgy_pool_lifecycle_mutex
#ifndef PGY_RUNTIME_DECLS_ONLY
    = PTHREAD_MUTEX_INITIALIZER
#endif
;
PGY_RT_GLOBAL __thread PgyTask *g_pgy_thread_current
#ifndef PGY_RUNTIME_DECLS_ONLY
    = NULL
#endif
;
/* How many pool tasks this thread is currently nested inside (a worker's
 * top-level run counts 1; every help-run adds 1). A blocked channel wait
 * compensates (spawns a spare worker) only when this thread is actually
 * inside a pool task -- main blocking outside any task does not shrink the
 * pool's parallelism, so it does not compensate. */
PGY_RT_GLOBAL __thread int g_pgy_pool_task_depth
#ifndef PGY_RUNTIME_DECLS_ONLY
    = 0
#endif
;
/* Which pool this thread is a worker of (NULL on non-worker threads, e.g.
 * main). The compensation tick used to hardcode g_pgy_pool, so a
 * BLOCKING-pool worker parked on a channel spawned a useless spare in the
 * MAIN pool while its own pool stayed starved (docs/189 C13-④); resolving
 * the pool through this stamp routes compensation to the pool that is
 * actually blocked. Main-thread help runs main-pool tasks, so NULL
 * resolves to the main pool. */
PGY_RT_GLOBAL __thread PgyThreadPool *g_pgy_thread_pool
#ifndef PGY_RUNTIME_DECLS_ONLY
    = NULL
#endif
;

/* Forward declaration — blocking pool shutdown called from pgy_pool_shutdown */
PGY_RT_DECL void pgy_blocking_pool_shutdown(void);

PGY_RT_DECL void
pgy_pool_enqueue(PgyThreadPool *pool, PgyTask *task)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


/* Pop one task, own shard first, then sweep the others (the steal). NULL
 * means every shard was empty at the moment it was inspected. */
PGY_RT_DECL PgyTask *
pgy_pool_try_pop(PgyThreadPool *pool, size_t start)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


/* Park until work may be available (pending != 0) or shutdown. Returns the
 * shutdown flag observed under the queue mutex. The sleepers++ BEFORE the
 * pending re-check pairs with the producer's pending++ BEFORE its sleepers
 * check: whichever runs second sees the other side, so no lost wakeup. */
PGY_RT_DECL bool
pgy_pool_park_until_signal(PgyThreadPool *pool)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


/* The one run protocol, shared by workers and help-first awaiters. The
 * caller's task identity is saved/restored so cancellation scoping follows
 * the task actually running. */
PGY_RT_DECL void
pgy_pool_run_task(PgyTask *task)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
    g_pgy_pool_task_depth++;
    result = task->fn(task->arg);
    g_pgy_pool_task_depth--;
    g_pgy_thread_current = prev;

    pthread_mutex_lock(&task->mutex);
    task->result = result;
    atomic_store_explicit(&task->state, PGY_TASK_DONE, memory_order_release);
    pthread_cond_broadcast(&task->cond);
    pthread_mutex_unlock(&task->mutex);
}
#else
;
#endif


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

    g_pgy_thread_pool = pool;
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

#include "runtime/pgy_parallel_pool_lifecycle.h"

PGY_RT_DECL PgyTaskHandle
pgy_spawn(void *(*fn)(void *), void *arg)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


#include "runtime/pgy_parallel_chunk.h"

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
