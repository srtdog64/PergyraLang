/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * AsyncScope parallel-pattern helpers.
 */

#include "async_scope.h"

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
