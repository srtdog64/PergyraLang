#!/usr/bin/env bash
#
# secure_token_reuse_failclosed_smoke.sh — the inline secure-slot twin must
# issue a FRESH token identity on every claim (monotonic counter, in lockstep
# with the extern twin in pgy_runtime_lib_secure_slot_exports.h). A token
# retained across release/re-claim must FAIL CLOSED at use time, never validate
# against the reclaimed slot. The previous address-derived token reproduced the
# same id whenever the claim temp landed at the same address (always true for
# repeated claims through one call site), silently accepting stale tokens.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CC="${CC:-gcc}"
OUT="$(mktemp -d)/secure_token_reuse"

fail() { echo "[secure-token-reuse] FAIL: $*" >&2; exit 1; }

"$CC" -Wall -Wextra -Werror -std=c11 \
    -I"$ROOT_DIR/src/runtime" \
    "$ROOT_DIR/src/tests/secure_token_reuse_test.c" \
    -o "$OUT" || fail "test did not compile"

expect_ok() {
    local mode="$1" want="$2" got
    got="$("$OUT" "$mode")" || fail "$mode aborted unexpectedly"
    [ "$got" = "$want" ] || fail "$mode = $got, expected $want"
    echo "[secure-token-reuse] $mode = $want"
}

expect_panic() {
    local mode="$1" want_class="$2" out rc
    out="$("$OUT" "$mode" 2>&1)" && rc=0 || rc=$?
    [ "$rc" -ne 0 ] \
        || fail "$mode did NOT fail closed (stale token accepted): $out"
    echo "$out" | grep -qi 'PGY PANIC' \
        || fail "$mode exited $rc but without a panic: $out"
    echo "$out" | grep -qi "class=$want_class" \
        || fail "$mode panicked but not class=$want_class: $out"
    echo "[secure-token-reuse] $mode -> fail-closed (class=$want_class)"
}

expect_ok fresh_ok 7
expect_ok distinct_ids DISTINCT
expect_panic stale_read  invalid-secure-token
expect_panic stale_write invalid-secure-token

echo "[secure-token-reuse] PASS — every claim gets a fresh identity; stale tokens fail closed"
