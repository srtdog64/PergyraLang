/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime_lib_mn_exports.h -- M:N fiber scheduler materialization for the
 * emitted-program runtime (WO-MN-1 R1/R2, docs/194).
 *
 * This header exists ONLY inside the linked-runtime materializations: the
 * LLVM-leg object/bitcode TU (pgy_runtime_lib.c, PGY_RUNTIME_LIB_INTERNAL)
 * and the C-leg cext object TU (pgy_runtime_cext_lib.c,
 * PGY_RUNTIME_EXTERN_DEFS). The legacy inline runtime mode never sees these
 * symbols: the driver refuses MovableScheduler spawn-lane rows before codegen
 * in that mode, and the lane dispatch carries a declared fail-closed branch
 * as the defence-in-depth backstop.
 *
 * The core is pulled in as source so ONE recipe covers both materializations
 * (object and bitcode) -- the same single-recipe rule the runtime cache
 * exists to enforce. Symbols are the pgy_mn_ house family (R0); the
 * async_scope/channel/effects surfaces are NOT part of this seam.
 */

#ifndef PERGYRA_RUNTIME_PGY_RUNTIME_LIB_MN_EXPORTS_H
#define PERGYRA_RUNTIME_PGY_RUNTIME_LIB_MN_EXPORTS_H

#ifndef PGY_RUNTIME_MN_MATERIALIZED
#error "the M:N executor materializes only inside the linked-runtime object/bitcode builds"
#endif

#include "async/concurrent_queue.c"
#include "async/fiber.c"
#include "async/scheduler.c"
#include "async/scheduler_fiber_ops.c"

/*
 * The Movable-lane executor singleton. TU-local by construction: the only
 * consumer (pgy_lane_spawn_dispatch) materializes in this same TU, so the
 * docs/190 two-instance class cannot occur -- there is exactly one
 * materialization that ever references it.
 */
static PgyMnScheduler *g_pgy_mn_executor = NULL;
static pthread_mutex_t g_pgy_mn_executor_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Fiber entry: run the ONE task run protocol. pgy_pool_run_task scopes the
 * cooperative-cancel TLS around fn and publishes result + DONE + broadcast,
 * so await/cancel machinery and the executor-invariance contract hold
 * unchanged on this executor. */
static void
pgy_mn_executor_run_task(void *raw)
{
    pgy_pool_run_task((PgyTask *)raw);
}

static PgyMnScheduler *
pgy_mn_executor_acquire(void)
{
    PgyMnScheduler *scheduler;

    pthread_mutex_lock(&g_pgy_mn_executor_mutex);
    if (g_pgy_mn_executor == NULL) {
        scheduler = pgy_mn_scheduler_create(NULL);
        if (scheduler != NULL) {
            pgy_mn_scheduler_start(scheduler);
            g_pgy_mn_executor = scheduler;
        }
    }
    scheduler = g_pgy_mn_executor;
    pthread_mutex_unlock(&g_pgy_mn_executor_mutex);
    return scheduler;
}

/*
 * Movable-lane spawn: same construction protocol as pgy_spawn (budget charge
 * first, task built outside any lock, cooperative cancel node inherited from
 * the spawning context), then handed to the M:N scheduler instead of the
 * bounded pool. The scheduler starts lazily on the first movable task and is
 * torn down by process exit; workers park when idle.
 */
PgyTaskHandle
pgy_mn_executor_submit(void *(*fn)(void *), void *arg)
{
    PgyTaskHandle handle = {0};
    PgyTask *task;
    PgyMnScheduler *scheduler;

    if (fn == NULL) {
        pgy_parallel_warn("mn-spawn", "task function is null");
        return handle;
    }

    if (pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_SPAWN_COUNT, 1, "spawn");

    task = (PgyTask *)calloc(1, sizeof(PgyTask));
    if (task == NULL) {
        pgy_parallel_warn("mn-spawn", "task allocation failed");
        return handle;
    }
    task->model = PGY_TASK_MODEL_THREAD;
    task->lane = PGY_LANE_MOVABLE_SCHEDULER;
    task->fn = fn;
    task->arg = arg;
    atomic_store_explicit(&task->state, PGY_TASK_PENDING,
                          memory_order_relaxed);
    pgy_cancel_probe_install(pgy_parallel_cancel_probe);
    task->cancel_node = pgy_cancel_node_create(pgy_current_cancel_node());
    if (!pgy_task_sync_init(task, "mn-spawn")) {
        pgy_cancel_release(task->cancel_node);
        free(task);
        return handle;
    }

    scheduler = pgy_mn_executor_acquire();
    if (scheduler == NULL) {
        pgy_parallel_warn("mn-spawn", "movable executor unavailable");
        pthread_cond_destroy(&task->cond);
        pthread_mutex_destroy(&task->mutex);
        pgy_cancel_release(task->cancel_node);
        free(task);
        return handle;
    }

    handle.task = task;
    pgy_mn_scheduler_spawn(scheduler, pgy_mn_executor_run_task, task);
    return handle;
}

#endif /* PERGYRA_RUNTIME_PGY_RUNTIME_LIB_MN_EXPORTS_H */
