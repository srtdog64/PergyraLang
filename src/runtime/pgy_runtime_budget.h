#ifndef PGY_RUNTIME_BUDGET_H
#define PGY_RUNTIME_BUDGET_H

#include <stdint.h>

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
    uint64_t limit[PGY_BUDGET_KIND_COUNT];
    uint64_t used[PGY_BUDGET_KIND_COUNT];
    int      initialized;
} PgyBudgetState;

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
}

/* Saturating add so a charge near UINT64_MAX cannot wrap past its ceiling. */
static inline uint64_t
pgy_budget_saturating_add(uint64_t a, uint64_t b)
{
    uint64_t sum = a + b;
    return sum < a ? PGY_BUDGET_UNLIMITED : sum;
}

static inline int
pgy_budget_kind_valid(int kind)
{
    return kind >= 0 && kind < (int)PGY_BUDGET_KIND_COUNT;
}

#endif /* PGY_RUNTIME_BUDGET_H */
