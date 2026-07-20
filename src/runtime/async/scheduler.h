/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * PgyMnScheduler implementation for Pergyra's Structured Effect Async (SEA) model
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_SCHEDULER_H
#define PERGYRA_SCHEDULER_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include "fiber.h"
#include "concurrent_queue.h"

/* PgyMnScheduler configuration */
typedef struct PgyMnSchedulerConfig {
    uint32_t numWorkers;
    bool isDeterministic;      /* For testing */
    uint32_t randomSeed;       /* For deterministic mode */
    size_t stackSizeHint;
    bool enableWorkStealing;
} PgyMnSchedulerConfig;

/* Worker thread state */
typedef struct PgyMnWorker {
    uint32_t id;
    pthread_t osThread;
    struct PgyMnScheduler* scheduler;
    
    /* Local run queue for better cache locality */
    PgyMnQueue* localRunQueue;
    
    /* Current fiber */
    PgyMnFiber* currentFiber;
    
    /* Statistics */
    atomic_uint_least64_t tasksExecuted;
    atomic_uint_least64_t stealAttempts;
    atomic_uint_least64_t stealSuccesses;
    
    /* Worker state */
    atomic_bool shouldStop;
    atomic_bool isParked;      /* Sleeping due to no work */
} PgyMnWorker;

/* PgyMnScheduler structure */
typedef struct PgyMnScheduler {
    /* Configuration */
    PgyMnSchedulerConfig config;
    
    /* Workers */
    uint32_t numWorkers;
    PgyMnWorker* workers;
    
    /* Global queue for new fibers */
    PgyMnQueue* globalRunQueue;
    
    /* I/O and timer handling */
#if defined(__linux__)
    int epollFd;              /* Linux epoll */
    pthread_t ioWorker;       /* Dedicated I/O thread */
#endif
    
    /* PgyMnScheduler state */
    atomic_bool isRunning;
    atomic_uint_least64_t totalFibers;
    atomic_uint_least64_t activeFibers;
    
    /* Work stealing */
    atomic_uint_least32_t stealingVictim;  /* Round-robin victim selection */
    
    /* Parking/waking workers */
    pthread_mutex_t parkMutex;
    pthread_cond_t parkCondition;
    atomic_uint_least32_t parkedWorkers;
} PgyMnScheduler;

/* PgyMnScheduler lifecycle - BSD style with PascalCase */
PgyMnScheduler* pgy_mn_scheduler_create(const PgyMnSchedulerConfig* config);
void pgy_mn_scheduler_destroy(PgyMnScheduler* scheduler);

/* PgyMnScheduler control */
void pgy_mn_scheduler_start(PgyMnScheduler* scheduler);
void pgy_mn_scheduler_stop(PgyMnScheduler* scheduler);

/* PgyMnFiber scheduling */
void pgy_mn_scheduler_spawn(PgyMnScheduler* scheduler, PgyMnFiberFn routine, void* arg);
void pgy_mn_scheduler_spawn_with_priority(PgyMnScheduler* scheduler, PgyMnFiberFn routine, void* arg, uint32_t priority);
bool pgy_mn_scheduler_enqueue_fiber_with_priority(PgyMnScheduler* scheduler, PgyMnFiber* fiber, uint32_t priority);

/* Called by fibers */
void pgy_mn_scheduler_yield(void);
void pgy_mn_scheduler_block(PgyMnFiber* fiber);
void pgy_mn_scheduler_unblock(PgyMnFiber* fiber);

/* Work stealing */
bool pgy_mn_scheduler_steal_work(PgyMnWorker* thief);

/* I/O and timer integration */
void pgy_mn_scheduler_register_io_event(PgyMnScheduler* scheduler, int fd, uint32_t events, PgyMnFiber* fiber);
void pgy_mn_scheduler_unregister_io_event(PgyMnScheduler* scheduler, int fd);
void pgy_mn_scheduler_schedule_timer(PgyMnScheduler* scheduler, uint64_t deadlineNs, PgyMnFiber* fiber);

/* Deterministic testing support */
void pgy_mn_scheduler_set_deterministic_mode(PgyMnScheduler* scheduler, bool enabled, uint32_t seed);

/* Statistics */
typedef struct PgyMnSchedulerStats {
    uint64_t totalFibersCreated;
    uint64_t totalFibersCompleted;
    uint64_t totalContextSwitches;
    uint64_t totalStealAttempts;
    uint64_t totalStealSuccesses;
    uint64_t totalIoEvents;
} PgyMnSchedulerStats;

void pgy_mn_scheduler_get_stats(PgyMnScheduler* scheduler, PgyMnSchedulerStats* stats);

/* Thread-local access to current scheduler and fiber */
PgyMnScheduler* pgy_mn_scheduler_get_current(void);
void pgy_mn_scheduler_set_current(PgyMnScheduler* scheduler);

#endif /* PERGYRA_SCHEDULER_H */
