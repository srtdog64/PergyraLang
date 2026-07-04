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
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
read -r -a CC_ARGS <<< "${CC:-gcc}"
OUT="$(mktemp -d)/checked_arith.exe"
COMPILE_LOG="$OUT.compile.log"
read -r -a PLATFORM_CFLAG_ARGS <<< "${PLATFORM_CFLAGS:-}"
read -r -a THREAD_LINK_ARGS <<< "${THREAD_LINK_LIB:-}"

path_for_c_compiler() {
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            pgy_path_for_windows_tool "$1"
            ;;
        *)
            printf '%s\n' "$1"
            ;;
    esac
}

INCLUDE_DIR_CC="$(path_for_c_compiler "$ROOT_DIR/src/runtime")"
SOURCE_CC="$(path_for_c_compiler "$ROOT_DIR/src/tests/checked_arith_test.c")"
OUT_CC="$(path_for_c_compiler "$OUT")"

fail() { echo "[checked-arith] FAIL: $*" >&2; exit 1; }

if ! "${CC_ARGS[@]}" "${PLATFORM_CFLAG_ARGS[@]}" -Wall -Wextra -Werror -std=c11 \
    -I"$INCLUDE_DIR_CC" \
    "$SOURCE_CC" \
    -o "$OUT_CC" "${THREAD_LINK_ARGS[@]}" >"$COMPILE_LOG" 2>&1; then
    cat "$COMPILE_LOG" >&2
    fail "test did not compile"
fi

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
