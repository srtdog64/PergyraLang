/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Scheduler implementation for Pergyra's SEA model
 * BSD Style + C# naming conventions
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/epoll.h>
#endif

#include "scheduler.h"
#include "fiber.h"
#include "concurrent_queue.h"

/* Thread-local current scheduler */
static __thread Scheduler* tlsCurrentScheduler = NULL;

static bool
scheduler_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

static void
scheduler_warn(const char* op, const char* reason, Scheduler* scheduler)
{
    fprintf(stderr,
        "[pgy][scheduler] %s failed: %s (scheduler=%p)\n",
        op != NULL ? op : "operation",
        reason != NULL ? reason : "unknown",
        (void*)scheduler);
}

static bool
scheduler_workers_valid(Scheduler* scheduler, const char* op)
{
    if (scheduler == NULL)
        return false;
    if (scheduler->numWorkers > 0 && scheduler->workers == NULL) {
        scheduler_warn(op, "worker count is nonzero but worker array is null", scheduler);
        return false;
    }
    return true;
}

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

#ifndef _WIN32
/* I/O worker thread (POSIX/Linux only — uses epoll) */
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
#endif /* !_WIN32 */

Scheduler* SchedulerCreate(const SchedulerConfig* config)
{
    Scheduler* scheduler = (Scheduler*)calloc(1, sizeof(Scheduler));
    if (scheduler == NULL) {
        scheduler_warn("create", "scheduler allocation failed", NULL);
        return NULL;
    }
    
    /* Copy configuration */
    if (config != NULL) {
        scheduler->config = *config;
    } else {
        /* Default configuration */
#ifdef _WIN32
        {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            scheduler->config.numWorkers = si.dwNumberOfProcessors;
        }
#else
        scheduler->config.numWorkers = sysconf(_SC_NPROCESSORS_ONLN);
#endif
        scheduler->config.isDeterministic = false;
        scheduler->config.enableWorkStealing = true;
        scheduler->config.stackSizeHint = FIBER_STACK_SIZE;
    }
    
    /* Ensure at least one worker */
    if (scheduler->config.numWorkers == 0) {
        scheduler->config.numWorkers = 1;
    }
    
    scheduler->numWorkers = scheduler->config.numWorkers;
    if (!scheduler_array_fits(scheduler->numWorkers, sizeof(WorkerThread))) {
        scheduler_warn("create", "worker array allocation size overflow", scheduler);
        free(scheduler);
        return NULL;
    }
    
    /* Initialize global queue */
    scheduler->globalRunQueue = ConcurrentQueueCreate();
    if (scheduler->globalRunQueue == NULL) {
        scheduler_warn("create", "global run queue allocation failed", scheduler);
        free(scheduler);
        return NULL;
    }
    
#ifndef _WIN32
    /* Initialize epoll for I/O (Linux only) */
    scheduler->epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (scheduler->epollFd < 0) {
        scheduler_warn("create", "epoll initialization failed", scheduler);
        ConcurrentQueueDestroy(scheduler->globalRunQueue);
        free(scheduler);
        return NULL;
    }
#endif
    
    /* Initialize parking */
    pthread_mutex_init(&scheduler->parkMutex, NULL);
    pthread_cond_init(&scheduler->parkCondition, NULL);
    
    /* Allocate workers */
    if (!scheduler_array_fits(scheduler->numWorkers, sizeof(WorkerThread))) {
        scheduler_warn("create", "worker array size overflow", scheduler);
#ifndef _WIN32
        close(scheduler->epollFd);
#endif
        ConcurrentQueueDestroy(scheduler->globalRunQueue);
        pthread_mutex_destroy(&scheduler->parkMutex);
        pthread_cond_destroy(&scheduler->parkCondition);
        free(scheduler);
        return NULL;
    }
    scheduler->workers = (WorkerThread*)calloc(scheduler->numWorkers, sizeof(WorkerThread));
    if (scheduler->workers == NULL) {
        scheduler_warn("create", "worker array allocation failed", scheduler);
#ifndef _WIN32
        close(scheduler->epollFd);
#endif
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
            scheduler_warn("create", "worker local queue allocation failed", scheduler);
            /* Clean up and fail */
            for (uint32_t j = 0; j < i; j++) {
                ConcurrentQueueDestroy(scheduler->workers[j].localRunQueue);
            }
            free(scheduler->workers);
#ifndef _WIN32
            close(scheduler->epollFd);
#endif
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
    for (uint32_t i = 0;
         scheduler->workers != NULL && i < scheduler->numWorkers;
         i++) {
        ConcurrentQueueDestroy(scheduler->workers[i].localRunQueue);
    }
    
    /* Free workers */
    free(scheduler->workers);
    
#ifndef _WIN32
    /* Close epoll */
    close(scheduler->epollFd);
#endif
    
    /* Destroy global queue */
    ConcurrentQueueDestroy(scheduler->globalRunQueue);
    
    /* Destroy synchronization */
    pthread_mutex_destroy(&scheduler->parkMutex);
    pthread_cond_destroy(&scheduler->parkCondition);
    
    free(scheduler);
}

void SchedulerStart(Scheduler* scheduler)
{
    uint32_t startedWorkers = 0;
#ifndef _WIN32
    bool ioStarted = false;
#endif

    if (scheduler == NULL) {
        scheduler_warn("start", "scheduler is null", scheduler);
        return;
    }
    if (atomic_load(&scheduler->isRunning)) {
        scheduler_warn("start", "scheduler is already running", scheduler);
        return;
    }
    if (!scheduler_workers_valid(scheduler, "start")
        || scheduler->globalRunQueue == NULL) {
        scheduler_warn("start", "scheduler queues are not initialized", scheduler);
        return;
    }
    
    atomic_store(&scheduler->isRunning, true);
    
    /* Start worker threads */
    for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
        WorkerThread* worker = &scheduler->workers[i];
        if (pthread_create(&worker->osThread, NULL, WorkerThreadMain, worker) != 0) {
            scheduler_warn("start", "worker thread creation failed", scheduler);
            goto startup_failed;
        }
        startedWorkers++;
    }
    
#ifndef _WIN32
    /* Start I/O worker */
    if (pthread_create(&scheduler->ioWorker, NULL, IoWorkerMain, scheduler) != 0) {
        scheduler_warn("start", "io worker creation failed", scheduler);
        goto startup_failed;
    }
    ioStarted = true;
#endif
    return;

startup_failed:
    atomic_store(&scheduler->isRunning, false);
    for (uint32_t i = 0; i < startedWorkers; i++) {
        atomic_store(&scheduler->workers[i].shouldStop, true);
    }
    pthread_mutex_lock(&scheduler->parkMutex);
    pthread_cond_broadcast(&scheduler->parkCondition);
    pthread_mutex_unlock(&scheduler->parkMutex);
    for (uint32_t i = 0; i < startedWorkers; i++) {
        if (pthread_join(scheduler->workers[i].osThread, NULL) != 0) {
            scheduler_warn("start", "worker thread rollback join failed", scheduler);
        }
    }
#ifndef _WIN32
    if (ioStarted && pthread_join(scheduler->ioWorker, NULL) != 0) {
        scheduler_warn("start", "io worker rollback join failed", scheduler);
    }
#endif
    scheduler_warn("start", "scheduler startup aborted", scheduler);
}

void SchedulerStop(Scheduler* scheduler)
{
    if (scheduler == NULL) {
        scheduler_warn("stop", "scheduler is null", scheduler);
        return;
    }
    if (!atomic_load(&scheduler->isRunning)) {
        scheduler_warn("stop", "scheduler is not running", scheduler);
        return;
    }
    if (!scheduler_workers_valid(scheduler, "stop")) {
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
        if (pthread_join(scheduler->workers[i].osThread, NULL) != 0) {
            scheduler_warn("stop", "worker thread join failed", scheduler);
        }
    }
    
#ifndef _WIN32
    /* Stop I/O worker */
    if (pthread_join(scheduler->ioWorker, NULL) != 0) {
        scheduler_warn("stop", "io worker join failed", scheduler);
    }
#endif
}

Scheduler* SchedulerGetCurrent(void)
{
    return tlsCurrentScheduler;
}

void SchedulerSetCurrent(Scheduler* scheduler)
{
    tlsCurrentScheduler = scheduler;
}
