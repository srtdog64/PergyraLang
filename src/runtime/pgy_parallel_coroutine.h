#include "pgy_runtime_linkage.h"
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
    PgyExecutionLane       lane;
    void *(*fn)(void *);
    void                  *arg;
    void                  *result;
    bool                   done;
    bool                   detached;
    bool                   queued;
    PgyCancelNode         *cancel_node;
    PgyRuntimeContext      runtime_context;
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
    PgyRuntimeContext *scheduler_runtime_context;
} PgyCoroRuntime;

PGY_RT_GLOBAL __thread PgyCoroRuntime g_pgy_coro
#ifndef PGY_RUNTIME_DECLS_ONLY
    = {0}
#endif
;

PGY_RT_DECL PgyCancelNode *
pgy_current_cancel_node(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
#if PGY_COROUTINES_AVAILABLE
    if (g_pgy_coro.current != NULL)
        return g_pgy_coro.current->cancel_node;
#endif
    return g_pgy_thread_current != NULL ? g_pgy_thread_current->cancel_node : NULL;
}
#else
;
#endif


PGY_RT_DECL void
pgy_coro_enqueue(PgyCoroTask *task)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL PgyCoroTask *
pgy_coro_dequeue(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL void
pgy_coro_destroy(PgyCoroTask *task)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


#ifdef _WIN32
PGY_RT_DECL bool
pgy_coro_ensure_scheduler(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


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
    if (!pgy_runtime_context_bind(g_pgy_coro.scheduler_runtime_context)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "coroutine failed to restore scheduler context");
    }
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
    if (!pgy_runtime_context_bind(g_pgy_coro.scheduler_runtime_context)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "coroutine failed to restore scheduler context");
    }
    setcontext(&g_pgy_coro.scheduler_ctx);
}

PGY_RT_DECL bool
pgy_coro_init_task_posix(PgyCoroTask *task)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif

#endif

PGY_RT_DECL PgyTaskHandle
pgy_async_spawn(void *(*fn)(void *), void *arg)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyTaskHandle handle = {0};

    /* SPAWN_COUNT budget charge (R6): async/spawn tasks use this coroutine model
     * which does NOT route through pgy_spawn, so it carries its own charge to the
     * same fork-bomb ceiling. Deny-before-allocate, behind the imposed fast-path.
     * Both backends reach here via the lane-owned spawn dispatcher. */
    if (pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");

    PgyCoroTask *task = (PgyCoroTask *)calloc(1, sizeof(PgyCoroTask));
    if (task == NULL)
        return handle;

    task->model = PGY_TASK_MODEL_COROUTINE;
    task->lane = PGY_LANE_LOCAL_ASYNC;
    task->fn = fn;
    task->arg = arg;
    if (!pgy_runtime_context_capture_task(&task->runtime_context)) {
        free(task);
        return handle;
    }
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
#else
;
#endif


PGY_RT_DECL bool
pgy_async_progress_one(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyCoroTask *task = pgy_coro_dequeue();
    PgyRuntimeContext *previous_context;
    if (task == NULL)
        return false;

    previous_context = pgy_runtime_context_current();
    g_pgy_coro.scheduler_runtime_context = previous_context;
    if (!pgy_runtime_context_bind(&task->runtime_context)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "coroutine captured runtime context is invalid");
    }
    g_pgy_coro.current = task;
#ifdef _WIN32
    if (!pgy_coro_ensure_scheduler()) {
        (void)pgy_runtime_context_bind(previous_context);
        return false;
    }
    SwitchToFiber(task->fiber);
#else
    swapcontext(&g_pgy_coro.scheduler_ctx, &task->ctx);
#endif
    g_pgy_coro.current = NULL;
    if (!pgy_runtime_context_bind(previous_context)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "coroutine scheduler context restore failed");
    }

    if (task->done && task->detached)
        pgy_coro_destroy(task);

    return true;
}
#else
;
#endif


PGY_RT_DECL void
pgy_async_progress_until(bool (*predicate)(void *), void *arg)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    while (!predicate(arg)) {
        if (!pgy_async_progress_one())
            break;
    }
}
#else
;
#endif


PGY_RT_DECL bool
pgy_async_in_coroutine(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return g_pgy_coro.current != NULL;
}
#else
;
#endif


PGY_RT_DECL void
pgy_async_yield(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyCoroTask *current = g_pgy_coro.current;
    if (current == NULL)
        return;

    pgy_coro_enqueue(current);
    g_pgy_coro.current = NULL;
    if (!pgy_runtime_context_bind(g_pgy_coro.scheduler_runtime_context)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "coroutine yield lost scheduler context");
    }
#ifdef _WIN32
    if (g_pgy_coro.scheduler_ready)
        SwitchToFiber(g_pgy_coro.scheduler_fiber);
#else
    swapcontext(&current->ctx, &g_pgy_coro.scheduler_ctx);
#endif
    g_pgy_coro.current = current;
    if (!pgy_runtime_context_bind(&current->runtime_context)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "coroutine yield lost task context");
    }
}
#else
;
#endif


PGY_RT_DECL void
pgy_async_detach(PgyTaskHandle handle)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL bool
pgy_async_task_done(void *raw)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyCoroTask *task = (PgyCoroTask *)raw;
    return task == NULL || task->done;
}
#else
;
#endif

#else
PGY_RT_DECL PgyCancelNode *
pgy_current_cancel_node(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return g_pgy_thread_current != NULL ? g_pgy_thread_current->cancel_node : NULL;
}
#else
;
#endif


PGY_RT_DECL PgyTaskHandle
pgy_async_spawn(void *(*fn)(void *), void *arg)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyTaskHandle handle = pgy_spawn(fn, arg);
    if (handle.task != NULL)
        pgy_task_handle_set_lane(handle, PGY_LANE_LOCAL_ASYNC);
    return handle;
}
#else
;
#endif


PGY_RT_DECL bool
pgy_async_progress_one(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return false;
}
#else
;
#endif


PGY_RT_DECL void
pgy_async_progress_until(bool (*predicate)(void *), void *arg)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    (void)predicate;
    (void)arg;
}
#else
;
#endif


PGY_RT_DECL bool
pgy_async_in_coroutine(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return false;
}
#else
;
#endif


PGY_RT_DECL void
pgy_async_yield(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
}
#else
;
#endif


PGY_RT_DECL void
pgy_async_detach(PgyTaskHandle handle)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (handle.task == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "detach task handle is null");
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      "detached async requires coroutine runtime support");
}
#else
;
#endif

#endif /* PGY_COROUTINES_AVAILABLE */

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_COROUTINE_H */
