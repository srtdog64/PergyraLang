#!/usr/bin/env bash
#
# parallel_disjoint_split_smoke.sh — Disjointness evidence at the parallel
# boundary (docs/178 WO-DOP-1 rung 0, landed 2026-07-08).
#
# The DOP idiom: one Array is split into two construction-guaranteed
# disjoint halves (`base.Slice(0, B)` / `base.Slice(B, LEN)`, shared
# immutable boundary) and two parallel arms write one half each in place.
# The parallel boundary admits exactly that pair; everything else keeps
# the fail-closed collection-capture reject:
#
#   - admit_pair          runs on BOTH backends and prints 110
#   - reject_same_slice   both arms write one half   -> reject
#   - reject_base_in_arm  arm touches the base array -> reject
#   - reject_mut_boundary `let mut` split boundary   -> reject (no fact)
#   - reject_overlap_views arbitrary overlapping views -> reject (no pair)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[parallel-disjoint] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/parallel_disjoint_split"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

REJECT_NEEDLE="cannot capture mutable collection"

fail() { echo "[parallel-disjoint] FAIL: $*" >&2; exit 1; }

compile() {
    # compile <backend> <fixture> <out-name>; returns rc, captures log.
    local backend="$1" fixture="$2" out_name="$3"
    local src out rc
    src="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$out_name")"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$out_name.log" 2>&1
    rc=$?
    set -e
    return $rc
}

expect_reject() {
    # The admission is semantic-layer, so one backend's voice suffices.
    local fixture="$1"
    if compile c "$fixture" "rej_${fixture%.pgy}.exe"; then
        fail "$fixture compiled but must fail closed"
    fi
    grep -Fq "$REJECT_NEEDLE" "$OUT_DIR/rej_${fixture%.pgy}.exe.log" ||
        fail "$fixture failed without the collection-capture reject"
}

expect_runs() {
    local backend="$1" fixture="$2" want="$3"
    local exe="run_${backend}_${fixture%.pgy}.exe"
    compile "$backend" "$fixture" "$exe" ||
        fail "$backend/$fixture must compile: $(tail -2 "$OUT_DIR/$exe.log")"
    local got
    got="$("$OUT_DIR/$exe" | tr -d '\r')" || fail "$backend/$fixture crashed at runtime"
    [ "$got" = "$want" ] || fail "$backend/$fixture printed '$got', expected '$want'"
}

# C-only platforms (macOS CI, Windows C-only) narrow the voice set via env;
# default exercises both backends.
BACKENDS="${PGY_PARALLEL_DISJOINT_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    expect_runs "$backend" admit_pair.pgy "110"
done

expect_reject reject_same_slice.pgy
expect_reject reject_base_in_arm.pgy
expect_reject reject_mut_boundary.pgy
expect_reject reject_overlap_views.pgy

echo "[parallel-disjoint] disjoint split pair admitted (110 on: $BACKENDS); same-slice/base-in-arm/mut-boundary/overlap all fail closed"
