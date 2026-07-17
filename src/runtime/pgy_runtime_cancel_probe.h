/*
 * Copyright (c) 2026 Pergyra Language Project
 * Cooperative-cancellation probe hook (docs/181 SS2.4, docs/182 SS2.2).
 *
 * Parked channel waits poll this hook to honor task cancellation
 * without coupling the channel header to the parallel task machinery:
 * the static-inline task ops instantiate AFTER the channels in the
 * bitcode TU and BEFORE them in generated C, so no direct call
 * compiles in both. The parallel runtime installs its probe at every
 * task-creation path; a NULL probe means no cancellation source exists
 * yet and every wait behaves exactly as before. The hook is per-TU
 * state, matching the per-TU pool instances of each backend.
 */

#ifndef PGY_RUNTIME_CANCEL_PROBE_H
#define PGY_RUNTIME_CANCEL_PROBE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

/* Installed by spawner threads and read by channel-waiting threads with no
 * common lock; a plain pointer would be a formal C11 data race the compiler
 * may miscompile at -O2 even where an aligned store is atomic in hardware
 * (docs/189 C13). release/acquire pairs the install with the reads. */
static _Atomic(bool (*)(void)) g_pgy_cancel_probe;

static inline bool
pgy_cancel_probe_cancelled(void)
{
    bool (*probe)(void) = atomic_load_explicit(&g_pgy_cancel_probe,
                                               memory_order_acquire);
    return probe != NULL && probe();
}

static inline void
pgy_cancel_probe_install(bool (*probe)(void))
{
    atomic_store_explicit(&g_pgy_cancel_probe, probe,
                          memory_order_release);
}

#endif /* PGY_RUNTIME_CANCEL_PROBE_H */
