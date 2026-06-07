/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * AsyncScope parallel-pattern helpers.
 */

#include <stdlib.h>
#include <stdio.h>

#include "async_scope.h"

static void
async_scope_patterns_warn(const char* op, const char* reason,
                          AsyncScope* scope)
{
    fprintf(stderr,
        "[pgy][async_scope_patterns] %s failed: %s (scope=%p)\n",
        op != NULL ? op : "operation",
        reason != NULL ? reason : "unknown",
        (void*)scope);
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
        if (AsyncScopeSpawn(scope, ParallelForWorker, &state) == NULL) {
            async_scope_patterns_warn("parallel_for", "worker spawn failed",
                scope);
            AsyncScopeCancel(scope);
            break;
        }
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

typedef struct RaceWorkerArg {
    RaceState* state;
    size_t index;
} RaceWorkerArg;

static void RaceWorker(void* arg)
{
    RaceWorkerArg* worker = (RaceWorkerArg*)arg;
    RaceState* state = worker != NULL ? worker->state : NULL;
    int myIndex = worker != NULL ? (int)worker->index : -1;
    
    if (state != NULL && myIndex >= 0 && myIndex < (int)state->taskCount) {
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
    RaceWorkerArg* workerArgs = NULL;
    int winner;

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

    if (taskCount > SIZE_MAX / sizeof(*workerArgs)) {
        AsyncScopeDestroy(raceScope);
        return -1;
    }
    workerArgs = (RaceWorkerArg*)calloc(taskCount, sizeof(*workerArgs));
    if (workerArgs == NULL) {
        AsyncScopeDestroy(raceScope);
        return -1;
    }
    
    /* Initialize tasks */
    for (size_t i = 0; i < taskCount; i++) {
        tasks[i].completed = false;
    }
    
    /* Spawn all tasks */
    for (size_t i = 0; i < taskCount; i++) {
        workerArgs[i].state = &state;
        workerArgs[i].index = i;
        if (AsyncScopeSpawn(raceScope, RaceWorker, &workerArgs[i]) == NULL) {
            async_scope_patterns_warn("race", "worker spawn failed",
                raceScope);
            AsyncScopeCancel(raceScope);
            break;
        }
    }
    
    /* Wait for winner */
    AsyncScopeWaitAll(raceScope);
    
    winner = atomic_load(&state.winnerIndex);
    
    /* Clean up */
    AsyncScopeDestroy(raceScope);
    free(workerArgs);
    
    return winner;
}

typedef struct MapReduceWorkerArg {
    MapFunction mapper;
    void* input;
    void** output;
} MapReduceWorkerArg;

static void MapReduceWorker(void* arg)
{
    MapReduceWorkerArg* worker = (MapReduceWorkerArg*)arg;

    if (worker == NULL || worker->mapper == NULL || worker->output == NULL)
        return;
    *worker->output = worker->mapper(worker->input);
}

void*
AsyncScopeMapReduce(AsyncScope* scope, void** inputs, size_t inputCount,
                    MapFunction mapper, ReduceFunction reducer, void* initial)
{
    MapReduceWorkerArg* workerArgs = NULL;
    ParallelTask* tasks = NULL;
    void** mapped = NULL;
    void* acc = initial;

    if (scope == NULL || mapper == NULL || reducer == NULL)
        return initial;
    if (inputCount == 0)
        return initial;
    if (inputs == NULL)
        return initial;
    if (inputCount > SIZE_MAX / sizeof(*workerArgs)
        || inputCount > SIZE_MAX / sizeof(*tasks)
        || inputCount > SIZE_MAX / sizeof(*mapped)) {
        async_scope_patterns_warn("map_reduce", "array size overflow", scope);
        return initial;
    }

    workerArgs = (MapReduceWorkerArg*)calloc(inputCount, sizeof(*workerArgs));
    tasks = (ParallelTask*)calloc(inputCount, sizeof(*tasks));
    mapped = (void**)calloc(inputCount, sizeof(*mapped));
    if (workerArgs == NULL || tasks == NULL || mapped == NULL) {
        async_scope_patterns_warn("map_reduce", "allocation failed", scope);
        free(workerArgs);
        free(tasks);
        free(mapped);
        return initial;
    }

    for (size_t i = 0; i < inputCount; i++) {
        workerArgs[i].mapper = mapper;
        workerArgs[i].input = inputs[i];
        workerArgs[i].output = &mapped[i];
        tasks[i].routine = MapReduceWorker;
        tasks[i].arg = &workerArgs[i];
    }

    AsyncScopeParallelFor(scope, tasks, inputCount);

    for (size_t i = 0; i < inputCount; i++)
        acc = reducer(acc, mapped[i]);

    free(workerArgs);
    free(tasks);
    free(mapped);
    return acc;
}
