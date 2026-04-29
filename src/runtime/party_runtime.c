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
static char* party_runtime_strdup(const char* text);
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

static char*
party_runtime_strdup(const char* text)
{
    size_t length;
    char* copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text) + 1;
    copy = (char*)malloc(length);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length);
    return copy;
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

    map->partyTypeName = party_runtime_strdup(partyType);
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
        entry->roleId = party_runtime_strdup(roleBindings[i].slotName != NULL
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

#include "party_runtime_dispatch.h"

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

    char* ownedName = party_runtime_strdup(name);
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
        stats->roleId = party_runtime_strdup(roleId);
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
