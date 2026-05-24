/*
 * Copyright (c) 2025 Pergyra Language Project
 * World-Roster async handle execution.
 */

#include "world_roster_internal.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

struct RosterHandle {
    pthread_t thread;
    bool threadStarted;
    atomic_bool completed;
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
    atomic_store_explicit(&handle->completed, true, memory_order_release);
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
    atomic_init(&handle->completed, false);

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

    if (!atomic_load_explicit(&handle->completed, memory_order_acquire)) {
        if (timeoutMs == 0) {
            while (!atomic_load_explicit(&handle->completed, memory_order_acquire)) {
#ifdef _WIN32
                Sleep(0);
#else
                sched_yield();
#endif
            }
        } else {
            uint64_t start = world_roster_now_ns();
            while (!atomic_load_explicit(&handle->completed, memory_order_acquire)) {
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
