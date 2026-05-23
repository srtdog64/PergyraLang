/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party fiber statistics runtime owner.
 */

#include "party_runtime_internal.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * g_fiberStats is a process-global registry. Multiple party_runtime_dispatch
 * call paths from different scheduler threads can drive UpdateFiberStats
 * concurrently. The growth path realloc()s the stats array and rebuilds the
 * open-addressed index, so a concurrent reader following an unlocked path
 * would race against rehash, which is the AI-generated UB pattern documented
 * in docs/113_memory_concurrency_model.md.
 *
 * Lock discipline: every reader and writer of g_fiberStats acquires this
 * mutex. The lock is a flat per-process mutex because the registry is
 * already a single shared aggregate; per-bucket locking would add
 * complexity without buying anything for the small expected stat volume.
 */
static pthread_mutex_t g_fiberStatsMutex = PTHREAD_MUTEX_INITIALIZER;

static struct {
    FiberStats* stats;
    size_t count;
    size_t capacity;
    uint64_t* indexHashes;
    size_t* indexSlots;
    size_t indexCapacity;
    bool indexHealthy;
} g_fiberStats = {0};

static void
party_runtime_free_fiber_stats_snapshot(FiberStats *snapshot, size_t count)
{
    if (snapshot == NULL)
        return;

    for (size_t i = 0; i < count; i++) {
        free((void *)snapshot[i].roleId);
        snapshot[i].roleId = NULL;
    }
    free(snapshot);
}

static bool
party_runtime_copy_fiber_stats_snapshot_role(FiberStats *slot,
                                             const FiberStats *source)
{
    char *roleId;
    size_t len;

    if (slot == NULL || source == NULL)
        return false;

    *slot = *source;
    slot->roleId = NULL;
    if (source->roleId == NULL)
        return true;

    len = strlen(source->roleId);
    roleId = (char *)malloc(len + 1U);
    if (roleId == NULL)
        return false;
    memcpy(roleId, source->roleId, len + 1U);
    slot->roleId = roleId;
    return true;
}

static char*
party_stats_strdup(const char* text)
{
    size_t length;
    char* copy;

    if (text == NULL)
        return NULL;

    length = strlen(text);
    if (length == SIZE_MAX)
        return NULL;
    length++;
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
party_stats_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

static bool
fiber_stats_rebuild_index(size_t newCapacity)
{
    if (!party_stats_array_fits(newCapacity, sizeof(uint64_t))
        || !party_stats_array_fits(newCapacity, sizeof(size_t))) {
        return false;
    }

    uint64_t* hashes = (uint64_t*)calloc(newCapacity, sizeof(uint64_t));
    size_t* slots = (size_t*)calloc(newCapacity, sizeof(size_t));
    uint64_t* oldHashes;
    size_t* oldSlots;
    size_t oldCapacity;
    bool oldHealthy;

    if (hashes == NULL || slots == NULL) {
        free(hashes);
        free(slots);
        return false;
    }

    oldHashes = g_fiberStats.indexHashes;
    oldSlots = g_fiberStats.indexSlots;
    oldCapacity = g_fiberStats.indexCapacity;
    oldHealthy = g_fiberStats.indexHealthy;
    g_fiberStats.indexHashes = hashes;
    g_fiberStats.indexSlots = slots;
    g_fiberStats.indexCapacity = newCapacity;
    g_fiberStats.indexHealthy = true;

    for (size_t i = 0; i < g_fiberStats.count; i++) {
        if (g_fiberStats.stats[i].roleId != NULL
            && !fiber_stats_index_insert(g_fiberStats.stats[i].roleId, i)) {
            free(g_fiberStats.indexHashes);
            free(g_fiberStats.indexSlots);
            g_fiberStats.indexHashes = oldHashes;
            g_fiberStats.indexSlots = oldSlots;
            g_fiberStats.indexCapacity = oldCapacity;
            g_fiberStats.indexHealthy = oldHealthy;
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

    if (countAfterInsert > SIZE_MAX / 4U)
        return false;
    if (capacity != 0 && countAfterInsert * 4U < capacity * 3U)
        return true;

    if (capacity == 0) {
        capacity = 32U;
    } else {
        if (capacity > SIZE_MAX / 2U)
            return false;
        capacity *= 2U;
    }
    while (countAfterInsert * 4U >= capacity * 3U) {
        if (capacity > SIZE_MAX / 2U)
            return false;
        capacity *= 2U;
    }
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
    if (g_fiberStats.indexCapacity == 0 || !g_fiberStats.indexHealthy)
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
    if (!fiber_stats_ensure_index_capacity(g_fiberStats.count + 1U)) {
        g_fiberStats.indexHealthy = false;
        return false;
    }

    hash = fiber_stats_hash_role(roleId);
    for (size_t probe = 0; probe < g_fiberStats.indexCapacity; probe++) {
        size_t slot = (size_t)((hash + probe) & (g_fiberStats.indexCapacity - 1U));
        if (g_fiberStats.indexHashes[slot] == 0
            || g_fiberStats.indexSlots[slot] == statIndex) {
            g_fiberStats.indexHashes[slot] = hash;
            g_fiberStats.indexSlots[slot] = statIndex;
            g_fiberStats.indexHealthy = true;
            return true;
        }
    }
    g_fiberStats.indexHealthy = false;
    return false;
}

void
UpdateFiberStats(const char* roleId, const FiberResult* result)
{
    FiberStats* stats;

    if (roleId == NULL || result == NULL)
        return;

    pthread_mutex_lock(&g_fiberStatsMutex);
    stats = fiber_stats_lookup(roleId);
    if (stats == NULL) {
        if (g_fiberStats.count >= g_fiberStats.capacity) {
            size_t newCapacity;
            FiberStats* newStats;
            if (g_fiberStats.capacity == 0) {
                newCapacity = 16U;
            } else {
                if (g_fiberStats.capacity > SIZE_MAX / 2U) {
                    party_runtime_warn("fiber_stats", "stats capacity overflow");
                    goto done;
                }
                newCapacity = g_fiberStats.capacity * 2U;
            }
            if (!party_stats_array_fits(newCapacity, sizeof(FiberStats))) {
                party_runtime_warn("fiber_stats", "stats allocation size overflow");
                goto done;
            }
            newStats = (FiberStats*)realloc(g_fiberStats.stats,
                                            newCapacity * sizeof(FiberStats));
            if (newStats == NULL) {
                party_runtime_warn("fiber_stats", "stats array growth failed");
                goto done;
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
            goto done;
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

done:
    pthread_mutex_unlock(&g_fiberStatsMutex);
}

FiberStats
GetFiberStats(const char* roleId)
{
    FiberStats empty = {0};
    FiberStats* stats;
    FiberStats result;

    if (roleId == NULL)
        return empty;

    pthread_mutex_lock(&g_fiberStatsMutex);
    stats = fiber_stats_lookup(roleId);
    /* roleId is registry-owned storage; callers must treat it as borrowed. */
    result = stats != NULL ? *stats : empty;
    pthread_mutex_unlock(&g_fiberStatsMutex);
    return result;
}

void
party_runtime_dump_fiber_stats(void)
{
    FiberStats* snapshot = NULL;
    size_t snapshot_count = 0;

    printf("\nFiber Statistics:\n");

    pthread_mutex_lock(&g_fiberStatsMutex);
    snapshot_count = g_fiberStats.count;
    if (snapshot_count > 0
        && party_stats_array_fits(snapshot_count, sizeof(FiberStats))) {
        snapshot = (FiberStats*)malloc(snapshot_count * sizeof(FiberStats));
        if (snapshot != NULL) {
            size_t copied = 0;
            for (; copied < snapshot_count; copied++) {
                if (!party_runtime_copy_fiber_stats_snapshot_role(
                        &snapshot[copied], &g_fiberStats.stats[copied])) {
                    party_runtime_free_fiber_stats_snapshot(snapshot, copied);
                    snapshot = NULL;
                    break;
                }
            }
        }
    }
    if (snapshot_count > 0 && snapshot == NULL) {
        party_runtime_warn("fiber_stats", "stats snapshot allocation failed");
        snapshot_count = 0;
    }
    pthread_mutex_unlock(&g_fiberStatsMutex);

    for (size_t i = 0; i < snapshot_count; i++) {
        FiberStats* stats = &snapshot[i];
        printf("  Role: %s\n", stats->roleId != NULL ? stats->roleId : "<unknown>");
        printf("    Executions: %llu\n", (unsigned long long)stats->totalExecutions);
        printf("    Avg Time: %llu ns\n", (unsigned long long)stats->avgTimeNs);
        printf("    Min/Max: %llu / %llu\n",
               (unsigned long long)stats->minTimeNs,
               (unsigned long long)stats->maxTimeNs);
        printf("    Errors: %u\n", stats->errorCount);
    }
    party_runtime_free_fiber_stats_snapshot(snapshot, snapshot_count);
}
