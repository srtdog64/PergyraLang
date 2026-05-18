/*
 * Copyright (c) 2025 Pergyra Language Project
 * World-Roster async handle execution.
 */

#include "world_roster_internal.h"

#include <pthread.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

struct RosterHandle {
    pthread_t thread;
    bool threadStarted;
    bool completed;
    RosterContext* roster;
    JoinStrategy defaultStrategy;
    DispatcherConfig* config;
    RosterExecutionResult result;
};

static void*
world_roster_async_runner(void* arg)
{
    struct RosterHandle* handle = (struct RosterHandle*)arg;
    if (handle == NULL)
        return NULL;
    handle->result = ExecuteRoster(handle->roster,
                                   handle->defaultStrategy,
                                   handle->config);
    handle->completed = true;
    return NULL;
}

RosterHandle*
ExecuteRosterAsync(RosterContext* roster,
                   JoinStrategy defaultStrategy,
                   DispatcherConfig* config)
{
    if (roster == NULL) {
        world_roster_warn("execute_roster_async", "roster is null");
        return NULL;
    }

    struct RosterHandle* handle =
        (struct RosterHandle*)calloc(1, sizeof(struct RosterHandle));
    if (handle == NULL) {
        world_roster_warn("execute_roster_async", "handle allocation failed");
        return NULL;
    }

    handle->roster = roster;
    handle->defaultStrategy = defaultStrategy;
    handle->config = config;

    if (pthread_create(&handle->thread, NULL,
                       world_roster_async_runner, handle) != 0) {
        free(handle);
        world_roster_warn("execute_roster_async", "thread creation failed");
        return NULL;
    }
    handle->threadStarted = true;
    return handle;
}

RosterExecutionResult
WaitForRoster(RosterHandle* handle, uint64_t timeoutMs)
{
    RosterExecutionResult empty = {0};
    if (handle == NULL) {
        world_roster_warn("wait_for_roster", "handle is null");
        return empty;
    }

    if (!handle->completed) {
        if (timeoutMs == 0) {
            while (!handle->completed) {
#ifdef _WIN32
                Sleep(0);
#else
                sched_yield();
#endif
            }
        } else {
            uint64_t start = world_roster_now_ns();
            while (!handle->completed) {
                if ((world_roster_now_ns() - start) / 1000000ULL >= timeoutMs) {
                    world_roster_warn("wait_for_roster", "timeout exceeded");
                    return empty;
                }
                world_roster_sleep_ms(1);
            }
        }
    }

    if (handle->threadStarted) {
        pthread_join(handle->thread, NULL);
        handle->threadStarted = false;
    }

    RosterExecutionResult result = handle->result;
    free(handle);
    return result;
}
