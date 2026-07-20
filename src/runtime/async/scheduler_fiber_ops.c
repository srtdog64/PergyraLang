/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * PgyMnScheduler fiber enqueue/yield/block operations.
 */

#include <stdio.h>

#include "scheduler.h"
#include "fiber.h"
#include "concurrent_queue.h"
#include "../pgy_runtime_panic_contract.h"

static void
scheduler_ops_warn(const char* op, const char* reason, PgyMnScheduler* scheduler)
{
    fprintf(stderr,
        "[pgy][scheduler] %s failed: %s (scheduler=%p)\n",
        op != NULL ? op : "operation",
        reason != NULL ? reason : "unknown",
        (void*)scheduler);
}

void pgy_mn_scheduler_spawn(PgyMnScheduler* scheduler, PgyMnFiberFn routine, void* arg)
{
    pgy_mn_scheduler_spawn_with_priority(scheduler, routine, arg, 0);
}

void pgy_mn_scheduler_spawn_with_priority(PgyMnScheduler* scheduler, PgyMnFiberFn routine, void* arg, uint32_t priority)
{
    PgyMnFiber* fiber;

    if (scheduler == NULL) {
        scheduler_ops_warn("spawn", "scheduler is null", scheduler);
        return;
    }
    if (routine == NULL) {
        scheduler_ops_warn("spawn", "routine is null", scheduler);
        return;
    }
    if (scheduler->globalRunQueue == NULL) {
        scheduler_ops_warn("spawn", "scheduler global queue is null", scheduler);
        return;
    }
    
    fiber = pgy_mn_fiber_create(routine, arg);
    if (fiber == NULL) {
        scheduler_ops_warn("spawn", "fiber creation failed", scheduler);
        return;
    }

    if (!pgy_mn_scheduler_enqueue_fiber_with_priority(scheduler, fiber, priority))
        pgy_mn_fiber_destroy(fiber);
}

bool pgy_mn_scheduler_enqueue_fiber_with_priority(PgyMnScheduler* scheduler, PgyMnFiber* fiber, uint32_t priority)
{
    if (scheduler == NULL) {
        scheduler_ops_warn("enqueue", "scheduler is null", scheduler);
        return false;
    }
    if (fiber == NULL) {
        scheduler_ops_warn("enqueue", "fiber is null", scheduler);
        return false;
    }
    if (scheduler->globalRunQueue == NULL) {
        scheduler_ops_warn("enqueue", "scheduler global queue is null", scheduler);
        return false;
    }
    
    fiber->scheduler = scheduler;
    fiber->priority = priority;
    
    /* Update statistics */
    atomic_fetch_add(&scheduler->totalFibers, 1);
    atomic_fetch_add(&scheduler->activeFibers, 1);
    
    /* Add to global queue */
    if (!pgy_mn_queue_push(scheduler->globalRunQueue, fiber)) {
        atomic_fetch_sub(&scheduler->totalFibers, 1);
        atomic_fetch_sub(&scheduler->activeFibers, 1);
        return false;
    }
    
    /* Wake a parked worker if available */
    if (atomic_load(&scheduler->parkedWorkers) > 0) {
        pthread_mutex_lock(&scheduler->parkMutex);
        pthread_cond_signal(&scheduler->parkCondition);
        pthread_mutex_unlock(&scheduler->parkMutex);
    }

    return true;
}

void pgy_mn_scheduler_yield(void)
{
    PgyMnFiber* current = pgy_mn_fiber_get_current();
    if (current == NULL) {
        return;
    }
    
    current->state = FIBER_STATE_READY;
    pgy_mn_fiber_yield();
}

void pgy_mn_scheduler_block(PgyMnFiber* fiber)
{
    if (fiber == NULL) {
        return;
    }
    
    fiber->state = FIBER_STATE_BLOCKED;
    
    if (fiber == pgy_mn_fiber_get_current()) {
        pgy_mn_fiber_yield();
    }
}

void pgy_mn_scheduler_unblock(PgyMnFiber* fiber)
{
    if (fiber == NULL || fiber->state != FIBER_STATE_BLOCKED) {
        return;
    }
    
    fiber->state = FIBER_STATE_READY;
    
    /* Add to scheduler queue */
    PgyMnScheduler* scheduler = fiber->scheduler;
    if (scheduler != NULL) {
        if (scheduler->globalRunQueue == NULL) {
            scheduler_ops_warn("unblock", "scheduler global queue is null", scheduler);
            return;
        }
        if (!pgy_mn_queue_push(scheduler->globalRunQueue, fiber)) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "scheduler failed to unblock fiber");
        }
        
        /* Wake a parked worker */
        if (atomic_load(&scheduler->parkedWorkers) > 0) {
            pthread_mutex_lock(&scheduler->parkMutex);
            pthread_cond_signal(&scheduler->parkCondition);
            pthread_mutex_unlock(&scheduler->parkMutex);
        }
    }
}

bool pgy_mn_scheduler_steal_work(PgyMnWorker* thief)
{
    if (thief == NULL || thief->scheduler == NULL || thief->localRunQueue == NULL) {
        scheduler_ops_warn("steal", "worker or local queue is null", NULL);
        return false;
    }
    PgyMnScheduler* scheduler = thief->scheduler;
    if (scheduler->numWorkers == 0 || scheduler->workers == NULL) {
        scheduler_ops_warn("steal", "scheduler worker array is not initialized", scheduler);
        return false;
    }
    uint32_t victimId = atomic_fetch_add(&scheduler->stealingVictim, 1) % scheduler->numWorkers;
    
    /* Don't steal from self */
    if (victimId == thief->id) {
        victimId = (victimId + 1) % scheduler->numWorkers;
    }
    
    PgyMnWorker* victim = &scheduler->workers[victimId];
    if (victim->localRunQueue == NULL) {
        scheduler_ops_warn("steal", "victim local queue is null", scheduler);
        return false;
    }
    
    /* Try to steal from victim's local queue */
    void* stolen = pgy_mn_queue_pop(victim->localRunQueue);
    if (stolen != NULL) {
        if (!pgy_mn_queue_push(thief->localRunQueue, stolen)) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "scheduler failed to enqueue stolen fiber");
        }
        atomic_fetch_add(&thief->stealSuccesses, 1);
        return true;
    }
    
    atomic_fetch_add(&thief->stealAttempts, 1);
    return false;
}
