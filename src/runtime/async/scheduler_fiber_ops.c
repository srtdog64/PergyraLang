/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Scheduler fiber enqueue/yield/block operations.
 */

#include <stdio.h>

#include "scheduler.h"
#include "fiber.h"
#include "concurrent_queue.h"

static void
scheduler_ops_warn(const char* op, const char* reason, Scheduler* scheduler)
{
    fprintf(stderr,
        "[pgy][scheduler] %s failed: %s (scheduler=%p)\n",
        op != NULL ? op : "operation",
        reason != NULL ? reason : "unknown",
        (void*)scheduler);
}

void SchedulerSpawn(Scheduler* scheduler, FiberStartRoutine routine, void* arg)
{
    SchedulerSpawnWithPriority(scheduler, routine, arg, 0);
}

void SchedulerSpawnWithPriority(Scheduler* scheduler, FiberStartRoutine routine, void* arg, uint32_t priority)
{
    Fiber* fiber;

    if (scheduler == NULL) {
        scheduler_ops_warn("spawn", "scheduler is null", scheduler);
        return;
    }
    if (routine == NULL) {
        scheduler_ops_warn("spawn", "routine is null", scheduler);
        return;
    }
    
    fiber = FiberCreate(routine, arg);
    if (fiber == NULL) {
        scheduler_ops_warn("spawn", "fiber creation failed", scheduler);
        return;
    }

    if (!SchedulerEnqueueFiberWithPriority(scheduler, fiber, priority))
        FiberDestroy(fiber);
}

bool SchedulerEnqueueFiberWithPriority(Scheduler* scheduler, Fiber* fiber, uint32_t priority)
{
    if (scheduler == NULL) {
        scheduler_ops_warn("enqueue", "scheduler is null", scheduler);
        return false;
    }
    if (fiber == NULL) {
        scheduler_ops_warn("enqueue", "fiber is null", scheduler);
        return false;
    }
    
    fiber->scheduler = scheduler;
    fiber->priority = priority;
    
    /* Update statistics */
    atomic_fetch_add(&scheduler->totalFibers, 1);
    atomic_fetch_add(&scheduler->activeFibers, 1);
    
    /* Add to global queue */
    if (!ConcurrentQueuePush(scheduler->globalRunQueue, fiber)) {
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

void SchedulerYield(void)
{
    Fiber* current = FiberGetCurrent();
    if (current == NULL) {
        return;
    }
    
    current->state = FIBER_STATE_READY;
    FiberYield();
}

void SchedulerBlock(Fiber* fiber)
{
    if (fiber == NULL) {
        return;
    }
    
    fiber->state = FIBER_STATE_BLOCKED;
    
    if (fiber == FiberGetCurrent()) {
        FiberYield();
    }
}

void SchedulerUnblock(Fiber* fiber)
{
    if (fiber == NULL || fiber->state != FIBER_STATE_BLOCKED) {
        return;
    }
    
    fiber->state = FIBER_STATE_READY;
    
    /* Add to scheduler queue */
    Scheduler* scheduler = fiber->scheduler;
    if (scheduler != NULL) {
        ConcurrentQueuePush(scheduler->globalRunQueue, fiber);
        
        /* Wake a parked worker */
        if (atomic_load(&scheduler->parkedWorkers) > 0) {
            pthread_mutex_lock(&scheduler->parkMutex);
            pthread_cond_signal(&scheduler->parkCondition);
            pthread_mutex_unlock(&scheduler->parkMutex);
        }
    }
}

bool SchedulerStealWork(WorkerThread* thief)
{
    Scheduler* scheduler = thief->scheduler;
    uint32_t victimId = atomic_fetch_add(&scheduler->stealingVictim, 1) % scheduler->numWorkers;
    
    /* Don't steal from self */
    if (victimId == thief->id) {
        victimId = (victimId + 1) % scheduler->numWorkers;
    }
    
    WorkerThread* victim = &scheduler->workers[victimId];
    
    /* Try to steal from victim's local queue */
    void* stolen = ConcurrentQueuePop(victim->localRunQueue);
    if (stolen != NULL) {
        ConcurrentQueuePush(thief->localRunQueue, stolen);
        atomic_fetch_add(&thief->stealSuccesses, 1);
        return true;
    }
    
    atomic_fetch_add(&thief->stealAttempts, 1);
    return false;
}
