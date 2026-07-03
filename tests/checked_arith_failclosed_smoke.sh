#!/usr/bin/env bash
#
# checked_arith_failclosed_smoke.sh — the fail-closed integer add/multiply
# primitives must return the right value when they fit and PANIC (never wrap)
# when they overflow. This is the size-computation-overflow class
# (the libssh2 / c-ares `count * size` and `4 + length` patterns) made
# fail-closed: an overflowed size cannot silently become a tiny allocation.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
CC="${CC:-gcc}"
OUT="$(mktemp -d)/checked_arith"
read -r -a PLATFORM_CFLAG_ARGS <<< "${PLATFORM_CFLAGS:-}"
read -r -a THREAD_LINK_ARGS <<< "${THREAD_LINK_LIB:-}"

fail() { echo "[checked-arith] FAIL: $*" >&2; exit 1; }

"$CC" "${PLATFORM_CFLAG_ARGS[@]}" -Wall -Wextra -Werror -std=c11 \
    -I"$ROOT_DIR/src/runtime" \
    "$ROOT_DIR/src/tests/checked_arith_test.c" \
    -o "$OUT" "${THREAD_LINK_ARGS[@]}" || fail "test did not compile"

expect_ok() {
    local mode="$1" want="$2" got
    got="$("$OUT" "$mode")" || fail "$mode aborted unexpectedly"
    [ "$got" = "$want" ] || fail "$mode = $got, expected $want"
    echo "[checked-arith] $mode = $want (in range, returned)"
}

expect_panic() {
    local mode="$1" out rc
    out="$("$OUT" "$mode" 2>&1)" && rc=0 || rc=$?
    [ "$rc" -ne 0 ] || fail "$mode did NOT fail closed (overflow wrapped silently): $out"
    echo "$out" | grep -qi 'PGY PANIC' || fail "$mode exited $rc but without a panic: $out"
    echo "$out" | grep -qi 'class=arithmetic-overflow' \
        || fail "$mode panicked but not class=arithmetic-overflow: $out"
    echo "[checked-arith] $mode -> fail-closed (class=arithmetic-overflow)"
}

expect_ok  add_ok   5
expect_ok  mul_ok   42
expect_ok  mul_zero 0
expect_ok  mul_neg  -42
expect_panic add_of
expect_panic mul_of
expect_panic mul_of_neg

echo "[checked-arith] PASS — overflow fails closed, in-range arithmetic is exact"
