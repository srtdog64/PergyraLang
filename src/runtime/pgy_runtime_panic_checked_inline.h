/* Runtime panic helpers and checked arithmetic exports. */

#ifndef PGY_RUNTIME_PANIC_CHECKED_INLINE_H
#define PGY_RUNTIME_PANIC_CHECKED_INLINE_H

#include "pgy_runtime_linkage.h"

#include "pgy_runtime_capability.h"
#include "pgy_runtime_budget.h"

static inline struct timespec
pgy_timespec_after_ns(uint64_t timeout_ns)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(timeout_ns / 1000000000ull);
    ts.tv_nsec += (long)(timeout_ns % 1000000000ull);
    if (ts.tv_nsec >= 1000000000l) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000l;
    }
    return ts;
}

/* Slot safety checks (generational use-after-release and double-release
 * detection) are part of the canonical Slot<T> ABI in every build -- debug and
 * release alike -- so a program's memory shape never silently depends on the
 * build profile. PGY_WITH_SLOT_CHECKS remains defined for downstream feature
 * tests, but it is no longer an ABI selector. */
#define PGY_WITH_SLOT_CHECKS 1

/* =================================================================
 * Panic - Unrecoverable Error
 * ================================================================= */

#define PGY_PANIC(msg) \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, (msg))

#define PGY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            PGY_PANIC(msg); \
        } \
    } while (0)

static inline void
pgy_runtime_panic_internal_invariant_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      reason != NULL ? reason : "runtime invariant failed");
}

static inline void
pgy_runtime_panic_out_of_bounds_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      reason != NULL ? reason
                          : PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS);
}

static inline void
pgy_runtime_panic_authority_mismatch_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH,
                      reason != NULL ? reason
                          : PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH);
}

/* =================================================================
 * Domain-lifecycle runtime state tag (doc/12 section 2.3).
 *
 * The static lifecycle pass proves most operations safe at compile time and
 * leaves NO runtime cost for them. Where control flow makes a value's state
 * ambiguous (e.g. authorized on only one branch), it cannot prove the
 * precondition statically, so it emits a fail-closed runtime guard: a tiny
 * side-map records each tracked instance's current state index (keyed by the
 * instance's storage address), and the guard panics if the live state is not
 * in the operation's permitted-from set. Single source of truth for the
 * permitted set is the transition table, lowered to `valid_mask`
 * (bit s set == op permitted from state s).
 *
 * v1 limits (documented, not silent): fixed capacity, single-threaded tracking,
 * keyed by local storage address (no cross-alias identity). These are the
 * fail-closed backstop for the ambiguous minority, not the common path.
 * ================================================================= */

#ifndef PGY_LIFECYCLE_MAP_CAP
#define PGY_LIFECYCLE_MAP_CAP 256
#endif

typedef struct {
    const void *key;
    int32_t     state;
} PgyLifecycleEntry;

/* Locate (or, when create != 0, intern) the state slot for `inst`. Returns NULL
 * only for a NULL instance or when capacity is exhausted. */
static inline PgyLifecycleEntry *
pgy_runtime_lifecycle_slot(const void *inst, int create)
{
    static PgyLifecycleEntry entries[PGY_LIFECYCLE_MAP_CAP];
    size_t cap = (size_t)PGY_LIFECYCLE_MAP_CAP;
    uintptr_t h;
    size_t start;
    size_t probe;

    if (inst == NULL)
        return NULL;
    /* O(1) open-addressing hash over the instance pointer, replacing the prior
     * O(count) linear scan (which degraded ~17x at ~200 live subjects; docs/136).
     * key == NULL marks an empty slot (inst is never NULL here). Width-safe mix
     * (no shift >= pointer width). Twin of pgy_runtime_lifecycle_slot_ext. */
    h = (uintptr_t)inst;
    h ^= h >> 7;
    h *= (uintptr_t)2654435761u;
    h ^= h >> 11;
    start = (size_t)h % cap;
    for (probe = 0; probe < cap; probe++) {
        size_t idx = (start + probe) % cap;
        if (entries[idx].key == inst)
            return &entries[idx];
        if (entries[idx].key == NULL) {
            if (!create)
                return NULL;
            entries[idx].key = inst;
            entries[idx].state = 0;
            return &entries[idx];
        }
    }
    return NULL;
}

/* Record `state` as the live state of `inst` (used at construction and after a
 * statically-proven transition so the runtime tag stays current for a later
 * ambiguous guard). */
static inline void
pgy_runtime_lifecycle_set_export(const void *inst, int32_t state)
{
    PgyLifecycleEntry *e = pgy_runtime_lifecycle_slot(inst, 1);
    if (e != NULL)
        e->state = state;
}

/* Fail-closed guard for an operation whose precondition could not be proven
 * statically. Panics with a traceable record when the live state is outside the
 * permitted-from set; otherwise advances the recorded state to `to_state`. */
static inline void
pgy_runtime_lifecycle_guard_export(const void *inst, int32_t valid_mask,
                                   int32_t to_state, const char *op,
                                   const char *subject)
{
    PgyLifecycleEntry *e = pgy_runtime_lifecycle_slot(inst, 1);
    int32_t state = (e != NULL) ? e->state : 0;

    if (state < 0 || state >= 32 || ((valid_mask >> state) & 1) == 0) {
        fprintf(stderr, "%s lifecycle op=%s subject=%s state=%d permitted_mask=0x%x\n",
                PGY_RUNTIME_PANIC_PREFIX,
                op != NULL ? op : "<op>",
                subject != NULL ? subject : "<subject>",
                (int)state, (unsigned)valid_mask);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_LIFECYCLE_STATE,
                          PGY_RUNTIME_PANIC_REASON_INVALID_LIFECYCLE_STATE);
    }
    if (e != NULL && to_state >= 0)
        e->state = to_state;
}

/* =================================================================
 * Content capability gate (the runtime-enforced effect boundary).
 *
 * Process-wide granted set; gated ambient-authority operations panic
 * fail-closed (class capability-denied) when used outside the grant. Default is
 * PGY_CAP_ALL so trusted programs are unaffected; a loader imposes a restricted
 * manifest before running untrusted content. See pgy_runtime_capability.h.
 *
 * The granted set must be a single process-wide value. The C backend used to
 * hold it in a function-local static on the premise of "one single-TU C
 * output" -- but the extern runtime object made the C leg a two-TU world
 * (emitted program + cext object), which split this state across two copies
 * (docs/190 A1). So pgy_cap_granted_slot is PGY_RT_DECL: the cext object owns
 * the one definition (and thus the one `granted`), the emitted program links
 * to it, and every accessor below -- inlined per TU or not -- routes through
 * this single slot, so a loader's set_manifest and an ambient gate's require
 * see the same mask. In inline mode (PGY_RUNTIME_INLINE / the .bc default
 * build) it collapses to the old static-inline form. The LLVM build resolves
 * to the separate external twin in the runtime object (excluded from inlined
 * bitcode, llvm_fn_is_capability_runtime).
 * ================================================================= */

PGY_RT_DECL uint32_t *
pgy_cap_granted_slot(void)
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    static uint32_t granted = PGY_CAP_ALL;
    static int env_applied = 0;

    if (!env_applied) {
        unsigned env_mask;

        env_applied = 1;
        if (pgy_cap_env_grant(&env_mask))
            granted = env_mask;   /* host PGY_CAP_GRANT restricts the grant */
    }
    return &granted;
}
#else
;
#endif

static inline void
pgy_cap_set_manifest_export(uint32_t mask)
{
    *pgy_cap_granted_slot() = mask;
}

static inline void
pgy_cap_grant_all_export(void)
{
    *pgy_cap_granted_slot() = PGY_CAP_ALL;
}

static inline uint32_t
pgy_cap_granted_export(void)
{
    return *pgy_cap_granted_slot();
}

/* Fail-closed gate: every bit in `cap` must be granted, else panic with a
 * traceable record naming the operation and the required/granted masks. */
static inline void
pgy_cap_require_export(uint32_t cap, const char *op)
{
    uint32_t granted = *pgy_cap_granted_slot();

    if ((granted & cap) != cap) {
        fprintf(stderr, "%s capability op=%s required=0x%x granted=0x%x\n",
                PGY_RUNTIME_PANIC_PREFIX, op != NULL ? op : "<op>",
                (unsigned)cap, (unsigned)granted);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_CAPABILITY_DENIED,
                          PGY_RUNTIME_PANIC_REASON_CAPABILITY_DENIED);
    }
}

/* ---- resource budget (quantitative sandbox gate) ---- */

/* One process-wide budget, single-homed the same way as the capability slot
 * above: PGY_RT_DECL so the cext object owns the one `st` and every accounting
 * path (allocator, spawn, channel) charges it, instead of alloc/spawn charging
 * the object copy while a channel or a loader's set_limit touches a separate
 * emitted-TU copy (docs/190 A3). */
PGY_RT_DECL PgyBudgetState *
pgy_budget_state_slot(void)
#ifndef PGY_RUNTIME_DECLS_ONLY
{
    static PgyBudgetState st;
    if (!st.initialized)
        pgy_budget_state_init(&st);
    return &st;
}
#else
;
#endif

static inline void
pgy_budget_reset_export(void)
{
    pgy_budget_state_init(pgy_budget_state_slot());
}

static inline void
pgy_budget_set_limit_export(int kind, uint64_t limit)
{
    if (pgy_budget_kind_valid(kind)) {
        PgyBudgetState *st = pgy_budget_state_slot();
        st->limit[kind] = limit;
        if (limit != PGY_BUDGET_UNLIMITED)
            st->imposed = 1;
    }
}

static inline uint64_t
pgy_budget_used_export(int kind)
{
    return pgy_budget_kind_valid(kind)
        ? atomic_load_explicit(&pgy_budget_state_slot()->used[kind],
                               memory_order_relaxed)
        : 0;
}

static inline int
pgy_budget_is_imposed_export(void)
{
    return pgy_budget_state_slot()->imposed;
}

/* Fail-closed gate: atomically add `amount` to a kind's running total; the
 * charge that pushes the total past its ceiling panics with a traceable
 * record. */
static inline void
pgy_budget_charge_export(int kind, uint64_t amount, const char *op)
{
    PgyBudgetState *st;
    uint64_t used;

    if (!pgy_budget_kind_valid(kind))
        return;
    st = pgy_budget_state_slot();
    used = pgy_budget_charge_into(st, kind, amount);
    if (used > st->limit[kind]) {
        fprintf(stderr,
                "%s budget op=%s kind=%d used=%llu limit=%llu\n",
                PGY_RUNTIME_PANIC_PREFIX, op != NULL ? op : "<op>", kind,
                (unsigned long long)used,
                (unsigned long long)st->limit[kind]);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_BUDGET_EXCEEDED,
                          PGY_RUNTIME_PANIC_REASON_BUDGET_EXCEEDED);
    }
}

/* Arm the wall-clock deadline watchdog (see pgy_runtime_budget.h). Emitted once
 * at main entry; a no-op unless the host set PGY_BUDGET_WALL_MS. */
static inline void
pgy_budget_wall_arm_export(void)
{
    pgy_budget_wall_arm_impl();
}

static inline int32_t
pgy_checked_div_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    if (lhs == INT32_MIN && rhs == -1)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_DIVISION_OVERFLOW);
    return lhs / rhs;
}

static inline int64_t
pgy_checked_div_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    if (lhs == INT64_MIN && rhs == -1)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_DIVISION_OVERFLOW);
    return lhs / rhs;
}

static inline int32_t
pgy_checked_mod_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    /* INT_MIN % -1 is UB in C though the true remainder is 0; return it. */
    if (lhs == INT32_MIN && rhs == -1)
        return 0;
    return lhs % rhs;
}

static inline int64_t
pgy_checked_mod_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    /* INT_MIN % -1 is UB in C though the true remainder is 0; return it. */
    if (lhs == INT64_MIN && rhs == -1)
        return 0;
    return lhs % rhs;
}

/* Fail-closed integer addition/multiplication. The size-computation overflow
 * class (e.g. `4 + packet_length`, `num_attrs * sizeof(...)`) that wraps to a
 * tiny allocation in C code panics here instead of silently producing a wrong
 * value. Portable, UB-free checks (SEI CERT INT32-C), no compiler builtins, so
 * the emitted/runtime code compiles on any C compiler. */
static inline int64_t
pgy_checked_add_i64_export(int64_t lhs, int64_t rhs)
{
    if (((rhs > 0) && (lhs > INT64_MAX - rhs)) ||
        ((rhs < 0) && (lhs < INT64_MIN - rhs)))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_ADDITION_OVERFLOW);
    return lhs + rhs;
}

static inline int64_t
pgy_checked_mul_i64_export(int64_t lhs, int64_t rhs)
{
    int overflow = 0;
    if (lhs > 0) {
        if (rhs > 0) { overflow = (lhs > INT64_MAX / rhs); }
        else         { overflow = (rhs < INT64_MIN / lhs); }
    } else {
        if (rhs > 0) { overflow = (lhs < INT64_MIN / rhs); }
        else         { overflow = (lhs != 0 && rhs < INT64_MAX / lhs); }
    }
    if (overflow)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_MULTIPLICATION_OVERFLOW);
    return lhs * rhs;
}

/* i32 twins for the surface `Int` type (Pergyra Int lowers to int32_t). Same
 * portable, UB-free checks. */
static inline int32_t
pgy_checked_add_i32_export(int32_t lhs, int32_t rhs)
{
    if (((rhs > 0) && (lhs > INT32_MAX - rhs)) ||
        ((rhs < 0) && (lhs < INT32_MIN - rhs)))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_ADDITION_OVERFLOW);
    return lhs + rhs;
}

static inline int32_t
pgy_checked_mul_i32_export(int32_t lhs, int32_t rhs)
{
    int overflow = 0;
    if (lhs > 0) {
        if (rhs > 0) { overflow = (lhs > INT32_MAX / rhs); }
        else         { overflow = (rhs < INT32_MIN / lhs); }
    } else {
        if (rhs > 0) { overflow = (lhs < INT32_MIN / rhs); }
        else         { overflow = (lhs != 0 && rhs < INT32_MAX / lhs); }
    }
    if (overflow)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_MULTIPLICATION_OVERFLOW);
    return lhs * rhs;
}

/* Fail-closed float->int conversion twins (docs/189 C1); see
 * pgy_runtime_lib_checked_arith_core.h for the bound rationale. Keep both
 * homes in lockstep. */
static inline int32_t
pgy_checked_f2i_i32_export(double v)
{
    if (!(v > -2147483649.0 && v < 2147483648.0))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_FLOAT_TO_INT_OUT_OF_RANGE);
    return (int32_t)v;
}

static inline int64_t
pgy_checked_f2i_i64_export(double v)
{
    if (!(v >= -9223372036854775808.0 && v < 9223372036854775808.0))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_FLOAT_TO_INT_OUT_OF_RANGE);
    return (int64_t)v;
}

#endif /* PGY_RUNTIME_PANIC_CHECKED_INLINE_H */
