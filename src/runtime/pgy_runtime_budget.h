#ifndef PGY_RUNTIME_BUDGET_H
#define PGY_RUNTIME_BUDGET_H

#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "pgy_runtime_panic_contract.h"

/*
 * Content resource-budget vocabulary (the quantitative half of the sandbox).
 *
 * The capability gate (pgy_runtime_capability.h) answers a QUALITATIVE question:
 * "can this content do X at all?" (one bit per capability). It says nothing
 * about HOW MUCH: a program granted RENDER can still spin an infinite loop, a
 * program granted nothing can still exhaust memory or fork-bomb spawns. That is
 * a denial-of-service surface the capability mask cannot see (external red-team
 * R6, docs/134).
 *
 * This header is the QUANTITATIVE gate. A loader running untrusted content sets
 * a per-kind ceiling (pgy_budget_set_limit_export); each metered operation
 * charges its kind (pgy_budget_charge_export); the first charge that pushes a
 * kind's running total past its ceiling panics fail-closed with class
 * `budget-exceeded`. Same fail-closed discipline as the capability gate, applied
 * to a counter instead of a bit.
 *
 * Default limit is PGY_BUDGET_UNLIMITED for every kind, so ordinary (trusted)
 * programs are unaffected until a loader imposes a budget — opt-in by the host,
 * exactly like the capability default of PGY_CAP_ALL.
 */

typedef enum {
    PGY_BUDGET_ALLOC_BYTES = 0,  /* total bytes requested from the allocator   */
    PGY_BUDGET_ALLOC_COUNT,      /* number of allocations                      */
    PGY_BUDGET_SPAWN_COUNT,      /* fibers/tasks spawned (fork-bomb bound)     */
    PGY_BUDGET_CHANNEL_COUNT,    /* channels created                           */
    PGY_BUDGET_KIND_COUNT
} PgyBudgetKind;

#define PGY_BUDGET_UNLIMITED (~(uint64_t)0)

/*
 * Per-kind ceiling + running total. Shared shape between the static-inline twin
 * (C self-contained output) and the extern twin (LLVM-linked runtime object).
 * Default-constructed state must read as "every kind unlimited", so the state
 * is lazily initialised (a zero-init struct would mean every limit is 0 and the
 * first charge of any kind would panic). `initialized` drives that one-shot.
 */
typedef struct {
    /* `limit` is set once by the loader before the content runs, then read-only,
     * so it is plain. `used` is a write-many running total charged from metered
     * hot paths (the allocator), which a multi-threaded sandboxed program hits
     * from several fibers at once -- it is atomic so the bound stays sound (no
     * lost increments) under concurrency. */
    uint64_t          limit[PGY_BUDGET_KIND_COUNT];
    _Atomic uint64_t  used[PGY_BUDGET_KIND_COUNT];
    int      initialized;
    int      imposed;   /* a loader set at least one finite ceiling; metered
                           hot paths (the allocator) skip charging when 0 so
                           trusted programs pay nothing */
} PgyBudgetState;

/* Host-imposed budget channel: a loader sets PGY_BUDGET_* in the environment
 * before running untrusted content (the simple channel until a signed manifest
 * format exists). Read once at init; absent/empty leaves the kind unlimited. */
static inline void
pgy_budget_apply_env_limit(PgyBudgetState *s, int kind, const char *name)
{
    const char *v = getenv(name);
    if (v != NULL && v[0] != '\0') {
        s->limit[kind] = (uint64_t)strtoull(v, NULL, 10);
        s->imposed = 1;
    }
}

static inline void
pgy_budget_state_init(PgyBudgetState *s)
{
    int i;
    if (s == NULL)
        return;
    for (i = 0; i < PGY_BUDGET_KIND_COUNT; i++) {
        s->limit[i] = PGY_BUDGET_UNLIMITED;
        s->used[i] = 0;
    }
    s->initialized = 1;
    s->imposed = 0;
    pgy_budget_apply_env_limit(s, PGY_BUDGET_ALLOC_BYTES, "PGY_BUDGET_ALLOC_BYTES");
    pgy_budget_apply_env_limit(s, PGY_BUDGET_ALLOC_COUNT, "PGY_BUDGET_ALLOC_COUNT");
    pgy_budget_apply_env_limit(s, PGY_BUDGET_SPAWN_COUNT, "PGY_BUDGET_SPAWN_COUNT");
    pgy_budget_apply_env_limit(s, PGY_BUDGET_CHANNEL_COUNT, "PGY_BUDGET_CHANNEL_COUNT");
}

static inline int
pgy_budget_kind_valid(int kind)
{
    return kind >= 0 && kind < (int)PGY_BUDGET_KIND_COUNT;
}

/*
 * Atomically add `amount` to a kind's running total and return the post-charge
 * value. Shared by both twins so the atomic increment lives in one place. A
 * relaxed fetch-add is enough: each kind's total only ever needs to be a sound
 * monotonic count (no inter-kind ordering), and the ceiling check reads the
 * value this charge produced. Wrap past 2^64 is unreachable for any real budget
 * (the ceiling fires first). Invalid kind charges nothing.
 */
static inline uint64_t
pgy_budget_charge_into(PgyBudgetState *s, int kind, uint64_t amount)
{
    uint64_t old;
    if (s == NULL || !pgy_budget_kind_valid(kind))
        return 0;
    old = atomic_fetch_add_explicit(&s->used[kind], amount, memory_order_relaxed);
    return old + amount;
}

/*
 * Wall-clock deadline -- the time axis of the quantitative sandbox. The per-kind
 * counters above bound discrete resources, but a tight loop (`while (true) {}`)
 * that never allocates, spawns, or opens a channel consumes none of them while
 * running forever. A detached watchdog thread bounds that: it sleeps the host's
 * PGY_BUDGET_WALL_MS and then fail-closes the whole process (abort terminates
 * from any thread, so the bound holds regardless of what the main thread is
 * doing -- it catches the tight spin the counters cannot see). This is a
 * WALL-CLOCK deadline, not a CPU-time budget: a sleeping program still counts
 * against it. Default (env unset) arms nothing, so trusted programs are
 * unaffected -- opt-in by the host, like the counters. Armed once at main entry
 * via codegen (pgy_budget_wall_arm_export), not a constructor: one call site
 * means one watchdog, sidestepping the multi-TU duplication a header-defined
 * constructor would cause.
 */
static inline void *
pgy_budget_wall_watchdog(void *arg)
{
    unsigned long long ms = (unsigned long long)(uintptr_t)arg;
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000ULL);
    ts.tv_nsec = (long)((ms % 1000ULL) * 1000000ULL);
    nanosleep(&ts, NULL);
    fprintf(stderr, "%s budget op=wall-time kind=wall used=>%llums limit=%llums\n",
            PGY_RUNTIME_PANIC_PREFIX, ms, ms);
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_BUDGET_EXCEEDED,
                      PGY_RUNTIME_PANIC_REASON_BUDGET_EXCEEDED);
    return NULL;
}

static inline void
pgy_budget_wall_arm_impl(void)
{
    const char *v = getenv("PGY_BUDGET_WALL_MS");
    unsigned long long ms;
    pthread_t tid;

    if (v == NULL || v[0] == '\0')
        return;
    ms = strtoull(v, NULL, 10);
    if (ms == 0)
        return;
    if (pthread_create(&tid, NULL, pgy_budget_wall_watchdog,
                       (void *)(uintptr_t)ms) == 0)
        pthread_detach(tid);
}

#endif /* PGY_RUNTIME_BUDGET_H */
