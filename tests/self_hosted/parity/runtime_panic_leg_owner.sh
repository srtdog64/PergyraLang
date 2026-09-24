#!/usr/bin/env bash
# Every leg panics the same way on a runtime hard failure, and the output a
# program printed before the panic survives it. tests/runtime_panic_codegen_smoke.sh
# runs the native pipeline only, so a default route that returned a payload
# for UnwrapOption(None), or a panic that dropped buffered stdout, reached no
# gate.
# - stdout is a file, as in a pipe or a CI log: Log(Bool) and Log(Int) must be
#   flushed before the abort.
# - UnwrapOption(None), Unwrap(Err) and UnwrapErr(Ok) panic on the default C
#   route as they do natively; UnwrapOption(None) also on the default LLVM
#   route.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="runtime-panic-leg"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/runtime_panic_leg"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/index.pgy" <<'PGY'
func Main() -> Void {
    Log(true);
    Log(42);
    let items: Array<Int> = [1, 2, 3];
    let i: Int = 7;
    Log(items[i]);
}
PGY

cat >"$WORK_DIR/divide.pgy" <<'PGY'
func Divide(a: Int, b: Int) -> Int {
    return a / b;
}

func Main() -> Void {
    Log(false);
    Log(7);
    Log(Divide(1, 0));
}
PGY

cat >"$WORK_DIR/unwrap_none.pgy" <<'PGY'
func Pick(flag: Bool) -> Option<Int> {
    if flag { return Some(5); }
    return None;
}

func Main() -> Void {
    Log(1);
    let o: Option<Int> = Pick(false);
    Log(UnwrapOption(o));
}
PGY

cat >"$WORK_DIR/unwrap_err.pgy" <<'PGY'
enum DivErr { ByZero }

func Divide(a: Int, b: Int) -> Result<Int, DivErr> {
    if b == 0 { return Err(ByZero); }
    return Ok(a / b);
}

func Main() -> Void {
    Log(2);
    let r: Result<Int, DivErr> = Divide(1, 0);
    Log(Unwrap(r));
}
PGY

cat >"$WORK_DIR/unwrap_err_on_ok.pgy" <<'PGY'
enum DivErr { ByZero }

func Divide(a: Int, b: Int) -> Result<Int, DivErr> {
    if b == 0 { return Err(ByZero); }
    return Ok(a / b);
}

func Main() -> Void {
    Log(3);
    let r: Result<Int, DivErr> = Divide(4, 2);
    let e: DivErr = UnwrapErr(r);
    Log(4);
}
PGY

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-c) echo "--backend=c" ;;
        default-llvm) echo "--backend=llvm" ;;
        *) fail "unknown leg $1" ;;
    esac
}

# expect_panic NAME STDOUT CLASS REASON LEG...
expect_panic() {
    local name="$1" expected_out="$2" class="$3" reason="$4"
    shift 4
    printf '%s\n' "$expected_out" >"$WORK_DIR/$name.expected"
    local leg out_rel status
    for leg in "$@"; do
        out_rel="$WORK_REL/$name-$leg.exe"
        # shellcheck disable=SC2046
        (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$WORK_REL/$name.pgy" $(leg_flags "$leg") -o "$out_rel") \
            >"$ROOT_DIR/$out_rel.log" 2>&1 ||
            { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile $name"; }
        status=0
        "$ROOT_DIR/$out_rel" >"$WORK_DIR/$name-$leg.out" \
            2>"$WORK_DIR/$name-$leg.err" || status=$?
        [[ "$status" -ne 0 ]] || fail "$leg $name exited 0 instead of panicking"
        grep -Fq "[PGY PANIC]" "$WORK_DIR/$name-$leg.err" ||
            { cat "$WORK_DIR/$name-$leg.err" >&2; fail "$leg $name printed no panic line"; }
        grep -Fq "class=$class reason=$reason" "$WORK_DIR/$name-$leg.err" ||
            { cat "$WORK_DIR/$name-$leg.err" >&2; fail "$leg $name panicked with another class or reason"; }
        tr -d '\r' <"$WORK_DIR/$name-$leg.out" >"$WORK_DIR/$name-$leg.out.lf"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" >&2 || true
              fail "$leg $name lost the output printed before the panic"; }
    done
}

expect_panic index $'true\n42' out-of-bounds "array index out of bounds" \
    native-c native-llvm default-c default-llvm
expect_panic divide $'false\n7' divide-by-zero "integer division or modulo by zero" \
    native-c native-llvm default-c default-llvm
expect_panic unwrap_none '1' internal-invariant "Option unwrap on None value" \
    native-c native-llvm default-c default-llvm
expect_panic unwrap_err '2' internal-invariant "Result unwrap on Err value" \
    native-c native-llvm default-c
# Native used to refuse UnwrapErr (E6 in
# docs/audits/red_team_ownership_runtime_arithmetic_audit_2026-09-23.md); it
# now panics on Ok the way the default C route does.
expect_panic unwrap_err_on_ok '3' internal-invariant "Result unwrap_err on Ok value" \
    native-c native-llvm default-c

echo "[$LABEL] index, divide and unwrap panics keep their class and the printed output on every leg that builds them: PASS"
