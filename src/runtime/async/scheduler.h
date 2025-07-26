/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Scheduler implementation for Pergyra's Structured Effect Async (SEA) model
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_SCHEDULER_H
#define PERGYRA_SCHEDULER_H

#include <pthread.h>
#include <stdatomic.h>
#include "fiber.h"
#include "concurrent_queue.h"

/* Scheduler configuration */
typedef struct SchedulerConfig {
    uint32_t numWorkers;
    bool isDeterministic;      /* For testing */
    uint32_t randomSeed;       /* For deterministic mode */
    size_t stackSizeHint;
    bool enableWorkStealing;
} SchedulerConfig;

/* Worker thread state */
typedef struct WorkerThread {
    uint32_t id;
    pthread_t osThread;
    struct Scheduler* scheduler;
    
    /* Local run queue for better cache locality */
    ConcurrentQueue* localRunQueue;
    
    /* Current fiber */
    Fiber* currentFiber;
    
    /* Statistics */
    atomic_uint64_t tasksExecuted;
    atomic_uint64_t stealAttempts;
    atomic_uint64_t stealSuccesses;
    
    /* Worker state */
    atomic_bool shouldStop;
    atomic_bool isParked;      /* Sleeping due to no work */
} WorkerThread;

/* Scheduler structure */
typedef struct Scheduler {
    /* Configuration */
    SchedulerConfig config;
    
    /* Workers */
    uint32_t numWorkers;
    WorkerThread* workers;
    
    /* Global queue for new fibers */
    ConcurrentQueue* globalRunQueue;
    
    /* I/O and timer handling */
    int epollFd;              /* Linux epoll */
    pthread_t ioWorker;       /* Dedicated I/O thread */
    
    /* Scheduler state */
    atomic_bool isRunning;
    atomic_uint64_t totalFibers;
    atomic_uint64_t activeFibers;
    
    /* Work stealing */
    atomic_uint32_t stealingVictim;  /* Round-robin victim selection */
    
    /* Parking/waking workers */
    pthread_mutex_t parkMutex;
    pthread_cond_t parkCondition;
    atomic_uint32_t parkedWorkers;
} Scheduler;

/* Scheduler lifecycle - BSD style with PascalCase */
Scheduler* SchedulerCreate(const SchedulerConfig* config);
void SchedulerDestroy(Scheduler* scheduler);

/* Scheduler control */
void SchedulerStart(Scheduler* scheduler);
void SchedulerStop(Scheduler* scheduler);

/* Fiber scheduling */
void SchedulerSpawn(Scheduler* scheduler, FiberStartRoutine routine, void* arg);
void SchedulerSpawnWithPriority(Scheduler* scheduler, FiberStartRoutine routine, void* arg, uint32_t priority);

/* Called by fibers */
void SchedulerYield(void);
void SchedulerBlock(Fiber* fiber);
void SchedulerUnblock(Fiber* fiber);

/* Work stealing */
bool SchedulerStealWork(WorkerThread* thief);

/* I/O and timer integration */
void SchedulerRegisterIoEvent(Scheduler* scheduler, int fd, uint32_t events, Fiber* fiber);
void SchedulerUnregisterIoEvent(Scheduler* scheduler, int fd);
void SchedulerScheduleTimer(Scheduler* scheduler, uint64_t deadlineNs, Fiber* fiber);

/* Deterministic testing support */
void SchedulerSetDeterministicMode(Scheduler* scheduler, bool enabled, uint32_t seed);

/* Statistics */
typedef struct SchedulerStats {
    uint64_t totalFibersCreated;
    uint64_t totalFibersCompleted;
    uint64_t totalContextSwitches;
    uint64_t totalStealAttempts;
    uint64_t totalStealSuccesses;
    uint64_t totalIoEvents;
} SchedulerStats;

void SchedulerGetStats(Scheduler* scheduler, SchedulerStats* stats);

/* Thread-local access to current scheduler and fiber */
Scheduler* SchedulerGetCurrent(void);
void SchedulerSetCurrent(Scheduler* scheduler);

#endif /* PERGYRA_SCHEDULER_H */
