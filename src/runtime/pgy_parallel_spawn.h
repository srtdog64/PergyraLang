#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_SPAWN_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_SPAWN_H

#include "pgy_runtime_linkage.h"

/* Task construction and the lifecycle-locked pool handoff for spawn. */
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

    /* Charge before spawning so a task fork bomb fails on the operation that
     * crosses the host ceiling. Both backends funnel through this owner. */
    if (pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");

    /* This lock-free pre-check avoids constructing a task for an inactive
     * pool. The lifecycle lock below is still the authoritative re-check. */
    if (!atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire)) {
        pgy_parallel_warn("spawn",
            "worker pool inactive; task runs inline (serial). "
            "parallel/spawn does not run concurrently without a live pool "
            "(pgy_pool_init).");
        return pgy_spawn_inline_completed(fn, arg, "spawn", false,
                                          PGY_LANE_WORKER_POOL);
    }

    /* Construction stays outside the process-wide lifecycle lock. The lock
     * covers only the liveness re-check and enqueue. */
    PgyTask *task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL) {
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
        return handle;
    }

    pthread_mutex_lock(&g_pgy_pool_lifecycle_mutex);
    if (atomic_load_explicit(&g_pgy_pool_shutting_down, memory_order_acquire)
        || !atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire)) {
        bool going_down = atomic_load_explicit(&g_pgy_pool_shutting_down,
                                               memory_order_acquire);
        pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
        pthread_cond_destroy(&task->cond);
        pthread_mutex_destroy(&task->mutex);
        pgy_cancel_release(task->cancel_node);
        free(task);
        if (going_down) {
            pgy_parallel_warn("spawn", "pool is shutting down");
            return handle;
        }
        pgy_parallel_warn("spawn",
            "worker pool inactive; task runs inline (serial). "
            "parallel/spawn does not run concurrently without a live pool "
            "(pgy_pool_init).");
        return pgy_spawn_inline_completed(fn, arg, "spawn", false,
                                          PGY_LANE_WORKER_POOL);
    }
    handle.task = task;
    pgy_pool_enqueue(&g_pgy_pool, task);
    pthread_mutex_unlock(&g_pgy_pool_lifecycle_mutex);
    return handle;
}
#else
;
#endif

#endif /* PERGYRA_RUNTIME_PGY_PARALLEL_SPAWN_H */
