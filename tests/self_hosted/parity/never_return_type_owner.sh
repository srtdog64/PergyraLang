#!/usr/bin/env bash
# `-> Never` (docs/205 L1, docs/grammar/01_syntax.md): a Never call ends its
# path, a Never function may not return or complete, and Never is not a value.
#
# - positive programs run on every route that admits their shape and stop at
#   the Never call with its exit code;
# - `return` in, falling off the end of, or a value use of a Never function is
#   refused on every route with no binary;
# - a function whose name contains "panic" but whose type is not Never
#   returns normally (docs/205 L1b: LLVM once marked it noreturn by name).
# - a statement after a Never call is not lowered, as after a return (native
#   MIR lowering once failed on the dead statement's SSA use).
# The three-routine positives are not run on the default LLVM route: that route
# refuses any three-routine program today, Never or not.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-never-return-type"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/never_return_type"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

leg_flags() {
    case "$1" in
        native-c) echo "--native-pipeline --backend=c" ;;
        native-llvm) echo "--native-pipeline --backend=llvm" ;;
        default-c) echo "--backend=c" ;;
        default-llvm) echo "--backend=llvm" ;;
    esac
}

compile() {
    local fixture="$1" leg="$2" out_rel="$3"
    # shellcheck disable=SC2046
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" $(leg_flags "$leg") -o "$out_rel") \
        >"$WORK_DIR/$(basename "$out_rel").log" 2>&1
}

# name:fixture:expected stdout ('|'-joined):expected exit code:legs
POSITIVE_CASES=(
    "main:$FIXTURES/never_main_positive.pgy:1|stop:3:native-c native-llvm default-c default-llvm"
    "chain:$FIXTURES/never_chain_positive.pgy:chain:5:native-c native-llvm default-c default-llvm"
    "exit-tail:$FIXTURES/never_exit_tail_positive.pgy::1:native-c native-llvm default-c default-llvm"
    "pick:$FIXTURES/never_pick_positive.pgy:1|no pick:3:native-c native-llvm default-c"
    "panic-name:$FIXTURES/panic_named_user_function.pgy:2|after:0:native-c native-llvm default-c default-llvm"
    "dead-after:$FIXTURES/never_dead_statement_positive.pgy:1:0:native-c native-llvm default-c"
)
for entry in "${POSITIVE_CASES[@]}"; do
    IFS=: read -r name fixture expected code legs <<<"$entry"
    for leg in $legs; do
        out_rel="$WORK_REL/$name-$leg.exe"
        compile "$fixture" "$leg" "$out_rel" || {
            cat "$WORK_DIR/$name-$leg.exe.log" >&2
            fail "$name did not compile on $leg"
        }
        set +e
        "$ROOT_DIR/$out_rel" >"$WORK_DIR/$name-$leg.out" 2>&1
        status=$?
        set -e
        actual="$(tr -d '\r' <"$WORK_DIR/$name-$leg.out" | paste -sd '|' -)"
        [[ "$actual" == "$expected" ]] ||
            fail "$name on $leg printed '$actual', expected '$expected'"
        [[ "$status" == "$code" ]] ||
            fail "$name on $leg exited $status, expected $code"
    done
done

# name:fixture:native diagnostic fragment:default-route diagnostic code
NEGATIVE_CASES=(
    "return:$FIXTURES/never_return_negative.pgy:A Never function cannot return:return_type_mismatch"
    "fallthrough:$FIXTURES/never_fallthrough_negative.pgy:with return type 'Never' may fall through:never_function_fallthrough"
    "value:$FIXTURES/never_value_negative.pgy:cannot assign 'Never' to 'Int':let_type_mismatch"
)
for entry in "${NEGATIVE_CASES[@]}"; do
    IFS=: read -r name fixture native_text default_code <<<"$entry"
    for leg in native-c native-llvm default-c default-llvm; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$fixture" "$leg" "$out_rel"; then
            fail "$leg accepted the Never $name program"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] ||
            fail "$leg left a binary for the refused Never $name program"
        case "$leg" in
            native-*) want="$native_text" ;;
            *) want="$default_code" ;;
        esac
        grep -Fq -- "$want" "$WORK_DIR/$name-$leg.exe.log" ||
            fail "$leg refused Never $name for another reason (want: $want)"
    done
done

echo "[$LABEL] Never calls end paths; return, fallthrough and value use are refused on native and default C/LLVM: PASS"
