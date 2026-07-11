#!/usr/bin/env bash
#
# parallel_vision_surface_smoke.sh — the declared-but-unexecutable
# parallel surfaces (docs/181) must fail closed with the vision-surface
# diagnostic instead of silently no-op'ing:
#
#   - any_join.pgy       parallel (x in xs) join with any -> reject (R3)
#   - reactive_form.pgy  role parallel on/every block     -> reject
#
# (The all-join form graduated to execution in rung 0 -- its witness is
# parallel_join_smoke.sh / the parallel_join_collection compare case.)
# When a rung of docs/181 opens one of these forms for real execution,
# this smoke goes RED on purpose: update docs/181 and replace the reject
# with that rung's witness.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[parallel-vision-surface] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/parallel_vision_surface"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

NEEDLE="declared vision surface"

fail() { echo "[parallel-vision-surface] FAIL: $*" >&2; exit 1; }

expect_reject() {
    # Parse-stage reject is backend-independent; the C voice suffices.
    local fixture="$1"
    local src out
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/rej_${fixture%.pgy}.exe")"
    if (cd "$ROOT_DIR" && "$PGY" "$src" --backend=c -o "$out") \
            >"$OUT_DIR/${fixture%.pgy}.log" 2>&1; then
        fail "$fixture compiled but must fail closed"
    fi
    grep -Fq "$NEEDLE" "$OUT_DIR/${fixture%.pgy}.log" ||
        fail "$fixture failed without the vision-surface diagnostic"
}

expect_reject any_join.pgy
expect_reject reactive_form.pgy

echo "[parallel-vision-surface] any-join and reactive-form fail closed with the docs/181 vision-surface diagnostic"
