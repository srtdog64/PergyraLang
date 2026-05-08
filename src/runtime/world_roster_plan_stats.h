#ifndef PERGYRA_WORLD_ROSTER_PLAN_STATS_H
#define PERGYRA_WORLD_ROSTER_PLAN_STATS_H

#include "../common/string_compat.h"

HierarchicalExecutionPlan*
GenerateWorldExecutionPlan(WorldContext* world)
{
    if (world == NULL) {
        world_roster_warn("generate_world_execution_plan", "world is null");
        return NULL;
    }

    HierarchicalExecutionPlan* plan =
        (HierarchicalExecutionPlan*)calloc(1, sizeof(HierarchicalExecutionPlan));
    if (plan == NULL) {
        world_roster_warn("generate_world_execution_plan", "plan allocation failed");
        return NULL;
    }

    plan->worldName = world_roster_strdup(world->name);
    plan->rosterCount = world->rosterCount;
    plan->canParallelizeRosters = (world->rosterCount > 1);
    plan->rosters = calloc(world->rosterCount, sizeof(*plan->rosters));
    if (plan->rosters == NULL && world->rosterCount > 0) {
        FreeExecutionPlan(plan);
        world_roster_warn("generate_world_execution_plan", "roster plan allocation failed");
        return NULL;
    }

    for (size_t i = 0; i < world->rosterCount; i++) {
        RosterContext* roster = world->rosters[i].instance;
        plan->rosters[i].rosterName = world_roster_strdup(world->rosters[i].slotName);
        plan->rosters[i].partyCount = roster != NULL ? roster->partyCount : 0;
        plan->totalParties += plan->rosters[i].partyCount;
        plan->canParallelizeParties = plan->canParallelizeParties || plan->rosters[i].partyCount > 1;

        plan->rosters[i].parties =
            calloc(plan->rosters[i].partyCount, sizeof(*plan->rosters[i].parties));
        for (size_t j = 0; roster != NULL && j < roster->partyCount; j++) {
            PartyContext* context = roster->partySlots[j].partyContext;
            FiberMap* map = PartyContextGetFiberMap(context);
            plan->rosters[i].parties[j].partyName =
                world_roster_strdup(roster->partySlots[j].slotName);
            plan->rosters[i].parties[j].fiberMap = map;
            plan->rosters[i].parties[j].roleCount = map != NULL ? map->entryCount : 0;
            plan->totalRoles += plan->rosters[i].parties[j].roleCount;
            plan->totalFibers += plan->rosters[i].parties[j].roleCount;
        }
    }

    OptimizeExecutionPlan(plan, NULL);
    return plan;
}

void
OptimizeExecutionPlan(HierarchicalExecutionPlan* plan,
                      const ExecutionConstraints* constraints)
{
    (void)constraints;
    if (plan == NULL) {
        return;
    }

    plan->estimatedCpuFibers = 0;
    plan->estimatedGpuFibers = 0;
    for (size_t i = 0; i < plan->rosterCount; i++) {
        for (size_t j = 0; j < plan->rosters[i].partyCount; j++) {
            FiberMap* map = plan->rosters[i].parties[j].fiberMap;
            if (map == NULL) {
                continue;
            }
            for (size_t k = 0; k < map->entryCount; k++) {
                if (map->entries[k].schedulerTag == SCHEDULER_GPU_FIBER) {
                    plan->estimatedGpuFibers++;
                } else {
                    plan->estimatedCpuFibers++;
                }
            }
        }
    }
}

WorldStatistics*
GetWorldStatistics(WorldContext* world)
{
    if (world == NULL) {
        world_roster_warn("get_world_statistics", "world is null");
        return NULL;
    }

    WorldStatistics* stats = (WorldStatistics*)calloc(1, sizeof(WorldStatistics));
    if (stats == NULL) {
        world_roster_warn("get_world_statistics", "stats allocation failed");
        return NULL;
    }

    stats->totalFrames = world->frameCount;
    stats->rosterCount = world->rosterCount;
    stats->rosterStats = calloc(world->rosterCount, sizeof(*stats->rosterStats));
    if (stats->rosterStats == NULL && world->rosterCount > 0) {
        FreeWorldStatistics(stats);
        return NULL;
    }

    for (size_t i = 0; i < world->rosterCount; i++) {
        RosterContext* roster = world->rosters[i].instance;
        stats->rosterStats[i].rosterName = world_roster_strdup(world->rosters[i].slotName);
        stats->rosterStats[i].partyCount = roster != NULL ? roster->partyCount : 0;
        stats->rosterStats[i].partyStats =
            calloc(stats->rosterStats[i].partyCount,
                   sizeof(*stats->rosterStats[i].partyStats));

        for (size_t j = 0; roster != NULL && j < roster->partyCount; j++) {
            PartyContext* context = roster->partySlots[j].partyContext;
            FiberMap* map = PartyContextGetFiberMap(context);
            stats->rosterStats[i].partyStats[j].partyName =
                world_roster_strdup(roster->partySlots[j].slotName);
            stats->rosterStats[i].partyStats[j].roleCount = map != NULL ? map->entryCount : 0;
            if (map != NULL && map->entryCount > 0) {
                stats->rosterStats[i].partyStats[j].roleStats =
                    (FiberStats*)calloc(map->entryCount, sizeof(FiberStats));
                for (size_t k = 0; k < map->entryCount; k++) {
                    stats->rosterStats[i].partyStats[j].roleStats[k] =
                        GetFiberStats(map->entries[k].roleId);
                    stats->rosterStats[i].partyStats[j].avgPartyTimeNs +=
                        stats->rosterStats[i].partyStats[j].roleStats[k].avgTimeNs;
                    stats->rosterStats[i].avgExecutionTimeNs +=
                        stats->rosterStats[i].partyStats[j].roleStats[k].avgTimeNs;
                    stats->rosterStats[i].totalExecutions +=
                        stats->rosterStats[i].partyStats[j].roleStats[k].totalExecutions;
                    stats->rosterStats[i].errorCount +=
                        stats->rosterStats[i].partyStats[j].roleStats[k].errorCount;
                }
            }
        }
        if (stats->rosterStats[i].partyCount > 0) {
            stats->avgFrameTimeNs += stats->rosterStats[i].avgExecutionTimeNs;
        }
    }
    if (stats->rosterCount > 0) {
        stats->avgFrameTimeNs /= stats->rosterCount;
        stats->maxFrameTimeNs = stats->avgFrameTimeNs;
    }

    return stats;
}

void
DumpWorldState(WorldContext* world,
               bool includeRosters,
               bool includeParties,
               bool includeRoles)
{
    if (world == NULL) {
        world_roster_warn("dump_world_state", "world is null");
        return;
    }

    printf("World %s frames=%llu running=%s\n",
           world->name != NULL ? world->name : "<unnamed>",
           (unsigned long long)world->frameCount,
           world->isRunning ? "true" : "false");
    if (!includeRosters) {
        return;
    }

    for (size_t i = 0; i < world->rosterCount; i++) {
        RosterContext* roster = world->rosters[i].instance;
        printf("  Roster %s type=%s parties=%zu\n",
               world->rosters[i].slotName,
               world->rosters[i].rosterType,
               roster != NULL ? roster->partyCount : 0U);
        if (!includeParties || roster == NULL) {
            continue;
        }
        for (size_t j = 0; j < roster->partyCount; j++) {
            PartyContext* context = roster->partySlots[j].partyContext;
            FiberMap* map = PartyContextGetFiberMap(context);
            printf("    Party %s type=%s roles=%zu\n",
                   roster->partySlots[j].slotName,
                   roster->partySlots[j].partyType,
                   map != NULL ? map->entryCount : 0U);
            if (!includeRoles || map == NULL) {
                continue;
            }
            for (size_t k = 0; k < map->entryCount; k++) {
                printf("      Role %s scheduler=%d priority=%d\n",
                       map->entries[k].roleId,
                       (int)map->entries[k].schedulerTag,
                       (int)map->entries[k].priority);
            }
        }
    }
}

char*
GenerateWorldVisualization(WorldContext* world, const char* format)
{
    if (world == NULL) {
        world_roster_warn("generate_world_visualization", "world is null");
        return NULL;
    }

    const char* mode = format != NULL ? format : "text";
    size_t capacity = 1024U + (world->rosterCount * 256U);
    char* out = (char*)calloc(capacity, 1);
    if (out == NULL) {
        world_roster_warn("generate_world_visualization", "buffer allocation failed");
        return NULL;
    }

    if (strcmp(mode, "dot") == 0) {
        snprintf(out, capacity, "digraph %s {\n", world->name != NULL ? world->name : "World");
        for (size_t i = 0; i < world->rosterCount; i++) {
            pergyra_str_append(out, capacity, "  world -> ");
            pergyra_str_append(out, capacity, world->rosters[i].slotName);
            pergyra_str_append(out, capacity, ";\n");
        }
        pergyra_str_append(out, capacity, "}\n");
        return out;
    }

    if (strcmp(mode, "json") == 0) {
        snprintf(out, capacity, "{\"world\":\"%s\",\"rosterCount\":%zu}",
                 world->name != NULL ? world->name : "World",
                 world->rosterCount);
        return out;
    }

    snprintf(out, capacity, "World %s\n", world->name != NULL ? world->name : "World");
    for (size_t i = 0; i < world->rosterCount; i++) {
        pergyra_str_append(out, capacity, "- Roster ");
        pergyra_str_append(out, capacity, world->rosters[i].slotName);
        pergyra_str_append(out, capacity, "\n");
    }
    return out;
}

void
FreeRosterContext(RosterContext* roster)
{
    if (roster == NULL) {
        return;
    }
    for (size_t i = 0; i < roster->partyCount; i++) {
        free((void*)roster->partySlots[i].slotName);
        free((void*)roster->partySlots[i].partyType);
    }
    free(roster->partySlots);
    free((void*)roster->name);
    free((void*)roster->systemType);
    free(roster);
}

void
FreeWorldContext(WorldContext* world)
{
    if (world == NULL) {
        return;
    }
    for (size_t i = 0; i < world->rosterCount; i++) {
        free((void*)world->rosters[i].slotName);
        free((void*)world->rosters[i].rosterType);
        FreeRosterContext(world->rosters[i].instance);
    }
    free(world->rosters);
    free((void*)world->name);
    free(world);
}

void
FreeExecutionPlan(HierarchicalExecutionPlan* plan)
{
    if (plan == NULL) {
        return;
    }
    for (size_t i = 0; i < plan->rosterCount; i++) {
        free((void*)plan->rosters[i].rosterName);
        for (size_t j = 0; j < plan->rosters[i].partyCount; j++) {
            free((void*)plan->rosters[i].parties[j].partyName);
        }
        free(plan->rosters[i].parties);
    }
    free(plan->rosters);
    free((void*)plan->worldName);
    free(plan);
}

void
FreeWorldStatistics(WorldStatistics* stats)
{
    if (stats == NULL) {
        return;
    }
    for (size_t i = 0; i < stats->rosterCount; i++) {
        free((void*)stats->rosterStats[i].rosterName);
        for (size_t j = 0; j < stats->rosterStats[i].partyCount; j++) {
            free((void*)stats->rosterStats[i].partyStats[j].partyName);
            free(stats->rosterStats[i].partyStats[j].roleStats);
        }
        free(stats->rosterStats[i].partyStats);
    }
    free(stats->rosterStats);
    free(stats);
}

#endif /* PERGYRA_WORLD_ROSTER_PLAN_STATS_H */
