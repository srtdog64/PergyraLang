#!/usr/bin/env bash
# A defer body of several Log, direct-call and assignment statements
# (docs/205 F5) runs in order at scope exit on native C/LLVM and on the
# default C route, which carries it as consecutive "k/n" MIR rows and rebuilds
# one defer block in mir_lower. The default route admits an assignment only to
# a local binding, and a control statement in a defer body is still refused
# there with no binary. The default LLVM route refuses
# every defer today, so it is not a leg.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-defer-multi-statement"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/defer_multi_statement"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/self_hosted/fixtures"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
printf 'body\ninner\nfirst\ncleanup second\n2\n' >"$WORK_DIR/expected"

compile() {
    local fixture="$1" leg="$2" out_rel="$3"
    local flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
    esac
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$fixture" "${flags[@]}" -o "$out_rel") \
        >"$ROOT_DIR/$out_rel.log" 2>&1
}

for leg in native-c native-llvm default-c; do
    out_rel="$WORK_REL/multi-$leg.exe"
    compile "$FIXTURES/defer_multi_statement.pgy" "$leg" "$out_rel" ||
        { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile the multi-statement defer"; }
    "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/multi-$leg.out" ||
        fail "$leg binary failed"
    cmp -s "$WORK_DIR/expected" "$WORK_DIR/multi-$leg.out" ||
        { diff -u "$WORK_DIR/expected" "$WORK_DIR/multi-$leg.out" >&2 || true
          fail "$leg ran the defer bodies in another order"; }
done

printf '1\nfirst\n3\n' >"$WORK_DIR/assignment.expected"
for leg in native-c native-llvm default-c; do
    out_rel="$WORK_REL/assignment-$leg.exe"
    compile "$FIXTURES/defer_assignment_body.pgy" "$leg" "$out_rel" ||
        { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile the assignment defer body"; }
    "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/assignment-$leg.out" ||
        fail "$leg assignment defer binary failed"
    cmp -s "$WORK_DIR/assignment.expected" "$WORK_DIR/assignment-$leg.out" ||
        { diff -u "$WORK_DIR/assignment.expected" "$WORK_DIR/assignment-$leg.out" >&2 || true
          fail "$leg ran the assignment defer body in another order"; }
done

printf '1\n2\n' >"$WORK_DIR/param.expected"
for leg in native-c native-llvm; do
    out_rel="$WORK_REL/param-$leg.exe"
    compile "$FIXTURES/defer_assignment_param_negative.pgy" "$leg" "$out_rel" ||
        { cat "$ROOT_DIR/$out_rel.log" >&2; fail "$leg did not compile the parameter defer assignment"; }
    "$ROOT_DIR/$out_rel" | tr -d '\r' >"$WORK_DIR/param-$leg.out" ||
        fail "$leg parameter defer assignment binary failed"
    cmp -s "$WORK_DIR/param.expected" "$WORK_DIR/param-$leg.out" ||
        { diff -u "$WORK_DIR/param.expected" "$WORK_DIR/param-$leg.out" >&2 || true
          fail "$leg ran the parameter defer assignment another way"; }
done
out_rel="$WORK_REL/param-default-c.exe"
if compile "$FIXTURES/defer_assignment_param_negative.pgy" default-c "$out_rel"; then
    fail "default-c accepted a defer assignment to a parameter"
fi
[[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "default-c left a binary for the refused parameter defer assignment"
grep -Fq "defer assignment target is not a local binding" "$ROOT_DIR/$out_rel.log" ||
    { cat "$ROOT_DIR/$out_rel.log" >&2; fail "default-c refused the parameter defer assignment for another reason"; }

out_rel="$WORK_REL/control-default-c.exe"
if compile "$FIXTURES/defer_control_body_negative.pgy" default-c "$out_rel"; then
    fail "default-c accepted a control statement in a defer body"
fi
[[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "default-c left a binary for the refused defer body"
grep -Fq "defer body statement is outside the Log, direct-call and assignment rung" \
    "$ROOT_DIR/$out_rel.log" ||
    { cat "$ROOT_DIR/$out_rel.log" >&2; fail "default-c refused the defer body for another reason"; }

echo "[$LABEL] multi-statement Log/call/assignment defer bodies run in order on native C/LLVM and the default C route; non-local assignment targets and control statements are refused there: PASS"
