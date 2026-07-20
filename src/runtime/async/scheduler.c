/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * PgyMnScheduler implementation for Pergyra's SEA model
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
#if defined(__linux__)
#include <sys/epoll.h>
#endif
#endif

#include "scheduler.h"
#include "fiber.h"
#include "concurrent_queue.h"
#include "../pgy_runtime_panic_contract.h"

/* Thread-local current scheduler */
static __thread PgyMnScheduler* tlsCurrentScheduler = NULL;

static bool
scheduler_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

static void
scheduler_warn(const char* op, const char* reason, PgyMnScheduler* scheduler)
{
    fprintf(stderr,
        "[pgy][scheduler] %s failed: %s (scheduler=%p)\n",
        op != NULL ? op : "operation",
        reason != NULL ? reason : "unknown",
        (void*)scheduler);
}

static bool
scheduler_workers_valid(PgyMnScheduler* scheduler, const char* op)
{
    if (scheduler == NULL)
        return false;
    if (scheduler->numWorkers > 0 && scheduler->workers == NULL) {
        scheduler_warn(op, "worker count is nonzero but worker array is null", scheduler);
        return false;
    }
    return true;
}

static void
scheduler_stat_decrement_nonzero(atomic_uint_least64_t* counter)
{
    uint_least64_t current;

    if (counter == NULL)
        return;

    current = atomic_load(counter);
    while (current > 0) {
        if (atomic_compare_exchange_weak(counter, &current, current - 1))
            return;
    }
}

/* Worker thread main function */
static void* pgy_mn_worker_main(void* arg)
{
    PgyMnWorker* worker = (PgyMnWorker*)arg;
    PgyMnScheduler* scheduler = worker->scheduler;
    
    /* Set thread-local scheduler */
    tlsCurrentScheduler = scheduler;
    
    while (!atomic_load(&worker->shouldStop)) {
        PgyMnFiber* fiber = NULL;
        
        /* Try local queue first */
        fiber = (PgyMnFiber*)pgy_mn_queue_pop(worker->localRunQueue);
        
        /* If no local work, try global queue */
        if (fiber == NULL) {
            fiber = (PgyMnFiber*)pgy_mn_queue_pop(scheduler->globalRunQueue);
        }
        
        /* If still no work, try work stealing */
        if (fiber == NULL && scheduler->config.enableWorkStealing) {
            if (pgy_mn_scheduler_steal_work(worker)) {
                continue; /* Stolen work added to local queue */
            }
        }
        
        /* If no work available, park the worker */
        if (fiber == NULL) {
            pthread_mutex_lock(&scheduler->parkMutex);
            atomic_store(&worker->isParked, true);
            atomic_fetch_add(&scheduler->parkedWorkers, 1);
            
            /* Wait for work or stop signal */
            if (pthread_cond_wait(&scheduler->parkCondition,
                                  &scheduler->parkMutex) != 0) {
                scheduler_warn("worker_park",
                               "park condition wait failed",
                               scheduler);
                atomic_store(&worker->shouldStop, true);
                atomic_store(&worker->isParked, false);
                atomic_fetch_sub(&scheduler->parkedWorkers, 1);
                pthread_mutex_unlock(&scheduler->parkMutex);
                break;
            }
            
            atomic_store(&worker->isParked, false);
            atomic_fetch_sub(&scheduler->parkedWorkers, 1);
            pthread_mutex_unlock(&scheduler->parkMutex);
            continue;
        }
        
        /* Execute the fiber.
         *
         * Run-to-completion depth (docs/194 WO-MN-1): a submitted movable
         * task never yields mid-body, so the worker runs the routine directly
         * on its own stack -- the work-stealing M:N worker set is real, the
         * per-fiber context switch is not engaged. The assembly
         * pgy_mn_fiber_switch_context this replaced restored a context that
         * create() never seeded (no entry trampoline on the fiber stack), so
         * the first switch jumped to garbage; a real context layer that can
         * START a new fiber on its own stack is the WO-MN-2 rung. Until it
         * lands, a fiber that reports READY again after running (a yield)
         * would be an internal invariant violation, handled below. */
        worker->currentFiber = fiber;
        fiber->state = FIBER_STATE_RUNNING;
        if (fiber->startRoutine == NULL) {
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "scheduler dequeued a fiber with no routine");
        }
        fiber->startRoutine(fiber->arg);
        if (fiber->state == FIBER_STATE_RUNNING)
            fiber->state = FIBER_STATE_DONE;
        worker->currentFiber = NULL;
        
        /* Handle fiber state */
        switch (fiber->state) {
            case FIBER_STATE_READY:
                /* A yield without the WO-MN-2 context layer: fail closed
                 * rather than requeue a frame that cannot be resumed. */
                PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                                  "fiber yielded but the context layer is not landed");
                break;
                
            case FIBER_STATE_DONE:
            case FIBER_STATE_ERROR:
                /* Update statistics */
                scheduler_stat_decrement_nonzero(&scheduler->totalFibers);
                scheduler_stat_decrement_nonzero(&scheduler->activeFibers);
                atomic_fetch_add(&worker->tasksExecuted, 1);
                
                /* Clean up fiber */
                pgy_mn_fiber_destroy(fiber);
                break;
                
            case FIBER_STATE_BLOCKED:
                /* PgyMnFiber is waiting for I/O or timer */
                break;
                
            default:
                break;
        }
    }
    
    return NULL;
}

#if defined(__linux__)
/* I/O worker thread (Linux only -- uses epoll). */
static void* IoWorkerMain(void* arg)
{
    PgyMnScheduler* scheduler = (PgyMnScheduler*)arg;
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
            PgyMnFiber* fiber = (PgyMnFiber*)events[i].data.ptr;
            if (fiber != NULL) {
                /* Unblock the fiber */
                pgy_mn_scheduler_unblock(fiber);
            }
        }
    }
    
    return NULL;
}
#endif /* __linux__ */

PgyMnScheduler* pgy_mn_scheduler_create(const PgyMnSchedulerConfig* config)
{
    PgyMnScheduler* scheduler = (PgyMnScheduler*)calloc(1, sizeof(PgyMnScheduler));
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
    if (!scheduler_array_fits(scheduler->numWorkers, sizeof(PgyMnWorker))) {
        scheduler_warn("create", "worker array allocation size overflow", scheduler);
        free(scheduler);
        return NULL;
    }
    
    /* Initialize global queue */
    scheduler->globalRunQueue = pgy_mn_queue_create();
    if (scheduler->globalRunQueue == NULL) {
        scheduler_warn("create", "global run queue allocation failed", scheduler);
        free(scheduler);
        return NULL;
    }
    
#if defined(__linux__)
    /* Initialize epoll for I/O (Linux only) */
    scheduler->epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (scheduler->epollFd < 0) {
        scheduler_warn("create", "epoll initialization failed", scheduler);
        pgy_mn_queue_destroy(scheduler->globalRunQueue);
        free(scheduler);
        return NULL;
    }
#endif
    
    /* Initialize parking */
    if (pthread_mutex_init(&scheduler->parkMutex, NULL) != 0) {
        scheduler_warn("create", "park mutex initialization failed", scheduler);
#if defined(__linux__)
        close(scheduler->epollFd);
#endif
        pgy_mn_queue_destroy(scheduler->globalRunQueue);
        free(scheduler);
        return NULL;
    }
    if (pthread_cond_init(&scheduler->parkCondition, NULL) != 0) {
        scheduler_warn("create", "park condition initialization failed", scheduler);
#if defined(__linux__)
        close(scheduler->epollFd);
#endif
        pgy_mn_queue_destroy(scheduler->globalRunQueue);
        pthread_mutex_destroy(&scheduler->parkMutex);
        free(scheduler);
        return NULL;
    }
    
    /* Allocate workers */
    if (!scheduler_array_fits(scheduler->numWorkers, sizeof(PgyMnWorker))) {
        scheduler_warn("create", "worker array size overflow", scheduler);
#if defined(__linux__)
        close(scheduler->epollFd);
#endif
        pgy_mn_queue_destroy(scheduler->globalRunQueue);
        pthread_mutex_destroy(&scheduler->parkMutex);
        pthread_cond_destroy(&scheduler->parkCondition);
        free(scheduler);
        return NULL;
    }
    scheduler->workers = (PgyMnWorker*)calloc(scheduler->numWorkers, sizeof(PgyMnWorker));
    if (scheduler->workers == NULL) {
        scheduler_warn("create", "worker array allocation failed", scheduler);
#if defined(__linux__)
        close(scheduler->epollFd);
#endif
        pgy_mn_queue_destroy(scheduler->globalRunQueue);
        pthread_mutex_destroy(&scheduler->parkMutex);
        pthread_cond_destroy(&scheduler->parkCondition);
        free(scheduler);
        return NULL;
    }
    
    /* Initialize workers */
    for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
        PgyMnWorker* worker = &scheduler->workers[i];
        worker->id = i;
        worker->scheduler = scheduler;
        worker->localRunQueue = pgy_mn_queue_create();
        
        if (worker->localRunQueue == NULL) {
            scheduler_warn("create", "worker local queue allocation failed", scheduler);
            /* Clean up and fail */
            for (uint32_t j = 0; j < i; j++) {
                pgy_mn_queue_destroy(scheduler->workers[j].localRunQueue);
            }
            free(scheduler->workers);
#if defined(__linux__)
            close(scheduler->epollFd);
#endif
            pgy_mn_queue_destroy(scheduler->globalRunQueue);
            pthread_mutex_destroy(&scheduler->parkMutex);
            pthread_cond_destroy(&scheduler->parkCondition);
            free(scheduler);
            return NULL;
        }
    }
    
    return scheduler;
}

void pgy_mn_scheduler_destroy(PgyMnScheduler* scheduler)
{
    if (scheduler == NULL) {
        return;
    }
    
    /* Stop the scheduler */
    pgy_mn_scheduler_stop(scheduler);
    
    /* Destroy worker queues */
    for (uint32_t i = 0;
         scheduler->workers != NULL && i < scheduler->numWorkers;
         i++) {
        pgy_mn_queue_destroy(scheduler->workers[i].localRunQueue);
    }
    
    /* Free workers */
    free(scheduler->workers);
    
#if defined(__linux__)
    /* Close epoll */
    close(scheduler->epollFd);
#endif
    
    /* Destroy global queue */
    pgy_mn_queue_destroy(scheduler->globalRunQueue);
    
    /* Destroy synchronization */
    pthread_mutex_destroy(&scheduler->parkMutex);
    pthread_cond_destroy(&scheduler->parkCondition);
    
    free(scheduler);
}

void pgy_mn_scheduler_start(PgyMnScheduler* scheduler)
{
    uint32_t startedWorkers = 0;
#if defined(__linux__)
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
        PgyMnWorker* worker = &scheduler->workers[i];
        if (pthread_create(&worker->osThread, NULL, pgy_mn_worker_main, worker) != 0) {
            scheduler_warn("start", "worker thread creation failed", scheduler);
            goto startup_failed;
        }
        startedWorkers++;
    }
    
#if defined(__linux__)
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
#if defined(__linux__)
    if (ioStarted && pthread_join(scheduler->ioWorker, NULL) != 0) {
        scheduler_warn("start", "io worker rollback join failed", scheduler);
    }
#endif
    scheduler_warn("start", "scheduler startup aborted", scheduler);
}

void pgy_mn_scheduler_stop(PgyMnScheduler* scheduler)
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
    
#if defined(__linux__)
    /* Stop I/O worker */
    if (pthread_join(scheduler->ioWorker, NULL) != 0) {
        scheduler_warn("stop", "io worker join failed", scheduler);
    }
#endif
}

PgyMnScheduler* pgy_mn_scheduler_get_current(void)
{
    return tlsCurrentScheduler;
}

void pgy_mn_scheduler_set_current(PgyMnScheduler* scheduler)
{
    tlsCurrentScheduler = scheduler;
}

void pgy_mn_scheduler_register_io_event(PgyMnScheduler* scheduler, int fd, uint32_t events,
                              PgyMnFiber* fiber)
{
    if (scheduler == NULL || fiber == NULL || fd < 0) {
        scheduler_warn("register_io", "scheduler, fiber, or fd is invalid",
            scheduler);
        return;
    }
#if defined(__linux__)
    if (scheduler->epollFd < 0) {
        scheduler_warn("register_io", "epoll is not initialized", scheduler);
        return;
    }
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = events;
    event.data.ptr = fiber;
    if (epoll_ctl(scheduler->epollFd, EPOLL_CTL_ADD, fd, &event) != 0) {
        if (errno == EEXIST
            && epoll_ctl(scheduler->epollFd, EPOLL_CTL_MOD, fd, &event) == 0) {
            return;
        }
        scheduler_warn("register_io", "epoll_ctl add/mod failed", scheduler);
    }
#else
    (void)events;
    scheduler_warn("register_io", "I/O event registration is unavailable on this platform",
        scheduler);
#endif
}

void pgy_mn_scheduler_unregister_io_event(PgyMnScheduler* scheduler, int fd)
{
    if (scheduler == NULL || fd < 0) {
        scheduler_warn("unregister_io", "scheduler or fd is invalid", scheduler);
        return;
    }
#if defined(__linux__)
    if (scheduler->epollFd < 0) {
        scheduler_warn("unregister_io", "epoll is not initialized", scheduler);
        return;
    }
    if (epoll_ctl(scheduler->epollFd, EPOLL_CTL_DEL, fd, NULL) != 0
        && errno != ENOENT) {
        scheduler_warn("unregister_io", "epoll_ctl delete failed", scheduler);
    }
#else
    scheduler_warn("unregister_io", "I/O event registration is unavailable on this platform",
        scheduler);
#endif
}

void pgy_mn_scheduler_schedule_timer(PgyMnScheduler* scheduler, uint64_t deadlineNs,
                            PgyMnFiber* fiber)
{
    (void)scheduler;
    (void)deadlineNs;
    (void)fiber;
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      "scheduler timer support is not implemented");
}

void pgy_mn_scheduler_set_deterministic_mode(PgyMnScheduler* scheduler, bool enabled,
                                   uint32_t seed)
{
    if (scheduler == NULL) {
        scheduler_warn("set_deterministic", "scheduler is null", scheduler);
        return;
    }
    scheduler->config.isDeterministic = enabled;
    scheduler->config.randomSeed = seed;
}

void pgy_mn_scheduler_get_stats(PgyMnScheduler* scheduler, PgyMnSchedulerStats* stats)
{
    uint64_t completed = 0;
    uint64_t stealAttempts = 0;
    uint64_t stealSuccesses = 0;
    uint64_t active;

    if (stats == NULL) {
        scheduler_warn("get_stats", "stats output is null", scheduler);
        return;
    }
    memset(stats, 0, sizeof(*stats));
    if (scheduler == NULL) {
        scheduler_warn("get_stats", "scheduler is null", scheduler);
        return;
    }

    if (scheduler->workers != NULL) {
        for (uint32_t i = 0; i < scheduler->numWorkers; i++) {
            completed += atomic_load(&scheduler->workers[i].tasksExecuted);
            stealAttempts += atomic_load(&scheduler->workers[i].stealAttempts);
            stealSuccesses += atomic_load(&scheduler->workers[i].stealSuccesses);
        }
    }
    active = atomic_load(&scheduler->activeFibers);
    stats->totalFibersCompleted = completed;
    stats->totalFibersCreated = (UINT64_MAX - completed < active)
        ? UINT64_MAX
        : completed + active;
    stats->totalStealAttempts = stealAttempts;
    stats->totalStealSuccesses = stealSuccesses;
    stats->totalContextSwitches = completed;
    stats->totalIoEvents = 0;
}
