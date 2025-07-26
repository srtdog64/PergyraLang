/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Async runtime integration header
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_ASYNC_RUNTIME_H
#define PERGYRA_ASYNC_RUNTIME_H

#include "fiber.h"
#include "scheduler.h"
#include "effects.h"
#include "async_scope.h"
#include "channel.h"

/* Runtime initialization */
typedef struct AsyncRuntimeConfig {
    uint32_t numWorkers;        /* 0 for CPU count */
    size_t defaultStackSize;    /* 0 for default */
    bool enableTracing;
    bool enableStatistics;
    bool deterministicMode;     /* For testing */
    uint32_t randomSeed;        /* For deterministic mode */
} AsyncRuntimeConfig;

/* Global async runtime management */
void AsyncRuntimeInitialize(const AsyncRuntimeConfig* config);
void AsyncRuntimeShutdown(void);
bool AsyncRuntimeIsInitialized(void);

/* Convenience functions for common patterns */

/* async/await pattern helpers */
typedef struct AsyncTask {
    Fiber* fiber;
    void* result;
    bool completed;
    char* error;
} AsyncTask;

AsyncTask* AsyncTaskRun(FiberStartRoutine routine, void* arg);
void* AsyncTaskAwait(AsyncTask* task);
void AsyncTaskCancel(AsyncTask* task);
void AsyncTaskDestroy(AsyncTask* task);

/* Parallel execution helpers */
void ParallelForEach(void** items, size_t count, void (*processor)(void* item));
void* ParallelMap(void** inputs, size_t count, void* (*mapper)(void* input));

/* Async iterators */
typedef struct AsyncIterator {
    bool (*MoveNext)(struct AsyncIterator* self);
    void* (*GetCurrent)(struct AsyncIterator* self);
    void (*Destroy)(struct AsyncIterator* self);
    void* userData;
} AsyncIterator;

/* Timer utilities */
void AsyncSleep(uint64_t milliseconds);
AsyncTask* AsyncAfter(uint64_t milliseconds, FiberStartRoutine routine, void* arg);

/* Error handling patterns */
typedef struct AsyncResult {
    bool isSuccess;
    void* value;
    char* error;
} AsyncResult;

AsyncResult AsyncTry(FiberStartRoutine routine, void* arg);

/* Async state machine support for compiler */
typedef struct AsyncStateMachine {
    int state;
    void* locals;
    Fiber* fiber;
    void (*moveNext)(struct AsyncStateMachine* self);
} AsyncStateMachine;

/* Future/Promise pattern */
typedef struct Future Future;
typedef struct Promise Promise;

Promise* PromiseCreate(void);
Future* PromiseGetFuture(Promise* promise);
void PromiseSetValue(Promise* promise, void* value);
void PromiseSetError(Promise* promise, const char* error);
void PromiseDestroy(Promise* promise);

void* FutureGet(Future* future);
bool FutureIsReady(Future* future);
void FutureDestroy(Future* future);

/* Actor pattern support */
typedef struct Actor {
    AsyncScope* scope;
    Channel* mailbox;
    void (*messageHandler)(struct Actor* self, void* message);
    void* state;
} Actor;

Actor* ActorCreate(void (*handler)(Actor* self, void* message), void* initialState);
void ActorSend(Actor* actor, void* message);
void ActorStop(Actor* actor);
void ActorDestroy(Actor* actor);

/* Structured logging for async operations */
typedef enum {
    ASYNC_LOG_DEBUG,
    ASYNC_LOG_INFO,
    ASYNC_LOG_WARNING,
    ASYNC_LOG_ERROR
} AsyncLogLevel;

void AsyncLog(AsyncLogLevel level, const char* format, ...);
void AsyncLogFiber(Fiber* fiber, AsyncLogLevel level, const char* format, ...);

/* Performance monitoring */
typedef struct AsyncPerfStats {
    uint64_t totalFibers;
    uint64_t activeFibers;
    uint64_t contextSwitches;
    uint64_t channelOps;
    uint64_t effectsProcessed;
    double avgFiberLifetimeMs;
    double avgContextSwitchNs;
} AsyncPerfStats;

void AsyncGetPerfStats(AsyncPerfStats* stats);
void AsyncResetPerfStats(void);

#endif /* PERGYRA_ASYNC_RUNTIME_H */
