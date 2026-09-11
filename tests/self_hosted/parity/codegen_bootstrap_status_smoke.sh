#!/usr/bin/env bash
# Exercise the production stdout boundary without bootstrapping a compiler.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
WORK_BASE="$ROOT_DIR/.tmp/self_hosted/codegen_bootstrap_status"
mkdir -p "$WORK_BASE"
B="$(mktemp -d "$WORK_BASE/run.XXXXXX")"
fail() { echo "[codegen-bootstrap-status] $* ($B)" >&2; exit 1; }
# Source the extracted boundary from a real file: bash 3.2, which is still
# /bin/bash on the macOS runners, reads a process substitution short and
# silently defines nothing.
sed -n '/^run_native_stdout() {/,/^}/p' \
    "$ROOT_DIR/tests/self_hosted/parity/codegen_bootstrap.sh" \
    >"$B/run_native_stdout.sh"
[[ -s "$B/run_native_stdout.sh" ]] || fail "production boundary not extracted"
# shellcheck source=/dev/null
source "$B/run_native_stdout.sh"
declare -F run_native_stdout >/dev/null || fail "production boundary missing"
run_native_capture() {
    printf '%s\r\n' "$5" >"$2"
    : >"$3"
    return "$6"
}
for expected_rc in 0 7; do
    # A previous output must never replace a fresh diagnostic.
    printf 'stale output\n' >"$B/probe.out"
    set +e
    actual="$(run_native_stdout probe ignored "observed=$expected_rc" "$expected_rc")"
    actual_rc=$?
    set -e
    [[ "$actual_rc" -eq "$expected_rc" ]] || fail "exit status drifted"
    [[ "$actual" == "observed=$expected_rc" ]] || fail "stdout lost or stale"
done
echo "[codegen-bootstrap-status] success/failure status and fresh diagnostic stdout: PASS"
