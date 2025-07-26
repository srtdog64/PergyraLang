/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Scheduler implementation for Pergyra's SEA model
 * BSD Style + C# naming conventions
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/epoll.h>
#include <errno.h>
#include <assert.h>
#include "scheduler.h"
#include "fiber.h"
#include "concurrent_queue.h"

/* Thread-local current scheduler */
static __thread Scheduler* tlsCurrentScheduler = NULL;

/* Worker thread main function */
static void* WorkerThreadMain(void* arg)
{
    WorkerThread* worker = (WorkerThread*)arg;
    Scheduler* scheduler = worker->scheduler;
    
    /* Set thread-local scheduler */
    tlsCurrentScheduler = scheduler;
    
    while (!atomic_load(&worker->shouldStop)) {
        Fiber* fiber = NULL;
        
        /* Try local queue first */
        fiber = (Fiber*)ConcurrentQueuePop(worker->localRunQueue);
        
        /* If no local work, try global queue */
        if (fiber == NULL) {
            fiber = (Fiber*)ConcurrentQueuePop(scheduler->globalRunQueue);
        }
        
        /* If still no work, try work stealing */
        if (fiber == NULL && scheduler->config.enableWorkStealing) {
            if (SchedulerStealWork(worker)) {
                continue; /* Stolen work added to local queue */
            }
        }
        
        /* If no work available, park the worker */
        if (fiber == NULL) {
            pthread_mutex_lock(&scheduler->parkMutex);
            atomic_store(&worker->isParked, true);
            atomic_fetch_add(&scheduler->parkedWorkers, 1);
            
            /* Wait for work or stop signal */
            pthread_cond_wait(&scheduler->parkCondition, &scheduler->parkMutex);
            
            atomic_store(&worker->isParked, false);
            atomic_fetch_sub(&scheduler->parkedWorkers, 1);
            pthread_mutex_unlock(&scheduler->parkMutex);
            continue;
        }
        
        /* Execute the fiber */
        worker->currentFiber = fiber;
        fiber->state = FIBER_STATE_RUNNING;
        
        /* Context switch to fiber */
        FiberContext schedulerContext;
        FiberSwitchContext(&schedulerContext, &fiber->context);
        
        /* Fiber yielded or completed */
        worker->currentFiber = NULL;
        
        /* Handle fiber state */
        switch (fiber->state) {
            case FIBER_STATE_READY:
                /* Re-queue for execution */
                ConcurrentQueuePush(worker->localRunQueue, fiber);
                break;
                
            case FIBER_STATE_DONE:
            case FIBER_STATE_ERROR:
                /* Update statistics */
                atomic_fetch_add(&scheduler->totalFibers, -1);
                atomic_fetch_add(&worker->tasksExecuted, 1);
                
                /* Clean up fiber */
                FiberDestroy(fiber);
                break;
                
            case FIBER_STATE_BLOCKED:
                /* Fiber is waiting for I/O or timer */
                break;
                
            default:
                break;
        }
    }
    
    return NULL;
}

/* I/O worker thread */
static void* IoWorkerMain(void* arg)
{
    Scheduler* scheduler = (Scheduler*)arg;
    struct epoll_event events[128];
    
    while (atomic_load(&scheduler->isRunning)) {
        int nEvents = epoll_wait(scheduler->epollFd, events, 128, 100); /* 100ms timeout */
        
        if (nEvents < 0) {
            if (errno == EINTR) {
                continue;
            }
            /* Log error */
            break;
        }
        
        /* Process I/O events */
        for (int i = 0; i < nEvents; i++) {
            Fiber* fiber = (Fiber*)events[i].data.ptr;
            if (fiber != NULL) {
                /* Unblock the fiber */
                SchedulerUnblock(fiber);
            }
        }
    }
    
    return NULL;
}

Scheduler* SchedulerCreate(const SchedulerConfig* config)
{
    Scheduler* scheduler = (Scheduler*)calloc(1, sizeof(Scheduler));
    if (scheduler == NULL) {
        return NULL;
    }
    
    /* Copy configuration */
    if (config != NULL) {
        scheduler->config = *config;
    } else {
        /* Default configuration */
        scheduler->config.numWorkers = sysconf(_SC_NPROCESSORS_ONLN);
        scheduler->config.isDeterministic = false;
        scheduler->config.enableWorkStealing = true;
        scheduler->config.stackSizeHint = FIBER_STACK_SIZE;
    }
    
    /* Ensure at least one worker */
    if (scheduler->config.numWorkers == 0) {
        scheduler->config.numWorkers = 1;
    }
    
    scheduler->numWorkers = scheduler->config.numWorkers;
    
    /* Initialize global queue */
    scheduler->globalRunQueue = ConcurrentQueueCreate();
    if (scheduler->globalRunQueue == NULL) {
        free(scheduler);
        return NULL;
    }
    
    /* Initialize epoll for I/O */
    scheduler->epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (scheduler->epollFd < 0) {
        ConcurrentQueueDestroy(scheduler->globalRunQueue);
        free(scheduler);
        return NULL;
    }
    
    /* Initialize parking */
    pthread_mutex_init(&scheduler->parkMutex, NULL);
    pthread_cond_init(&scheduler->parkCondition, NULL);
    
    /* Allocate workers */
    scheduler->workers = (WorkerThread*)calloc(scheduler->numWorkers, sizeof(WorkerThread));
    if (scheduler->workers == NULL) {
        close(scheduler->epollFd);
        ConcurrentQueueDestroy(scheduler->globalRunQueue);
        pthread_mutex_destroy(&scheduler->parkMutex);
        pthread_cond_destroy(&scheduler->parkCondition);
        free(scheduler);
        return NULL;
    }
    
    /* Initialize workers */
    for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
        WorkerThread* worker = &scheduler->workers[i];
        worker->id = i;
        worker->scheduler = scheduler;
        worker->localRunQueue = ConcurrentQueueCreate();
        
        if (worker->localRunQueue == NULL) {
            /* Clean up and fail */
            for (uint32_t j = 0; j < i; j++) {
                ConcurrentQueueDestroy(scheduler->workers[j].localRunQueue);
            }
            free(scheduler->workers);
            close(scheduler->epollFd);
            ConcurrentQueueDestroy(scheduler->globalRunQueue);
            pthread_mutex_destroy(&scheduler->parkMutex);
            pthread_cond_destroy(&scheduler->parkCondition);
            free(scheduler);
            return NULL;
        }
    }
    
    return scheduler;
}

void SchedulerDestroy(Scheduler* scheduler)
{
    if (scheduler == NULL) {
        return;
    }
    
    /* Stop the scheduler */
    SchedulerStop(scheduler);
    
    /* Destroy worker queues */
    for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
        ConcurrentQueueDestroy(scheduler->workers[i].localRunQueue);
    }
    
    /* Free workers */
    free(scheduler->workers);
    
    /* Close epoll */
    close(scheduler->epollFd);
    
    /* Destroy global queue */
    ConcurrentQueueDestroy(scheduler->globalRunQueue);
    
    /* Destroy synchronization */
    pthread_mutex_destroy(&scheduler->parkMutex);
    pthread_cond_destroy(&scheduler->parkCondition);
    
    free(scheduler);
}

void SchedulerStart(Scheduler* scheduler)
{
    if (scheduler == NULL || atomic_load(&scheduler->isRunning)) {
        return;
    }
    
    atomic_store(&scheduler->isRunning, true);
    
    /* Start worker threads */
    for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
        WorkerThread* worker = &scheduler->workers[i];
        pthread_create(&worker->osThread, NULL, WorkerThreadMain, worker);
    }
    
    /* Start I/O worker */
    pthread_create(&scheduler->ioWorker, NULL, IoWorkerMain, scheduler);
}

void SchedulerStop(Scheduler* scheduler)
{
    if (scheduler == NULL || !atomic_load(&scheduler->isRunning)) {
        return;
    }
    
    atomic_store(&scheduler->isRunning, false);
    
    /* Signal workers to stop */
    for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
        atomic_store(&scheduler->workers[i].shouldStop, true);
    }
    
    /* Wake all parked workers */
    pthread_mutex_lock(&scheduler->parkMutex);
    pthread_cond_broadcast(&scheduler->parkCondition);
    pthread_mutex_unlock(&scheduler->parkMutex);
    
    /* Wait for workers to finish */
    for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
        pthread_join(scheduler->workers[i].osThread, NULL);
    }
    
    /* Stop I/O worker */
    pthread_join(scheduler->ioWorker, NULL);
}

void SchedulerSpawn(Scheduler* scheduler, FiberStartRoutine routine, void* arg)
{
    SchedulerSpawnWithPriority(scheduler, routine, arg, 0);
}

void SchedulerSpawnWithPriority(Scheduler* scheduler, FiberStartRoutine routine, void* arg, uint32_t priority)
{
    if (scheduler == NULL || routine == NULL) {
        return;
    }
    
    Fiber* fiber = FiberCreate(routine, arg);
    if (fiber == NULL) {
        return;
    }
    
    fiber->scheduler = scheduler;
    fiber->priority = priority;
    
    /* Update statistics */
    atomic_fetch_add(&scheduler->totalFibers, 1);
    atomic_fetch_add(&scheduler->activeFibers, 1);
    
    /* Add to global queue */
    ConcurrentQueuePush(scheduler->globalRunQueue, fiber);
    
    /* Wake a parked worker if available */
    if (atomic_load(&scheduler->parkedWorkers) > 0) {
        pthread_mutex_lock(&scheduler->parkMutex);
        pthread_cond_signal(&scheduler->parkCondition);
        pthread_mutex_unlock(&scheduler->parkMutex);
    }
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

Scheduler* SchedulerGetCurrent(void)
{
    return tlsCurrentScheduler;
}

void SchedulerSetCurrent(Scheduler* scheduler)
{
    tlsCurrentScheduler = scheduler;
}
