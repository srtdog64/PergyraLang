#!/usr/bin/env bash
# PgyRegion chained-block allocator witness (WO-REG-1 REG-1a, docs/197).
#
# PgyRegion is the runtime backing for the declared `region` lifetime scope.
# It is header-only static inline, so all three runtime materializations get an
# identical copy. This smoke proves, against the REAL runtime headers:
#
#   1. both linked-runtime objects (C-leg cext + LLVM-leg lib) compile with
#      PgyRegion present -- the header is well-formed in every materialization;
#   2. an emitted-shaped harness, built in BOTH the inline (self-contained) and
#      the extern (PGY_RUNTIME_DECLS_ONLY, linked against the cext object)
#      shapes, produces identical region behaviour:
#        a. chained growth -- allocating past one block keeps every prior
#           pointer valid and its bytes intact (stable pointers, no realloc);
#        b. alignment -- allocations at 1/8/16/64 are returned aligned;
#        c. region string concat -- a chained concat yields the right string
#           and the region owns every intermediate (freed at destroy, no leak);
#        d. reset -- reuse drops contents but keeps blocks;
#   3. the allocation budget fails closed: a region under a low
#      PGY_BUDGET_ALLOC_BYTES ceiling panics on the block acquisition that
#      exceeds it (the quantitative sandbox axis, R6), and an unbounded region
#      is unaffected.
#
# Usage: bash tests/region_arena_smoke.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LABEL="region-arena"

# Git Bash launched non-login (as by `mingw32-make`) does not reliably carry
# the native MinGW/LLVM tool directories into its PATH.  Use the shared tool
# path owner before selecting CC so the runtime materialization gate exercises
# the real Windows compiler instead of a missing or incompatible shell entry.
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

CC="${CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[$LABEL] SKIP: no C compiler ($CC)"
    exit 0
fi
NM="${NM:-nm}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

CFLAGS=(-std=c11 -O1 -fwrapv -fno-strict-aliasing -pthread
        -I"$ROOT_DIR/src/runtime" -I"$ROOT_DIR/src" -DPGY_LLVM_ENABLED)

# ---- both linked-runtime materializations must carry the header -----------
if ! "$CC" "${CFLAGS[@]}" -c "$ROOT_DIR/src/runtime/pgy_runtime_cext_lib.c" \
        -o "$WORK/rt.o" 2>"$WORK/rt.err"; then
    echo "[$LABEL] cext runtime object build failed" >&2
    tail -20 "$WORK/rt.err" >&2
    exit 1
fi
if ! "$CC" "${CFLAGS[@]}" -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
        -o "$WORK/rt_llvm.o" 2>"$WORK/rt_llvm.err"; then
    echo "[$LABEL] llvm-leg runtime object build failed" >&2
    tail -20 "$WORK/rt_llvm.err" >&2
    exit 1
fi

# ---- behavioural harness (shared body, built in both materializations) ----
cat > "$WORK/region_harness.c" <<'EOF'
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 0
#endif
#include "pgy_runtime.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", (msg)); failures++; } \
} while (0)

/* (a) chained growth: allocate many small blocks' worth from a tiny region,
 * stamping each so a later reallocation-in-place would corrupt an earlier
 * stamp. A bump arena must keep every pointer valid and its bytes intact. */
static void test_chained_growth(void)
{
    PgyRegion r = pgy_region_create(64);  /* tiny blocks force chaining */
    enum { N = 500 };
    unsigned char *ptrs[N];
    int i, j;

    for (i = 0; i < N; i++) {
        ptrs[i] = (unsigned char *)pgy_region_alloc(&r, 24, 1);
        CHECK(ptrs[i] != NULL, "growth alloc returned null");
        memset(ptrs[i], (unsigned char)(i & 0xFF), 24);
    }
    for (i = 0; i < N; i++)
        for (j = 0; j < 24; j++)
            CHECK(ptrs[i][j] == (unsigned char)(i & 0xFF),
                  "growth: earlier allocation was corrupted");
    /* distinctness: no two allocations overlap */
    for (i = 1; i < N; i++)
        CHECK(ptrs[i] != ptrs[i - 1], "growth: duplicate pointer");
    pgy_region_destroy(&r);
}

/* (b) alignment: each requested power-of-two alignment is honoured. */
static void test_alignment(void)
{
    PgyRegion r = pgy_region_create(0);
    size_t aligns[] = { 1, 8, 16, 64 };
    size_t k;

    for (k = 0; k < sizeof(aligns) / sizeof(aligns[0]); k++) {
        /* interleave a 1-byte alloc so the next base is deliberately odd */
        (void)pgy_region_alloc(&r, 1, 1);
        void *p = pgy_region_alloc(&r, 32, aligns[k]);
        CHECK(p != NULL, "aligned alloc returned null");
        CHECK(((uintptr_t)p & (aligns[k] - 1)) == 0, "alignment not honoured");
    }
    pgy_region_destroy(&r);
}

/* (c) region string concat: chained concat is correct and region-owned. The
 * heap StringConcat leaks the inner result here; the region frees all of it in
 * one destroy. */
static void test_string_concat(void)
{
    PgyRegion r = pgy_region_create(0);
    const char *a = "domain=";
    char *step1 = pgy_region_string_concat(&r, a, "vessel");
    char *step2 = pgy_region_string_concat(&r, step1, ":filled");
    char *dup   = pgy_region_strdup(&r, step2);

    CHECK(strcmp(step1, "domain=vessel") == 0, "concat step1 wrong");
    CHECK(strcmp(step2, "domain=vessel:filled") == 0, "concat step2 wrong");
    CHECK(dup != NULL && strcmp(dup, step2) == 0, "region strdup wrong");
    /* step1 stays valid after step2 built on top of it (stable pointers) */
    CHECK(strcmp(step1, "domain=vessel") == 0, "concat step1 clobbered");
    pgy_region_destroy(&r);
}

/* (d) reset: reuse drops contents but keeps blocks, and re-allocation works. */
static void test_reset(void)
{
    PgyRegion r = pgy_region_create(64);
    size_t blocks_before = 0;
    size_t blocks_after = 0;
    PgyRegionBlock *blk;
    char *first = (char *)pgy_region_alloc(&r, 48, 1);
    char *head = (char *)pgy_region_alloc(&r, 48, 1);
    CHECK(first != NULL, "reset: first alloc null");
    CHECK(head != NULL, "reset: head alloc null");
    memset(first, 'A', 48);
    for (blk = r.current; blk != NULL; blk = blk->next)
        blocks_before++;
    pgy_region_reset(&r);
    char *second = (char *)pgy_region_alloc(&r, 48, 1);
    char *third = (char *)pgy_region_alloc(&r, 48, 1);
    CHECK(second != NULL, "reset: second alloc null");
    CHECK(third != NULL, "reset: third alloc null");
    for (blk = r.current; blk != NULL; blk = blk->next)
        blocks_after++;
    CHECK(blocks_after == blocks_before,
          "reset: retained blocks were not reused before acquiring another");
    /* The head block is reused first; the second allocation must land in the
     * older reset block rather than growing the chain. */
    CHECK(second == head, "reset: did not reuse the head block");
    CHECK(third != second, "reset: reused allocation overlapped");
    pgy_region_destroy(&r);
}

int main(void)
{
    test_chained_growth();
    test_alignment();
    test_string_concat();
    test_reset();
    if (failures != 0) {
        fprintf(stderr, "%d region checks failed\n", failures);
        return 1;
    }
    printf("region-behaviour ok\n");
    return 0;
}
EOF

build_and_run() {
    label="$1"; shift
    if ! "$CC" "${CFLAGS[@]}" "$@" "$WORK/region_harness.c" -o "$WORK/$label.exe" \
            -lm 2>"$WORK/$label.err"; then
        echo "[$LABEL] $label harness build failed" >&2
        tail -20 "$WORK/$label.err" >&2
        exit 1
    fi
    if ! "$WORK/$label.exe" >"$WORK/$label.out" 2>&1; then
        echo "[$LABEL] $label harness FAILED" >&2
        cat "$WORK/$label.out" >&2
        exit 1
    fi
    grep -q "region-behaviour ok" "$WORK/$label.out" || {
        echo "[$LABEL] $label harness produced no verdict" >&2
        cat "$WORK/$label.out" >&2
        exit 1
    }
}

# inline (self-contained) materialization
build_and_run inline
# extern materialization (emitted-program shape), linked against the cext object
build_and_run extern -DPGY_RUNTIME_DECLS_ONLY "$WORK/rt.o"

# ---- budget fail-closed (subprocess: a low ceiling must panic) -------------
cat > "$WORK/budget_harness.c" <<'EOF'
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 0
#endif
#include "pgy_runtime.h"

int main(void)
{
    /* First block acquisition charges its capacity (>= 8 KiB default) to
     * PGY_BUDGET_ALLOC_BYTES. With the host ceiling set to 1024 below, that
     * first charge must exceed the limit and fail closed. */
    PgyRegion r = pgy_region_create(0);
    (void)pgy_region_alloc(&r, 16, 1);
    printf("budget-not-enforced\n");   /* must never print under the ceiling */
    pgy_region_destroy(&r);
    return 0;
}
EOF

if ! "$CC" "${CFLAGS[@]}" "$WORK/budget_harness.c" -o "$WORK/budget.exe" \
        -lm 2>"$WORK/budget_build.err"; then
    echo "[$LABEL] budget harness build failed" >&2
    tail -20 "$WORK/budget_build.err" >&2
    exit 1
fi

# unbounded: no ceiling -> region runs and prints
if ! ( unset PGY_BUDGET_ALLOC_BYTES; "$WORK/budget.exe" ) >"$WORK/budget_unbounded.out" 2>&1; then
    echo "[$LABEL] unbounded region unexpectedly failed" >&2
    cat "$WORK/budget_unbounded.out" >&2
    exit 1
fi
grep -q "budget-not-enforced" "$WORK/budget_unbounded.out" || {
    echo "[$LABEL] unbounded region produced no output" >&2
    cat "$WORK/budget_unbounded.out" >&2
    exit 1
}

# bounded: low ceiling -> the first block acquisition must fail closed
set +e
PGY_BUDGET_ALLOC_BYTES=1024 "$WORK/budget.exe" >"$WORK/budget_bounded.out" 2>&1
rc=$?
set -e
if [ "$rc" -eq 0 ]; then
    echo "[$LABEL] region ignored the allocation budget ceiling" >&2
    cat "$WORK/budget_bounded.out" >&2
    exit 1
fi
if grep -q "budget-not-enforced" "$WORK/budget_bounded.out"; then
    echo "[$LABEL] region allocated past the ceiling before failing" >&2
    cat "$WORK/budget_bounded.out" >&2
    exit 1
fi
grep -q "budget" "$WORK/budget_bounded.out" || {
    echo "[$LABEL] budget panic carried no traceable record" >&2
    cat "$WORK/budget_bounded.out" >&2
    exit 1
}

echo "[$LABEL] ok (chained growth + alignment + region concat + reset, inline==extern; budget fail-closed)"
