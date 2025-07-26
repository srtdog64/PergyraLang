/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * AsyncScope implementation for structured concurrency
 * BSD Style + C# naming conventions
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "async_scope.h"
#include "scheduler.h"

/* Initial capacity for fiber list */
#define INITIAL_FIBER_CAPACITY 16

AsyncScope* AsyncScopeCreate(AsyncScope* parent)
{
    AsyncScope* scope = (AsyncScope*)calloc(1, sizeof(AsyncScope));
    if (scope == NULL) {
        return NULL;
    }
    
    /* Initialize fiber list */
    scope->fiberCapacity = INITIAL_FIBER_CAPACITY;
    scope->fibers = (Fiber**)calloc(scope->fiberCapacity, sizeof(Fiber*));
    if (scope->fibers == NULL) {
        free(scope);
        return NULL;
    }
    
    /* Initialize mutex */
    pthread_mutex_init(&scope->fiberListMutex, NULL);
    pthread_mutex_init(&scope->disposeMutex, NULL);
    
    /* Set parent scope */
    scope->parentScope = parent;
    
    /* Initialize cancellation token */
    atomic_init(&scope->cancellationToken.isCancelled, false);
    scope->cancellationToken.scope = scope;
    
    return scope;
}

void AsyncScopeDestroy(AsyncScope* scope)
{
    if (scope == NULL) {
        return;
    }
    
    pthread_mutex_lock(&scope->disposeMutex);
    
    if (scope->isDisposed) {
        pthread_mutex_unlock(&scope->disposeMutex);
        return;
    }
    
    scope->isDisposed = true;
    
    /* Cancel all fibers */
    AsyncScopeCancel(scope);
    
    /* Wait for all fibers to complete */
    AsyncScopeWaitAll(scope);
    
    pthread_mutex_unlock(&scope->disposeMutex);
    
    /* Clean up resources */
    pthread_mutex_destroy(&scope->fiberListMutex);
    pthread_mutex_destroy(&scope->disposeMutex);
    
    if (scope->firstError != NULL) {
        free(scope->firstError);
    }
    
    free(scope->fibers);
    free(scope);
}

/* Internal function to add fiber to scope */
static void AsyncScopeAddFiber(AsyncScope* scope, Fiber* fiber)
{
    pthread_mutex_lock(&scope->fiberListMutex);
    
    /* Resize array if needed */
    if (scope->fiberCount >= scope->fiberCapacity) {
        size_t newCapacity = scope->fiberCapacity * 2;
        Fiber** newFibers = (Fiber**)realloc(scope->fibers, newCapacity * sizeof(Fiber*));
        
        if (newFibers != NULL) {
            scope->fibers = newFibers;
            scope->fiberCapacity = newCapacity;
        }
    }
    
    /* Add fiber if there's space */
    if (scope->fiberCount < scope->fiberCapacity) {
        scope->fibers[scope->fiberCount++] = fiber;
        atomic_fetch_add(&scope->totalSpawned, 1);
    }
    
    pthread_mutex_unlock(&scope->fiberListMutex);
}

/* Internal function to remove fiber from scope */
static void AsyncScopeRemoveFiber(AsyncScope* scope, Fiber* fiber)
{
    pthread_mutex_lock(&scope->fiberListMutex);
    
    for (size_t i = 0; i < scope->fiberCount; i++) {
        if (scope->fibers[i] == fiber) {
            /* Move last fiber to this position */
            scope->fibers[i] = scope->fibers[scope->fiberCount - 1];
            scope->fiberCount--;
            
            /* Update statistics */
            if (fiber->state == FIBER_STATE_ERROR) {
                atomic_fetch_add(&scope->totalFailed, 1);
                
                /* Record first error */
                if (!scope->hasError && fiber->errorMessage != NULL) {
                    scope->hasError = true;
                    scope->firstError = strdup(fiber->errorMessage);
                }
            } else {
                atomic_fetch_add(&scope->totalCompleted, 1);
            }
            
            break;
        }
    }
    
    pthread_mutex_unlock(&scope->fiberListMutex);
}

/* Fiber wrapper to track scope */
typedef struct ScopedFiberData {
    AsyncScope* scope;
    FiberStartRoutine originalRoutine;
    void* originalArg;
} ScopedFiberData;

static void ScopedFiberWrapper(void* arg)
{
    ScopedFiberData* data = (ScopedFiberData*)arg;
    AsyncScope* scope = data->scope;
    FiberStartRoutine routine = data->originalRoutine;
    void* originalArg = data->originalArg;
    
    /* Free wrapper data */
    free(data);
    
    /* Check cancellation before starting */
    if (AsyncScopeIsCancelled(scope)) {
        return;
    }
    
    /* Execute the original routine */
    routine(originalArg);
    
    /* Remove from scope when done */
    Fiber* currentFiber = FiberGetCurrent();
    AsyncScopeRemoveFiber(scope, currentFiber);
}

Fiber* AsyncScopeSpawn(AsyncScope* scope, FiberStartRoutine work, void* arg)
{
    return AsyncScopeSpawnWithPriority(scope, work, arg, 0);
}

Fiber* AsyncScopeSpawnWithPriority(AsyncScope* scope, FiberStartRoutine work, void* arg, uint32_t priority)
{
    if (scope == NULL || work == NULL || scope->isDisposed) {
        return NULL;
    }
    
    /* Check if already cancelled */
    if (AsyncScopeIsCancelled(scope)) {
        return NULL;
    }
    
    /* Create wrapper data */
    ScopedFiberData* data = (ScopedFiberData*)malloc(sizeof(ScopedFiberData));
    if (data == NULL) {
        return NULL;
    }
    
    data->scope = scope;
    data->originalRoutine = work;
    data->originalArg = arg;
    
    /* Get current scheduler */
    Scheduler* scheduler = SchedulerGetCurrent();
    if (scheduler == NULL) {
        free(data);
        return NULL;
    }
    
    /* Create fiber through scheduler */
    Fiber* fiber = FiberCreate(ScopedFiberWrapper, data);
    if (fiber == NULL) {
        free(data);
        return NULL;
    }
    
    /* Set parent fiber for structured concurrency */
    Fiber* currentFiber = FiberGetCurrent();
    if (currentFiber != NULL) {
        FiberAttachChild(currentFiber, fiber);
    }
    
    /* Add to scope tracking */
    AsyncScopeAddFiber(scope, fiber);
    
    /* Schedule the fiber */
    fiber->scheduler = scheduler;
    fiber->priority = priority;
    SchedulerSpawnWithPriority(scheduler, fiber->startRoutine, fiber->arg, priority);
    
    return fiber;
}

void AsyncScopeCancel(AsyncScope* scope)
{
    if (scope == NULL) {
        return;
    }
    
    /* Mark as cancelled */
    atomic_store(&scope->cancellationToken.isCancelled, true);
    
    /* Cancel all fibers */
    pthread_mutex_lock(&scope->fiberListMutex);
    
    for (size_t i = 0; i < scope->fiberCount; i++) {
        FiberCancel(scope->fibers[i]);
    }
    
    pthread_mutex_unlock(&scope->fiberListMutex);
}

bool AsyncScopeIsCancelled(AsyncScope* scope)
{
    if (scope == NULL) {
        return false;
    }
    
    /* Check own cancellation token */
    if (atomic_load(&scope->cancellationToken.isCancelled)) {
        return true;
    }
    
    /* Check parent scope */
    if (scope->parentScope != NULL) {
        return AsyncScopeIsCancelled(scope->parentScope);
    }
    
    return false;
}

CancellationToken* AsyncScopeGetCancellationToken(AsyncScope* scope)
{
    return scope != NULL ? &scope->cancellationToken : NULL;
}

void AsyncScopeWaitAll(AsyncScope* scope)
{
    if (scope == NULL) {
        return;
    }
    
    /* Wait for all fibers to complete */
    while (true) {
        pthread_mutex_lock(&scope->fiberListMutex);
        size_t count = scope->fiberCount;
        pthread_mutex_unlock(&scope->fiberListMutex);
        
        if (count == 0) {
            break;
        }
        
        /* Yield to allow other fibers to run */
        SchedulerYield();
    }
}

bool AsyncScopeWaitAllWithTimeout(AsyncScope* scope, uint64_t timeoutNs)
{
    if (scope == NULL) {
        return true;
    }
    
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (true) {
        pthread_mutex_lock(&scope->fiberListMutex);
        size_t count = scope->fiberCount;
        pthread_mutex_unlock(&scope->fiberListMutex);
        
        if (count == 0) {
            return true; /* All completed */
        }
        
        /* Check timeout */
        clock_gettime(CLOCK_MONOTONIC, &now);
        uint64_t elapsedNs = (now.tv_sec - start.tv_sec) * 1000000000ULL + 
                            (now.tv_nsec - start.tv_nsec);
        
        if (elapsedNs >= timeoutNs) {
            return false; /* Timeout */
        }
        
        /* Yield to allow other fibers to run */
        SchedulerYield();
    }
}

bool AsyncScopeHasError(AsyncScope* scope)
{
    return scope != NULL && scope->hasError;
}

const char* AsyncScopeGetFirstError(AsyncScope* scope)
{
    return scope != NULL ? scope->firstError : NULL;
}

AsyncScope* AsyncScopeCreateNested(AsyncScope* parent)
{
    return AsyncScopeCreate(parent);
}

/* Parallel execution helpers */
typedef struct ParallelForState {
    AsyncScope* scope;
    ParallelTask* tasks;
    size_t taskCount;
    atomic_size_t completed;
} ParallelForState;

static void ParallelForWorker(void* arg)
{
    ParallelForState* state = (ParallelForState*)arg;
    size_t index = atomic_fetch_add(&state->completed, 1);
    
    if (index < state->taskCount) {
        ParallelTask* task = &state->tasks[index];
        task->routine(task->arg);
    }
}

void AsyncScopeParallelFor(AsyncScope* scope, ParallelTask* tasks, size_t taskCount)
{
    if (scope == NULL || tasks == NULL || taskCount == 0) {
        return;
    }
    
    ParallelForState state = {
        .scope = scope,
        .tasks = tasks,
        .taskCount = taskCount,
        .completed = ATOMIC_VAR_INIT(0)
    };
    
    /* Spawn workers */
    for (size_t i = 0; i < taskCount; i++) {
        AsyncScopeSpawn(scope, ParallelForWorker, &state);
    }
    
    /* Wait for completion */
    AsyncScopeWaitAll(scope);
}

/* Race implementation */
typedef struct RaceState {
    AsyncScope* scope;
    RaceTask* tasks;
    size_t taskCount;
    atomic_int winnerIndex;
} RaceState;

static void RaceWorker(void* arg)
{
    RaceState* state = (RaceState*)arg;
    
    /* Find my task index */
    Fiber* currentFiber = FiberGetCurrent();
    int myIndex = -1;
    
    pthread_mutex_lock(&state->scope->fiberListMutex);
    for (size_t i = 0; i < state->scope->fiberCount; i++) {
        if (state->scope->fibers[i] == currentFiber) {
            myIndex = (int)i;
            break;
        }
    }
    pthread_mutex_unlock(&state->scope->fiberListMutex);
    
    if (myIndex >= 0 && myIndex < (int)state->taskCount) {
        RaceTask* task = &state->tasks[myIndex];
        
        /* Execute task */
        task->routine(task->arg);
        
        /* Try to be the winner */
        int expected = -1;
        if (atomic_compare_exchange_strong(&state->winnerIndex, &expected, myIndex)) {
            task->completed = true;
            
            /* Cancel other tasks */
            AsyncScopeCancel(state->scope);
        }
    }
}

int AsyncScopeRace(AsyncScope* scope, RaceTask* tasks, size_t taskCount)
{
    if (scope == NULL || tasks == NULL || taskCount == 0) {
        return -1;
    }
    
    /* Create nested scope for race */
    AsyncScope* raceScope = AsyncScopeCreateNested(scope);
    if (raceScope == NULL) {
        return -1;
    }
    
    RaceState state = {
        .scope = raceScope,
        .tasks = tasks,
        .taskCount = taskCount,
        .winnerIndex = ATOMIC_VAR_INIT(-1)
    };
    
    /* Initialize tasks */
    for (size_t i = 0; i < taskCount; i++) {
        tasks[i].completed = false;
    }
    
    /* Spawn all tasks */
    for (size_t i = 0; i < taskCount; i++) {
        AsyncScopeSpawn(raceScope, RaceWorker, &state);
    }
    
    /* Wait for winner */
    AsyncScopeWaitAll(raceScope);
    
    int winner = atomic_load(&state.winnerIndex);
    
    /* Clean up */
    AsyncScopeDestroy(raceScope);
    
    return winner;
}
