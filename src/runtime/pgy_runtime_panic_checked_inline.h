/* Runtime panic helpers and checked arithmetic exports. */

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

/* Fail-closed by default: slot safety checks (generational use-after-release and
 * double-release detection) are ON in every build -- debug and release alike --
 * so a program's safety never silently depends on the build profile. This is a
 * deliberate departure from "zero overhead in release": Pergyra is fail-closed /
 * traceability-first, not zero-cost. Performance is bought back explicitly via
 * PGY_RAW_SLOTS only as a whole-program raw/unsafe build mode for measured
 * systems-tier code; it must not be mixed with checked runtime bitcode/objects.
 * (PGY_DEBUG / PGY_SAFE_SLOTS are still honoured as legacy force-on.) */
#if defined(PGY_RAW_SLOTS) && !defined(PGY_DEBUG) && !defined(PGY_SAFE_SLOTS)
#  define PGY_WITH_SLOT_CHECKS 0
#else
#  define PGY_WITH_SLOT_CHECKS 1
#endif

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
    static int count = 0;
    int i;

    if (inst == NULL)
        return NULL;
    for (i = 0; i < count; i++) {
        if (entries[i].key == inst)
            return &entries[i];
    }
    if (!create || count >= PGY_LIFECYCLE_MAP_CAP)
        return NULL;
    entries[count].key = inst;
    entries[count].state = 0;
    return &entries[count++];
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
