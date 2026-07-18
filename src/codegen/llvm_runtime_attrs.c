#ifdef PGY_LLVM_ENABLED

#include "llvm_runtime_attrs.h"

#include <stddef.h>
#include <string.h>

/*
 * Runtime panics are the cold error family: any function whose name carries
 * the "panic" marker traps and never returns.
 */
bool
llvm_fn_is_panic(const char *fn_name)
{
    return fn_name != NULL && strstr(fn_name, "panic") != NULL;
}

/*
 * Checked integer division/modulo carry fail-closed guards (divide-by-zero and
 * INT_MIN / -1 overflow). When the runtime bitcode is linked and inlined, a -O3
 * pass would constant-fold a literal INT_MIN / -1 (instcombine rewrites
 * `sdiv x, -1` to a negation) and discard the guard, so the surface program
 * silently produces a wrong value instead of panicking. Mark these functions
 * noinline + optnone so the guard always executes at runtime, matching the C
 * backend whose separately compiled runtime is never folded into the caller.
 */
bool
llvm_fn_is_checked_arith(const char *fn_name)
{
    return fn_name != NULL
        && (strstr(fn_name, "pgy_checked_div_") != NULL
            || strstr(fn_name, "pgy_checked_mod_") != NULL
            || strstr(fn_name, "pgy_checked_add_") != NULL
            || strstr(fn_name, "pgy_checked_mul_") != NULL
            /* fail-closed float->int conversion (docs/189 C1) carries the
             * same inline panic guard and takes the same treatment */
            || strstr(fn_name, "pgy_checked_f2i_") != NULL);
}

/*
 * Bounds-checked collection accessors carry a fail-closed out-of-bounds guard
 * that ends in an inline PGY_RUNTIME_PANIC (the same stderr+abort body as the
 * panic family). When the runtime bitcode provides the function and the LLVM
 * backend re-optimizes it, that inline panic body mis-lowers and crashes with an
 * access violation instead of printing and aborting -- exactly the hazard the
 * panic and checked-arith exclusions already guard against. Strip these bodies
 * so each call resolves to the separately compiled runtime object, whose guard
 * is byte-identical to the C backend's and never mis-lowers. These are the typed
 * array get/set accessors (pgy_array_get_Int, pgy_array_set_Long, ...) plus the
 * raw guarded cores they share.
 */
bool
llvm_fn_is_bounds_checked_accessor(const char *fn_name)
{
    /*
     * Only EXTERNAL (T) runtime symbols may be stripped from the inlined
     * bitcode: stripping replaces the body with an external declaration, which
     * the linker must be able to resolve against the separately compiled runtime
     * object. The typed/raw array accessors (pgy_array_get_/set_*) are external
     * and carry the inline out-of-bounds PGY_RUNTIME_PANIC that mis-lowers when
     * folded, so they must be stripped. pgy_map_grow_raw_export is `static`
     * (local) and delegates its bounds panic to the already-external
     * pgy_runtime_panic_out_of_bounds_export, so it neither needs nor tolerates
     * stripping -- stripping a static symbol yields an undefined-reference link
     * error (it broke HashMap on the LLVM backend until this was removed).
     */
    if (fn_name == NULL)
        return false;
    if (strstr(fn_name, "pgy_array_get_") != NULL
        || strstr(fn_name, "pgy_array_set_") != NULL
        /* Slice element accessors carry the same inline out-of-bounds
         * panic body as the array accessors, so they take the same
         * strip treatment (external in the exports object). slice_get
         * was a latent gap: its inlined panic body mis-lowered to an
         * access violation once the runtime bitcode was regenerated
         * (caught by slice_inline_index_oob/llvm). */
        || strstr(fn_name, "pgy_slice_get_") != NULL
        || strstr(fn_name, "pgy_slice_set_") != NULL)
        return true;
    /* Raw list/queue/map accessors carry the same inline bounds/shape
     * panic bodies (caught by list_get_oob/llvm after the bitcode
     * regeneration). Every *_raw_export in these families is external
     * except pgy_map_grow_raw_export, which is static and must keep its
     * body (see the note above). */
    if (strstr(fn_name, "_raw_export") != NULL
        && (strncmp(fn_name, "pgy_list_", 9) == 0
            || strncmp(fn_name, "pgy_queue_", 10) == 0
            || strncmp(fn_name, "pgy_map_", 8) == 0)
        && strcmp(fn_name, "pgy_map_grow_raw_export") != 0)
        return true;
    return false;
}

/*
 * Domain-lifecycle runtime tag helpers. They must NOT be folded into the caller
 * from bitcode, for two reasons: (1) pgy_runtime_lifecycle_guard_export takes a
 * fail-closed abort path whose inlined copy mis-lowers (the same hazard as the
 * panic family), and (2) both helpers share a single process-wide state side-map
 * that lives in the separately compiled runtime object -- inlined copies would
 * each carry their own static map and lose all cross-op state. Stripping their
 * bodies keeps them external so every call resolves to that one runtime object.
 * They return normally, so they are deliberately excluded from the noreturn set.
 */
bool
llvm_fn_is_lifecycle_runtime(const char *fn_name)
{
    return fn_name != NULL
        && (strcmp(fn_name, "pgy_runtime_lifecycle_guard_export") == 0
            || strcmp(fn_name, "pgy_runtime_lifecycle_set_export") == 0);
}

/*
 * Content capability gate helpers. Like the lifecycle helpers they must NOT be
 * folded from bitcode: pgy_cap_require_export takes a fail-closed abort path
 * whose inlined copy mis-lowers, and all four share one process-wide granted
 * mask that lives in the separately compiled runtime object - inlined per-call
 * copies would split it. Stripping keeps them external so every call resolves to
 * that one object.
 */
bool
llvm_fn_is_capability_runtime(const char *fn_name)
{
    return fn_name != NULL
        && (strcmp(fn_name, "pgy_cap_require_export") == 0
            || strcmp(fn_name, "pgy_cap_set_manifest_export") == 0
            || strcmp(fn_name, "pgy_cap_grant_all_export") == 0
            || strcmp(fn_name, "pgy_cap_granted_export") == 0);
}

/*
 * Resource budget gate helpers. Same reason as the capability gate: all share
 * one process-wide PgyBudgetState in the separately compiled runtime object, so
 * inlined per-call copies would split the running total -- is_imposed would read
 * the env-imposed copy while charge accumulated into a default (unlimited) copy,
 * silently disabling the bound. Stripping keeps them external so every call
 * resolves to that one shared state.
 */
bool
llvm_fn_is_budget_runtime(const char *fn_name)
{
    return fn_name != NULL
        && (strcmp(fn_name, "pgy_budget_charge_export") == 0
            || strcmp(fn_name, "pgy_budget_is_imposed_export") == 0
            || strcmp(fn_name, "pgy_budget_set_limit_export") == 0
            || strcmp(fn_name, "pgy_budget_reset_export") == 0
            || strcmp(fn_name, "pgy_budget_used_export") == 0
            || strcmp(fn_name, "pgy_budget_wall_arm_export") == 0);
}

/*
 * Stateful runtime families whose process-wide state (or fail-closed inline
 * panic body) lives in the separately compiled runtime object: the virtual
 * clock (static atomic mode/ns pair), zone-authority checks and their
 * last-error TLS record, thread-pool lifecycle/spawn/await, channels
 * (whose blocked-wait quantum feeds the pool compensation tick), and the
 * task/async cancellation surface (pgy_task_is_cancelled / pgy_task_cancel /
 * pgy_async_detach read and write the coroutine + current-task TLS,
 * g_pgy_coro / g_pgy_thread_current). Inlining their bitcode bodies would
 * duplicate that state per leg -- the exact split-brain class that hit
 * cap/budget before those got the PGY_RUNTIME_BC_BUILD guard, and that hit
 * the cancel probe on the C leg (docs/190 A2): a task-cancel family inlined
 * from bitcode reads a program-module-private zero copy of the TLS, so
 * is_cancelled() is stuck false and a cancelled join-any loser never
 * retires. zone-authority additionally carries the inline PGY_RUNTIME_PANIC
 * body that mis-lowers when folded (docs/189 C5+C7). Strip them so every
 * call resolves to the one runtime object. Prefix-matched; the exclusion
 * loop's external-linkage guard (llvm_api.c) keeps a stray static helper
 * (e.g. pgy_async_progress_one) from being stripped into a link error.
 */
bool
llvm_fn_is_stateful_runtime(const char *fn_name)
{
    if (fn_name == NULL)
        return false;
    return strncmp(fn_name, "pgy_zone_authority_", 19) == 0
        || strncmp(fn_name, "pgy_clock_", 10) == 0
        || strncmp(fn_name, "pgy_pool_", 9) == 0
        || strncmp(fn_name, "pgy_lane_", 9) == 0
        || strncmp(fn_name, "pgy_parallel_", 13) == 0
        || strncmp(fn_name, "pgy_spawn", 9) == 0
        || strncmp(fn_name, "pgy_await", 9) == 0
        || strncmp(fn_name, "pgy_channel_", 12) == 0
        || strncmp(fn_name, "pgy_select_", 11) == 0
        || strncmp(fn_name, "pgy_task_", 9) == 0
        || strncmp(fn_name, "pgy_async_", 10) == 0;
}

/*
 * Functions that provably never return to their caller. The panic family
 * qualifies, plus an exact-name table of terminal runtime entrypoints.
 * Matching is exact (not substring) so that returning lookalikes such as
 * pgy_intent_exit_export, which exits an intent scope and returns, are never
 * mismarked noreturn.
 */
bool
llvm_fn_never_returns(const char *fn_name)
{
    static const char *const exact_never_return[] = {
        "pgy_exit",
    };
    size_t i;

    if (fn_name == NULL)
        return false;
    if (llvm_fn_is_panic(fn_name))
        return true;
    for (i = 0; i < sizeof(exact_never_return) / sizeof(exact_never_return[0]); i++) {
        if (strcmp(fn_name, exact_never_return[i]) == 0)
            return true;
    }
    return false;
}

/*
 * Runtime helpers that touch no memory at all: pure scalar math over float
 * arguments. Marking them readnone lets the optimizer constant-fold, CSE, and
 * hoist calls out of loops. Random is excluded because it carries hidden
 * generator state and is therefore not pure.
 */
bool
llvm_fn_is_readnone_runtime(const char *fn_name)
{
    static const char *const readnone_runtime[] = {
        "Sqrt", "Pow", "Floor", "Ceil", "Round",
        "Sin", "Cos", "Tan", "Asin", "Acos", "Atan", "Atan2",
        "Exp", "MathLog", "Log10", "Log2",
    };
    size_t i;

    if (fn_name == NULL)
        return false;
    for (i = 0; i < sizeof(readnone_runtime) / sizeof(readnone_runtime[0]); i++) {
        if (strcmp(fn_name, readnone_runtime[i]) == 0)
            return true;
    }
    return false;
}

/*
 * Runtime helpers that only read memory reachable through their arguments and
 * return a scalar without allocating. Marking them readonly lets the optimizer
 * CSE repeated calls (StringIndexOf and pgy_string_equals dominate the
 * self-hosted lexer hot path) and hoist loop-invariant ones. Allocating string
 * builders such as Substring and StringConcat are intentionally excluded: their
 * allocation is an observable effect that must not be removed or duplicated.
 */
bool
llvm_fn_is_readonly_runtime(const char *fn_name)
{
    static const char *const readonly_runtime[] = {
        "StringContains", "StringIndexOf", "pgy_string_equals",
        "SubContains", "SubContainsWithLen",
        "SubEquals", "SubEqualsWithLen",
        "SubIndexOf", "SubIndexOfWithLen",
        "SubStartsWith", "SubStartsWithLen",
        "ToInt", "ToFloat",
    };
    size_t i;

    if (fn_name == NULL)
        return false;
    for (i = 0; i < sizeof(readonly_runtime) / sizeof(readonly_runtime[0]); i++) {
        if (strcmp(fn_name, readonly_runtime[i]) == 0)
            return true;
    }
    return false;
}

#endif
