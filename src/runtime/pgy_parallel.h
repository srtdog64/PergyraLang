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
#ifdef _WIN32
#include <windows.h>
#else
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
pgy_cancel_retain(PgyCancelNode *node)
{
    if (node != NULL)
        (void)atomic_fetch_add_explicit(&node->refcount, 1, memory_order_relaxed);
}

static inline PgyCancelNode *
pgy_cancel_node_create(PgyCancelNode *parent)
{
    PgyCancelNode *node = (PgyCancelNode *)calloc(1, sizeof(PgyCancelNode));
    if (node == NULL)
        return NULL;
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
static bool          g_pgy_pool_active = false;
static __thread PgyTask *g_pgy_thread_current = NULL;

/* Forward declaration — blocking pool shutdown called from pgy_pool_shutdown */
static inline void pgy_blocking_pool_shutdown(void);

static void *
pgy_worker_loop(void *arg)
{
    PgyThreadPool *pool = (PgyThreadPool *)arg;

    for (;;) {
        pthread_mutex_lock(&pool->queue_mutex);
        while (pool->queue_head == NULL && !pool->shutdown)
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);

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
    if (g_pgy_pool_active)
        return;

    if (worker_count == 0)
        worker_count = 4;

    memset(&g_pgy_pool, 0, sizeof(g_pgy_pool));
    g_pgy_pool.worker_count = worker_count;
    pthread_mutex_init(&g_pgy_pool.queue_mutex, NULL);
    pthread_cond_init(&g_pgy_pool.queue_cond, NULL);

    g_pgy_pool.workers = (pthread_t *)calloc(worker_count, sizeof(pthread_t));
    for (size_t i = 0; i < worker_count; i++)
        pthread_create(&g_pgy_pool.workers[i], NULL,
                       pgy_worker_loop, &g_pgy_pool);

    g_pgy_pool_active = true;
}

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
    g_pgy_pool_active = false;

    /* Also shut down the blocking pool if it was started */
    pgy_blocking_pool_shutdown();
}

static inline PgyTaskHandle
pgy_spawn(void *(*fn)(void *), void *arg)
{
    PgyTaskHandle handle = {0};

    if (!g_pgy_pool_active) {
        PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
        if (task == NULL)
            return handle;
        task->model = PGY_TASK_MODEL_THREAD;
        task->fn = fn;
        task->arg = arg;
        task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
        task->result = pgy_cancel_is_requested(task->cancel_node)
            ? NULL
            : (fn != NULL ? fn(arg) : NULL);
        task->state = PGY_TASK_DONE;
        pthread_mutex_init(&task->mutex, NULL);
        pthread_cond_init(&task->cond, NULL);
        handle.task = task;
        return handle;
    }

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL)
        return handle;

    task->model = PGY_TASK_MODEL_THREAD;
    task->fn = fn;
    task->arg = arg;
    task->state = PGY_TASK_PENDING;
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    pthread_mutex_init(&task->mutex, NULL);
    pthread_cond_init(&task->cond, NULL);
    handle.task = task;

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

/* =================================================================
 * Blocking pool — dedicated thread pool for FFI / system calls
 *
 * Fiber workers must never block on OS-level I/O; doing so stalls
 * every fiber mapped to that worker.  spawn_blocking() offloads the
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
    for (size_t i = 0; i < worker_count; i++)
        pthread_create(&g_pgy_blocking_pool.workers[i], NULL,
                       pgy_worker_loop, &g_pgy_blocking_pool);

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

    /* Lazy-init blocking pool on first use */
    if (!g_pgy_blocking_pool_active)
        pgy_blocking_pool_init(0);

    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL)
        return handle;

    task->model = PGY_TASK_MODEL_THREAD;
    task->fn = fn;
    task->arg = arg;
    task->state = PGY_TASK_PENDING;
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
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

/* =================================================================
 * Cooperative coroutine runtime for `spawn/await/async`
 * ================================================================= */

#define PGY_COROUTINES_AVAILABLE 1

#if PGY_COROUTINES_AVAILABLE

#define PGY_CORO_STACK_SIZE (1024 * 128)

typedef struct PgyCoroTask {
    PgyTaskModel           model;
    void *(*fn)(void *);
    void                  *arg;
    void                  *result;
    bool                   done;
    bool                   detached;
    bool                   queued;
    PgyCancelNode         *cancel_node;
    struct PgyCoroTask    *next;
    struct PgyCoroTask    *waiter;
#ifdef _WIN32
    LPVOID                 fiber;
#else
    ucontext_t             ctx;
    void                  *stack;
    size_t                 stack_size;
#endif
} PgyCoroTask;

typedef struct {
#ifdef _WIN32
    LPVOID        scheduler_fiber;
    bool          scheduler_ready;
#else
    ucontext_t    scheduler_ctx;
#endif
    PgyCoroTask  *current;
    PgyCoroTask  *ready_head;
    PgyCoroTask  *ready_tail;
} PgyCoroRuntime;

static __thread PgyCoroRuntime g_pgy_coro = {0};

static inline PgyCancelNode *
pgy_current_cancel_node(void)
{
#if PGY_COROUTINES_AVAILABLE
    if (g_pgy_coro.current != NULL)
        return g_pgy_coro.current->cancel_node;
#endif
    return g_pgy_thread_current != NULL ? g_pgy_thread_current->cancel_node : NULL;
}

static inline void
pgy_coro_enqueue(PgyCoroTask *task)
{
    if (task == NULL || task->done || task->queued)
        return;

    task->next = NULL;
    task->queued = true;
    if (g_pgy_coro.ready_tail != NULL)
        g_pgy_coro.ready_tail->next = task;
    else
        g_pgy_coro.ready_head = task;
    g_pgy_coro.ready_tail = task;
}

static inline PgyCoroTask *
pgy_coro_dequeue(void)
{
    PgyCoroTask *task = g_pgy_coro.ready_head;
    if (task == NULL)
        return NULL;
    g_pgy_coro.ready_head = task->next;
    if (g_pgy_coro.ready_head == NULL)
        g_pgy_coro.ready_tail = NULL;
    task->next = NULL;
    task->queued = false;
    return task;
}

static inline void
pgy_coro_destroy(PgyCoroTask *task)
{
    if (task == NULL)
        return;
    pgy_cancel_release(task->cancel_node);
#ifdef _WIN32
    if (task->fiber != NULL)
        DeleteFiber(task->fiber);
#else
    free(task->stack);
#endif
    free(task);
}

#ifdef _WIN32
static inline bool
pgy_coro_ensure_scheduler(void)
{
    if (g_pgy_coro.scheduler_ready)
        return true;

    LPVOID fiber = ConvertThreadToFiber(NULL);
    if (fiber == NULL) {
        DWORD err = GetLastError();
        if (err == ERROR_ALREADY_FIBER)
            fiber = GetCurrentFiber();
    }
    if (fiber == NULL)
        return false;

    g_pgy_coro.scheduler_fiber = fiber;
    g_pgy_coro.scheduler_ready = true;
    return true;
}

static VOID WINAPI
pgy_coro_entry_win(void *raw_task)
{
    PgyCoroTask *task = (PgyCoroTask *)raw_task;
    g_pgy_coro.current = task;
    task->result = task->fn != NULL ? task->fn(task->arg) : NULL;
    task->done = true;

    if (task->waiter != NULL)
        pgy_coro_enqueue(task->waiter);

    g_pgy_coro.current = NULL;
    SwitchToFiber(g_pgy_coro.scheduler_fiber);
}
#else
static void
pgy_coro_entry(uintptr_t raw_task)
{
    PgyCoroTask *task = (PgyCoroTask *)raw_task;
    g_pgy_coro.current = task;
    task->result = task->fn != NULL ? task->fn(task->arg) : NULL;
    task->done = true;

    if (task->waiter != NULL)
        pgy_coro_enqueue(task->waiter);

    g_pgy_coro.current = NULL;
    setcontext(&g_pgy_coro.scheduler_ctx);
}

static inline bool
pgy_coro_init_task_posix(PgyCoroTask *task)
{
    task->stack_size = PGY_CORO_STACK_SIZE;
    task->stack = malloc(task->stack_size);
    if (task->stack == NULL)
        return false;

    getcontext(&task->ctx);
    task->ctx.uc_stack.ss_sp = task->stack;
    task->ctx.uc_stack.ss_size = task->stack_size;
    task->ctx.uc_link = &g_pgy_coro.scheduler_ctx;
    makecontext(&task->ctx, (void (*)(void))pgy_coro_entry, 1, (uintptr_t)task);
    return true;
}
#endif

static inline PgyTaskHandle
pgy_async_spawn(void *(*fn)(void *), void *arg)
{
    PgyTaskHandle handle = {0};
    PgyCoroTask *task = (PgyCoroTask *)calloc(1, sizeof(PgyCoroTask));
    if (task == NULL)
        return handle;

    task->model = PGY_TASK_MODEL_COROUTINE;
    task->fn = fn;
    task->arg = arg;
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
#ifdef _WIN32
    if (!pgy_coro_ensure_scheduler()) {
        pgy_cancel_release(task->cancel_node);
        free(task);
        return handle;
    }
    task->fiber = CreateFiber(PGY_CORO_STACK_SIZE, pgy_coro_entry_win, task);
    if (task->fiber == NULL) {
        pgy_cancel_release(task->cancel_node);
        free(task);
        return handle;
    }
#else
    if (!pgy_coro_init_task_posix(task)) {
        pgy_cancel_release(task->cancel_node);
        free(task);
        return handle;
    }
#endif

    pgy_coro_enqueue(task);
    handle.task = task;
    return handle;
}

static inline bool
pgy_async_progress_one(void)
{
    PgyCoroTask *task = pgy_coro_dequeue();
    if (task == NULL)
        return false;

    g_pgy_coro.current = task;
#ifdef _WIN32
    if (!pgy_coro_ensure_scheduler())
        return false;
    SwitchToFiber(task->fiber);
#else
    swapcontext(&g_pgy_coro.scheduler_ctx, &task->ctx);
#endif
    g_pgy_coro.current = NULL;

    if (task->done && task->detached)
        pgy_coro_destroy(task);

    return true;
}

static inline void
pgy_async_progress_until(bool (*predicate)(void *), void *arg)
{
    while (!predicate(arg)) {
        if (!pgy_async_progress_one())
            break;
    }
}

static inline bool
pgy_async_in_coroutine(void)
{
    return g_pgy_coro.current != NULL;
}

static inline void
pgy_async_yield(void)
{
    PgyCoroTask *current = g_pgy_coro.current;
    if (current == NULL)
        return;

    pgy_coro_enqueue(current);
    g_pgy_coro.current = NULL;
#ifdef _WIN32
    if (g_pgy_coro.scheduler_ready)
        SwitchToFiber(g_pgy_coro.scheduler_fiber);
#else
    swapcontext(&current->ctx, &g_pgy_coro.scheduler_ctx);
#endif
    g_pgy_coro.current = current;
}

static inline void
pgy_async_detach(PgyTaskHandle handle)
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    if (header == NULL)
        return;

    if (header->model == PGY_TASK_MODEL_COROUTINE) {
        PgyCoroTask *task = (PgyCoroTask *)handle.task;
        task->detached = true;
        if (!pgy_async_in_coroutine())
            (void)pgy_async_progress_one();
    }
}

static inline bool
pgy_async_task_done(void *raw)
{
    PgyCoroTask *task = (PgyCoroTask *)raw;
    return task == NULL || task->done;
}
#endif /* PGY_COROUTINES_AVAILABLE */

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
    if (header == NULL)
        return NULL;

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

    PgyTask *task = (PgyTask *)handle.task;
    pthread_mutex_lock(&task->mutex);
    while (task->state != PGY_TASK_DONE) {
        if (pgy_async_in_coroutine()) {
            pthread_mutex_unlock(&task->mutex);
            pgy_async_yield();
            pthread_mutex_lock(&task->mutex);
        } else {
            pthread_cond_wait(&task->cond, &task->mutex);
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
        CType _pgy_value = _pgy_result_ptr != NULL ? *_pgy_result_ptr : (CType)0; \
        free(_pgy_result_ptr); \
        _pgy_value; \
    })

#define pgy_await_void(handle) \
    ((void)pgy_await((handle)))

/* =================================================================
 * Parallel block helpers
 * ================================================================= */

typedef struct {
    void (*fn)(void);
} PgyParallelArg;

static void *
pgy_parallel_wrapper(void *raw)
{
    PgyParallelArg *parg = (PgyParallelArg *)raw;
    if (parg != NULL && parg->fn != NULL)
        parg->fn();
    return NULL;
}

static inline void
pgy_parallel_run(void (**tasks)(void), size_t count)
{
    if (count == 0)
        return;

    if (count == 1) {
        if (tasks[0] != NULL)
            tasks[0]();
        return;
    }

    PgyTaskHandle *handles = (PgyTaskHandle *)calloc(count, sizeof(PgyTaskHandle));
    PgyParallelArg *args = (PgyParallelArg *)calloc(count, sizeof(PgyParallelArg));

    for (size_t i = 0; i < count; i++) {
        args[i].fn = tasks[i];
        handles[i] = pgy_spawn(pgy_parallel_wrapper, &args[i]);
    }

    for (size_t i = 0; i < count; i++)
        pgy_await(handles[i]);

    free(handles);
    free(args);
}

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_H */
