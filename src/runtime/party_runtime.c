/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party System Runtime Implementation
 */

#include "party_runtime.h"
#include "../async/fiber.h"
#include "../async/scheduler.h"
#include "../slot_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============= Global State ============= */

/* Scheduler registry */
static struct {
    SchedulerTag tag;
    const char* name;
    FiberScheduler* scheduler;
} g_schedulerRegistry[16] = {0};

static size_t g_schedulerCount = 0;

/* Fiber map cache */
FiberMapCache* g_fiberMapCache = NULL;

/* Statistics tracking */
static struct {
    FiberStats* stats;
    size_t count;
    size_t capacity;
} g_fiberStats = {0};

/* ============= FiberMap Generation ============= */

FiberMap* GenerateFiberMap(
    const char* partyType,
    const struct {
        const char* slotName;
        uint32_t instanceSlotId;
        RoleParallelMetadata* metadata;
    }* roleBindings,
    size_t bindingCount)
{
    /* Allocate FiberMap */
    FiberMap* map = (FiberMap*)calloc(1, sizeof(FiberMap));
    if (!map) return NULL;
    
    map->partyTypeName = strdup(partyType);
    map->entries = (FiberMapEntry*)calloc(bindingCount, sizeof(FiberMapEntry));
    if (!map->entries) {
        free(map);
        return NULL;
    }
    
    /* Count entries that have parallel blocks */
    size_t entryCount = 0;
    
    /* Generate entries for each role with parallel block */
    for (size_t i = 0; i < bindingCount; i++) {
        const RoleParallelMetadata* metadata = roleBindings[i].metadata;
        
        /* Skip roles without parallel blocks */
        if (!metadata || !metadata->function) {
            continue;
        }
        
        FiberMapEntry* entry = &map->entries[entryCount];
        
        /* Fill entry data */
        entry->roleId = strdup(roleBindings[i].slotName);
        entry->instanceSlotId = roleBindings[i].instanceSlotId;
        entry->parallelFn = metadata->function;
        entry->schedulerTag = metadata->scheduler;
        entry->priority = metadata->priority;
        entry->executionIntervalMs = metadata->intervalMs;
        entry->isContinuous = metadata->continuous;
        
        entryCount++;
    }
    
    map->entryCount = entryCount;
    
    /* Generate cache key based on party type and role configuration */
    map->cacheKey = HashString(partyType);
    for (size_t i = 0; i < entryCount; i++) {
        map->cacheKey ^= HashString(map->entries[i].roleId);
        map->cacheKey ^= (uint64_t)map->entries[i].schedulerTag << 32;
    }
    
    /* Determine if this map can be statically cached */
    map->isStatic = true; /* For now, assume all maps are cacheable */
    
    return map;
}

void FreeFiberMap(FiberMap* map)
{
    if (!map) return;
    
    /* Free role IDs */
    for (size_t i = 0; i < map->entryCount; i++) {
        free((void*)map->entries[i].roleId);
    }
    
    free((void*)map->partyTypeName);
    free(map->entries);
    free(map);
}

/* ============= Context API Implementation ============= */

void* ContextGetRole(
    PartyContext* context,
    const char* slotName,
    const char* requiredAbility)
{
    if (!context || !slotName) return NULL;
    
    /* Lock for thread safety */
    SpinLockAcquire(&context->contextLock);
    
    void* result = NULL;
    
    /* Find the role slot */
    for (size_t i = 0; i < context->roleCount; i++) {
        if (strcmp(context->roles[i].slotName, slotName) == 0) {
            /* Check if role has required ability */
            if (requiredAbility) {
                bool hasAbility = false;
                for (size_t j = 0; j < context->roles[i].abilityCount; j++) {
                    if (strcmp(context->roles[i].abilities[j], requiredAbility) == 0) {
                        hasAbility = true;
                        break;
                    }
                }
                
                if (!hasAbility) {
                    /* Role doesn't have required ability */
                    break;
                }
            }
            
            /* Return cached instance or load from slot */
            if (context->roles[i].roleInstance) {
                result = context->roles[i].roleInstance;
            } else {
                /* Load from slot and cache */
                void* instance = GetSlotPointer(context->roles[i].slotId);
                context->roles[i].roleInstance = instance;
                result = instance;
            }
            break;
        }
    }
    
    SpinLockRelease(&context->contextLock);
    return result;
}

RoleQueryResult ContextFindRoles(
    PartyContext* context,
    const char* requiredAbility)
{
    RoleQueryResult result = {0};
    if (!context || !requiredAbility) return result;
    
    SpinLockAcquire(&context->contextLock);
    
    /* Count matching roles */
    size_t matches = 0;
    for (size_t i = 0; i < context->roleCount; i++) {
        for (size_t j = 0; j < context->roles[i].abilityCount; j++) {
            if (strcmp(context->roles[i].abilities[j], requiredAbility) == 0) {
                matches++;
                break;
            }
        }
    }
    
    if (matches > 0) {
        /* Allocate result arrays */
        result.instances = (void**)calloc(matches, sizeof(void*));
        result.slotNames = (const char**)calloc(matches, sizeof(char*));
        result.count = matches;
        
        /* Fill results */
        size_t idx = 0;
        for (size_t i = 0; i < context->roleCount && idx < matches; i++) {
            for (size_t j = 0; j < context->roles[i].abilityCount; j++) {
                if (strcmp(context->roles[i].abilities[j], requiredAbility) == 0) {
                    /* Get instance */
                    if (!context->roles[i].roleInstance) {
                        context->roles[i].roleInstance = 
                            GetSlotPointer(context->roles[i].slotId);
                    }
                    
                    result.instances[idx] = context->roles[i].roleInstance;
                    result.slotNames[idx] = context->roles[i].slotName;
                    idx++;
                    break;
                }
            }
        }
    }
    
    SpinLockRelease(&context->contextLock);
    return result;
}

void* ContextGetShared(PartyContext* context, const char* fieldName)
{
    if (!context || !fieldName) return NULL;
    
    SpinLockAcquire(&context->contextLock);
    
    void* result = NULL;
    for (size_t i = 0; i < context->sharedFieldCount; i++) {
        if (strcmp(context->sharedFields[i].fieldName, fieldName) == 0) {
            result = context->sharedFields[i].value;
            break;
        }
    }
    
    SpinLockRelease(&context->contextLock);
    return result;
}

/* ============= Runtime Dispatcher ============= */

/* Fiber wrapper structure */
typedef struct {
    FiberMapEntry* entry;
    PartyContext* context;
    void* roleInstance;
    FiberResult* result;
    volatile bool* shouldStop;
} FiberWrapper;

/* Periodic execution wrapper */
static void PeriodicFiberFunction(void* userData)
{
    FiberWrapper* wrapper = (FiberWrapper*)userData;
    uint64_t startTime = GetTimeNanos();
    
    while (!*wrapper->shouldStop) {
        /* Execute the role's parallel function */
        wrapper->entry->parallelFn(wrapper->roleInstance, wrapper->context);
        
        /* Sleep for interval */
        if (wrapper->entry->executionIntervalMs > 0) {
            FiberSleep(wrapper->entry->executionIntervalMs);
        } else {
            /* Continuous execution - yield to avoid hogging CPU */
            FiberYield();
        }
    }
    
    /* Record execution time */
    wrapper->result->executionTimeNs = GetTimeNanos() - startTime;
    wrapper->result->success = true;
}

/* One-shot execution wrapper */
static void OneshotFiberFunction(void* userData)
{
    FiberWrapper* wrapper = (FiberWrapper*)userData;
    uint64_t startTime = GetTimeNanos();
    
    /* Execute once */
    wrapper->entry->parallelFn(wrapper->roleInstance, wrapper->context);
    
    /* Record result */
    wrapper->result->executionTimeNs = GetTimeNanos() - startTime;
    wrapper->result->success = true;
}

/* Dispatch implementation */
DispatchResult DispatchParallel(
    FiberMap* map,
    PartyContext* context,
    JoinStrategy joinStrategy,
    DispatcherConfig* config)
{
    DispatchResult result = {0};
    
    if (!map || !context || map->entryCount == 0) {
        return result;
    }
    
    /* Allocate result array */
    result.results = (FiberResult*)calloc(map->entryCount, sizeof(FiberResult));
    result.resultCount = map->entryCount;
    
    /* Fiber handles and control flags */
    Fiber** fibers = (Fiber**)calloc(map->entryCount, sizeof(Fiber*));
    FiberWrapper* wrappers = (FiberWrapper*)calloc(map->entryCount, sizeof(FiberWrapper));
    volatile bool* stopFlags = (volatile bool*)calloc(map->entryCount, sizeof(bool));
    
    uint64_t dispatchStartTime = GetTimeNanos();
    
    /* Create and schedule fibers */
    for (size_t i = 0; i < map->entryCount; i++) {
        FiberMapEntry* entry = &map->entries[i];
        
        /* Initialize result */
        result.results[i].roleId = entry->roleId;
        result.results[i].success = false;
        
        /* Get role instance */
        void* roleInstance = GetSlotPointer(entry->instanceSlotId);
        if (!roleInstance) {
            result.results[i].error = "Failed to load role instance";
            continue;
        }
        
        /* Setup wrapper */
        wrappers[i].entry = entry;
        wrappers[i].context = context;
        wrappers[i].roleInstance = roleInstance;
        wrappers[i].result = &result.results[i];
        wrappers[i].shouldStop = &stopFlags[i];
        
        /* Get scheduler */
        FiberScheduler* scheduler = GetSchedulerForTag(entry->schedulerTag);
        if (!scheduler) {
            result.results[i].error = "Scheduler not found";
            continue;
        }
        
        /* Create fiber based on execution mode */
        FiberFunction fiberFn = entry->isContinuous ? 
            PeriodicFiberFunction : OneshotFiberFunction;
        
        fibers[i] = CreateFiber(fiberFn, &wrappers[i], 
            config ? config->maxMemoryPerFiber : DEFAULT_FIBER_STACK_SIZE);
        
        if (!fibers[i]) {
            result.results[i].error = "Failed to create fiber";
            continue;
        }
        
        /* Schedule with priority */
        ScheduleFiberWithPriority(scheduler, fibers[i], entry->priority);
    }
    
    /* Wait based on join strategy */
    bool allSucceeded = true;
    
    switch (joinStrategy) {
        case JOIN_ALL:
            /* Wait for all fibers to complete */
            for (size_t i = 0; i < map->entryCount; i++) {
                if (fibers[i]) {
                    WaitForFiber(fibers[i]);
                    if (!result.results[i].success) {
                        allSucceeded = false;
                    }
                }
            }
            break;
            
        case JOIN_ANY:
            /* Wait for first to complete */
            WaitForAnyFiber(fibers, map->entryCount);
            break;
            
        case JOIN_RACE:
            /* Wait for first successful */
            while (true) {
                size_t completed = WaitForAnyFiber(fibers, map->entryCount);
                if (result.results[completed].success) {
                    break;
                }
                /* Mark as done to skip in next wait */
                fibers[completed] = NULL;
            }
            break;
            
        case JOIN_MAJORITY:
            /* Wait for >50% to succeed */
            size_t successCount = 0;
            size_t requiredCount = (map->entryCount / 2) + 1;
            
            for (size_t i = 0; i < map->entryCount; i++) {
                if (fibers[i]) {
                    WaitForFiber(fibers[i]);
                    if (result.results[i].success) {
                        successCount++;
                        if (successCount >= requiredCount) {
                            break;
                        }
                    }
                }
            }
            allSucceeded = (successCount >= requiredCount);
            break;
    }
    
    /* Stop continuous fibers */
    for (size_t i = 0; i < map->entryCount; i++) {
        if (map->entries[i].isContinuous) {
            stopFlags[i] = true;
        }
    }
    
    /* Final wait for continuous fibers to stop */
    for (size_t i = 0; i < map->entryCount; i++) {
        if (fibers[i] && map->entries[i].isContinuous) {
            WaitForFiber(fibers[i]);
        }
    }
    
    /* Calculate total execution time */
    result.totalExecutionTimeNs = GetTimeNanos() - dispatchStartTime;
    result.allSucceeded = allSucceeded;
    
    /* Update statistics */
    for (size_t i = 0; i < map->entryCount; i++) {
        UpdateFiberStats(result.results[i].roleId, &result.results[i]);
    }
    
    /* Cleanup */
    free(fibers);
    free(wrappers);
    free((void*)stopFlags);
    
    return result;
}

/* ============= Scheduler Management ============= */

bool RegisterScheduler(SchedulerTag tag, const char* name, FiberScheduler* scheduler)
{
    if (g_schedulerCount >= 16) return false;
    
    g_schedulerRegistry[g_schedulerCount].tag = tag;
    g_schedulerRegistry[g_schedulerCount].name = strdup(name);
    g_schedulerRegistry[g_schedulerCount].scheduler = scheduler;
    g_schedulerCount++;
    
    return true;
}

FiberScheduler* GetSchedulerForTag(SchedulerTag tag)
{
    /* Check registered schedulers */
    for (size_t i = 0; i < g_schedulerCount; i++) {
        if (g_schedulerRegistry[i].tag == tag) {
            return g_schedulerRegistry[i].scheduler;
        }
    }
    
    /* Return default scheduler based on tag */
    switch (tag) {
        case SCHEDULER_MAIN_THREAD:
            return GetMainThreadScheduler();
        case SCHEDULER_CPU_FIBER:
            return GetCPUScheduler();
        case SCHEDULER_GPU_FIBER:
            return GetGPUScheduler();
        case SCHEDULER_IO_FIBER:
            return GetIOScheduler();
        case SCHEDULER_BACKGROUND_THREAD:
            return GetBackgroundScheduler();
        default:
            return GetDefaultScheduler();
    }
}

/* ============= Statistics ============= */

static void UpdateFiberStats(const char* roleId, FiberResult* result)
{
    /* Find or create stats entry */
    FiberStats* stats = NULL;
    
    for (size_t i = 0; i < g_fiberStats.count; i++) {
        if (strcmp(g_fiberStats.stats[i].roleId, roleId) == 0) {
            stats = &g_fiberStats.stats[i];
            break;
        }
    }
    
    if (!stats) {
        /* Create new entry */
        if (g_fiberStats.count >= g_fiberStats.capacity) {
            /* Grow array */
            size_t newCapacity = g_fiberStats.capacity ? g_fiberStats.capacity * 2 : 16;
            FiberStats* newStats = (FiberStats*)realloc(g_fiberStats.stats, 
                newCapacity * sizeof(FiberStats));
            if (!newStats) return;
            
            g_fiberStats.stats = newStats;
            g_fiberStats.capacity = newCapacity;
        }
        
        stats = &g_fiberStats.stats[g_fiberStats.count++];
        memset(stats, 0, sizeof(FiberStats));
        stats->roleId = strdup(roleId);
        stats->minTimeNs = UINT64_MAX;
    }
    
    /* Update statistics */
    stats->totalExecutions++;
    stats->totalTimeNs += result->executionTimeNs;
    
    if (result->executionTimeNs < stats->minTimeNs) {
        stats->minTimeNs = result->executionTimeNs;
    }
    if (result->executionTimeNs > stats->maxTimeNs) {
        stats->maxTimeNs = result->executionTimeNs;
    }
    
    stats->avgTimeNs = stats->totalTimeNs / stats->totalExecutions;
    
    if (!result->success) {
        stats->errorCount++;
    }
}

FiberStats GetFiberStats(const char* roleId)
{
    FiberStats empty = {0};
    
    for (size_t i = 0; i < g_fiberStats.count; i++) {
        if (strcmp(g_fiberStats.stats[i].roleId, roleId) == 0) {
            return g_fiberStats.stats[i];
        }
    }
    
    return empty;
}

/* ============= Debugging ============= */

void DumpFiberMaps(void)
{
    printf("=== Fiber Map Dump ===\n");
    printf("Registered Schedulers: %zu\n", g_schedulerCount);
    
    for (size_t i = 0; i < g_schedulerCount; i++) {
        printf("  [%d] %s -> %p\n", 
            g_schedulerRegistry[i].tag,
            g_schedulerRegistry[i].name,
            g_schedulerRegistry[i].scheduler);
    }
    
    printf("\nFiber Statistics:\n");
    for (size_t i = 0; i < g_fiberStats.count; i++) {
        FiberStats* s = &g_fiberStats.stats[i];
        printf("  Role: %s\n", s->roleId);
        printf("    Executions: %llu\n", s->totalExecutions);
        printf("    Avg Time: %llu ns\n", s->avgTimeNs);
        printf("    Min/Max: %llu / %llu ns\n", s->minTimeNs, s->maxTimeNs);
        printf("    Errors: %u\n", s->errorCount);
    }
}

/* ============= Helper Functions ============= */

static uint64_t HashString(const char* str)
{
    uint64_t hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash;
}

static uint64_t GetTimeNanos(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
