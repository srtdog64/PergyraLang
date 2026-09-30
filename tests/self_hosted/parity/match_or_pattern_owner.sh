#!/usr/bin/env bash
# `case A | B:` on the default route (PP-070, H5b). The HIR pattern owner
# names an arm's alternatives (AstMatchCasePatternAlternatives); every later
# owner reads that list instead of splitting the atom.
#
# - native, the default C route and the default LLVM route print the same
#   lines for Int and payload-free enum or-patterns;
# - the default route's MIR JSON carries every alternative in
#   `match_patterns`, as native MIR does;
# - an enum match without default counts each alternative toward coverage;
# - an or-pattern that destructures a payload is refused at that pattern.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-match-or-pattern"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/match_or_pattern"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/match_or_pattern.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
printf 'auth\nauth\nmissing\nother\ntrue\ntrue\nfalse\n' >"$WORK_DIR/expected.out"

# run NAME FLAGS...: compile the fixture, run it, compare with expected.out.
run_leg() {
    local name="$1"; shift
    local bin="$WORK_REL/$name.bin"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$SOURCE_REL" "$@" -o "$bin") >"$WORK_DIR/$name.compile.log" 2>&1 ||
        { cat "$WORK_DIR/$name.compile.log" >&2; fail "$name compile failed"; }
    local exe="$ROOT_DIR/$bin"
    [[ -x "$exe" ]] || exe="$exe.exe"
    [[ -x "$exe" ]] || fail "$name published no executable"
    "$exe" | tr -d '\r' >"$WORK_DIR/$name.out"
    cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/$name.out" ||
        { diff "$WORK_DIR/expected.out" "$WORK_DIR/$name.out" >&2 || true; fail "$name output drifted"; }
}
run_leg native --native-pipeline
run_leg default-c --backend=c
run_leg default-llvm

# Both MIR producers carry every alternative on one row.
(cd "$ROOT_DIR" && "$PGY" "$SOURCE_REL" --native-pipeline --mir-json) \
    >"$WORK_DIR/native.mir.json" 2>"$WORK_DIR/native.mir.err" ||
    { cat "$WORK_DIR/native.mir.err" >&2; fail "native MIR JSON failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/self.mir.json") >"$WORK_DIR/self.mir.out" 2>"$WORK_DIR/self.mir.err" ||
    { cat "$WORK_DIR/self.mir.err" >&2; fail "default-route MIR JSON failed"; }
for mir in native.mir.json self.mir.json; do
    for pattern in '"match_patterns":["401","403"]' '"match_patterns":["Red","Amber"]'; do
        tr -d ' \r\n' <"$WORK_DIR/$mir" | grep -Fq "$pattern" ||
            fail "$mir lost the or-pattern row $pattern"
    done
done

# A destructuring alternative is refused at that pattern, as native refuses it.
cat >"$WORK_DIR/destructure.pgy" <<'PGY'
func Pick(value: Option<Int>) -> Int {
    match value {
        case Some(x) | None: return 1;
    }
    return 0;
}

func Main() -> Void {
    Log(ToString(Pick(Some(1))));
}
PGY
if (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
    "$PGY" "$WORK_REL/destructure.pgy" --backend=c -o "$WORK_REL/destructure.bin") \
    >"$WORK_DIR/destructure.log" 2>&1; then
    fail "default route accepted a destructuring or-pattern"
fi
tr -d '\r' <"$WORK_DIR/destructure.log" | grep -Fxq "Code: match_or_pattern_destructure" ||
    { cat "$WORK_DIR/destructure.log" >&2; fail "destructuring or-pattern lost its code"; }
tr -d '\r' <"$WORK_DIR/destructure.log" | grep -Fxq "Span: destructure.pgy:3:14" ||
    { cat "$WORK_DIR/destructure.log" >&2; fail "destructuring or-pattern lost its position"; }

echo "[$LABEL] Int and enum or-patterns on native, default C and default LLVM, MIR rows, destructuring refusal: PASS"
