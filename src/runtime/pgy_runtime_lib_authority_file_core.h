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
#include <stdatomic.h>
#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
extern int lstat(const char *path, struct stat *buf);
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
extern int clock_gettime(int clk_id, struct timespec *tp);
extern int nanosleep(const struct timespec *req, struct timespec *rem);
#endif
extern char *realpath(const char *path, char *resolved_path);
#endif
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
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
#include "runtime/pgy_runtime_security_log.h"

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
            pgy_runtime_log_authority_failure(stderr,
                PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE,
                PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE,
                resolved_zone, resolved_participant);
        }
        return false;
    }
    if (participant_ptr == NULL) {
        pgy_runtime_record_zone_authority_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT,
            PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT);
        if (emit_stderr) {
            pgy_runtime_log_authority_failure(stderr,
                PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT,
                PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT,
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
            pgy_runtime_log_authority_failure(stderr,
                PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH,
                PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH,
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

void
pgy_runtime_panic_authority_mismatch_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH,
                      reason != NULL ? reason
                          : PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH);
}

#include "pgy_runtime_lib_clock_core.h"

#include "pgy_runtime_lib_checked_arith_core.h"

#include "pgy_runtime_lib_lifecycle_core.h"

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
