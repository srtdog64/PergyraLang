#!/usr/bin/env bash
# The call hand-off rule (src/semantic/call_handoff_rule.def): its rendered C
# header, C source and Pergyra owner stay equal to a fresh render, a malformed
# rule is refused by the renderer, and both renderings answer every decision
# cell the same way.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PYTHON_BIN="${PYTHON_BIN:-python3}"
CC_BIN="${CC:-cc}"
RULE="$ROOT_DIR/src/semantic/call_handoff_rule.def"
HEADER="$ROOT_DIR/src/semantic/call_handoff_rule.h"
SOURCE="$ROOT_DIR/src/semantic/call_handoff_rule.c"
OWNER="$ROOT_DIR/src/self_hosted/semantic/call_handoff_rule_projection_owner.pgy"
RENDER="$ROOT_DIR/scripts/render_call_handoff_rule.py"
BUILD_DIR="$ROOT_DIR/.tmp/call_handoff_rule"
mkdir -p "$BUILD_DIR"
TMP_DIR="$(mktemp -d "$BUILD_DIR/run.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT
fail() { echo "[call-handoff-rule] $*" >&2; exit 1; }
render() { "$PYTHON_BIN" -B "$RENDER" "$@"; }

render "$RULE" "$HEADER" "$SOURCE" "$OWNER" --check

# A hand edit of any rendered output, or a rule edited without
# rendering, is stale.
for edited in header source owner rule; do
    cp "$RULE" "$TMP_DIR/rule.def"
    cp "$HEADER" "$TMP_DIR/rule.h"
    cp "$SOURCE" "$TMP_DIR/rule.c"
    cp "$OWNER" "$TMP_DIR/rule.pgy"
    case "$edited" in
        header) printf '/* edited */\n' >>"$TMP_DIR/rule.h" ;;
        source) printf '/* edited */\n' >>"$TMP_DIR/rule.c" ;;
        owner) printf '// edited\n' >>"$TMP_DIR/rule.pgy" ;;
        rule)
            sed 's/^PGY_CALL_HANDOFF_ROW(RETURN_ONLY, FRESH, ANY, ANY, ANY, RESULT_OWNS)$/PGY_CALL_HANDOFF_ROW(RETURN_ONLY, FRESH, ANY, ANY, ANY, MOVED)/' \
                "$RULE" >"$TMP_DIR/rule.def"
            ! cmp -s "$RULE" "$TMP_DIR/rule.def" || fail "the rule edit did not apply"
            ;;
    esac
    if render "$TMP_DIR/rule.def" "$TMP_DIR/rule.h" "$TMP_DIR/rule.c" "$TMP_DIR/rule.pgy" --check \
        >"$TMP_DIR/check.log" 2>&1; then
        fail "a stale $edited passed the drift check"
    fi
    grep -Fq "stale" "$TMP_DIR/check.log" || fail "the $edited drift reported no stale output"
done

# expect_refused <label> <sed program> applies the program to the rule; the
# renderer must refuse the result with a line naming <diagnostic>.
expect_refused() {
    local label="$1" program="$2" diagnostic="$3"
    sed -e "$program" "$RULE" >"$TMP_DIR/$label.def"
    ! cmp -s "$RULE" "$TMP_DIR/$label.def" || fail "malformed rule $label did not change the rule"
    if render "$TMP_DIR/$label.def" "$TMP_DIR/$label.h" "$TMP_DIR/$label.c" "$TMP_DIR/$label.pgy" \
        >"$TMP_DIR/$label.log" 2>&1; then
        fail "malformed rule was rendered: $label"
    fi
    grep -Fq -- "$diagnostic" "$TMP_DIR/$label.log" ||
        { cat "$TMP_DIR/$label.log" >&2; fail "malformed rule $label lost its diagnostic"; }
}
LAST_ROW='/^PGY_CALL_HANDOFF_ROW(KEEP_ONLY|MOVING|UNPROVED, ANY, ANY, ANY, ANY, CLONE_ARGUMENT)$/'
expect_refused missing-cell "${LAST_ROW}d" "cells without an action"
expect_refused shadowed-row "\$a PGY_CALL_HANDOFF_ROW(READ_ONLY, ANY, ANY, ANY, ANY, REFUSE)" \
    "shadowed by earlier rows"
expect_refused handoff-used-again \
    's/^PGY_CALL_HANDOFF_ROW(KEEP_ONLY|MOVING|UNPROVED, LOCAL, ONCE, SYNC, ANY, HAND_OFF)$/PGY_CALL_HANDOFF_ROW(KEEP_ONLY|MOVING|UNPROVED, LOCAL, ANY, SYNC, ANY, HAND_OFF)/' \
    "only a LOCAL origin used ONCE"
expect_refused rule-row-divergence-action \
    "\$a PGY_CALL_HANDOFF_ROW(READ_ONLY, ANY, ANY, ANY, ANY, KEEP_ROW_STATE)" \
    "is for divergence rows only"
expect_refused undeclared-divergence \
    "\$a PGY_CALL_HANDOFF_DIVERGENCE_ROW(NATIVE_UNKNOWN, MOVING, LOCAL, ONCE, SYNC, AS_GIVEN, REFUSE)" \
    "undeclared divergence"
expect_refused inert-divergence-row \
    "\$a PGY_CALL_HANDOFF_DIVERGENCE_ROW(NATIVE_KEEP_REUSE_REFUSED, MOVING, LOCAL, USED_AGAIN, SYNC, AS_GIVEN, CLONE_ARGUMENT)" \
    "changes no cell"
expect_refused divergence-without-slice \
    's/^PGY_CALL_HANDOFF_DIVERGENCE(SELF_LATER_USE_NOT_READ, SELF_HOST, INPUT, S5)$/PGY_CALL_HANDOFF_DIVERGENCE(SELF_LATER_USE_NOT_READ, SELF_HOST, INPUT, S1)/' \
    "must name a later slice"
expect_refused dropped-origin '/^PGY_CALL_HANDOFF_AXIS(ORIGIN, BORROWED)$/d' "unknown origin BORROWED"
expect_refused unknown-macro "\$a PGY_CALL_HANDOFF_ROWS(ANY, ANY, ANY, ANY, ANY, REFUSE)" "unknown macro"

# Decision cells: the probe checks the named cells of the native rendering,
# then both renderings dump every cell and the dumps must be equal.
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/registry/call_handoff_rule_probe.c" "$SOURCE" -o "$BUILD_DIR/probe.exe"
"$BUILD_DIR/probe.exe" || fail "a native decision cell differs from the rule"
"$BUILD_DIR/probe.exe" --dump >"$BUILD_DIR/native_cells.txt"
cells="$(grep -c '^cell ' "$BUILD_DIR/native_cells.txt" || true)"
[[ "$cells" -gt 0 ]] || fail "the native probe dumped no cell"

PGY_BIN="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here "call-handoff-rule" "$PGY_BIN"
probe_src="$(pgy_path_for_compiler "$PGY_BIN" "$ROOT_DIR/tests/registry/call_handoff_rule_probe.pgy")"
probe_c="$(pgy_path_for_compiler "$PGY_BIN" "$BUILD_DIR/probe_pgy.c")"
# The native pipeline compiles the generated Pergyra owner: the subject is
# that owner's table, not the self-host driver's coverage.
(cd "$ROOT_DIR" && "$PGY_BIN" --native-pipeline "$probe_src" --emit-c -o "$probe_c") \
    >"$BUILD_DIR/probe_pgy.log" 2>&1 ||
    { cat "$BUILD_DIR/probe_pgy.log" >&2; fail "the Pergyra probe did not compile"; }
"$CC_BIN" -std=c11 -D_POSIX_C_SOURCE=200809L -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -pthread "$BUILD_DIR/probe_pgy.c" -o "$BUILD_DIR/probe_pgy.exe" -lm \
    >"$BUILD_DIR/probe_pgy_cc.log" 2>&1 ||
    { cat "$BUILD_DIR/probe_pgy_cc.log" >&2; fail "the Pergyra probe C did not build"; }
"$BUILD_DIR/probe_pgy.exe" | tr -d '\r' >"$BUILD_DIR/self_host_cells.txt"
cmp -s "$BUILD_DIR/native_cells.txt" "$BUILD_DIR/self_host_cells.txt" ||
    { diff "$BUILD_DIR/native_cells.txt" "$BUILD_DIR/self_host_cells.txt" | head -20 >&2
      fail "the C header and the Pergyra owner disagree on a cell"; }
names="$(grep -c '^name ' "$BUILD_DIR/native_cells.txt" || true)"
[[ "$names" -gt 0 ]] || fail "the native probe dumped no MIR JSON spelling"
echo "[call-handoff-rule] $cells cells and $names spellings: the C header and the Pergyra owner agree"
echo "[call-handoff-rule] drift, malformed-rule and decision-cell checks: ok"
