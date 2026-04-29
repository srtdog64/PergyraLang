/*
 * Copyright (c) 2025 Pergyra Language Project
 * World-Roster Runtime Implementation
 */

#include "world_roster.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif

struct RosterHandle {
    pthread_t thread;
    bool threadStarted;
    bool completed;
    RosterContext* roster;
    JoinStrategy defaultStrategy;
    DispatcherConfig* config;
    RosterExecutionResult result;
};

static void world_roster_warn(const char* op, const char* reason)
{
    fprintf(stderr,
            "[pgy][world-roster] %s failed: %s\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown");
}

static char* world_roster_strdup(const char* text)
{
    if (text == NULL) {
        return NULL;
    }
    size_t len = strlen(text) + 1U;
    char* copy = (char*)malloc(len);
    if (copy != NULL) {
        memcpy(copy, text, len);
    }
    return copy;
}

static uint64_t world_roster_now_ns(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static void world_roster_sleep_ms(uint64_t timeoutMs)
{
#ifdef _WIN32
    Sleep((DWORD)timeoutMs);
#else
    struct timespec req;
    req.tv_sec = (time_t)(timeoutMs / 1000ULL);
    req.tv_nsec = (long)((timeoutMs % 1000ULL) * 1000000ULL);
    nanosleep(&req, NULL);
#endif
}

static void world_roster_free_dispatch_result(DispatchResult* result)
{
    if (result == NULL) {
        return;
    }
    free(result->results);
    result->results = NULL;
    result->resultCount = 0;
    result->allSucceeded = false;
    result->totalExecutionTimeNs = 0;
}

static void world_roster_free_execution_result(RosterExecutionResult* result)
{
    if (result == NULL || result->partyResults == NULL) {
        return;
    }
    for (size_t i = 0; i < result->resultCount; i++) {
        world_roster_free_dispatch_result(&result->partyResults[i].result);
    }
    free(result->partyResults);
    result->partyResults = NULL;
    result->resultCount = 0;
    result->allSucceeded = false;
    result->totalExecutionTimeNs = 0;
}

static void world_roster_free_frame_result(WorldFrameResult* result)
{
    if (result == NULL || result->rosterResults == NULL) {
        return;
    }
    for (size_t i = 0; i < result->resultCount; i++) {
        world_roster_free_execution_result(&result->rosterResults[i].result);
    }
    free(result->rosterResults);
    result->rosterResults = NULL;
    result->resultCount = 0;
    result->allSucceeded = false;
    result->frameTimeNs = 0;
}

static const char* world_roster_first_error(const RosterExecutionResult* result)
{
    if (result == NULL || result->partyResults == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < result->resultCount; i++) {
        const DispatchResult* dispatch = &result->partyResults[i].result;
        if (dispatch->results == NULL) {
            continue;
        }
        for (size_t j = 0; j < dispatch->resultCount; j++) {
            if (!dispatch->results[j].success && dispatch->results[j].error != NULL) {
                return dispatch->results[j].error;
            }
        }
    }
    return NULL;
}

static void* world_roster_async_runner(void* arg)
{
    struct RosterHandle* handle = (struct RosterHandle*)arg;
    if (handle == NULL) {
        return NULL;
    }
    handle->result = ExecuteRoster(handle->roster, handle->defaultStrategy, handle->config);
    handle->completed = true;
    return NULL;
}

RosterContext*
CreateRoster(const char* rosterType, const char* instanceName)
{
    RosterContext* roster = (RosterContext*)calloc(1, sizeof(RosterContext));
    if (roster == NULL) {
        world_roster_warn("create_roster", "roster allocation failed");
        return NULL;
    }

    roster->systemType = world_roster_strdup(rosterType != NULL ? rosterType : "Roster");
    roster->name = world_roster_strdup(instanceName != NULL ? instanceName : rosterType);
    if (roster->systemType == NULL || roster->name == NULL) {
        free((void*)roster->systemType);
        free((void*)roster->name);
        free(roster);
        world_roster_warn("create_roster", "name allocation failed");
        return NULL;
    }

    return roster;
}

bool
RosterAddParty(RosterContext* roster,
               const char* slotName,
               void* partyInstance,
               PartyContext* partyContext)
{
    if (roster == NULL || slotName == NULL || partyContext == NULL) {
        world_roster_warn("roster_add_party", "roster/slotName/partyContext is null");
        return false;
    }

    size_t newCount = roster->partyCount + 1U;
    void* grown = realloc(roster->partySlots, newCount * sizeof(*roster->partySlots));
    if (grown == NULL) {
        world_roster_warn("roster_add_party", "party slot growth failed");
        return false;
    }
    roster->partySlots = grown;

    roster->partySlots[roster->partyCount].slotName = world_roster_strdup(slotName);
    roster->partySlots[roster->partyCount].partyType =
        world_roster_strdup(partyContext->partyName != NULL ? partyContext->partyName : slotName);
    if (roster->partySlots[roster->partyCount].slotName == NULL
        || roster->partySlots[roster->partyCount].partyType == NULL) {
        free((void*)roster->partySlots[roster->partyCount].slotName);
        free((void*)roster->partySlots[roster->partyCount].partyType);
        world_roster_warn("roster_add_party", "slot name allocation failed");
        return false;
    }

    roster->partySlots[roster->partyCount].partyInstance = partyInstance;
    roster->partySlots[roster->partyCount].partyContext = partyContext;
    roster->partySlots[roster->partyCount].isArray = false;
    roster->partySlots[roster->partyCount].arraySize = 0;
    roster->partyCount = newCount;
    return true;
}

RosterExecutionResult
ExecuteRoster(RosterContext* roster,
              JoinStrategy defaultStrategy,
              DispatcherConfig* config)
{
    RosterExecutionResult result = {0};
    if (roster == NULL) {
        world_roster_warn("execute_roster", "roster is null");
        return result;
    }

    result.partyResults =
        (RosterPartyResult*)calloc(roster->partyCount, sizeof(RosterPartyResult));
    if (result.partyResults == NULL && roster->partyCount > 0) {
        world_roster_warn("execute_roster", "result allocation failed");
        return result;
    }
    result.resultCount = roster->partyCount;
    result.allSucceeded = true;

    uint64_t start = world_roster_now_ns();
    for (size_t i = 0; i < roster->partyCount; i++) {
        result.partyResults[i].partySlot = roster->partySlots[i].slotName;

        PartyContext* context = roster->partySlots[i].partyContext;
        FiberMap* map = PartyContextGetFiberMap(context);
        if (context == NULL || map == NULL) {
            DispatchResult dispatch = {0};
            dispatch.results = (FiberResult*)calloc(1, sizeof(FiberResult));
            dispatch.resultCount = 1;
            dispatch.allSucceeded = false;
            if (dispatch.results != NULL) {
                dispatch.results[0].roleId = roster->partySlots[i].slotName;
                dispatch.results[0].success = false;
                dispatch.results[0].error = context == NULL
                                                ? "Party context missing"
                                                : "Party fiber map missing";
            }
            result.partyResults[i].result = dispatch;
            result.allSucceeded = false;
            continue;
        }

        result.partyResults[i].result = DispatchParallel(map, context, defaultStrategy, config);
        if (!result.partyResults[i].result.allSucceeded) {
            result.allSucceeded = false;
        }
        result.totalExecutionTimeNs += result.partyResults[i].result.totalExecutionTimeNs;
    }

    if (roster->partyCount == 0) {
        result.allSucceeded = true;
    }
    if (result.totalExecutionTimeNs == 0) {
        result.totalExecutionTimeNs = world_roster_now_ns() - start;
    }
    return result;
}

RosterHandle*
ExecuteRosterAsync(RosterContext* roster,
                   JoinStrategy defaultStrategy,
                   DispatcherConfig* config)
{
    if (roster == NULL) {
        world_roster_warn("execute_roster_async", "roster is null");
        return NULL;
    }

    struct RosterHandle* handle = (struct RosterHandle*)calloc(1, sizeof(struct RosterHandle));
    if (handle == NULL) {
        world_roster_warn("execute_roster_async", "handle allocation failed");
        return NULL;
    }

    handle->roster = roster;
    handle->defaultStrategy = defaultStrategy;
    handle->config = config;

    if (pthread_create(&handle->thread, NULL, world_roster_async_runner, handle) != 0) {
        free(handle);
        world_roster_warn("execute_roster_async", "thread creation failed");
        return NULL;
    }
    handle->threadStarted = true;
    return handle;
}

RosterExecutionResult
WaitForRoster(RosterHandle* handle, uint64_t timeoutMs)
{
    RosterExecutionResult empty = {0};
    if (handle == NULL) {
        world_roster_warn("wait_for_roster", "handle is null");
        return empty;
    }

    if (!handle->completed) {
        if (timeoutMs == 0) {
            while (!handle->completed) {
#ifdef _WIN32
                Sleep(0);
#else
                sched_yield();
#endif
            }
        } else {
            uint64_t start = world_roster_now_ns();
            while (!handle->completed) {
                if ((world_roster_now_ns() - start) / 1000000ULL >= timeoutMs) {
                    world_roster_warn("wait_for_roster", "timeout exceeded");
                    return empty;
                }
                world_roster_sleep_ms(1);
            }
        }
    }

    if (handle->threadStarted) {
        pthread_join(handle->thread, NULL);
        handle->threadStarted = false;
    }

    RosterExecutionResult result = handle->result;
    free(handle);
    return result;
}

WorldContext*
CreateWorld(const char* worldName)
{
    WorldContext* world = (WorldContext*)calloc(1, sizeof(WorldContext));
    if (world == NULL) {
        world_roster_warn("create_world", "world allocation failed");
        return NULL;
    }

    world->name = world_roster_strdup(worldName != NULL ? worldName : "World");
    if (world->name == NULL) {
        free(world);
        world_roster_warn("create_world", "world name allocation failed");
        return NULL;
    }

    world->startTime = world_roster_now_ns();
    return world;
}

bool
WorldAddRoster(WorldContext* world, const char* slotName, RosterContext* roster)
{
    if (world == NULL || slotName == NULL || roster == NULL) {
        world_roster_warn("world_add_roster", "world/slotName/roster is null");
        return false;
    }

    size_t newCount = world->rosterCount + 1U;
    void* grown = realloc(world->rosters, newCount * sizeof(*world->rosters));
    if (grown == NULL) {
        world_roster_warn("world_add_roster", "roster slot growth failed");
        return false;
    }
    world->rosters = grown;

    world->rosters[world->rosterCount].slotName = world_roster_strdup(slotName);
    world->rosters[world->rosterCount].rosterType =
        world_roster_strdup(roster->systemType != NULL ? roster->systemType : slotName);
    if (world->rosters[world->rosterCount].slotName == NULL
        || world->rosters[world->rosterCount].rosterType == NULL) {
        free((void*)world->rosters[world->rosterCount].slotName);
        free((void*)world->rosters[world->rosterCount].rosterType);
        world_roster_warn("world_add_roster", "roster slot allocation failed");
        return false;
    }

    world->rosters[world->rosterCount].instance = roster;
    world->rosterCount = newCount;
    return true;
}

WorldFrameResult
ExecuteWorldFrame(WorldContext* world, DispatcherConfig* config)
{
    WorldFrameResult result = {0};
    if (world == NULL) {
        world_roster_warn("execute_world_frame", "world is null");
        return result;
    }

    result.rosterResults =
        (WorldRosterResult*)calloc(world->rosterCount, sizeof(WorldRosterResult));
    if (result.rosterResults == NULL && world->rosterCount > 0) {
        world_roster_warn("execute_world_frame", "result allocation failed");
        return result;
    }
    result.resultCount = world->rosterCount;
    result.allSucceeded = true;

    uint64_t start = world_roster_now_ns();
    for (size_t i = 0; i < world->rosterCount; i++) {
        result.rosterResults[i].rosterSlot = world->rosters[i].slotName;
        result.rosterResults[i].result =
            ExecuteRoster(world->rosters[i].instance, JOIN_ALL, config);
        if (!result.rosterResults[i].result.allSucceeded) {
            result.allSucceeded = false;
        }
    }

    result.frameTimeNs = world_roster_now_ns() - start;
    world->frameCount++;
    result.totalFrames = world->frameCount;
    return result;
}

void
RunWorldLoop(WorldContext* world,
             WorldLoopConfig* loopConfig,
             DispatcherConfig* dispatchConfig)
{
    if (world == NULL) {
        world_roster_warn("run_world_loop", "world is null");
        return;
    }

    const uint64_t targetFrameNs = (loopConfig != NULL && loopConfig->targetFrameTimeNs > 0)
                                       ? loopConfig->targetFrameTimeNs
                                       : 16666666ULL;
    const uint64_t maxFrames = loopConfig != NULL ? loopConfig->maxFrames : 0;

    world->isRunning = true;
    while (world->isRunning && (maxFrames == 0 || world->frameCount < maxFrames)) {
        if (loopConfig != NULL && loopConfig->onFrameStart != NULL) {
            loopConfig->onFrameStart(world, world->frameCount + 1U);
        }

        uint64_t frameStart = world_roster_now_ns();
        WorldFrameResult frame = ExecuteWorldFrame(world, dispatchConfig);

        if (loopConfig != NULL && loopConfig->onFrameEnd != NULL) {
            loopConfig->onFrameEnd(world, &frame);
        }
        if (!frame.allSucceeded && loopConfig != NULL && loopConfig->onRosterError != NULL) {
            for (size_t i = 0; i < frame.resultCount; i++) {
                if (!frame.rosterResults[i].result.allSucceeded) {
                    const char* error =
                        world_roster_first_error(&frame.rosterResults[i].result);
                    loopConfig->onRosterError(frame.rosterResults[i].rosterSlot,
                                              error != NULL ? error : "roster execution failed");
                }
            }
        }

        if (loopConfig != NULL && loopConfig->adaptiveSync) {
            uint64_t elapsed = world_roster_now_ns() - frameStart;
            if (elapsed < targetFrameNs) {
                world_roster_sleep_ms((targetFrameNs - elapsed) / 1000000ULL);
            }
        }

        world_roster_free_frame_result(&frame);
    }
}

void
StopWorld(WorldContext* world)
{
    if (world != NULL) {
        world->isRunning = false;
    }
}

PartyContext*
RosterFindParty(RosterContext* roster, const char* partySlot)
{
    if (roster == NULL || partySlot == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < roster->partyCount; i++) {
        if (roster->partySlots[i].slotName != NULL
            && strcmp(roster->partySlots[i].slotName, partySlot) == 0) {
            return roster->partySlots[i].partyContext;
        }
    }
    return NULL;
}

RosterContext*
WorldFindRoster(WorldContext* world, const char* rosterSlot)
{
    if (world == NULL || rosterSlot == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < world->rosterCount; i++) {
        if (world->rosters[i].slotName != NULL
            && strcmp(world->rosters[i].slotName, rosterSlot) == 0) {
            return world->rosters[i].instance;
        }
    }
    return NULL;
}

PartyContext*
WorldFindParty(WorldContext* world, const char* rosterSlot, const char* partySlot)
{
    RosterContext* roster = WorldFindRoster(world, rosterSlot);
    return RosterFindParty(roster, partySlot);
}

#include "world_roster_plan_stats.h"
