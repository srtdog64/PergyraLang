#!/usr/bin/env bash
# `pgy --report-copies` lists the copies the compiler inserted (its Clone
# landings) through the installed self-host driver: one line per copy, in
# source order, with the statement's position, the copied expression and the
# place that receives it. A program with no inserted copy reports nothing; a
# refused program prints its diagnostic and exits 1; the native pipeline,
# which has no such report, refuses the request. Before 2026-11-24 a compiler
# copy was visible only in the MIR JSON.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="copy-report"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/copy_report"
WORK_DIR="$ROOT_DIR/$WORK_REL"
FIXTURES="tests/cases/function_param_flow_summary"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat >"$WORK_DIR/copied_initializer.pgy" <<'PGY'
func Main() -> Int {
    var items: Array<Int> = [1, 2];
    let before: Array<Int> = items;
    items[0] = 5;
    Print(ToString(before[0] * 10 + items[0]));
    return 0;
}
PGY

cat >"$WORK_DIR/no_copy.pgy" <<'PGY'
func Main() -> Void {
    var xs: Array<Int> = [1, 2];
    ArrayPush(xs, 3);
    Log(ArrayLength(xs));
}
PGY

cat >"$WORK_DIR/refused.pgy" <<'PGY'
func Main() -> Void {
    let x: Int = "text";
    Log(x);
}
PGY

cp "$ROOT_DIR/$FIXTURES/default_aggregate_move_reuse_landed_valid.pgy" "$WORK_DIR/moved_twice.pgy"
cp "$ROOT_DIR/$FIXTURES/string_element_kept_by_callee_landed_valid.pgy" "$WORK_DIR/element_argument.pgy"

# report NAME: runs the request; sets STATUS and leaves the output in NAME.out.
report() {
    STATUS=0
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" --report-copies "$WORK_REL/$1.pgy") >"$WORK_DIR/$1.out" 2>&1 || STATUS=$?
    tr -d '\r' <"$WORK_DIR/$1.out" >"$WORK_DIR/$1.txt"
}

# expect_lines NAME LINE...: the report is exactly these lines.
expect_lines() {
    local name="$1"; shift
    report "$name"
    [[ "$STATUS" == 0 ]] || { cat "$WORK_DIR/$name.out" >&2; fail "$name exited $STATUS"; }
    printf '%s\n' "$@" >"$WORK_DIR/$name.expected"
    cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name.txt" ||
        { diff -u "$WORK_DIR/$name.expected" "$WORK_DIR/$name.txt" >&2 || true; fail "$name reported other copies"; }
}

expect_lines copied_initializer \
    "copied_initializer.pgy:3:5: copy of \`items\` (value of \`before\`)"
expect_lines moved_twice \
    "moved_twice.pgy:23:5: copy of \`a\` (argument 1 of \`AddBlock\`)" \
    "moved_twice.pgy:24:5: copy of \`a\` (argument 1 of \`AddBlock\`)"
expect_lines element_argument \
    "element_argument.pgy:17:9: copy of \`parts[i]\` (argument 2 of \`Join\`)"

report no_copy
[[ "$STATUS" == 0 ]] || { cat "$WORK_DIR/no_copy.out" >&2; fail "no_copy exited $STATUS"; }
[[ ! -s "$WORK_DIR/no_copy.txt" ]] || { cat "$WORK_DIR/no_copy.out" >&2; fail "no_copy reported a copy"; }

report refused
[[ "$STATUS" == 1 ]] || fail "refused exited $STATUS"
grep -Fxq "Code: let_type_mismatch" "$WORK_DIR/refused.txt" ||
    { cat "$WORK_DIR/refused.out" >&2; fail "refused did not print its diagnostic"; }

STATUS=0
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" "$PGY" --native-pipeline --report-copies \
    "$WORK_REL/no_copy.pgy") >"$WORK_DIR/native.out" 2>&1 || STATUS=$?
[[ "$STATUS" == 1 ]] || fail "the native pipeline accepted --report-copies"

echo "[$LABEL] 3 programs report their compiler copies with position, expression and place; a program with none reports nothing; a refused program and the native pipeline exit 1: PASS"
