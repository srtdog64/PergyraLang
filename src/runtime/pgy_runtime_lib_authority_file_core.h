#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 1
#endif

#ifndef PGY_RUNTIME_MAX_FILE_BYTES
#define PGY_RUNTIME_MAX_FILE_BYTES (64u * 1024u * 1024u)
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
extern int clock_gettime(int clk_id, struct timespec *tp);
extern int nanosleep(const struct timespec *req, struct timespec *rem);
#endif
extern char *realpath(const char *path, char *resolved_path);
#endif
#ifdef _WIN32
#include <windows.h>
#endif
/* The spawn primitives in pgy_parallel.h charge the resource budget (SPAWN_COUNT).
 * Pull the budget vocabulary in first (for PgyBudgetKind), and declare the extern
 * budget twin -- defined further down in this same TU -- so the charge resolves
 * in the .bc/runtime object. LLVM-chain only: this is the extern-twin TU, so the
 * declaration matches the (non-static) definitions below, with no static-inline
 * conflict (the C side uses the inline twin via platform_io_core.h instead). */
#include "runtime/pgy_runtime_budget.h"
extern int pgy_budget_is_imposed_export(void);
extern void pgy_budget_charge_export(int kind, uint64_t amount, const char *op);
#include "runtime/pgy_parallel.h"
#include "runtime/pgy_runtime_authority_contract.h"
#include "runtime/pgy_runtime_panic_contract.h"
#include "runtime/pgy_runtime_capability.h"
#include "runtime/pgy_runtime_budget.h"

static char *pgy_runtime_lib_strdup(const char *src);
static pthread_mutex_t pgy_runtime_lib_rng_mutex = PTHREAD_MUTEX_INITIALIZER;

_Thread_local bool pgy_zone_authority_last_ok = true;
_Thread_local char pgy_zone_authority_last_zone[128] = "";
_Thread_local char pgy_zone_authority_last_participant[128] = "";
_Thread_local char pgy_zone_authority_last_code[64] =
    PGY_ZONE_AUTHORITY_CODE_OK;
_Thread_local char pgy_zone_authority_last_reason[192] = "";

static void
pgy_runtime_record_zone_authority_last(bool ok,
                                       const char *zone_name,
                                       const char *participant_name,
                                       const char *code,
                                       const char *reason)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    pgy_zone_authority_last_ok = ok;
    snprintf(pgy_zone_authority_last_zone, sizeof(pgy_zone_authority_last_zone),
             "%s", resolved_zone);
    snprintf(pgy_zone_authority_last_participant,
             sizeof(pgy_zone_authority_last_participant),
             "%s", resolved_participant);
    snprintf(pgy_zone_authority_last_code, sizeof(pgy_zone_authority_last_code),
             "%s", code != NULL ? code
                                 : (ok ? PGY_ZONE_AUTHORITY_CODE_OK
                                       : PGY_ZONE_AUTHORITY_CODE_UNKNOWN));
    snprintf(pgy_zone_authority_last_reason, sizeof(pgy_zone_authority_last_reason),
             "%s", reason != NULL ? reason : "");
}

static bool
pgy_runtime_zone_authority_validate_core(void *zone_ptr, void *participant_ptr,
                                         const char *zone_name,
                                         const char *participant_name,
                                         bool emit_stderr)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (zone_ptr == NULL) {
        pgy_runtime_record_zone_authority_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE,
            PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_MISSING_ZONE,
                resolved_zone, resolved_participant);
        }
        return false;
    }
    if (participant_ptr == NULL) {
        pgy_runtime_record_zone_authority_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT,
            PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_MISSING_PARTICIPANT,
                resolved_zone, resolved_participant);
        }
        return false;
    }

    pgy_runtime_record_zone_authority_last(true, resolved_zone, resolved_participant,
        PGY_ZONE_AUTHORITY_CODE_OK, "");
    return true;
}

static bool
pgy_runtime_zone_authority_validate_token_core(void *zone_ptr,
                                               void *participant_ptr,
                                               int64_t expected_token,
                                               int64_t provided_token,
                                               const char *zone_name,
                                               const char *participant_name,
                                               bool emit_stderr)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (!pgy_runtime_zone_authority_validate_core(zone_ptr, participant_ptr,
            resolved_zone, resolved_participant, emit_stderr)) {
        return false;
    }

    if (expected_token <= 0 || provided_token <= 0
        || expected_token != provided_token) {
        pgy_runtime_record_zone_authority_last(false, resolved_zone,
            resolved_participant, PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH,
            PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_TOKEN_MISMATCH,
                resolved_zone, resolved_participant);
        }
        return false;
    }

    pgy_runtime_record_zone_authority_last(true, resolved_zone,
        resolved_participant, PGY_ZONE_AUTHORITY_CODE_OK, "");
    return true;
}

static void
pgy_runtime_warn_invalid_channel(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][channel] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid channel operation");
}

static size_t
pgy_runtime_channel_capacity_or_default(const char *op, size_t cap)
{
    if (cap == 0) {
        pgy_runtime_warn_invalid_channel(op, "zero capacity requested; using capacity=1");
        return 1;
    }
    return cap;
}

static void
pgy_runtime_warn_invalid_intent_index(const char *op, int32_t index, int32_t count)
{
    fprintf(stderr, "[pgy][intent] %s: invalid index %d (count=%d)\n",
            op != NULL ? op : "<op>", (int)index, (int)count);
}

static void
pgy_runtime_warn_intent_enter_failure(const char *name, const char *reason,
                                      int32_t priority, bool is_concurrent)
{
    fprintf(stderr, "[pgy][intent] enter %s failed: %s (priority=%d concurrent=%s)\n",
            name != NULL ? name : "<intent>",
            reason != NULL ? reason : "unknown reason",
            (int)priority,
            is_concurrent ? "true" : "false");
}

static void
pgy_runtime_warn_invalid_collection(const char *op, const char *reason)
{
    fprintf(stderr, "[pgy][collection] %s: %s\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "invalid collection operation");
}

void
pgy_zone_authority_check_export(void *zone_ptr, void *participant_ptr,
                                const char *zone_name,
                                const char *participant_name)
{
    if (!pgy_runtime_zone_authority_validate_core(zone_ptr, participant_ptr,
            zone_name, participant_name, true)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH,
                          PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH);
    }
}

void
pgy_zone_authority_check_token_export(void *zone_ptr, void *participant_ptr,
                                      int64_t expected_token,
                                      int64_t provided_token,
                                      const char *zone_name,
                                      const char *participant_name)
{
    if (!pgy_runtime_zone_authority_validate_token_core(zone_ptr, participant_ptr,
            expected_token, provided_token, zone_name, participant_name, true)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH,
                          PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH);
    }
}

void
pgy_runtime_panic_internal_invariant_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      reason != NULL ? reason : "runtime invariant failed");
}

void
pgy_runtime_panic_out_of_bounds_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      reason != NULL ? reason
                          : PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS);
}

int32_t
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

/* Domain-lifecycle runtime state tag -- external (non-inline) twins of the
 * static-inline definitions in pgy_runtime_panic_checked_inline.h. The C backend
 * inlines those; the LLVM backend resolves these external symbols out of the
 * separately compiled runtime object/bitcode. Keep both in lockstep. See
 * docs/semantics/12_domain_lifecycle_evidence.md section 2.3. */
#ifndef PGY_LIFECYCLE_MAP_CAP
#define PGY_LIFECYCLE_MAP_CAP 256
#endif

typedef struct {
    const void *key;
    int32_t     state;
} PgyLifecycleEntryExt;

static PgyLifecycleEntryExt *
pgy_runtime_lifecycle_slot_ext(const void *inst, int create)
{
    static PgyLifecycleEntryExt entries[PGY_LIFECYCLE_MAP_CAP];
    size_t cap = (size_t)PGY_LIFECYCLE_MAP_CAP;
    uintptr_t h;
    size_t start;
    size_t probe;

    if (inst == NULL)
        return NULL;
    /* O(1) open-addressing hash over the instance pointer, replacing the prior
     * O(count) linear scan (which degraded ~17x at ~200 live subjects; docs/136).
     * key == NULL marks an empty slot (inst is never NULL here). Width-safe mix
     * (no shift >= pointer width). Twin of the inline pgy_runtime_lifecycle_slot. */
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

void
pgy_runtime_lifecycle_set_export(const void *inst, int32_t state)
{
    PgyLifecycleEntryExt *e = pgy_runtime_lifecycle_slot_ext(inst, 1);
    if (e != NULL)
        e->state = state;
}

void
pgy_runtime_lifecycle_guard_export(const void *inst, int32_t valid_mask,
                                   int32_t to_state, const char *op,
                                   const char *subject)
{
    PgyLifecycleEntryExt *e = pgy_runtime_lifecycle_slot_ext(inst, 1);
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

/* Content capability gate -- external (non-inline) twins of the static-inline
 * definitions in pgy_runtime_panic_checked_inline.h, for the LLVM-linked runtime
 * object. One process-wide granted set; gated ops panic fail-closed outside it.
 * See pgy_runtime_capability.h and docs/semantics/15.
 *
 * pgy_runtime_lib.c is compiled twice -- once to the .bc (clang, runtime module
 * llvm-linked + inlined into the program) and once to the native cache object
 * (linked). If both defined this state it would split across instances (one
 * inlined into the program from the .bc, one in the cache object), so an LLVM
 * program's gate would read one copy and mutate another. To keep ONE instance,
 * only the native object defines it; the .bc build (PGY_RUNTIME_BC_BUILD)
 * declares it extern so every reference resolves to that single definition. */
#ifdef PGY_RUNTIME_BC_BUILD
extern uint32_t g_pgy_cap_granted;
#else
uint32_t g_pgy_cap_granted = PGY_CAP_ALL;
#endif

/* Apply the host PGY_CAP_GRANT restriction exactly once, before the first gated
 * op observes the granted set (mirror of the inline twin's env latch in
 * pgy_runtime_panic_checked_inline.h). pgy_cap_require_export -- the critical
 * path every gated ambient op goes through -- is bitcode-stripped to this one
 * runtime object, so this latch runs on the single shared g_pgy_cap_granted. */
static int g_pgy_cap_env_applied = 0;

static void
pgy_cap_apply_env_once(void)
{
    unsigned env_mask;

    if (g_pgy_cap_env_applied)
        return;
    g_pgy_cap_env_applied = 1;
    if (pgy_cap_env_grant(&env_mask))
        g_pgy_cap_granted = env_mask;
}

void
pgy_cap_set_manifest_export(uint32_t mask)
{
    g_pgy_cap_granted = mask;
}

void
pgy_cap_grant_all_export(void)
{
    g_pgy_cap_granted = PGY_CAP_ALL;
}

uint32_t
pgy_cap_granted_export(void)
{
    pgy_cap_apply_env_once();
    return g_pgy_cap_granted;
}

void
pgy_cap_require_export(uint32_t cap, const char *op)
{
    pgy_cap_apply_env_once();
    if ((g_pgy_cap_granted & cap) != cap) {
        fprintf(stderr, "%s capability op=%s required=0x%x granted=0x%x\n",
                PGY_RUNTIME_PANIC_PREFIX, op != NULL ? op : "<op>",
                (unsigned)cap, (unsigned)g_pgy_cap_granted);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_CAPABILITY_DENIED,
                          PGY_RUNTIME_PANIC_REASON_CAPABILITY_DENIED);
    }
}

/* Resource budget gate -- external twins of the static-inline definitions in
 * pgy_runtime_panic_checked_inline.h. One process-wide per-kind budget; metered
 * ops panic fail-closed on overrun. See pgy_runtime_budget.h and docs/15. */
/* Single instance across the .bc and the native cache object (see the
 * g_pgy_cap_granted note above for why) -- the .bc build only declares it. */
#ifdef PGY_RUNTIME_BC_BUILD
extern PgyBudgetState g_pgy_budget;
#else
PgyBudgetState g_pgy_budget;
#endif

/* noinline so the optimizer cannot inline these into their callers (e.g. the
 * collection alloc paths) inside the .bc/runtime object. Inlined copies would
 * each reference this TU's g_pgy_budget, and since the LLVM bitcode-strip only
 * removes the standalone function bodies, the inlined copies (and their
 * g_pgy_budget references) would survive -- splitting the budget across several
 * instances so is_imposed reads one and charge accumulates into another. Kept
 * non-inline, every call resolves to this one shared state. */
#if defined(__GNUC__) || defined(__clang__)
#  define PGY_BUDGET_NOINLINE __attribute__((noinline))
#else
#  define PGY_BUDGET_NOINLINE
#endif

PGY_BUDGET_NOINLINE void
pgy_budget_reset_export(void)
{
    pgy_budget_state_init(&g_pgy_budget);
}

PGY_BUDGET_NOINLINE void
pgy_budget_set_limit_export(int kind, uint64_t limit)
{
    if (!g_pgy_budget.initialized)
        pgy_budget_state_init(&g_pgy_budget);
    if (pgy_budget_kind_valid(kind)) {
        g_pgy_budget.limit[kind] = limit;
        if (limit != PGY_BUDGET_UNLIMITED)
            g_pgy_budget.imposed = 1;
    }
}

PGY_BUDGET_NOINLINE uint64_t
pgy_budget_used_export(int kind)
{
    if (!g_pgy_budget.initialized)
        pgy_budget_state_init(&g_pgy_budget);
    return pgy_budget_kind_valid(kind)
        ? atomic_load_explicit(&g_pgy_budget.used[kind], memory_order_relaxed)
        : 0;
}

PGY_BUDGET_NOINLINE int
pgy_budget_is_imposed_export(void)
{
    if (!g_pgy_budget.initialized)
        pgy_budget_state_init(&g_pgy_budget);
    return g_pgy_budget.imposed;
}

PGY_BUDGET_NOINLINE void
pgy_budget_charge_export(int kind, uint64_t amount, const char *op)
{
    uint64_t used;

    if (!g_pgy_budget.initialized)
        pgy_budget_state_init(&g_pgy_budget);
    if (!pgy_budget_kind_valid(kind))
        return;
    used = pgy_budget_charge_into(&g_pgy_budget, kind, amount);
    if (used > g_pgy_budget.limit[kind]) {
        fprintf(stderr, "%s budget op=%s kind=%d used=%llu limit=%llu\n",
                PGY_RUNTIME_PANIC_PREFIX, op != NULL ? op : "<op>", kind,
                (unsigned long long)used,
                (unsigned long long)g_pgy_budget.limit[kind]);
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_BUDGET_EXCEEDED,
                          PGY_RUNTIME_PANIC_REASON_BUDGET_EXCEEDED);
    }
}

/* Arm the wall-clock deadline watchdog (see pgy_runtime_budget.h). Emitted once
 * at main entry by the LLVM backend; a no-op unless PGY_BUDGET_WALL_MS is set. */
PGY_BUDGET_NOINLINE void
pgy_budget_wall_arm_export(void)
{
    pgy_budget_wall_arm_impl();
}

int64_t
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

int32_t
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

int64_t
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

int32_t
pgy_checked_add_i32_export(int32_t lhs, int32_t rhs)
{
    if (((rhs > 0) && (lhs > INT32_MAX - rhs)) ||
        ((rhs < 0) && (lhs < INT32_MIN - rhs)))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_ADDITION_OVERFLOW);
    return lhs + rhs;
}

int64_t
pgy_checked_add_i64_export(int64_t lhs, int64_t rhs)
{
    if (((rhs > 0) && (lhs > INT64_MAX - rhs)) ||
        ((rhs < 0) && (lhs < INT64_MIN - rhs)))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_ADDITION_OVERFLOW);
    return lhs + rhs;
}

int32_t
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

int64_t
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

bool
pgy_zone_authority_validate_flags_export(bool has_zone, bool has_participant,
                                         char *zone_name,
                                         char *participant_name)
{
    void *zone_ptr = has_zone ? (void *)&pgy_zone_authority_last_ok : NULL;
    void *participant_ptr =
        has_participant ? (void *)pgy_zone_authority_last_zone : NULL;
    return pgy_runtime_zone_authority_validate_core(zone_ptr, participant_ptr,
        zone_name, participant_name, false);
}

bool
pgy_zone_authority_validate_token_flags_export(bool has_zone,
                                               bool has_participant,
                                               int64_t expected_token,
                                               int64_t provided_token,
                                               char *zone_name,
                                               char *participant_name)
{
    void *zone_ptr = has_zone ? (void *)&pgy_zone_authority_last_ok : NULL;
    void *participant_ptr =
        has_participant ? (void *)pgy_zone_authority_last_zone : NULL;
    return pgy_runtime_zone_authority_validate_token_core(zone_ptr,
        participant_ptr, expected_token, provided_token, zone_name,
        participant_name, false);
}

bool
pgy_zone_authority_last_ok_rt_export(void)
{
    return pgy_zone_authority_last_ok;
}

char *
pgy_zone_authority_last_zone_rt_export(void)
{
    return pgy_zone_authority_last_zone;
}

char *
pgy_zone_authority_last_participant_rt_export(void)
{
    return pgy_zone_authority_last_participant;
}

char *
pgy_zone_authority_last_code_rt_export(void)
{
    return pgy_zone_authority_last_code;
}

char *
pgy_zone_authority_last_reason_rt_export(void)
{
    return pgy_zone_authority_last_reason;
}

#include "pgy_runtime_lib_file_path_core.h"
