/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party fiber statistics runtime owner.
 */

#include "party_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    FiberStats* stats;
    size_t count;
    size_t capacity;
    uint64_t* indexHashes;
    size_t* indexSlots;
    size_t indexCapacity;
} g_fiberStats = {0};

static char*
party_stats_strdup(const char* text)
{
    size_t length;
    char* copy;

    if (text == NULL)
        return NULL;

    length = strlen(text) + 1U;
    copy = (char*)malloc(length);
    if (copy == NULL)
        return NULL;

    memcpy(copy, text, length);
    return copy;
}

static uint64_t
party_stats_hash_string(const char* str)
{
    uint64_t hash = 5381;
    int c = 0;

    if (str == NULL)
        return 0;

    while ((c = *str++) != 0)
        hash = ((hash << 5) + hash) + (uint64_t)c;
    return hash;
}

static uint64_t
fiber_stats_hash_role(const char* roleId)
{
    uint64_t hash = party_stats_hash_string(roleId);
    return hash != 0 ? hash : 1;
}

static bool fiber_stats_index_insert(const char* roleId, size_t statIndex);

static bool
fiber_stats_rebuild_index(size_t newCapacity)
{
    uint64_t* hashes = (uint64_t*)calloc(newCapacity, sizeof(uint64_t));
    size_t* slots = (size_t*)calloc(newCapacity, sizeof(size_t));
    uint64_t* oldHashes;
    size_t* oldSlots;
    size_t oldCapacity;

    if (hashes == NULL || slots == NULL) {
        free(hashes);
        free(slots);
        return false;
    }

    oldHashes = g_fiberStats.indexHashes;
    oldSlots = g_fiberStats.indexSlots;
    oldCapacity = g_fiberStats.indexCapacity;
    g_fiberStats.indexHashes = hashes;
    g_fiberStats.indexSlots = slots;
    g_fiberStats.indexCapacity = newCapacity;

    for (size_t i = 0; i < g_fiberStats.count; i++) {
        if (g_fiberStats.stats[i].roleId != NULL
            && !fiber_stats_index_insert(g_fiberStats.stats[i].roleId, i)) {
            free(g_fiberStats.indexHashes);
            free(g_fiberStats.indexSlots);
            g_fiberStats.indexHashes = oldHashes;
            g_fiberStats.indexSlots = oldSlots;
            g_fiberStats.indexCapacity = oldCapacity;
            return false;
        }
    }

    free(oldHashes);
    free(oldSlots);
    return true;
}

static bool
fiber_stats_ensure_index_capacity(size_t countAfterInsert)
{
    size_t capacity = g_fiberStats.indexCapacity;

    if (capacity != 0 && countAfterInsert * 4U < capacity * 3U)
        return true;

    capacity = capacity != 0 ? capacity * 2U : 32U;
    while (countAfterInsert * 4U >= capacity * 3U)
        capacity *= 2U;
    return fiber_stats_rebuild_index(capacity);
}

static FiberStats*
fiber_stats_lookup_linear(const char* roleId)
{
    for (size_t i = 0; i < g_fiberStats.count; i++) {
        if (g_fiberStats.stats[i].roleId != NULL
            && strcmp(g_fiberStats.stats[i].roleId, roleId) == 0) {
            return &g_fiberStats.stats[i];
        }
    }
    return NULL;
}

static FiberStats*
fiber_stats_lookup(const char* roleId)
{
    uint64_t hash;

    if (roleId == NULL)
        return NULL;
    if (g_fiberStats.indexCapacity == 0)
        return fiber_stats_lookup_linear(roleId);

    hash = fiber_stats_hash_role(roleId);
    for (size_t probe = 0; probe < g_fiberStats.indexCapacity; probe++) {
        size_t slot = (size_t)((hash + probe) & (g_fiberStats.indexCapacity - 1U));
        if (g_fiberStats.indexHashes[slot] == 0)
            return NULL;
        if (g_fiberStats.indexHashes[slot] != hash)
            continue;

        size_t statIndex = g_fiberStats.indexSlots[slot];
        if (statIndex < g_fiberStats.count
            && g_fiberStats.stats[statIndex].roleId != NULL
            && strcmp(g_fiberStats.stats[statIndex].roleId, roleId) == 0) {
            return &g_fiberStats.stats[statIndex];
        }
    }
    return fiber_stats_lookup_linear(roleId);
}

static bool
fiber_stats_index_insert(const char* roleId, size_t statIndex)
{
    uint64_t hash;

    if (roleId == NULL)
        return false;
    if (!fiber_stats_ensure_index_capacity(g_fiberStats.count + 1U))
        return false;

    hash = fiber_stats_hash_role(roleId);
    for (size_t probe = 0; probe < g_fiberStats.indexCapacity; probe++) {
        size_t slot = (size_t)((hash + probe) & (g_fiberStats.indexCapacity - 1U));
        if (g_fiberStats.indexHashes[slot] == 0
            || g_fiberStats.indexSlots[slot] == statIndex) {
            g_fiberStats.indexHashes[slot] = hash;
            g_fiberStats.indexSlots[slot] = statIndex;
            return true;
        }
    }
    return false;
}

void
UpdateFiberStats(const char* roleId, const FiberResult* result)
{
    FiberStats* stats;

    if (roleId == NULL || result == NULL)
        return;

    stats = fiber_stats_lookup(roleId);
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
        stats->roleId = party_stats_strdup(roleId);
        if (stats->roleId == NULL) {
            g_fiberStats.count--;
            party_runtime_warn("fiber_stats", "role id allocation failed");
            return;
        }
        if (!fiber_stats_index_insert(stats->roleId, g_fiberStats.count - 1U))
            party_runtime_warn("fiber_stats", "stats index update failed");
        stats->minTimeNs = UINT64_MAX;
    }

    stats->totalExecutions++;
    stats->totalTimeNs += result->executionTimeNs;
    if (result->executionTimeNs < stats->minTimeNs)
        stats->minTimeNs = result->executionTimeNs;
    if (result->executionTimeNs > stats->maxTimeNs)
        stats->maxTimeNs = result->executionTimeNs;
    stats->avgTimeNs =
        stats->totalExecutions > 0 ? stats->totalTimeNs / stats->totalExecutions : 0;
    if (!result->success)
        stats->errorCount++;
}

FiberStats
GetFiberStats(const char* roleId)
{
    FiberStats empty = {0};
    FiberStats* stats;

    if (roleId == NULL)
        return empty;

    stats = fiber_stats_lookup(roleId);
    return stats != NULL ? *stats : empty;
}

void
party_runtime_dump_fiber_stats(void)
{
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
