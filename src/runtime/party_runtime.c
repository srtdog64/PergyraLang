/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party System Runtime Implementation
 */

#include "party_runtime.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#include <time.h>
#endif

/* ============= Global State ============= */

static struct {
    SchedulerTag tag;
    const char* name;
    FiberScheduler* scheduler;
} g_schedulerRegistry[16] = {0};

static size_t g_schedulerCount = 0;

static struct {
    FiberStats* stats;
    size_t count;
    size_t capacity;
} g_fiberStats = {0};

static void party_runtime_warn_scheduler(const char* reason,
                                         SchedulerTag tag,
                                         const char* name);
static void party_runtime_warn(const char* op, const char* reason);
static uint64_t HashString(const char* str);
static uint64_t GetTimeNanos(void);
static void UpdateFiberStats(const char* roleId, const FiberResult* result);
static void party_dispatch_sleep_ms(uint32_t milliseconds);
static void party_dispatch_yield(void);
static size_t party_context_find_role_index_by_name(const PartyContext* context,
                                                    const char* slotName);
static size_t party_context_find_role_index_by_slot(const PartyContext* context,
                                                    uint32_t slotId);
static void* party_context_role_instance_by_slot(PartyContext* context,
                                                 uint32_t slotId);

static void
party_runtime_warn_scheduler(const char* reason, SchedulerTag tag, const char* name)
{
    fprintf(stderr,
            "[pgy][party] scheduler registration failed: %s (tag=%d, name=%s)\n",
            reason != NULL ? reason : "unknown",
            (int)tag,
            name != NULL ? name : "<null>");
}

static void
party_runtime_warn(const char* op, const char* reason)
{
    fprintf(stderr,
            "[pgy][party] %s failed: %s\n",
            op != NULL ? op : "operation",
            reason != NULL ? reason : "unknown");
}

/* ============= FiberMap Generation ============= */

FiberMap*
GenerateFiberMap(const char* partyType,
                 const PartyRoleBinding* roleBindings,
                 size_t bindingCount)
{
    if (partyType == NULL || roleBindings == NULL) {
        party_runtime_warn("generate_fiber_map", "partyType or roleBindings is null");
        return NULL;
    }

    FiberMap* map = (FiberMap*)calloc(1, sizeof(FiberMap));
    if (map == NULL) {
        party_runtime_warn("generate_fiber_map", "map allocation failed");
        return NULL;
    }

    map->partyTypeName = strdup(partyType);
    if (map->partyTypeName == NULL) {
        free(map);
        party_runtime_warn("generate_fiber_map", "party type allocation failed");
        return NULL;
    }

    map->entries = (FiberMapEntry*)calloc(bindingCount, sizeof(FiberMapEntry));
    if (map->entries == NULL) {
        free((void*)map->partyTypeName);
        free(map);
        party_runtime_warn("generate_fiber_map", "entry allocation failed");
        return NULL;
    }

    size_t entryCount = 0;
    for (size_t i = 0; i < bindingCount; i++) {
        const RoleParallelMetadata* metadata = roleBindings[i].metadata;
        if (metadata == NULL || metadata->function == NULL) {
            continue;
        }

        FiberMapEntry* entry = &map->entries[entryCount];
        entry->roleId = strdup(roleBindings[i].slotName != NULL
                                   ? roleBindings[i].slotName
                                   : "<anonymous>");
        if (entry->roleId == NULL) {
            FreeFiberMap(map);
            party_runtime_warn("generate_fiber_map", "role id allocation failed");
            return NULL;
        }

        entry->instanceSlotId = roleBindings[i].instanceSlotId;
        entry->parallelFn = metadata->function;
        entry->schedulerTag = metadata->scheduler;
        entry->priority = metadata->priority;
        entry->executionIntervalMs = metadata->intervalMs;
        entry->isContinuous = metadata->continuous;
        entryCount++;
    }

    map->entryCount = entryCount;
    map->cacheKey = HashString(partyType);
    for (size_t i = 0; i < entryCount; i++) {
        map->cacheKey ^= HashString(map->entries[i].roleId);
        map->cacheKey ^= (uint64_t)map->entries[i].schedulerTag << 32;
    }
    map->isStatic = true;

    return map;
}

void
FreeFiberMap(FiberMap* map)
{
    if (map == NULL) {
        return;
    }

    for (size_t i = 0; i < map->entryCount; i++) {
        free((void*)map->entries[i].roleId);
    }

    free((void*)map->partyTypeName);
    free(map->entries);
    free(map);
}

void
PartyContextAttachFiberMap(PartyContext* context, FiberMap* map)
{
    if (context == NULL) {
        party_runtime_warn("party_context.attach_fiber_map", "context is null");
        return;
    }

    pthread_mutex_lock(&context->contextLock);
    context->fiberMap = map;
    pthread_mutex_unlock(&context->contextLock);
}

FiberMap*
PartyContextGetFiberMap(PartyContext* context)
{
    if (context == NULL) {
        party_runtime_warn("party_context.get_fiber_map", "context is null");
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    FiberMap* map = context->fiberMap;
    pthread_mutex_unlock(&context->contextLock);
    return map;
}

/* ============= Context API Implementation ============= */

static size_t
party_context_find_role_index_by_name(const PartyContext* context, const char* slotName)
{
    if (context == NULL || slotName == NULL) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < context->roleCount; i++) {
        if (context->roles[i].slotName != NULL
            && strcmp(context->roles[i].slotName, slotName) == 0) {
            return i;
        }
    }

    return SIZE_MAX;
}

static size_t
party_context_find_role_index_by_slot(const PartyContext* context, uint32_t slotId)
{
    if (context == NULL) {
        return SIZE_MAX;
    }

    for (size_t i = 0; i < context->roleCount; i++) {
        if (context->roles[i].slotId == slotId) {
            return i;
        }
    }

    return SIZE_MAX;
}

static void*
party_context_role_instance_by_slot(PartyContext* context, uint32_t slotId)
{
    if (context == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    size_t index = party_context_find_role_index_by_slot(context, slotId);
    void* instance = index != SIZE_MAX ? context->roles[index].roleInstance : NULL;
    pthread_mutex_unlock(&context->contextLock);
    return instance;
}

void*
ContextGetRole(PartyContext* context, const char* slotName, const char* requiredAbility)
{
    if (context == NULL || slotName == NULL) {
        party_runtime_warn("context.get_role", "context or slotName is null");
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    size_t index = party_context_find_role_index_by_name(context, slotName);
    void* result = NULL;

    if (index != SIZE_MAX) {
        bool hasAbility = (requiredAbility == NULL);
        if (requiredAbility != NULL) {
            for (size_t j = 0; j < context->roles[index].abilityCount; j++) {
                if (context->roles[index].abilities[j] != NULL
                    && strcmp(context->roles[index].abilities[j], requiredAbility) == 0) {
                    hasAbility = true;
                    break;
                }
            }
        }

        if (hasAbility) {
            result = context->roles[index].roleInstance;
        }
    }

    pthread_mutex_unlock(&context->contextLock);

    if (result == NULL) {
        party_runtime_warn("context.get_role",
                           requiredAbility != NULL
                               ? "role missing required ability or instance"
                               : "role slot not found or instance unavailable");
    }

    return result;
}

RoleQueryResult
ContextFindRoles(PartyContext* context, const char* requiredAbility)
{
    RoleQueryResult result = {0};
    if (context == NULL || requiredAbility == NULL) {
        party_runtime_warn("context.find_roles", "context or requiredAbility is null");
        return result;
    }

    pthread_mutex_lock(&context->contextLock);

    size_t matches = 0;
    for (size_t i = 0; i < context->roleCount; i++) {
        for (size_t j = 0; j < context->roles[i].abilityCount; j++) {
            if (context->roles[i].abilities[j] != NULL
                && strcmp(context->roles[i].abilities[j], requiredAbility) == 0
                && context->roles[i].roleInstance != NULL) {
                matches++;
                break;
            }
        }
    }

    if (matches > 0) {
        result.instances = (void**)calloc(matches, sizeof(void*));
        result.slotNames = (const char**)calloc(matches, sizeof(const char*));
        if (result.instances == NULL || result.slotNames == NULL) {
            free(result.instances);
            free((void*)result.slotNames);
            result.instances = NULL;
            result.slotNames = NULL;
            matches = 0;
            party_runtime_warn("context.find_roles", "result allocation failed");
        } else {
            size_t idx = 0;
            for (size_t i = 0; i < context->roleCount && idx < matches; i++) {
                for (size_t j = 0; j < context->roles[i].abilityCount; j++) {
                    if (context->roles[i].abilities[j] != NULL
                        && strcmp(context->roles[i].abilities[j], requiredAbility) == 0
                        && context->roles[i].roleInstance != NULL) {
                        result.instances[idx] = context->roles[i].roleInstance;
                        result.slotNames[idx] = context->roles[i].slotName;
                        idx++;
                        break;
                    }
                }
            }
            result.count = matches;
        }
    }

    pthread_mutex_unlock(&context->contextLock);
    return result;
}

void*
ContextGetShared(PartyContext* context, const char* fieldName)
{
    if (context == NULL || fieldName == NULL) {
        party_runtime_warn("context.get_shared", "context or fieldName is null");
        return NULL;
    }

    pthread_mutex_lock(&context->contextLock);
    void* result = NULL;
    for (size_t i = 0; i < context->sharedFieldCount; i++) {
        if (context->sharedFields[i].fieldName != NULL
            && strcmp(context->sharedFields[i].fieldName, fieldName) == 0) {
            result = context->sharedFields[i].value;
            break;
        }
    }
    pthread_mutex_unlock(&context->contextLock);

    if (result == NULL) {
        party_runtime_warn("context.get_shared", "shared field not found");
    }

    return result;
}

/* ============= Runtime Dispatcher ============= */

typedef struct {
    FiberMapEntry* entry;
    PartyContext* context;
    void* roleInstance;
    FiberResult* result;
    atomic_bool* stopFlag;
    atomic_bool* completedFlag;
} PartyDispatchThreadData;

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

    result.results = (FiberResult*)calloc(map->entryCount, sizeof(FiberResult));
    if (result.results == NULL) {
        party_runtime_warn("dispatch_parallel", "result allocation failed");
        return result;
    }
    result.resultCount = map->entryCount;

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

        if (GetSchedulerForTag(entry->schedulerTag) == NULL && entry->schedulerTag != SCHEDULER_ANY) {
            party_runtime_warn("dispatch_parallel",
                               "scheduler tag unresolved; using thread fallback");
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

    if (joinStrategy == JOIN_CUSTOM) {
        party_runtime_warn("dispatch_parallel", "JOIN_CUSTOM falls back to JOIN_ALL");
        joinStrategy = JOIN_ALL;
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

/* ============= Scheduler Management ============= */

bool
RegisterScheduler(SchedulerTag tag, const char* name, FiberScheduler* scheduler)
{
    if (name == NULL || name[0] == '\0') {
        party_runtime_warn_scheduler("name is null or empty", tag, name);
        return false;
    }
    if (scheduler == NULL) {
        party_runtime_warn_scheduler("scheduler pointer is null", tag, name);
        return false;
    }

    for (size_t i = 0; i < g_schedulerCount; i++) {
        if (g_schedulerRegistry[i].tag == tag) {
            party_runtime_warn_scheduler("duplicate scheduler tag", tag, name);
            return false;
        }
        if (g_schedulerRegistry[i].name != NULL
            && strcmp(g_schedulerRegistry[i].name, name) == 0) {
            party_runtime_warn_scheduler("duplicate scheduler name", tag, name);
            return false;
        }
    }

    if (g_schedulerCount >= 16) {
        party_runtime_warn_scheduler("registry is full", tag, name);
        return false;
    }

    char* ownedName = strdup(name);
    if (ownedName == NULL) {
        party_runtime_warn_scheduler("name allocation failed", tag, name);
        return false;
    }

    g_schedulerRegistry[g_schedulerCount].tag = tag;
    g_schedulerRegistry[g_schedulerCount].name = ownedName;
    g_schedulerRegistry[g_schedulerCount].scheduler = scheduler;
    g_schedulerCount++;
    return true;
}

FiberScheduler*
GetSchedulerForTag(SchedulerTag tag)
{
    for (size_t i = 0; i < g_schedulerCount; i++) {
        if (g_schedulerRegistry[i].tag == tag) {
            return g_schedulerRegistry[i].scheduler;
        }
    }

    FiberScheduler* current = SchedulerGetCurrent();
    if (current != NULL) {
        if (tag != SCHEDULER_ANY) {
            party_runtime_warn("scheduler.lookup",
                               "scheduler tag not registered; falling back to current scheduler");
        }
        return current;
    }

    if (tag != SCHEDULER_ANY) {
        party_runtime_warn("scheduler.lookup", "scheduler tag not registered");
    }
    return NULL;
}

/* ============= Statistics ============= */

static void
UpdateFiberStats(const char* roleId, const FiberResult* result)
{
    if (roleId == NULL || result == NULL) {
        return;
    }

    FiberStats* stats = NULL;
    for (size_t i = 0; i < g_fiberStats.count; i++) {
        if (g_fiberStats.stats[i].roleId != NULL
            && strcmp(g_fiberStats.stats[i].roleId, roleId) == 0) {
            stats = &g_fiberStats.stats[i];
            break;
        }
    }

    if (stats == NULL) {
        if (g_fiberStats.count >= g_fiberStats.capacity) {
            size_t newCapacity = g_fiberStats.capacity > 0 ? g_fiberStats.capacity * 2U : 16U;
            FiberStats* newStats =
                (FiberStats*)realloc(g_fiberStats.stats, newCapacity * sizeof(FiberStats));
            if (newStats == NULL) {
                party_runtime_warn("fiber_stats", "stats array growth failed");
                return;
            }
            g_fiberStats.stats = newStats;
            g_fiberStats.capacity = newCapacity;
        }

        stats = &g_fiberStats.stats[g_fiberStats.count++];
        memset(stats, 0, sizeof(FiberStats));
        stats->roleId = strdup(roleId);
        if (stats->roleId == NULL) {
            g_fiberStats.count--;
            party_runtime_warn("fiber_stats", "role id allocation failed");
            return;
        }
        stats->minTimeNs = UINT64_MAX;
    }

    stats->totalExecutions++;
    stats->totalTimeNs += result->executionTimeNs;
    if (result->executionTimeNs < stats->minTimeNs) {
        stats->minTimeNs = result->executionTimeNs;
    }
    if (result->executionTimeNs > stats->maxTimeNs) {
        stats->maxTimeNs = result->executionTimeNs;
    }
    stats->avgTimeNs = stats->totalExecutions > 0
                           ? stats->totalTimeNs / stats->totalExecutions
                           : 0;
    if (!result->success) {
        stats->errorCount++;
    }
}

FiberStats
GetFiberStats(const char* roleId)
{
    FiberStats empty = {0};
    if (roleId == NULL) {
        return empty;
    }

    for (size_t i = 0; i < g_fiberStats.count; i++) {
        if (g_fiberStats.stats[i].roleId != NULL
            && strcmp(g_fiberStats.stats[i].roleId, roleId) == 0) {
            return g_fiberStats.stats[i];
        }
    }

    return empty;
}

/* ============= Debugging ============= */

void
DumpFiberMaps(void)
{
    printf("=== Fiber Map Dump ===\n");
    printf("Registered Schedulers: %zu\n", g_schedulerCount);

    for (size_t i = 0; i < g_schedulerCount; i++) {
        printf("  [%d] %s -> %p\n",
               g_schedulerRegistry[i].tag,
               g_schedulerRegistry[i].name,
               (void*)g_schedulerRegistry[i].scheduler);
    }

    printf("\nFiber Statistics:\n");
    for (size_t i = 0; i < g_fiberStats.count; i++) {
        FiberStats* stats = &g_fiberStats.stats[i];
        printf("  Role: %s\n", stats->roleId);
        printf("    Executions: %llu\n", (unsigned long long)stats->totalExecutions);
        printf("    Avg Time: %llu ns\n", (unsigned long long)stats->avgTimeNs);
        printf("    Min/Max: %llu / %llu\n",
               (unsigned long long)stats->minTimeNs,
               (unsigned long long)stats->maxTimeNs);
        printf("    Errors: %u\n", stats->errorCount);
    }
}

/* ============= Helper Functions ============= */

static uint64_t
HashString(const char* str)
{
    if (str == NULL) {
        return 0;
    }

    uint64_t hash = 5381;
    int c = 0;
    while ((c = *str++) != 0) {
        hash = ((hash << 5) + hash) + (uint64_t)c;
    }
    return hash;
}

static uint64_t
GetTimeNanos(void)
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
