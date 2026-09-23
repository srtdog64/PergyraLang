#!/usr/bin/env bash
# Native C, native LLVM and the default routes agree on the numeric rules
# that the 2026-09-23 red-team audit (A2, A3, A4, R4, E4) found split between
# them. tests/compare_backends.sh runs the native pipeline only, so it cannot
# see a default-route answer or a front end that accepts what the other
# refuses. Values run on every leg that compiles the case; refusals are
# checked on both front ends (native and self-host).
# - An unsuffixed integer literal is an Int and must fit 32 bits;
#   -2147483648 is one literal. Past the range both front ends refuse it.
# - The default route reports an out-of-range Long literal with the range
#   diagnostic, not as an invalid expression graph.
# - `%` over Float, and an Int compared with a Float, are refused by both
#   checkers before any backend runs, also inside a builtin call argument.
# - An Int value widens into a Long binding, assignment and return, but an
#   Array<Int> literal is not an Array<Long>.
# - ToInt of an out-of-range decimal gives the same Int on every leg.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="numeric-literal-conversion"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/numeric_literal_conversion"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CASES="tests/cases/backend_compare"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compile() {
    local source="$1" leg="$2" out_rel="$3"
    local flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
        *) fail "unknown leg $leg" ;;
    esac
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$source" "${flags[@]}" -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

# expect_values NAME SOURCE EXPECTED_FILE LEG...
expect_values() {
    local name="$1" source="$2" expected="$3"
    shift 3
    local leg out_rel
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$source" "$leg" "$out_rel" ||
            { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name binary failed"
        cmp -s "$ROOT_DIR/$expected" "$WORK_DIR/$name-$leg.out" ||
            { diff -u "$ROOT_DIR/$expected" "$WORK_DIR/$name-$leg.out" >&2 || true
              fail "$leg computed another value for $name"; }
    done
}

# expect_refused NAME NATIVE_TEXT DEFAULT_TEXT: the program read from stdin
# is refused by both front ends, publishes nothing, and each log names why.
expect_refused() {
    local name="$1" native_text="$2" default_text="$3"
    local source_rel="$WORK_REL/$name.pgy" leg out_rel text
    cat >"$ROOT_DIR/$source_rel"
    for leg in native-c default-c; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$source_rel" "$leg" "$out_rel"; then
            fail "$leg accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for $name"
        text="$default_text"
        [[ "$leg" == native-c ]] && text="$native_text"
        grep -Fq -- "$text" "$ROOT_DIR/$out_rel.log" ||
            { cat "$ROOT_DIR/$out_rel.log" >&2
              fail "$leg refused $name without: $text"; }
    done
}

INT_RANGE="Int literal is outside the signed 32-bit range"
LONG_RANGE="Long literal is outside the signed 64-bit range"

# The default LLVM route refuses the widening and ToInt shapes before
# emission (direct-MIR admission), so they run on the other three legs.
expect_values int-literal-signed-minimum \
    "$CASES/int_literal_signed_minimum/main.pgy" \
    "$CASES/int_literal_signed_minimum/expected.stdout" \
    native-c native-llvm default-c default-llvm
expect_values int-to-long-widening \
    "$CASES/int_to_long_widening/main.pgy" \
    "$CASES/int_to_long_widening/expected.stdout" \
    native-c native-llvm default-c
expect_values string-to-int-long-narrowing \
    "$CASES/string_to_int_long_narrowing/main.pgy" \
    "$CASES/string_to_int_long_narrowing/expected.stdout" \
    native-c native-llvm default-c

expect_refused int-literal-past-int "$INT_RANGE" "$INT_RANGE" <<'PGY'
func Main() -> Void {
    Log(ToString(2147483648));
}
PGY
expect_refused int-literal-quotient "$INT_RANGE" "$INT_RANGE" <<'PGY'
func Main() -> Void {
    Log(ToString(4294967296 / 65536));
}
PGY
expect_refused int-literal-past-long "$INT_RANGE" "$INT_RANGE" <<'PGY'
func Main() -> Void {
    let x: Int = 99999999999999999999999;
    Log(ToString(x));
}
PGY
# Native typed 3000000001 as Int here and accepted the program only when the
# Long argument came first; the literal is refused wherever it stands.
expect_refused int-literal-in-long-call "$INT_RANGE" "$INT_RANGE" <<'PGY'
func Twice(x: Long) -> Long { return x + x; }
func Main() -> Void { Log(Max(Twice(3000000000), 3000000001)); }
PGY
expect_refused long-literal-past-long "$LONG_RANGE" "$LONG_RANGE" <<'PGY'
func Main() -> Void {
    let a: Long = 9223372036854775808L;
    Log(ToString(a));
}
PGY
expect_refused float-remainder "Operator '%' requires Int or Long operands" \
    "Code: modulo_operand_not_integer" <<'PGY'
func Main() -> Void {
    let a: Float = 7.5;
    let b: Float = 2.0;
    Log(ToString(a % b));
}
PGY
expect_refused int-float-compare "Cannot compare 'Int' and 'Float'" \
    "Code: compare_type_mismatch" <<'PGY'
func Main() -> Void {
    let i: Int = 1;
    let f: Float = 2.5;
    Log(ToString(i < f));
}
PGY
expect_refused int-array-into-long-array \
    "cannot assign 'Array<Int>' to 'Array<Long>'" \
    "Code: let_type_mismatch" <<'PGY'
func Main() -> Void {
    let n: Int = 3;
    let xs: Array<Long> = [n];
    Log(ToString(xs[0]));
}
PGY

echo "[$LABEL] Int literal range, Float remainder, Int-to-Long widening and ToInt agree on every leg that compiles them: PASS"
