#!/usr/bin/env bash
# Force a real backing-growth allocation failure in the installed C emitter.
set -euo pipefail
ulimit -c 0
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/array-growth-allocation.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[array-growth-allocation] $*; evidence=$REL" >&2; exit 1; }
if ! "$DRIVER" --emit-c-artifact-verified \
    tests/self_hosted/fixtures/array_growth_allocation_failure.pgy "$REL/probe.c" \
    >"$WORK/emit.log" 2>&1; then
    tail -n 30 "$WORK/emit.log" >&2; fail "the installed emitter refused the complete array program";
fi
cat >"$WORK/fault.c" <<'C'
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
void *pgy_test_backing_realloc(void *previous, size_t bytes) {
    if (previous != NULL) {
        fputs("forced-array-backing-failure\n", stderr);
        return NULL;
    }
    return realloc(previous, bytes);
}
C
flags=(-std=c11 -O0 -g -fwrapv -fno-strict-aliasing -Isrc -Isrc/runtime -pthread)
if ! "$CC" "${flags[@]}" "$WORK/probe.c" -lm -o "$WORK/normal.exe" \
    >"$WORK/normal.build.log" 2>&1; then
    tail -n 20 "$WORK/normal.build.log" >&2; fail "normal emitted C did not compile";
fi
if ! "$CC" "${flags[@]}" -Drealloc=pgy_test_backing_realloc \
    -c "$WORK/probe.c" -o "$WORK/probe.o" >"$WORK/fault.build.log" 2>&1 ||
   ! "$CC" "${flags[@]}" "$WORK/probe.o" "$WORK/fault.c" -lm -o "$WORK/fault.exe" \
    >>"$WORK/fault.build.log" 2>&1; then
    tail -n 20 "$WORK/fault.build.log" >&2; fail "fault-injected emitted C did not compile";
fi
for selected in {0..8}; do
    if ! timeout 30 "$WORK/normal.exe" "$selected" >"$WORK/$selected.normal" 2>&1; then
        cat "$WORK/$selected.normal" >&2; fail "normal case $selected failed";
    fi
    grep -Fxq 'array-grow-ok' "$WORK/$selected.normal" || fail "normal value check $selected did not complete"
    fault_status=0
    timeout 30 "$WORK/fault.exe" "$selected" >"$WORK/$selected.fault" 2>&1 || fault_status=$?
    [[ "$fault_status" -eq 134 ]] || fail "case $selected exited $fault_status instead of the OOM abort"
    grep -Fxq 'forced-array-backing-failure' "$WORK/$selected.fault" || fail "case $selected did not reach backing growth"
    grep -Fq 'class=oom' "$WORK/$selected.fault" || {
        cat "$WORK/$selected.fault" >&2; fail "case $selected did not use the OOM contract";
    }
done
echo "[array-growth-allocation] PASS: nine array families preserve values and reject failed backing growth; evidence=$REL"
