#!/usr/bin/env bash
# A method receiver evaluates before the method's arguments, and a party
# ability call's arguments evaluate left to right, on the default C route as
# on native C and LLVM (docs/205 §11.1). tests/compare_backends.sh runs the
# native pipeline only, so the default route's order reached no gate: it
# printed "a b receiver" where native printed "receiver a b".
# - A call receiver, an index receiver and a field of a call receiver are
#   bound ahead of the arguments.
# - A binding receiver and a field of one stay in place, so an identity
#   method's writes land in the caller's object.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="default-route-method-receiver-order"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/default_route_method_receiver_order"
WORK_DIR="$ROOT_DIR/$WORK_REL"
CASES="tests/cases/backend_compare"

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

# expect_output NAME FIXTURE EXPECTED LEG...
expect_output() {
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
              fail "$leg evaluated $name in another order"; }
    done
}

# The default LLVM route refuses these programs before emission, so they run
# on the three legs that compile them.
expect_output method-receiver "$CASES/method_receiver_before_arguments/main.pgy" \
    "$(printf '%s\n' receiver a b 111 single c 101 \
        index d 201 index e f 211 items index g 201 outer h 301 \
        i j 16 16 k l 18 18)" \
    native-c native-llvm default-c
expect_output party-ability-arguments \
    "$CASES/party_ability_call_argument_order/main.pgy" \
    "$(printf '%s\n' a b 111 c 201)" \
    native-c native-llvm default-c
# Only the default C route admits a party that is not a binding; it must run
# once, before the argument.
expect_output party-once "tests/self_hosted/fixtures/party_ability_call_party_once.pgy" \
    "$(printf '%s\n' party c 201)" \
    default-c

echo "[$LABEL] call, index and field-of-call receivers run before the arguments, binding receivers keep their address, and party ability calls read the party once and their arguments left to right on native C, native LLVM and the default C route: PASS"
