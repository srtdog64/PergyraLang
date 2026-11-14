#!/usr/bin/env bash
# The entrypoint's Int result is the process exit status on every leg, and an
# entrypoint whose result is not Void, Int or Never is refused
# (docs/truth/decisions/2026-11-15_main_int_result_is_exit_status.md).
# Before this gate native C and LLVM dropped the value and exited 0, the
# default C route returned it, and the default LLVM route dropped it or
# refused the program, so `return 3` from Main gave 0 or 3 by route.
# - each program is compiled and run on native C/LLVM and the default C/LLVM
#   routes; stdout and exit status must equal the expected values;
# - a status wider than the host keeps is truncated as Exit(code) truncates
#   it: the expected value is what Exit(300) gives on the native C leg;
# - Bool and String entrypoints, which every leg used to build or try to
#   build, are refused by both front ends with no artifact.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="entrypoint-exit-status-leg"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/entrypoint_exit_status_leg"
WORK_DIR="$ROOT_DIR/$WORK_REL"
LEGS=(native-c native-llvm default-c default-llvm)

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/int_literal.pgy" <<'PGY'
func Main() -> Int {
    Log(7);
    return 3;
}
PGY

cat >"$WORK_DIR/int_helper.pgy" <<'PGY'
func Twice(x: Int) -> Int {
    return x * 2;
}

func Main() -> Int {
    let v: Int = Twice(2);
    Log(v);
    return v;
}
PGY

cat >"$WORK_DIR/int_branch.pgy" <<'PGY'
func Pick(flag: Bool) -> Int {
    if flag { return 9; }
    return 2;
}

func Main() -> Int {
    let v: Int = Pick(true);
    if v > 5 {
        Log(v);
        return v;
    }
    Log(0);
    return 1;
}
PGY

cat >"$WORK_DIR/int_zero.pgy" <<'PGY'
func Main() -> Int {
    Log(1);
    return 0;
}
PGY

cat >"$WORK_DIR/int_exit.pgy" <<'PGY'
func Main() -> Int {
    Log(8);
    Exit(5);
}
PGY

cat >"$WORK_DIR/void_main.pgy" <<'PGY'
func Main() -> Void {
    Log(7);
}
PGY

cat >"$WORK_DIR/void_early.pgy" <<'PGY'
func Main() -> Void {
    let n: Int = 4;
    Log(n);
    if n > 3 {
        return;
    }
    Log(0);
}
PGY

cat >"$WORK_DIR/int_wide.pgy" <<'PGY'
func Main() -> Int {
    Log(2);
    return 300;
}
PGY

cat >"$WORK_DIR/exit_wide.pgy" <<'PGY'
func Main() -> Void {
    Log(2);
    Exit(300);
}
PGY

cat >"$WORK_DIR/bool_main.pgy" <<'PGY'
func Main() -> Bool {
    Log(5);
    return true;
}
PGY

cat >"$WORK_DIR/string_main.pgy" <<'PGY'
func Main() -> String {
    Log(6);
    return "done";
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

# compile NAME LEG [FLAG...]: builds WORK_DIR/NAME-LEG.exe, returns the
# compiler status.
compile() {
    local name="$1" leg="$2" status=0
    shift 2
    # shellcheck disable=SC2046
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/$name.pgy" $(leg_flags "$leg") "$@" \
        -o "$WORK_REL/$name-$leg.exe") \
        >"$WORK_DIR/$name-$leg.compile.log" 2>&1 || status=$?
    return "$status"
}

# run_status NAME LEG: prints the exit status of the built program.
run_status() {
    local name="$1" leg="$2" status=0
    "$WORK_DIR/$name-$leg.exe" >"$WORK_DIR/$name-$leg.out" \
        2>"$WORK_DIR/$name-$leg.err" </dev/null || status=$?
    echo "$status"
}

# expect_status NAME STDOUT STATUS: every leg prints STDOUT and exits STATUS.
expect_status() {
    local name="$1" expected_out="$2" expected_status="$3" leg status
    printf '%s\n' "$expected_out" >"$WORK_DIR/$name.expected"
    for leg in "${LEGS[@]}"; do
        compile "$name" "$leg" ||
            { cat "$WORK_DIR/$name-$leg.compile.log" >&2; fail "$leg did not compile $name"; }
        status="$(run_status "$name" "$leg")"
        [[ "$status" == "$expected_status" ]] ||
            fail "$leg $name exited $status instead of $expected_status"
        tr -d '\r' <"$WORK_DIR/$name-$leg.out" >"$WORK_DIR/$name-$leg.out.lf"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" ||
            { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out.lf" >&2 || true
              fail "$leg $name printed other output"; }
    done
}

# expect_refused NAME: every leg refuses NAME with the entrypoint result
# diagnostic and writes no program.
expect_refused() {
    local name="$1" leg
    for leg in "${LEGS[@]}"; do
        if compile "$name" "$leg" --error-format=json; then
            fail "$leg accepted $name, whose entrypoint result is no exit status"
        fi
        [[ ! -e "$WORK_DIR/$name-$leg.exe" ]] ||
            fail "$leg wrote a program for refused $name"
        grep -Fq '"cause_ir":"semantic:entrypoint:result_type"' \
            "$WORK_DIR/$name-$leg.compile.log" ||
            { cat "$WORK_DIR/$name-$leg.compile.log" >&2
              fail "$leg refused $name without the entrypoint result diagnostic"; }
    done
}

expect_status int_literal '7' 3
expect_status int_helper '4' 4
expect_status int_branch '9' 9
expect_status int_zero '1' 0
expect_status int_exit '8' 5
expect_status void_main '7' 0
expect_status void_early '4' 0

compile exit_wide native-c ||
    { cat "$WORK_DIR/exit_wide-native-c.compile.log" >&2; fail "native-c did not compile exit_wide"; }
wide_status="$(run_status exit_wide native-c)"
[[ "$wide_status" != 0 ]] || fail "Exit(300) exited 0 on native-c"
expect_status int_wide '2' "$wide_status"

expect_refused bool_main
expect_refused string_main

echo "[$LABEL] Int entrypoint results are the exit status and other results are refused on every leg: PASS"
