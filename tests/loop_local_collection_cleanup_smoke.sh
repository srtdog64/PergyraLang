#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here loop-local-collection "$PGY"
mkdir -p "$ROOT_DIR/.tmp"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/loop-local-collection.XXXXXX")"
CASES="$ROOT_DIR/tests/self_hosted/parity/fixture/loop_local_collection_cleanup"
fail() { echo "[loop-local-collection] $*; evidence: $WORK" >&2; exit 1; }
for backend in ${PGY_TEST_BACKENDS:-c llvm}; do
    [[ "$backend" == c || "$backend" == llvm ]] || fail "unknown backend $backend"
    for name in positive explicit sanitized declared indexed; do
        artifact="$WORK/$name.$backend.exe"
        pgy_run_with_timeout 45 "$WORK/$name.$backend.build.out" "$WORK/$name.$backend.build.err" \
            "$PGY" --native-pipeline "--backend=$backend" \
            "$(pgy_path_for_compiler "$PGY" "${CASES}_$name.pgy")" \
            -o "$(pgy_path_for_compiler "$PGY" "$artifact")" || fail "$name/$backend compile"
        pgy_run_with_timeout 10 "$WORK/$name.$backend.out" "$WORK/$name.$backend.err" \
            "$artifact" || fail "$name/$backend execution"
        [[ ! -s "$WORK/$name.$backend.err" ]] || fail "$name/$backend stderr"
        actual="$(tr -d '\r\n' < "$WORK/$name.$backend.out")"
        expected=1026
        [[ "$name" != explicit ]] || expected=3
        [[ "$name" != sanitized ]] || expected=ok
        [[ "$name" != declared ]] || expected=6
        [[ "$name" != indexed ]] || expected=row0row1row2
        [[ "$actual" == "$expected" ]] || fail "$name/$backend output $actual"
    done
    for invalid in escaped deferred indexed_escape; do
    artifact="$WORK/$invalid.$backend.exe"
    if pgy_run_with_timeout 45 "$WORK/$invalid.$backend.out" "$WORK/$invalid.$backend.err" \
        "$PGY" --native-pipeline "--backend=$backend" \
        "$(pgy_path_for_compiler "$PGY" "${CASES}_${invalid}_invalid.pgy")" \
        -o "$(pgy_path_for_compiler "$PGY" "$artifact")"; then
        fail "$invalid storage was granted automatic retirement"
    else
        status=$?
        [[ "$status" == 1 ]] || fail "$invalid storage exit $status"
    fi
    grep -Eq 'PGY_SEM_BORROW_ESCAPE|MIR collection ownership transition is invalid' \
        "$WORK/$invalid.$backend.out" "$WORK/$invalid.$backend.err" || fail "unrelated refusal"
    [[ ! -e "$artifact" ]] || fail "$invalid storage published an artifact"
    done
done
echo "[loop-local-collection] PASS: builtin/declared reads, borrowed/owned, continue/break/return, nested loops and explicit drops; escape/defer refuse; evidence: $WORK"
