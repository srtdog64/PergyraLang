#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifndef PGY_HOST_CLOCK_REAL
static const char *probe_mode;
static int samples;
static int probe_clock_gettime(clockid_t id, struct timespec *ts)
{
    /* Observe the actual clock request, not just its returned number. */
    assert(id == CLOCK_MONOTONIC);
    if (strcmp(probe_mode, "failure") == 0) { errno = EIO; return -1; }
    ts->tv_sec = (time_t)(INT32_MAX / 1000 + 10);
    ts->tv_nsec = 1000000L * samples++;
    if (strcmp(probe_mode, "negative") == 0) ts->tv_sec = -1;
    if (strcmp(probe_mode, "invalid-ns") == 0) ts->tv_nsec = 1000000000L;
    if (strcmp(probe_mode, "overflow") == 0) ts->tv_sec = (time_t)INT64_MAX;
    if (strcmp(probe_mode, "ns-overflow") == 0)
        ts->tv_sec = (time_t)(INT64_MAX / 1000000000LL + 1);
    return 0;
}
#define clock_gettime probe_clock_gettime
#endif

#ifdef PGY_HOST_CLOCK_LINKED
#define PGY_LLVM_ENABLED
#include "../src/runtime/pgy_runtime_lib.c"
#else
#include "../src/runtime/pgy_runtime.h"
#endif

_Static_assert(sizeof(pgy_now_ms()) == sizeof(int64_t), "Now must return Long");

int main(int argc, char **argv)
{
    (void)argc;
    pgy_cap_grant_all_export();
#ifndef PGY_HOST_CLOCK_REAL
    assert(argc == 2);
    probe_mode = argv[1];
    if (strcmp(probe_mode, "deny") == 0) pgy_cap_set_manifest_export(0);
    if (strcmp(probe_mode, "invalid-unit") == 0) {
        (void)pgy_host_monotonic_time(3);
        return 1;
    }
#ifdef PGY_HOST_CLOCK_LINKED
    if (strcmp(probe_mode, "ns-overflow") == 0) {
        (void)pgy_clock_now_ns_export();
        return 1;
    }
    if (strcmp(probe_mode, "real-advance") == 0) {
        pgy_clock_advance_ns_export(1);
        return 1;
    }
    if (strcmp(probe_mode, "virtual") == 0) {
        assert(setenv("PGY_VIRTUAL_CLOCK", "1", 1) == 0);
        assert(pgy_clock_now_ns_export() == 0);
        pgy_clock_advance_ns_export(5000000);
        assert(pgy_clock_now_ns_export() == 5000000);
        puts("virtual clock mode remains explicit and deterministic PASS");
        return 0;
    }
#endif
#else
    (void)argv;
#endif
    int64_t first = pgy_now_ms();
    int64_t second = pgy_now_ms();
    assert(first >= 0 && second >= first);
#ifndef PGY_HOST_CLOCK_REAL
    assert(strcmp(probe_mode, "ok") == 0);
    assert(first > INT32_MAX && second == first + 1);
    assert(first == (int64_t)(INT32_MAX / 1000 + 10) * 1000);
#endif
#ifdef PGY_HOST_CLOCK_LINKED
    int64_t ns = pgy_clock_now_ns_export();
    assert(ns >= second * 1000000LL);
#ifndef PGY_HOST_CLOCK_REAL
    assert(ns == (int64_t)(INT32_MAX / 1000 + 10) * 1000000000LL + 2000000);
#endif
#endif
    puts("host clock: Long range, monotonic source and gated call PASS");
    return 0;
}
