/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party System Runtime Implementation
 * Current public surface is intentionally limited to the subset that is
 * actually implemented in party_runtime.c. Async dispatch/cache/tracing
 * APIs are not yet part of the stable surface.
 */

#ifndef PERGYRA_PARTY_RUNTIME_H
#define PERGYRA_PARTY_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include "slot_manager.h"
#include "async/fiber.h"
#include "async/scheduler.h"

typedef Scheduler FiberScheduler;

/* ============= Scheduler Tags ============= */

typedef enum {
    SCHEDULER_MAIN_THREAD = 0,      /* UI/Game main thread */
    SCHEDULER_CPU_FIBER,            /* CPU-bound computation */
    SCHEDULER_GPU_FIBER,            /* GPU compute tasks */
    SCHEDULER_IO_FIBER,             /* I/O operations */
    SCHEDULER_BACKGROUND_THREAD,    /* Background tasks */
    SCHEDULER_COMPUTE_THREAD,       /* Heavy computation */
    SCHEDULER_NETWORK_THREAD,       /* Network operations */
    SCHEDULER_CUSTOM_1,             /* User-defined scheduler 1 */
    SCHEDULER_CUSTOM_2,             /* User-defined scheduler 2 */
    SCHEDULER_CUSTOM_3,             /* User-defined scheduler 3 */
    SCHEDULER_ANY                   /* Let runtime decide */
} SchedulerTag;

/* Scheduler priority levels */
typedef enum {
    PRIORITY_CRITICAL = 100,
    PRIORITY_HIGH = 75,
    PRIORITY_NORMAL = 50,
    PRIORITY_LOW = 25,
    PRIORITY_IDLE = 0
} SchedulerPriority;

/* ============= FiberMap Structure ============= */

/* Function pointer for parallel block execution */
typedef void (*ParallelFunction)(void* roleInstance, void* partyContext);

/* Single entry in the FiberMap */
typedef struct {
    const char* roleId;              /* Role slot name (e.g., "tank", "healer") */
    uint32_t instanceSlotId;         /* Slot ID of the role instance */
    ParallelFunction parallelFn;     /* Function to execute */
    SchedulerTag schedulerTag;       /* Which scheduler to use */
    SchedulerPriority priority;      /* Execution priority */
    uint32_t executionIntervalMs;    /* For periodic execution (0 = continuous) */
    bool isContinuous;               /* true for continuous, false for periodic */
} FiberMapEntry;

/*
 * Complete FiberMap for a party.
 *
 * Concurrency contract: DispatchParallel borrows this graph. Callers must not
 * mutate or free the FiberMap, its entries, or the attached PartyContext until
 * the dispatch result has joined. The runtime intentionally avoids a shallow
 * snapshot here because that would hide pointer/rehash races behind a copy that
 * is not semantically owned.
 */
typedef struct {
    const char* partyTypeName;       /* Party type (e.g., "DungeonParty") */
    FiberMapEntry* entries;          /* Array of fiber entries */
    size_t entryCount;               /* Number of entries */
    uint64_t cacheKey;               /* For caching compiled fiber maps */
    bool isStatic;                   /* true if can be cached at compile time */
} FiberMap;

/* ============= Party Context ============= */

/* Runtime context available to roles via 'context' keyword */
typedef struct PartyContext {
    /* Role lookup table */
    struct {
        const char* slotName;
        uint32_t slotId;
        void* roleInstance;          /* Cached instance pointer */
        const char** abilities;      /* List of implemented abilities */
        size_t abilityCount;
    }* roles;
    size_t roleCount;
    
    /* Shared party data */
    struct {
        const char* fieldName;
        uint32_t slotId;
        void* value;
    }* sharedFields;
    size_t sharedFieldCount;
    
    /* Party metadata */
    const char* partyName;
    FiberMap* fiberMap;             /* Attached execution map for roster/world runtime */
    bool inCombat;                   /* Example shared state */
    
    /* Synchronization */
    pthread_mutex_t contextLock;     /* For thread-safe access */
} PartyContext;

/* ============= FiberMap Generation ============= */

/* Compile-time metadata for role parallel blocks */
typedef struct {
    const char* roleName;
    ParallelFunction function;
    SchedulerTag scheduler;
    SchedulerPriority priority;
    uint32_t intervalMs;
    bool continuous;
} RoleParallelMetadata;

typedef struct {
    const char* slotName;
    uint32_t instanceSlotId;
    RoleParallelMetadata* metadata;
} PartyRoleBinding;

/* Generate FiberMap from party type and role bindings */
FiberMap* GenerateFiberMap(
    const char* partyType,
    const PartyRoleBinding* roleBindings,
    size_t bindingCount
);

/* Free a dynamically generated FiberMap */
void FreeFiberMap(FiberMap* map);

/* Attach or query the execution map bound to a party context */
void PartyContextAttachFiberMap(PartyContext* context, FiberMap* map);
FiberMap* PartyContextGetFiberMap(PartyContext* context);

/* ============= Runtime Dispatcher ============= */

/* Dispatcher configuration */
typedef struct {
    /* Scheduler pool limits */
    uint32_t maxCpuFibers;
    uint32_t maxGpuFibers;
    uint32_t maxIoFibers;
    uint32_t maxBackgroundThreads;
    
    /* Resource constraints */
    size_t maxMemoryPerFiber;
    uint64_t maxExecutionTimeMs;
    
    /* Error handling */
    void (*onFiberError)(const char* roleId, const char* error);
    void (*onTimeout)(const char* roleId);
} DispatcherConfig;

/* Dispatch result for join strategies */
typedef struct {
    const char* roleId;              /* DispatchResult-owned; free via FreeDispatchResult */
    bool success;
    void* result;                    /* Role-specific result data */
    uint64_t executionTimeNs;
    const char* error;               /* NULL if success */
} FiberResult;

/* Join strategies */
typedef enum {
    JOIN_ALL,                        /* Wait for all fibers */
    JOIN_ANY,                        /* First to complete */
    JOIN_RACE,                       /* First successful */
    JOIN_MAJORITY,                   /* >50% must succeed */
    JOIN_CUSTOM                      /* User-defined strategy */
} JoinStrategy;

/* Custom join function */
typedef bool (*CustomJoinFunction)(
    FiberResult* results,
    size_t count,
    void* userData
);

/*
 * Main dispatcher function.
 *
 * Dispatch is quiescent-borrowed: map/context storage remains caller-owned and
 * immutable for the full dispatch. Use a copied plan or a channel/result
 * boundary if another thread can mutate the source graph.
 */
typedef struct {
    FiberResult* results;
    size_t resultCount;
    bool allSucceeded;
    uint64_t totalExecutionTimeNs;
} DispatchResult;

DispatchResult DispatchParallel(
    FiberMap* map,
    PartyContext* context,
    JoinStrategy joinStrategy,
    DispatcherConfig* config
);

DispatchResult DispatchGeneratedFiberMap(
    const char* partyType,
    const PartyRoleBinding* roleBindings,
    size_t bindingCount,
    PartyContext* context,
    JoinStrategy joinStrategy,
    DispatcherConfig* config
);

void FreeDispatchResult(DispatchResult* result);

/* ============= Context API (for roles) ============= */

/* Get role by ability - used by 'context.GetRole<Ability>("slot")' */
void* ContextGetRole(
    PartyContext* context,
    const char* slotName,
    const char* requiredAbility
);

/* Find all roles with ability */
typedef struct {
    void** instances;
    const char** slotNames;
    size_t count;
} RoleQueryResult;

RoleQueryResult ContextFindRoles(
    PartyContext* context,
    const char* requiredAbility
);

/* Get shared field value */
void* ContextGetShared(
    PartyContext* context,
    const char* fieldName
);

/* ============= Scheduler Integration ============= */

/* Register custom scheduler */
bool RegisterScheduler(
    SchedulerTag tag,
    const char* name,
    FiberScheduler* scheduler
);

/* Get scheduler for tag */
FiberScheduler* GetSchedulerForTag(SchedulerTag tag);

/* ============= Debugging & Profiling ============= */

/* Fiber execution statistics */
typedef struct {
    const char* roleId;
    uint64_t totalExecutions;
    uint64_t totalTimeNs;
    uint64_t minTimeNs;
    uint64_t maxTimeNs;
    uint64_t avgTimeNs;
    uint32_t errorCount;
} FiberStats;

/* Get statistics for a role */
FiberStats GetFiberStats(const char* roleId);

/* Dump all fiber maps for debugging */
void DumpFiberMaps(void);

/* ============= Helper Macros ============= */

/* Define a role's parallel block metadata */
#define ROLE_PARALLEL_METADATA(role, fn, sched, prio, interval, cont) \
    { \
        .roleName = #role, \
        .function = (ParallelFunction)fn, \
        .scheduler = sched, \
        .priority = prio, \
        .intervalMs = interval, \
        .continuous = cont \
    }

/* Quick dispatcher for simple cases */
#define DISPATCH_PARTY(party, join) \
    DispatchGeneratedFiberMap( \
        #party, \
        party##_bindings, \
        party##_binding_count, \
        &party##_context, \
        JOIN_##join, \
        NULL \
    )

#endif /* PERGYRA_PARTY_RUNTIME_H */
