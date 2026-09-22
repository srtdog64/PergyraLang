#!/usr/bin/env bash
# A defer body of several Log and direct-call statements (docs/205 F5) runs
# in order at scope exit on native C/LLVM and on the default C route, which
# carries it as consecutive "k/n" MIR rows and rebuilds one defer block in
# mir_lower. An assignment in a defer body is still refused on the default
# route with no binary. The default LLVM route refuses every defer today, so
# it is not a leg.
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

out_rel="$WORK_REL/assignment-default-c.exe"
if compile "$FIXTURES/defer_assignment_body_negative.pgy" default-c "$out_rel"; then
    fail "default-c accepted an assignment in a defer body"
fi
[[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "default-c left a binary for the refused defer body"
grep -Fq "defer body statement is outside the Log and direct-call rung" \
    "$ROOT_DIR/$out_rel.log" ||
    { cat "$ROOT_DIR/$out_rel.log" >&2; fail "default-c refused the defer body for another reason"; }

echo "[$LABEL] multi-statement Log/call defer bodies run in order on native C/LLVM and the default C route; assignment bodies are refused there: PASS"
