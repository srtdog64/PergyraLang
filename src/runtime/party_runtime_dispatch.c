/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Party runtime parallel dispatch owner.
 */

#include "party_runtime_internal.h"

#include <stdatomic.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif

typedef struct {
    FiberMapEntry* entry;
    PartyContext* context;
    void* roleInstance;
    FiberResult* result;
    atomic_bool* stopFlag;
    atomic_bool* completedFlag;
} PartyDispatchThreadData;

static void
party_dispatch_sleep_ms(uint32_t milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec req;
    req.tv_sec = milliseconds / 1000U;
    req.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
    nanosleep(&req, NULL);
#endif
}

static void
party_dispatch_yield(void)
{
#ifdef _WIN32
    Sleep(0);
#else
    sched_yield();
#endif
}

static void*
PartyDispatchThreadMain(void* userData)
{
    PartyDispatchThreadData* data = (PartyDispatchThreadData*)userData;
    if (data == NULL || data->entry == NULL || data->entry->parallelFn == NULL
        || data->result == NULL) {
        return NULL;
    }

    uint64_t startTime = GetTimeNanos();

    if (!data->entry->isContinuous) {
        data->entry->parallelFn(data->roleInstance, data->context);
        data->result->success = true;
        data->result->executionTimeNs = GetTimeNanos() - startTime;
        atomic_store(data->completedFlag, true);
        return NULL;
    }

    bool markedComplete = false;
    while (!atomic_load(data->stopFlag)) {
        data->entry->parallelFn(data->roleInstance, data->context);
        if (!markedComplete) {
            data->result->success = true;
            atomic_store(data->completedFlag, true);
            markedComplete = true;
        }

        if (data->entry->executionIntervalMs > 0) {
            party_dispatch_sleep_ms(data->entry->executionIntervalMs);
        } else {
            party_dispatch_yield();
        }
    }

    if (!markedComplete) {
        data->result->success = true;
        atomic_store(data->completedFlag, true);
    }

    data->result->executionTimeNs = GetTimeNanos() - startTime;
    return NULL;
}

static void
party_dispatch_collect_status(const DispatchResult* result,
                              const atomic_bool* completedFlags,
                              size_t count,
                              size_t* completedCount,
                              size_t* successCount)
{
    *completedCount = 0;
    *successCount = 0;

    for (size_t i = 0; i < count; i++) {
        if (atomic_load(&completedFlags[i])) {
            (*completedCount)++;
            if (result->results[i].success) {
                (*successCount)++;
            }
        }
    }
}

static bool
party_dispatch_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

DispatchResult
DispatchParallel(FiberMap* map,
                 PartyContext* context,
                 JoinStrategy joinStrategy,
                 DispatcherConfig* config)
{
    DispatchResult result = {0};
    (void)config;

    if (map == NULL || context == NULL || map->entryCount == 0) {
        party_runtime_warn("dispatch_parallel", "map/context missing or entryCount is zero");
        return result;
    }
    if (!party_dispatch_array_fits(map->entryCount, sizeof(FiberResult))
        || !party_dispatch_array_fits(map->entryCount, sizeof(pthread_t))
        || !party_dispatch_array_fits(map->entryCount, sizeof(bool))
        || !party_dispatch_array_fits(map->entryCount, sizeof(atomic_bool))
        || !party_dispatch_array_fits(map->entryCount, sizeof(PartyDispatchThreadData))) {
        party_runtime_warn("dispatch_parallel", "dispatcher allocation size overflow");
        return result;
    }

    result.results = (FiberResult*)calloc(map->entryCount, sizeof(FiberResult));
    if (result.results == NULL) {
        party_runtime_warn("dispatch_parallel", "result allocation failed");
        return result;
    }
    result.resultCount = map->entryCount;

    if (joinStrategy == JOIN_CUSTOM) {
        uint64_t dispatchStartTime = GetTimeNanos();
        party_runtime_warn("dispatch_parallel",
                           "JOIN_CUSTOM is unsupported without a custom join contract");
        for (size_t i = 0; i < map->entryCount; i++) {
            result.results[i].roleId = map->entries[i].roleId;
            result.results[i].success = false;
            result.results[i].error =
                "JOIN_CUSTOM unsupported: no custom join contract configured";
        }
        result.allSucceeded = false;
        result.totalExecutionTimeNs = GetTimeNanos() - dispatchStartTime;
        return result;
    }

    pthread_t* threads = (pthread_t*)calloc(map->entryCount, sizeof(pthread_t));
    bool* threadStarted = (bool*)calloc(map->entryCount, sizeof(bool));
    atomic_bool* stopFlags = (atomic_bool*)calloc(map->entryCount, sizeof(atomic_bool));
    atomic_bool* completedFlags = (atomic_bool*)calloc(map->entryCount, sizeof(atomic_bool));
    PartyDispatchThreadData* threadData =
        (PartyDispatchThreadData*)calloc(map->entryCount, sizeof(PartyDispatchThreadData));

    if (threads == NULL || threadStarted == NULL || stopFlags == NULL
        || completedFlags == NULL || threadData == NULL) {
        free(result.results);
        free(threads);
        free(threadStarted);
        free(stopFlags);
        free(completedFlags);
        free(threadData);
        result.results = NULL;
        result.resultCount = 0;
        party_runtime_warn("dispatch_parallel", "dispatcher allocation failed");
        return result;
    }

    uint64_t dispatchStartTime = GetTimeNanos();

    for (size_t i = 0; i < map->entryCount; i++) {
        FiberMapEntry* entry = &map->entries[i];
        result.results[i].roleId = entry->roleId;
        result.results[i].success = false;

        void* roleInstance = party_context_role_instance_by_slot(context, entry->instanceSlotId);
        if (roleInstance == NULL) {
            result.results[i].error = "Role instance unavailable";
            atomic_store(&completedFlags[i], true);
            party_runtime_warn("dispatch_parallel", "role instance unavailable");
            continue;
        }

        if (GetSchedulerForTag(entry->schedulerTag) == NULL
            && entry->schedulerTag != SCHEDULER_ANY) {
            result.results[i].error = "Scheduler tag unresolved";
            atomic_store(&completedFlags[i], true);
            party_runtime_warn("dispatch_parallel",
                               "scheduler tag unresolved");
            continue;
        }

        threadData[i].entry = entry;
        threadData[i].context = context;
        threadData[i].roleInstance = roleInstance;
        threadData[i].result = &result.results[i];
        threadData[i].stopFlag = &stopFlags[i];
        threadData[i].completedFlag = &completedFlags[i];

        if (pthread_create(&threads[i],
                           NULL,
                           PartyDispatchThreadMain,
                           &threadData[i]) != 0) {
            result.results[i].error = "Thread creation failed";
            atomic_store(&completedFlags[i], true);
            party_runtime_warn("dispatch_parallel", "thread creation failed");
            continue;
        }

        threadStarted[i] = true;
    }

    size_t completedCount = 0;
    size_t successCount = 0;
    const size_t requiredMajority = (map->entryCount / 2U) + 1U;

    while (true) {
        party_dispatch_collect_status(&result,
                                      completedFlags,
                                      map->entryCount,
                                      &completedCount,
                                      &successCount);

        bool joinSatisfied = false;
        switch (joinStrategy) {
            case JOIN_ALL:
                joinSatisfied = (completedCount == map->entryCount);
                break;
            case JOIN_ANY:
                joinSatisfied = (completedCount > 0);
                break;
            case JOIN_RACE:
                joinSatisfied = (successCount > 0);
                break;
            case JOIN_MAJORITY:
                joinSatisfied = (successCount >= requiredMajority);
                break;
            case JOIN_CUSTOM:
                joinSatisfied = (completedCount == map->entryCount);
                break;
        }

        if (joinSatisfied || completedCount == map->entryCount) {
            break;
        }

        party_dispatch_sleep_ms(1);
    }

    for (size_t i = 0; i < map->entryCount; i++) {
        if (map->entries[i].isContinuous) {
            atomic_store(&stopFlags[i], true);
        }
    }

    for (size_t i = 0; i < map->entryCount; i++) {
        if (threadStarted[i]) {
            pthread_join(threads[i], NULL);
        }
    }

    party_dispatch_collect_status(&result,
                                  completedFlags,
                                  map->entryCount,
                                  &completedCount,
                                  &successCount);

    switch (joinStrategy) {
        case JOIN_ALL:
            result.allSucceeded = (successCount == map->entryCount);
            break;
        case JOIN_ANY:
            result.allSucceeded = (successCount > 0);
            break;
        case JOIN_RACE:
            result.allSucceeded = (successCount > 0);
            break;
        case JOIN_MAJORITY:
            result.allSucceeded = (successCount >= requiredMajority);
            break;
        case JOIN_CUSTOM:
            result.allSucceeded = (successCount == map->entryCount);
            break;
    }

    result.totalExecutionTimeNs = GetTimeNanos() - dispatchStartTime;

    for (size_t i = 0; i < map->entryCount; i++) {
        UpdateFiberStats(result.results[i].roleId, &result.results[i]);
    }

    free(threads);
    free(threadStarted);
    free(stopFlags);
    free(completedFlags);
    free(threadData);

    return result;
}
