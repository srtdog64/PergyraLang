/*
 * Copyright (c) 2025 Pergyra Language Project
 * World-Roster Runtime Implementation
 */

#include "world_roster_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

void world_roster_warn(const char* op, const char* reason)
{
    fprintf(stderr,
            "[pgy][world-roster] %s failed: %s\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown");
}

static char* world_roster_strdup(const char* text)
{
    size_t len;
    char* copy;

    if (text == NULL) {
        return NULL;
    }
    len = strlen(text);
    if (len > SIZE_MAX - 1U) {
        return NULL;
    }
    len++;
    copy = (char*)malloc(len);
    if (copy != NULL) {
        memcpy(copy, text, len);
    }
    return copy;
}

static bool world_roster_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

static bool
world_roster_party_slots_valid(const RosterContext* roster, const char* op)
{
    if (roster != NULL && roster->partyCount > 0 && roster->partySlots == NULL) {
        world_roster_warn(op, "party count is nonzero but party slot array is null");
        return false;
    }
    return true;
}

static bool
world_roster_slots_valid(const WorldContext* world, const char* op)
{
    if (world != NULL && world->rosterCount > 0 && world->rosters == NULL) {
        world_roster_warn(op, "roster count is nonzero but roster array is null");
        return false;
    }
    return true;
}

uint64_t world_roster_now_ns(void)
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

static void
world_roster_increment_frame(WorldContext* world)
{
    if (world != NULL && world->frameCount != UINT64_MAX) {
        world->frameCount++;
    }
}

void world_roster_sleep_ms(uint64_t timeoutMs)
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
    char* ownedSlotName;
    char* ownedPartyType;

    if (roster == NULL || slotName == NULL || partyContext == NULL) {
        world_roster_warn("roster_add_party", "roster/slotName/partyContext is null");
        return false;
    }

    ownedSlotName = world_roster_strdup(slotName);
    ownedPartyType =
        world_roster_strdup(partyContext->partyName != NULL ? partyContext->partyName : slotName);
    if (ownedSlotName == NULL || ownedPartyType == NULL) {
        free(ownedSlotName);
        free(ownedPartyType);
        world_roster_warn("roster_add_party", "slot name allocation failed");
        return false;
    }

    if (roster->partyCount == SIZE_MAX
        || !world_roster_array_fits(roster->partyCount + 1U,
            sizeof(*roster->partySlots))) {
        free(ownedSlotName);
        free(ownedPartyType);
        world_roster_warn("roster_add_party", "party slot size overflow");
        return false;
    }
    size_t newCount = roster->partyCount + 1U;
    void* grown = realloc(roster->partySlots, newCount * sizeof(*roster->partySlots));
    if (grown == NULL) {
        free(ownedSlotName);
        free(ownedPartyType);
        world_roster_warn("roster_add_party", "party slot growth failed");
        return false;
    }
    roster->partySlots = grown;

    roster->partySlots[roster->partyCount].slotName = ownedSlotName;
    roster->partySlots[roster->partyCount].partyType = ownedPartyType;
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
    if (!world_roster_party_slots_valid(roster, "execute_roster")) {
        return result;
    }

    if (!world_roster_array_fits(roster->partyCount,
            sizeof(RosterPartyResult))) {
        world_roster_warn("execute_roster", "result allocation size overflow");
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
    char* ownedSlotName;
    char* ownedRosterType;

    if (world == NULL || slotName == NULL || roster == NULL) {
        world_roster_warn("world_add_roster", "world/slotName/roster is null");
        return false;
    }

    ownedSlotName = world_roster_strdup(slotName);
    ownedRosterType =
        world_roster_strdup(roster->systemType != NULL ? roster->systemType : slotName);
    if (ownedSlotName == NULL || ownedRosterType == NULL) {
        free(ownedSlotName);
        free(ownedRosterType);
        world_roster_warn("world_add_roster", "roster slot allocation failed");
        return false;
    }

    if (world->rosterCount == SIZE_MAX
        || !world_roster_array_fits(world->rosterCount + 1U,
            sizeof(*world->rosters))) {
        free(ownedSlotName);
        free(ownedRosterType);
        world_roster_warn("world_add_roster", "roster slot size overflow");
        return false;
    }
    size_t newCount = world->rosterCount + 1U;
    void* grown = realloc(world->rosters, newCount * sizeof(*world->rosters));
    if (grown == NULL) {
        free(ownedSlotName);
        free(ownedRosterType);
        world_roster_warn("world_add_roster", "roster slot growth failed");
        return false;
    }
    world->rosters = grown;

    world->rosters[world->rosterCount].slotName = ownedSlotName;
    world->rosters[world->rosterCount].rosterType = ownedRosterType;
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
    if (!world_roster_slots_valid(world, "execute_world_frame")) {
        return result;
    }

    if (!world_roster_array_fits(world->rosterCount,
            sizeof(WorldRosterResult))) {
        world_roster_warn("execute_world_frame",
            "result allocation size overflow");
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
    world_roster_increment_frame(world);
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
