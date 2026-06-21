/*
 * Copyright (c) 2025 Pergyra Language Project
 * Cooperative coroutine runtime for Pergyra async tasks.
 * BSD 3-Clause License
 */

#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_COROUTINE_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_COROUTINE_H

/* =================================================================
 * Cooperative coroutine runtime for `spawn/await/async`
 * ================================================================= */

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
pgy_coro_entry(uint32_t raw_task_hi, uint32_t raw_task_lo)
{
    /* Mirror the guarded packing in pgy_coro_init_task_posix: on targets
     * where uintptr_t is 32-bit, shifting by 32 is UB, and the high word is
     * always zero there anyway. */
#if UINTPTR_MAX > UINT32_MAX
    uintptr_t raw_task = (((uintptr_t)raw_task_hi) << 32)
                       | (uintptr_t)raw_task_lo;
#else
    (void)raw_task_hi;
    uintptr_t raw_task = (uintptr_t)raw_task_lo;
#endif
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
    {
        uintptr_t raw_task = (uintptr_t)task;
        uint32_t raw_task_hi = 0;
#if UINTPTR_MAX > UINT32_MAX
        raw_task_hi = (uint32_t)(raw_task >> 32);
#endif
        makecontext(&task->ctx,
                    (void (*)(void))pgy_coro_entry,
                    2,
                    raw_task_hi,
                    (uint32_t)(raw_task & 0xffffffffu));
    }
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
    if (header == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "detach task handle is null");
    }

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
#else
static inline PgyCancelNode *
pgy_current_cancel_node(void)
{
    return g_pgy_thread_current != NULL ? g_pgy_thread_current->cancel_node : NULL;
}

static inline PgyTaskHandle
pgy_async_spawn(void *(*fn)(void *), void *arg)
{
    return pgy_spawn(fn, arg);
}

static inline bool
pgy_async_progress_one(void)
{
    return false;
}

static inline void
pgy_async_progress_until(bool (*predicate)(void *), void *arg)
{
    (void)predicate;
    (void)arg;
}

static inline bool
pgy_async_in_coroutine(void)
{
    return false;
}

static inline void
pgy_async_yield(void)
{
}

static inline void
pgy_async_detach(PgyTaskHandle handle)
{
    if (handle.task == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "detach task handle is null");
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      "detached async requires coroutine runtime support");
}
#endif /* PGY_COROUTINES_AVAILABLE */

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_COROUTINE_H */
