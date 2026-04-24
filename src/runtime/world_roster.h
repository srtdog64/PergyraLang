/*
 * Copyright (c) 2025 Pergyra Language Project
 * World-Roster Runtime Implementation
 * Hierarchical execution management from World to Ability
 */

#ifndef PERGYRA_WORLD_ROSTER_H
#define PERGYRA_WORLD_ROSTER_H

#include "party_runtime.h"
#include "slot_manager.h"

/* ============= Roster Level ============= */

/* Roster: Collection of related parties forming a system */
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
} RosterContext;

/* Create a new roster instance */
RosterContext* CreateRoster(
    const char* rosterType,
    const char* instanceName
);

/* Add party to roster */
bool RosterAddParty(
    RosterContext* roster,
    const char* slotName,
    void* partyInstance,
    PartyContext* partyContext
);

/* Execute all parties in roster */
typedef struct {
    const char* partySlot;
    DispatchResult result;
} RosterPartyResult;

typedef struct {
    RosterPartyResult* partyResults;
    size_t resultCount;
    bool allSucceeded;
    uint64_t totalExecutionTimeNs;
} RosterExecutionResult;

RosterExecutionResult ExecuteRoster(
    RosterContext* roster,
    JoinStrategy defaultStrategy,
    DispatcherConfig* config
);

/* Async execution */
typedef struct RosterHandle RosterHandle;

RosterHandle* ExecuteRosterAsync(
    RosterContext* roster,
    JoinStrategy defaultStrategy,
    DispatcherConfig* config
);

RosterExecutionResult WaitForRoster(
    RosterHandle* handle,
    uint64_t timeoutMs
);

/* ============= World Level ============= */

/* World: The top-level container of all rosters */
typedef struct {
    const char* name;
    
    /* Roster instances */
    struct {
        const char* slotName;
        const char* rosterType;
        RosterContext* instance;
    }* rosters;
    size_t rosterCount;
    
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

/* Add roster to world */
bool WorldAddRoster(
    WorldContext* world,
    const char* slotName,
    RosterContext* roster
);

/* World execution result */
typedef struct {
    const char* rosterSlot;
    RosterExecutionResult result;
} WorldRosterResult;

typedef struct {
    WorldRosterResult* rosterResults;
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
    void (*onRosterError)(const char* roster, const char* error);
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

/* Find party in roster */
PartyContext* RosterFindParty(
    RosterContext* roster,
    const char* partySlot
);

/* Find roster in world */
RosterContext* WorldFindRoster(
    WorldContext* world,
    const char* rosterSlot
);

/* Cross-roster party access */
PartyContext* WorldFindParty(
    WorldContext* world,
    const char* rosterSlot,
    const char* partySlot
);

/* ============= Hierarchical Execution ============= */

/* Execution plan for entire hierarchy */
typedef struct {
    /* World level */
    const char* worldName;
    size_t rosterCount;
    
    /* Per-roster plans */
    struct {
        const char* rosterName;
        size_t partyCount;
        
        /* Per-party fiber maps */
        struct {
            const char* partyName;
            FiberMap* fiberMap;
            size_t roleCount;
        }* parties;
    }* rosters;
    
    /* Total counts */
    size_t totalParties;
    size_t totalRoles;
    size_t totalFibers;
    
    /* Execution hints */
    bool canParallelizeRosters;
    bool canParallelizeParties;
    size_t estimatedCpuFibers;
    size_t estimatedGpuFibers;
} HierarchicalExecutionPlan;

typedef struct {
    uint32_t availableCpuCores;
    uint32_t availableGpuUnits;
    size_t availableMemory;
    bool preferLatency;  /* vs throughput */
} ExecutionConstraints;

/* Generate execution plan for world */
HierarchicalExecutionPlan* GenerateWorldExecutionPlan(
    WorldContext* world
);

/* Optimize execution plan */
void OptimizeExecutionPlan(
    HierarchicalExecutionPlan* plan,
    const ExecutionConstraints* constraints
);

/* ============= Monitoring & Debugging ============= */

/* Hierarchical statistics */
typedef struct {
    /* World stats */
    uint64_t totalFrames;
    uint64_t avgFrameTimeNs;
    uint64_t maxFrameTimeNs;
    
    /* Per-roster stats */
    struct {
        const char* rosterName;
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
    }* rosterStats;
    size_t rosterCount;
} WorldStatistics;

/* Get world statistics */
WorldStatistics* GetWorldStatistics(WorldContext* world);

/* Dump world state */
void DumpWorldState(
    WorldContext* world,
    bool includeRosters,
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
void FreeRosterContext(RosterContext* roster);
void FreeWorldContext(WorldContext* world);
void FreeExecutionPlan(HierarchicalExecutionPlan* plan);
void FreeWorldStatistics(WorldStatistics* stats);

/* ============= Integration Helpers ============= */

/* Macro for defining roster */
#define DEFINE_SYSTEMIC(name, ...) \
    static RosterContext* Create##name##Roster() { \
        RosterContext* sys = CreateRoster(#name, #name "_instance"); \
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

#endif /* PERGYRA_WORLD_ROSTER_H */
