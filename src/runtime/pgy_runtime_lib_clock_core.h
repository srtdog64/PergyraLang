#ifndef PGY_RUNTIME_LIB_CLOCK_CORE_H
#define PGY_RUNTIME_LIB_CLOCK_CORE_H

/* Reactive time axis shared by generated C and the LLVM runtime object.
 * PGY_VIRTUAL_CLOCK=1 latches deterministic virtual time on first use. */
static atomic_llong g_pgy_clock_virtual_ns;
static atomic_int   g_pgy_clock_mode; /* 0 unlatched, 1 real, 2 virtual */

static int
pgy_clock_latched_mode(void)
{
    int mode = atomic_load_explicit(&g_pgy_clock_mode,
                                    memory_order_acquire);
    if (mode == 0) {
        const char *v = getenv("PGY_VIRTUAL_CLOCK");
        mode = (v != NULL && v[0] == '1') ? 2 : 1;
        atomic_store_explicit(&g_pgy_clock_mode, mode,
                              memory_order_release);
    }
    return mode;
}

int64_t
pgy_clock_now_ns_export(void)
{
    if (pgy_clock_latched_mode() == 2)
        return (int64_t)atomic_load_explicit(&g_pgy_clock_virtual_ns,
                                             memory_order_acquire);
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
    }
}

void
pgy_clock_advance_ns_export(int64_t ns)
{
    if (pgy_clock_latched_mode() != 2) {
        fprintf(stderr,
            "%s clock lifecycle violation: op=advance on the real clock"
            " (PGY_VIRTUAL_CLOCK=1 is required)\n",
            PGY_RUNTIME_PANIC_PREFIX);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_LIFECYCLE_STATE,
                          PGY_RUNTIME_PANIC_REASON_INVALID_LIFECYCLE_STATE);
    }
    if (ns < 0) {
        fprintf(stderr,
            "%s clock lifecycle violation: op=advance by a negative"
            " duration\n",
            PGY_RUNTIME_PANIC_PREFIX);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_LIFECYCLE_STATE,
                          PGY_RUNTIME_PANIC_REASON_INVALID_LIFECYCLE_STATE);
    }
    atomic_fetch_add_explicit(&g_pgy_clock_virtual_ns, (long long)ns,
                              memory_order_acq_rel);
}

int32_t
pgy_clock_is_virtual_export(void)
{
    return pgy_clock_latched_mode() == 2 ? 1 : 0;
}

#endif /* PGY_RUNTIME_LIB_CLOCK_CORE_H */
