/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * AsyncScope for structured concurrency in Pergyra's SEA model
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_ASYNC_SCOPE_H
#define PERGYRA_ASYNC_SCOPE_H

#include <stdbool.h>
#include "fiber.h"

/* Forward declarations */
typedef struct AsyncScope AsyncScope;

/* Scope cancellation token */
typedef struct CancellationToken {
    atomic_bool isCancelled;
    AsyncScope* scope;
} CancellationToken;

/* AsyncScope - manages lifetime of child fibers */
struct AsyncScope {
    /* Fiber management */
    Fiber** fibers;
    size_t fiberCount;
    size_t fiberCapacity;
    pthread_mutex_t fiberListMutex;
    
    /* Cancellation */
    CancellationToken cancellationToken;
    
    /* Parent scope for nested scopes */
    AsyncScope* parentScope;
    
    /* Error handling */
    bool hasError;
    char* firstError;
    
    /* Lifecycle */
    bool isDisposed;
    pthread_mutex_t disposeMutex;
    
    /* Statistics */
    atomic_uint64_t totalSpawned;
    atomic_uint64_t totalCompleted;
    atomic_uint64_t totalFailed;
};

/* AsyncScope lifecycle - BSD style with PascalCase */
AsyncScope* AsyncScopeCreate(AsyncScope* parent);
void AsyncScopeDestroy(AsyncScope* scope);

/* Fiber spawning within scope */
Fiber* AsyncScopeSpawn(AsyncScope* scope, FiberStartRoutine work, void* arg);
Fiber* AsyncScopeSpawnWithPriority(AsyncScope* scope, FiberStartRoutine work, void* arg, uint32_t priority);

/* Cancellation */
void AsyncScopeCancel(AsyncScope* scope);
bool AsyncScopeIsCancelled(AsyncScope* scope);
CancellationToken* AsyncScopeGetCancellationToken(AsyncScope* scope);

/* Waiting for completion */
void AsyncScopeWaitAll(AsyncScope* scope);
bool AsyncScopeWaitAllWithTimeout(AsyncScope* scope, uint64_t timeoutNs);

/* Error handling */
bool AsyncScopeHasError(AsyncScope* scope);
const char* AsyncScopeGetFirstError(AsyncScope* scope);

/* Nested scope support */
AsyncScope* AsyncScopeCreateNested(AsyncScope* parent);

/* Utility functions for common patterns */

/* Execute multiple tasks in parallel and wait for all */
typedef struct ParallelTask {
    FiberStartRoutine routine;
    void* arg;
} ParallelTask;

void AsyncScopeParallelFor(AsyncScope* scope, ParallelTask* tasks, size_t taskCount);

/* Execute tasks and return when first completes */
typedef struct RaceTask {
    FiberStartRoutine routine;
    void* arg;
    void* result;
    bool completed;
} RaceTask;

int AsyncScopeRace(AsyncScope* scope, RaceTask* tasks, size_t taskCount);

/* Fan-out/fan-in pattern */
typedef void* (*MapFunction)(void* input);
typedef void* (*ReduceFunction)(void* acc, void* value);

void* AsyncScopeMapReduce(
    AsyncScope* scope,
    void** inputs,
    size_t inputCount,
    MapFunction mapper,
    ReduceFunction reducer,
    void* initial
);

#endif /* PERGYRA_ASYNC_SCOPE_H */
