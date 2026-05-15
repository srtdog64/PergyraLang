/*
 * Copyright (c) 2025 Pergyra Language Project
 * Party scheduler registry and debug dump implementation.
 */

#include "party_runtime_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    SchedulerTag tag;
    const char* name;
    FiberScheduler* scheduler;
} g_schedulerRegistry[16] = {0};

static FiberScheduler* g_schedulerByTag[SCHEDULER_CUSTOM_3 + 1] = {0};
static size_t g_schedulerCount = 0;

static void
party_runtime_warn_scheduler(const char* reason, SchedulerTag tag, const char* name)
{
    fprintf(stderr,
            "[pgy][party] scheduler registration failed: %s (tag=%d, name=%s)\n",
            reason != NULL ? reason : "unknown",
            (int)tag,
            name != NULL ? name : "<null>");
}

static char*
party_runtime_scheduler_strdup(const char* text)
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

    if (tag >= SCHEDULER_MAIN_THREAD && tag <= SCHEDULER_CUSTOM_3
        && g_schedulerByTag[tag] != NULL) {
        party_runtime_warn_scheduler("duplicate scheduler tag", tag, name);
        return false;
    }

    for (size_t i = 0; i < g_schedulerCount; i++) {
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

    char* ownedName = party_runtime_scheduler_strdup(name);
    if (ownedName == NULL) {
        party_runtime_warn_scheduler("name allocation failed", tag, name);
        return false;
    }

    g_schedulerRegistry[g_schedulerCount].tag = tag;
    g_schedulerRegistry[g_schedulerCount].name = ownedName;
    g_schedulerRegistry[g_schedulerCount].scheduler = scheduler;
    if (tag >= SCHEDULER_MAIN_THREAD && tag <= SCHEDULER_CUSTOM_3)
        g_schedulerByTag[tag] = scheduler;
    g_schedulerCount++;
    return true;
}

FiberScheduler*
GetSchedulerForTag(SchedulerTag tag)
{
    if (tag == SCHEDULER_ANY)
        return SchedulerGetCurrent();

    if (tag >= SCHEDULER_MAIN_THREAD && tag <= SCHEDULER_CUSTOM_3
        && g_schedulerByTag[tag] != NULL) {
        return g_schedulerByTag[tag];
    }

    if (tag != SCHEDULER_ANY)
        party_runtime_warn("scheduler.lookup", "scheduler tag not registered");
    return NULL;
}

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

    party_runtime_dump_fiber_stats();
}
