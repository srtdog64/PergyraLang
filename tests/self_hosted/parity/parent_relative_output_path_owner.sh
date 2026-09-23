#!/usr/bin/env bash
# `pgy x.pgy --emit-c -o ../gen/x.c` reaches the self-host driver as a path
# it may write. The driver's runtime IO policy refuses any `..` component,
# the rule for compiled user programs, so pgy resolves such an output path
# before it hands it over. Before that, the command failed at begin-temp with
# no cause while the absolute spelling worked.
# - a parent-relative output path writes the artifact on the default route,
#   as it does on the native pipeline;
# - an output directory that does not exist is refused with that cause, and
#   nothing is written.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="parent-relative-output-path"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_DIR="$ROOT_DIR/.tmp/self_hosted/parent_relative_output_path"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/spike" "$WORK_DIR/gen"
cat >"$WORK_DIR/spike/probe.pgy" <<'PGY'
func Main() -> Void {
    Log("parent-relative");
}
PGY

emit() {
    local out="$1" log="$2"; shift 2
    (cd "$WORK_DIR/spike" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" probe.pgy "$@" --emit-c -o "$out") >"$WORK_DIR/$log" 2>&1
}

emit ../gen/default.c default.log ||
    { cat "$WORK_DIR/default.log" >&2; fail "default route refused a parent-relative --emit-c output path"; }
[[ -s "$WORK_DIR/gen/default.c" ]] || fail "default route wrote no C for ../gen/default.c"

emit ../gen/native.c native.log --native-pipeline ||
    { cat "$WORK_DIR/native.log" >&2; fail "native pipeline refused a parent-relative --emit-c output path"; }
[[ -s "$WORK_DIR/gen/native.c" ]] || fail "native pipeline wrote no C for ../gen/native.c"

if emit ../missing/default.c missing.log; then
    fail "default route accepted an output directory that does not exist"
fi
grep -Fq "does not exist or cannot be resolved" "$WORK_DIR/missing.log" ||
    { cat "$WORK_DIR/missing.log" >&2; fail "missing output directory was refused without its cause"; }
[[ ! -e "$WORK_DIR/missing" ]] || fail "refused output left a directory behind"

echo "[$LABEL] parent-relative --emit-c output paths write on the default route and the native pipeline, and a missing directory is refused with its cause: PASS"
