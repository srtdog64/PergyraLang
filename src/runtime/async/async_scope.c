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
#include <stdio.h>
#include "async_scope.h"
#include "scheduler.h"

/* Initial capacity for fiber list */
#define INITIAL_FIBER_CAPACITY 16

static void
async_scope_warn(const char* op, const char* reason, AsyncScope* scope)
{
    fprintf(stderr,
        "[pgy][async_scope] %s failed: %s (scope=%p)\n",
        op != NULL ? op : "operation",
        reason != NULL ? reason : "unknown",
        (void*)scope);
}

static char*
async_scope_strdup(const char* text)
{
    size_t length;
    char* copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }
    length++;
    copy = (char*)malloc(length);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    return copy;
}

AsyncScope* AsyncScopeCreate(AsyncScope* parent)
{
    AsyncScope* scope = (AsyncScope*)calloc(1, sizeof(AsyncScope));
    if (scope == NULL) {
        async_scope_warn("create", "scope allocation failed", parent);
        return NULL;
    }
    
    /* Initialize fiber list */
    scope->fiberCapacity = INITIAL_FIBER_CAPACITY;
    if (scope->fiberCapacity > SIZE_MAX / sizeof(Fiber*)) {
        async_scope_warn("create", "fiber list allocation size overflow", parent);
        free(scope);
        return NULL;
    }
    scope->fibers = (Fiber**)calloc(scope->fiberCapacity, sizeof(Fiber*));
    if (scope->fibers == NULL) {
        async_scope_warn("create", "fiber list allocation failed", parent);
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
static bool AsyncScopeAddFiber(AsyncScope* scope, Fiber* fiber)
{
    bool added = false;

    pthread_mutex_lock(&scope->fiberListMutex);
    
    /* Resize array if needed */
    if (scope->fiberCount >= scope->fiberCapacity) {
        size_t newCapacity;
        Fiber** newFibers;

        if (scope->fiberCapacity > SIZE_MAX / 2
            || scope->fiberCapacity > SIZE_MAX / (2 * sizeof(Fiber*))) {
            async_scope_warn("add_fiber", "fiber list capacity overflow", scope);
            pthread_mutex_unlock(&scope->fiberListMutex);
            return false;
        }
        newCapacity = scope->fiberCapacity * 2;
        newFibers = (Fiber**)realloc(scope->fibers,
                                     newCapacity * sizeof(Fiber*));
        
        if (newFibers != NULL) {
            scope->fibers = newFibers;
            scope->fiberCapacity = newCapacity;
        } else {
            async_scope_warn("add_fiber", "fiber list growth failed", scope);
        }
    }
    
    /* Add fiber if there's space */
    if (scope->fiberCount < scope->fiberCapacity) {
        scope->fibers[scope->fiberCount++] = fiber;
        atomic_fetch_add(&scope->totalSpawned, 1);
        added = true;
    }
    
    pthread_mutex_unlock(&scope->fiberListMutex);
    return added;
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
                    scope->firstError = async_scope_strdup(fiber->errorMessage);
                }
            } else {
                atomic_fetch_add(&scope->totalCompleted, 1);
            }
            
            break;
        }
    }
    
    pthread_mutex_unlock(&scope->fiberListMutex);
}

static void AsyncScopeForgetFiber(AsyncScope* scope, Fiber* fiber)
{
    pthread_mutex_lock(&scope->fiberListMutex);
    for (size_t i = 0; i < scope->fiberCount; i++) {
        if (scope->fibers[i] == fiber) {
            scope->fibers[i] = scope->fibers[scope->fiberCount - 1];
            scope->fiberCount--;
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
        if (scope == NULL) {
            async_scope_warn("spawn", "scope is null", scope);
        } else if (work == NULL) {
            async_scope_warn("spawn", "work routine is null", scope);
        } else {
            async_scope_warn("spawn", "scope is disposed", scope);
        }
        return NULL;
    }
    
    /* Check if already cancelled */
    if (AsyncScopeIsCancelled(scope)) {
        async_scope_warn("spawn", "scope is cancelled", scope);
        return NULL;
    }
    
    /* Create wrapper data */
    ScopedFiberData* data = (ScopedFiberData*)malloc(sizeof(ScopedFiberData));
    if (data == NULL) {
        async_scope_warn("spawn", "wrapper allocation failed", scope);
        return NULL;
    }
    
    data->scope = scope;
    data->originalRoutine = work;
    data->originalArg = arg;
    
    /* Get current scheduler */
    Scheduler* scheduler = SchedulerGetCurrent();
    if (scheduler == NULL) {
        async_scope_warn("spawn", "no current scheduler", scope);
        free(data);
        return NULL;
    }
    
    /* Create fiber through scheduler */
    Fiber* fiber = FiberCreate(ScopedFiberWrapper, data);
    if (fiber == NULL) {
        async_scope_warn("spawn", "fiber creation failed", scope);
        free(data);
        return NULL;
    }
    
    /* Set parent fiber for structured concurrency */
    Fiber* currentFiber = FiberGetCurrent();
    if (currentFiber != NULL) {
        FiberAttachChild(currentFiber, fiber);
    }
    
    /* Add to scope tracking before enqueueing the exact same fiber. */
    if (!AsyncScopeAddFiber(scope, fiber)) {
        async_scope_warn("spawn", "scope tracking failed", scope);
        FiberDestroy(fiber);
        free(data);
        return NULL;
    }
    
    /* Schedule the fiber */
    if (!SchedulerEnqueueFiberWithPriority(scheduler, fiber, priority)) {
        async_scope_warn("spawn", "scheduler enqueue failed", scope);
        AsyncScopeForgetFiber(scope, fiber);
        FiberDestroy(fiber);
        free(data);
        return NULL;
    }
    
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
    if (timeoutNs == 0) {
        async_scope_warn("wait_timeout", "timeout is zero", scope);
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
            async_scope_warn("wait_timeout", "deadline reached before all fibers completed",
                scope);
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
