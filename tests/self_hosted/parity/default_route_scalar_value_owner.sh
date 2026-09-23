#!/usr/bin/env bash
# The default C route computes the same scalar values as native C for shapes
# tests/compare_backends.sh cannot see: that harness runs the native pipeline
# only, so a wrong answer on the default route reached no gate at all.
# - A C reserved word and the name its C escape spells are distinct bindings.
# - Abs, Min and Max keep a Long operand's width.
# - A Long literal past the signed 64-bit range is refused, not wrapped.
# - A call argument of Min, Max or Abs, as in Max(lo, Min(hi, v)), is typed
#   instead of refused.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="default-route-scalar-value"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/default_route_scalar_value"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compile() {
    local fixture="$1" leg="$2" out_rel="$3"
    local flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
        *) fail "unknown leg $leg" ;;
    esac
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" "${flags[@]}" -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

# expect_values NAME FIXTURE EXPECTED LEG...
expect_values() {
    local name="$1" fixture="$2" expected="$3"
    shift 3
    printf '%s\n' "$expected" >"$WORK_DIR/$name.expected"
    local leg out_rel
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$fixture" "$leg" "$out_rel" ||
            { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name binary failed"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" >&2 || true
              fail "$leg computed another value for $name"; }
    done
}

# The default LLVM route refuses Long Abs/Min/Max programs before emission, so
# the width case runs on the three legs that compile it.
expect_values reserved-word-escape \
    tests/cases/backend_compare/c_reserved_word_escape/main.pgy \
    $'100\n1\n100' native-c native-llvm default-c default-llvm
expect_values long-math-width "$FIXTURES/long_math_width.pgy" \
    $'5000000000\n5000000000\n-5000000000\n7\n3\n9\n9223372036854775807' \
    native-c native-llvm default-c
expect_values nested-polymorphic-builtin-argument \
    "$FIXTURES/nested_polymorphic_builtin_argument.pgy" \
    $'50\n0\n4\n7\n6' native-c native-llvm default-c default-llvm

# Both front ends refuse the out-of-range Long literal and publish nothing.
for leg in native-c default-c; do
    out_rel="$WORK_REL/long-literal-range-$leg.exe"
    if compile "$FIXTURES/long_literal_out_of_range_negative.pgy" "$leg" "$out_rel"; then
        fail "$leg accepted a Long literal past the signed 64-bit range"
    fi
    [[ ! -e "$ROOT_DIR/$out_rel" ]] ||
        fail "$leg left a binary for the out-of-range Long literal"
done
grep -Fq "Long literal is outside the signed 64-bit range" \
    "$ROOT_DIR/$WORK_REL/long-literal-range-native-c.exe.log" ||
    fail "native refusal lost its range diagnostic"

echo "[$LABEL] reserved-word escape, Long Abs/Min/Max and nested builtin arguments agree with native C, and an out-of-range Long literal is refused by both front ends: PASS"
