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
static pthread_mutex_t g_schedulerRegistryMutex = PTHREAD_MUTEX_INITIALIZER;

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
    char* ownedName = NULL;

    if (name == NULL || name[0] == '\0') {
        party_runtime_warn_scheduler("name is null or empty", tag, name);
        return false;
    }
    if (scheduler == NULL) {
        party_runtime_warn_scheduler("scheduler pointer is null", tag, name);
        return false;
    }

    pthread_mutex_lock(&g_schedulerRegistryMutex);

    if (tag >= SCHEDULER_MAIN_THREAD && tag <= SCHEDULER_CUSTOM_3
        && g_schedulerByTag[tag] != NULL) {
        pthread_mutex_unlock(&g_schedulerRegistryMutex);
        party_runtime_warn_scheduler("duplicate scheduler tag", tag, name);
        return false;
    }

    for (size_t i = 0; i < g_schedulerCount; i++) {
        if (g_schedulerRegistry[i].name != NULL
            && strcmp(g_schedulerRegistry[i].name, name) == 0) {
            pthread_mutex_unlock(&g_schedulerRegistryMutex);
            party_runtime_warn_scheduler("duplicate scheduler name", tag, name);
            return false;
        }
    }

    if (g_schedulerCount >= 16) {
        pthread_mutex_unlock(&g_schedulerRegistryMutex);
        party_runtime_warn_scheduler("registry is full", tag, name);
        return false;
    }

    ownedName = party_runtime_scheduler_strdup(name);
    if (ownedName == NULL) {
        pthread_mutex_unlock(&g_schedulerRegistryMutex);
        party_runtime_warn_scheduler("name allocation failed", tag, name);
        return false;
    }

    g_schedulerRegistry[g_schedulerCount].tag = tag;
    g_schedulerRegistry[g_schedulerCount].name = ownedName;
    g_schedulerRegistry[g_schedulerCount].scheduler = scheduler;
    if (tag >= SCHEDULER_MAIN_THREAD && tag <= SCHEDULER_CUSTOM_3)
        g_schedulerByTag[tag] = scheduler;
    g_schedulerCount++;
    pthread_mutex_unlock(&g_schedulerRegistryMutex);
    return true;
}

FiberScheduler*
GetSchedulerForTag(SchedulerTag tag)
{
    FiberScheduler* scheduler = NULL;

    if (tag == SCHEDULER_ANY)
        return pgy_mn_scheduler_get_current();

    if (tag >= SCHEDULER_MAIN_THREAD && tag <= SCHEDULER_CUSTOM_3) {
        pthread_mutex_lock(&g_schedulerRegistryMutex);
        scheduler = g_schedulerByTag[tag];
        pthread_mutex_unlock(&g_schedulerRegistryMutex);
        if (scheduler != NULL)
            return scheduler;
    }

    if (tag != SCHEDULER_ANY)
        party_runtime_warn("scheduler.lookup", "scheduler tag not registered");
    return NULL;
}

void
DumpFiberMaps(void)
{
    printf("=== PgyMnFiber Map Dump ===\n");

    pthread_mutex_lock(&g_schedulerRegistryMutex);
    printf("Registered Schedulers: %zu\n", g_schedulerCount);

    for (size_t i = 0; i < g_schedulerCount; i++) {
        printf("  [%d] %s -> %p\n",
               g_schedulerRegistry[i].tag,
               g_schedulerRegistry[i].name,
               (void*)g_schedulerRegistry[i].scheduler);
    }
    pthread_mutex_unlock(&g_schedulerRegistryMutex);

    party_runtime_dump_fiber_stats();
}
