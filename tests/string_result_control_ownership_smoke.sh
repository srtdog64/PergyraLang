#!/usr/bin/env bash
# Native fresh String-result summaries must cover every admitted if return.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here string-result-control "$PGY"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/string-result-control.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
FIXTURES="$ROOT_DIR/tests/self_hosted/parity/fixture"
BACKENDS="${PGY_TEST_BACKENDS:-c llvm}"
fail() { echo "[string-result-control] $*; evidence: $WORK_DIR" >&2; exit 1; }
printf '%s\n' 'branch-true,branch-false,control-true,control-false' >"$WORK_DIR/expected.out"

for backend in $BACKENDS; do
    [[ "$backend" == c || "$backend" == llvm ]] || fail "unknown backend: $backend"
    output="$WORK_DIR/fresh.$backend.exe"
    if ! (cd "$ROOT_DIR" && timeout 45s "$PGY" --native-pipeline \
        "$(pgy_path_for_compiler "$PGY" "$FIXTURES/string_result_control_fresh.pgy")" \
        "--backend=$backend" -o "$(pgy_path_for_compiler "$PGY" "$output")") \
        >"$WORK_DIR/fresh.$backend.build.log" 2>&1; then
        cat "$WORK_DIR/fresh.$backend.build.log" >&2
        fail "$backend fresh factories did not compile"
    fi
    timeout 10s "$output" >"$WORK_DIR/fresh.$backend.out" \
        2>"$WORK_DIR/fresh.$backend.err" || fail "$backend fresh factory execution failed"
    [[ ! -s "$WORK_DIR/fresh.$backend.err" ]] || fail "$backend fresh factories emitted stderr"
    tr -d '\r' <"$WORK_DIR/fresh.$backend.out" >"$WORK_DIR/fresh.$backend.normalized"
    cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/fresh.$backend.normalized" ||
        fail "$backend fresh factory output drift"
    for negative in borrowed reassigned loop_borrowed; do
        artifact="$WORK_DIR/$negative.$backend.exe"
        if (cd "$ROOT_DIR" && timeout 45s "$PGY" --native-pipeline \
            "$(pgy_path_for_compiler "$PGY" "$FIXTURES/string_result_control_$negative.pgy")" \
            "--backend=$backend" -o "$(pgy_path_for_compiler "$PGY" "$artifact")") \
            >"$WORK_DIR/$negative.$backend.log" 2>&1; then
            fail "$negative/$backend was granted a deep drop"
        else
            status="$?"
            [[ "$status" == 1 ]] || fail "$negative/$backend refusal status=$status"
        fi
        grep -Eq 'PGY_SEM_BORROW_ESCAPE|MIR collection ownership transition is invalid' \
            "$WORK_DIR/$negative.$backend.log" || fail "$negative/$backend failed for an unrelated reason"
        [[ ! -e "$artifact" ]] || fail "$negative/$backend published an artifact"
    done
done
echo "[string-result-control] PASS ($BACKENDS): fresh branches/control execute; borrowed branch, loop and reassignment refuse deep drop; evidence: $WORK_REL"
