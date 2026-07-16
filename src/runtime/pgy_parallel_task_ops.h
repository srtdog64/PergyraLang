/*
 * Task cancellation and await operations for pgy_parallel.h.
 *
 * The pool owner creates and schedules tasks; this owner consumes a
 * PgyTaskHandle and performs task-level control operations.
 */

#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_TASK_OPS_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_TASK_OPS_H

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

static inline void *
pgy_await(PgyTaskHandle handle)
{
    PgyTaskHeader *header = (PgyTaskHeader *)handle.task;
    if (header == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "await task handle is null");
    }

#if PGY_COROUTINES_AVAILABLE
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
#endif

    PgyTask *task = (PgyTask *)handle.task;
    pthread_mutex_lock(&task->mutex);
    while (task->state != PGY_TASK_DONE) {
        if (pgy_async_in_coroutine()) {
            pthread_mutex_unlock(&task->mutex);
            pgy_async_yield();
            pthread_mutex_lock(&task->mutex);
        } else if (g_pgy_thread_current != NULL) {
            /* Help-first await (docs/186 P-A1): a pool worker drains queued
             * tasks instead of parking -- parking every worker starves the
             * queue and deadlocks nested fan-out. Only when the queue is
             * empty AND the target is still incomplete is the target
             * necessarily RUNNING on another worker (pending tasks live in
             * the queue), so parking is then deadlock-free: the await graph
             * is acyclic (a task only awaits tasks it spawned) and bottoms
             * out at a running task with no incomplete awaits. */
            pthread_mutex_unlock(&task->mutex);
            bool helped = pgy_pool_help_run_one();
            pthread_mutex_lock(&task->mutex);
            if (!helped && task->state != PGY_TASK_DONE) {
                if (pthread_cond_wait(&task->cond, &task->mutex) != 0) {
                    pthread_mutex_unlock(&task->mutex);
                    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                                      "await condition wait failed");
                }
            }
        } else {
            if (pthread_cond_wait(&task->cond, &task->mutex) != 0) {
                pthread_mutex_unlock(&task->mutex);
                PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                                  "await condition wait failed");
            }
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
        if (_pgy_result_ptr == NULL) { \
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                              "Future await returned null result"); \
        } \
        CType _pgy_value = *_pgy_result_ptr; \
        free(_pgy_result_ptr); \
        _pgy_value; \
    })

#define pgy_await_void(handle) \
    ((void)pgy_await((handle)))

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_TASK_OPS_H */
