/*
 * Copyright (c) 2025 Pergyra Language Project
 * World-Systemic Runtime Implementation
 * Hierarchical execution management from World to Ability
 */

#ifndef PERGYRA_WORLD_SYSTEMIC_H
#define PERGYRA_WORLD_SYSTEMIC_H

#include "party_runtime.h"
#include "../slot_manager.h"

/* ============= Systemic Level ============= */

/* Systemic: Collection of related parties forming a system */
typedef struct {
    const char* name;
    
    /* Party slots */
    struct {
        const char* slotName;
        const char* partyType;
        void* partyInstance;        /* Runtime party instance */
        PartyContext* partyContext; /* Party's context */
        bool isArray;               /* If true, partyInstance is array */
        size_t arraySize;
    }* partySlots;
    size_t partyCount;
    
    /* Shared system data */
    struct {
        const char* fieldName;
        uint32_t slotId;
        void* value;
    }* sharedFields;
    size_t sharedFieldCount;
    
    /* System metadata */
    const char* systemType;
    void* customData;
} SystemicContext;

/* Create a new systemic instance */
SystemicContext* CreateSystemic(
    const char* systemicType,
    const char* instanceName
);

/* Add party to systemic */
bool SystemicAddParty(
    SystemicContext* systemic,
    const char* slotName,
    void* partyInstance,
    PartyContext* partyContext
);

/* Execute all parties in systemic */
typedef struct {
    const char* partySlot;
    DispatchResult result;
} SystemicPartyResult;

typedef struct {
    SystemicPartyResult* partyResults;
    size_t resultCount;
    bool allSucceeded;
    uint64_t totalExecutionTimeNs;
} SystemicExecutionResult;

SystemicExecutionResult ExecuteSystemic(
    SystemicContext* systemic,
    JoinStrategy defaultStrategy,
    DispatcherConfig* config
);

/* Async execution */
typedef struct SystemicHandle SystemicHandle;

SystemicHandle* ExecuteSystemicAsync(
    SystemicContext* systemic,
    JoinStrategy defaultStrategy,
    DispatcherConfig* config
);

SystemicExecutionResult WaitForSystemic(
    SystemicHandle* handle,
    uint64_t timeoutMs
);

/* ============= World Level ============= */

/* World: The top-level container of all systemics */
typedef struct {
    const char* name;
    
    /* Systemic instances */
    struct {
        const char* slotName;
        const char* systemicType;
        SystemicContext* instance;
    }* systemics;
    size_t systemicCount;
    
    /* World-level shared data */
    struct {
        const char* fieldName;
        uint32_t slotId;
        void* value;
    }* sharedFields;
    size_t sharedFieldCount;
    
    /* World state */
    bool isRunning;
    uint64_t startTime;
    uint64_t frameCount;
    
    /* Custom world data */
    void* customData;
} WorldContext;

/* Create a new world */
WorldContext* CreateWorld(const char* worldName);

/* Add systemic to world */
bool WorldAddSystemic(
    WorldContext* world,
    const char* slotName,
    SystemicContext* systemic
);

/* World execution result */
typedef struct {
    const char* systemicSlot;
    SystemicExecutionResult result;
} WorldSystemicResult;

typedef struct {
    WorldSystemicResult* systemicResults;
    size_t resultCount;
    bool allSucceeded;
    uint64_t frameTimeNs;
    uint64_t totalFrames;
} WorldFrameResult;

/* Execute one world frame */
WorldFrameResult ExecuteWorldFrame(
    WorldContext* world,
    DispatcherConfig* config
);

/* Main world loop */
typedef struct {
    uint64_t targetFrameTimeNs;  /* Target frame duration */
    uint64_t maxFrames;          /* 0 = infinite */
    bool adaptiveSync;           /* Adjust to maintain framerate */
    
    /* Callbacks */
    void (*onFrameStart)(WorldContext* world, uint64_t frameNum);
    void (*onFrameEnd)(WorldContext* world, WorldFrameResult* result);
    void (*onSystemicError)(const char* systemic, const char* error);
} WorldLoopConfig;

/* Run world loop */
void RunWorldLoop(
    WorldContext* world,
    WorldLoopConfig* loopConfig,
    DispatcherConfig* dispatchConfig
);

/* Stop world loop */
void StopWorld(WorldContext* world);

/* ============= Cross-Level Communication ============= */

/* Find party in systemic */
PartyContext* SystemicFindParty(
    SystemicContext* systemic,
    const char* partySlot
);

/* Find systemic in world */
SystemicContext* WorldFindSystemic(
    WorldContext* world,
    const char* systemicSlot
);

/* Cross-systemic party access */
PartyContext* WorldFindParty(
    WorldContext* world,
    const char* systemicSlot,
    const char* partySlot
);

/* ============= Hierarchical Execution ============= */

/* Execution plan for entire hierarchy */
typedef struct {
    /* World level */
    const char* worldName;
    size_t systemicCount;
    
    /* Per-systemic plans */
    struct {
        const char* systemicName;
        size_t partyCount;
        
        /* Per-party fiber maps */
        struct {
            const char* partyName;
            FiberMap* fiberMap;
            size_t roleCount;
        }* parties;
    }* systemics;
    
    /* Total counts */
    size_t totalParties;
    size_t totalRoles;
    size_t totalFibers;
    
    /* Execution hints */
    bool canParallelizeSystemics;
    bool canParallelizeParties;
    size_t estimatedCpuFibers;
    size_t estimatedGpuFibers;
} HierarchicalExecutionPlan;

/* Generate execution plan for world */
HierarchicalExecutionPlan* GenerateWorldExecutionPlan(
    WorldContext* world
);

/* Optimize execution plan */
void OptimizeExecutionPlan(
    HierarchicalExecutionPlan* plan,
    const struct {
        uint32_t availableCpuCores;
        uint32_t availableGpuUnits;
        size_t availableMemory;
        bool preferLatency;  /* vs throughput */
    }* constraints
);

/* ============= Monitoring & Debugging ============= */

/* Hierarchical statistics */
typedef struct {
    /* World stats */
    uint64_t totalFrames;
    uint64_t avgFrameTimeNs;
    uint64_t maxFrameTimeNs;
    
    /* Per-systemic stats */
    struct {
        const char* systemicName;
        uint64_t totalExecutions;
        uint64_t avgExecutionTimeNs;
        uint32_t errorCount;
        
        /* Per-party stats */
        struct {
            const char* partyName;
            uint64_t avgPartyTimeNs;
            
            /* Role-level stats (from FiberStats) */
            FiberStats* roleStats;
            size_t roleCount;
        }* partyStats;
        size_t partyCount;
    }* systemicStats;
    size_t systemicCount;
} WorldStatistics;

/* Get world statistics */
WorldStatistics* GetWorldStatistics(WorldContext* world);

/* Dump world state */
void DumpWorldState(
    WorldContext* world,
    bool includeSystemics,
    bool includeParties,
    bool includeRoles
);

/* Visualize world hierarchy */
char* GenerateWorldVisualization(
    WorldContext* world,
    const char* format  /* "dot", "json", "text" */
);

/* ============= Memory Management ============= */

/* Cleanup functions */
void FreeSystemicContext(SystemicContext* systemic);
void FreeWorldContext(WorldContext* world);
void FreeExecutionPlan(HierarchicalExecutionPlan* plan);
void FreeWorldStatistics(WorldStatistics* stats);

/* ============= Integration Helpers ============= */

/* Macro for defining systemic */
#define DEFINE_SYSTEMIC(name, ...) \
    static SystemicContext* Create##name##Systemic() { \
        SystemicContext* sys = CreateSystemic(#name, #name "_instance"); \
        __VA_ARGS__ \
        return sys; \
    }

/* Macro for world frame execution */
#define WORLD_FRAME(world) \
    ExecuteWorldFrame(world, NULL)

/* Macro for simple world loop */
#define RUN_WORLD(world, fps) \
    RunWorldLoop(world, &(WorldLoopConfig){ \
        .targetFrameTimeNs = 1000000000ULL / (fps), \
        .maxFrames = 0, \
        .adaptiveSync = true \
    }, NULL)

#endif /* PERGYRA_WORLD_SYSTEMIC_H */
