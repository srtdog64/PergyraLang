#!/usr/bin/env bash
# A function with a declared result must return on every path (red-team audit
# M8). Native semantic refuses a body that can complete with
# PGY_SEM_MISSING_RETURN (semantic_check_body_flow_summary); the default route
# had no such check and emitted C that fell off the end of the function.
# ast_body_flow_verdict_owner.pgy now applies native's flow rules on the
# default route.
#
# - each negative program is refused on native C/LLVM and default C/LLVM with
#   no binary and a well-formed JSON diagnostic whose code is
#   PGY_SEM_MISSING_RETURN, and the default C route also refuses it in text
#   mode with the self-host code. Two negatives have an empty body, one never
#   called (PP-017: the default route used to fail in MIR lowering there, and
#   its JSON receipt was reported malformed);
# - the positive program covers each shape native accepts (both arms, else-if,
#   match default, exhaustive enum and Option matches, `while true` and
#   `loop` without break, Never and Exit tails, a literal condition, a literal
#   range, a literal match subject, a Never function ending in Never calls)
#   and prints the same values with the same exit code on native C/LLVM and
#   default C. The default LLVM route refuses any program with three or more
#   routines today, so it runs only the negatives.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-missing-return-flow"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/missing_return_flow"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

compile() {
    local fixture="$1" leg="$2" out_rel="$3" format="${4:-json}"
    local flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
        *) fail "unknown leg $leg" ;;
    esac
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" "${flags[@]}" --error-format="$format" -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

# name:self-host code the default route prints in text mode
NEGATIVES=(
    if:missing_return
    else_arm:missing_return
    loop_break:missing_return
    dynamic_loop:missing_return
    match_int:missing_return
    enum_arm:missing_return
    empty_range:missing_return
    foreach:missing_return
    empty_body:missing_return
    empty_body_called:missing_return
    never_branch:never_function_fallthrough
)
for entry in "${NEGATIVES[@]}"; do
    IFS=: read -r name self_code <<<"$entry"
    fixture="$FIXTURES/missing_return_${name}_negative.pgy"
    [[ -f "$ROOT_DIR/$fixture" ]] || fail "missing fixture $fixture"
    for leg in native-c native-llvm default-c default-llvm; do
        out_rel="$WORK_REL/$name-$leg.exe"
        if compile "$fixture" "$leg" "$out_rel"; then
            fail "$leg accepted $fixture, which can reach the end of a function with a result"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg left a binary for $fixture"
        log="$ROOT_DIR/$out_rel.log"
        if [[ "$(head -c 2 "$log")" != '[{' ]] || grep -Fq "malformed" "$log" ||
            ! grep -Fq '"code":"PGY_SEM_MISSING_RETURN"' "$log" ||
            ! grep -Fq '"cause_ir":"semantic:cfg:missing_return_path"' "$log"; then
            cat "$log" >&2
            fail "$leg did not refuse $fixture with a PGY_SEM_MISSING_RETURN JSON diagnostic"
        fi
    done
    out_rel="$WORK_REL/$name-default-c-text.exe"
    if compile "$fixture" default-c "$out_rel" text; then
        fail "default-c accepted $fixture in text mode"
    fi
    [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "default-c left a text-mode binary for $fixture"
    grep -Fxq "Code: $self_code" "$ROOT_DIR/$out_rel.log" || {
        cat "$ROOT_DIR/$out_rel.log" >&2
        fail "default-c text mode refused $fixture without Code: $self_code"
    }
done

positive="$FIXTURES/missing_return_flow_positive.pgy"
expected='1|-1|0|10|0|1|2|4|0|3|0|1|7|5|0|2|stop'
for leg in native-c native-llvm default-c; do
    out_rel="$WORK_REL/positive-$leg.exe"
    compile "$positive" "$leg" "$out_rel" ||
        { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg refused $positive"; }
    set +e
    "$ROOT_DIR/$out_rel" >"$WORK_DIR/positive-$leg.out" 2>&1
    status=$?
    set -e
    actual="$(tr -d '\r' <"$WORK_DIR/positive-$leg.out" | paste -sd '|' -)"
    [[ "$actual" == "$expected" ]] || fail "$leg printed '$actual', expected '$expected'"
    [[ "$status" == 9 ]] || fail "$leg exited $status, expected 9"
done

echo "[$LABEL] ${#NEGATIVES[@]} bodies that can complete are refused with PGY_SEM_MISSING_RETURN on native and default C/LLVM (JSON) and on default C (text), and the accepted flow shapes agree with native: PASS"
