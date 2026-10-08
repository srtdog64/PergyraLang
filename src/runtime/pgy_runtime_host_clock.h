#ifndef PGY_RUNTIME_HOST_CLOCK_H
#define PGY_RUNTIME_HOST_CLOCK_H

#include <stdint.h>
#include <limits.h>
#include <time.h>
#include "pgy_runtime_panic_contract.h"
#ifdef _WIN32
#include <windows.h>
#endif

/* One host-clock owner for C inline calls and linked runtime exports. Clock
 * failures never masquerade as time zero, and narrowing never wraps time. */
static inline int64_t
pgy_host_monotonic_time(int64_t units_per_second)
{
    uint64_t seconds;
    uint64_t fractional;
    if (units_per_second != 1000 && units_per_second != 1000000000LL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "unsupported host clock unit");
    }
#ifdef _WIN32
    uint64_t ticks = (uint64_t)GetTickCount64();
    seconds = ticks / 1000;
    fractional = (ticks % 1000) * (uint64_t)(units_per_second / 1000);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0 || ts.tv_sec < 0 ||
        ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000L) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "monotonic host clock unavailable");
    }
    seconds = (uint64_t)ts.tv_sec;
    fractional = (uint64_t)ts.tv_nsec /
                 (uint64_t)(1000000000LL / units_per_second);
#endif
    if (seconds > ((uint64_t)INT64_MAX - fractional) /
                  (uint64_t)units_per_second) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          "monotonic host clock exceeds Long range");
    }
    return (int64_t)(seconds * (uint64_t)units_per_second + fractional);
}

#endif
