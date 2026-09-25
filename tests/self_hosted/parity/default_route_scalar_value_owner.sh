#!/usr/bin/env bash
# The default C route computes the same scalar values as native C for shapes
# tests/compare_backends.sh cannot see: that harness runs the native pipeline
# only, so a wrong answer on the default route reached no gate at all.
# - A C reserved word and the name its C escape spells are distinct bindings.
# - Abs, Min and Max keep a Long operand's width.
# - A Long literal past the signed 64-bit range is refused, not wrapped.
# - A call argument of Min, Max or Abs, as in Max(lo, Min(hi, v)), is typed
#   instead of refused.
# - `let x: Int = f()?` on an explicit Result<Int, E> declares its temporary
#   in the operand's own specialization. The default route declared the
#   one-argument Result<Int> struct, and the C compiler refused the
#   initializer. When the function returns Result<T, E> with another T, the
#   error leaves rebuilt in the return's specialization; a Result with
#   another error type is refused by the native checker and by default C
#   code generation.
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

# The default LLVM route refuses explicit Result<T, E> signatures before
# emission, so the try rows run on the legs that compile them.
expect_values try-enum-error \
    tests/cases/backend_compare/try_chain_enum_err/main.pgy \
    $'11\n1002\n1001\n1003\n1' native-c native-llvm default-c
expect_values try-method-chain \
    tests/cases/backend_compare/try_class_method_chain/main.pgy \
    $'8\n101\n-1\n53\n3' native-c native-llvm default-c

# Native C returns the operand's Result where the String one is declared and
# fails in the C compiler, so this row holds the default route to native LLVM.
cat >"$WORK_DIR/try_payload_conversion.pgy" <<'PGY'
enum Fault { Low, High }

func Check(n: Int) -> Result<Int, Fault> {
    if n < 0 { return Err(Low); }
    if n > 9 { return Err(High); }
    return Ok(n);
}

func Name(n: Int) -> Result<String, Fault> {
    let v: Int = Check(n)?;
    if v == 0 { return Ok("zero"); }
    return Ok("some");
}

func Plain(n: Int) -> Int {
    let v: Int = Check(n)?;
    return v + 1;
}

func Show(r: Result<String, Fault>) -> Void {
    match r {
        case Ok(s): Log(s);
        case Err(e):
            match e {
                case Low: Log("low");
                case High: Log("high");
            }
    }
}

func Main() -> Void {
    Show(Name(0));
    Show(Name(3));
    Show(Name(-4));
    Show(Name(12));
    Log(Plain(3));
}
PGY
expect_values try-payload-conversion "$WORK_REL/try_payload_conversion.pgy" \
    $'zero\nsome\nlow\nhigh\n4' native-llvm default-c

cat >"$WORK_DIR/try_error_mismatch.pgy" <<'PGY'
enum FaultA { Low }
enum FaultB { Bad }

func Check(n: Int) -> Result<Int, FaultA> {
    if n < 0 { return Err(Low); }
    return Ok(n);
}

func Wrap(n: Int) -> Result<Int, FaultB> {
    let v: Int = Check(n)?;
    return Ok(v + 1);
}

func Main() -> Void {
    let r: Result<Int, FaultB> = Wrap(3);
    if IsOk(r) { Log(Unwrap(r)); }
}
PGY
for leg in native-c native-llvm default-c; do
    out_rel="$WORK_REL/try-error-mismatch-$leg.exe"
    if compile "$WORK_REL/try_error_mismatch.pgy" "$leg" "$out_rel"; then
        fail "$leg accepted a try whose error type is not the return's"
    fi
    [[ ! -e "$ROOT_DIR/$out_rel" ]] ||
        fail "$leg left a binary for the mismatched try error type"
done
grep -Fq "try expression error type does not match the function's Result error type" \
    "$ROOT_DIR/$WORK_REL/try-error-mismatch-default-c.exe.log" ||
    { cat "$ROOT_DIR/$WORK_REL/try-error-mismatch-default-c.exe.log" >&2
      fail "default-c mismatched try error type lost its refusal"; }
# The native checker refuses it at the operand `Check(n)` (line 10, column
# 18) before either backend runs: native LLVM used to run it and native C
# failed in the C compiler.
for leg in native-c native-llvm; do
    log="$ROOT_DIR/$WORK_REL/try-error-mismatch-$leg.exe.log"
    grep -Fq "10:18 - '?' cannot propagate error type 'FaultA' out of a function returning 'Result<Int, FaultB>'" \
        "$log" ||
        { cat "$log" >&2; fail "$leg mismatched try error type lost its semantic refusal"; }
done

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

echo "[$LABEL] reserved-word escape, Long Abs/Min/Max, nested builtin arguments and try on an explicit Result agree with native C, and an out-of-range Long literal and a mismatched try error type are refused: PASS"
