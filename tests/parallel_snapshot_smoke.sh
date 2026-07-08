#!/usr/bin/env bash
#
# parallel_snapshot_smoke.sh — reader-snapshot capture at the parallel
# boundary (docs/178 Copy evidence, statement level; landed 2026-07-09).
#
# A single-writer primitive scalar read by other arms is admitted: reader
# arms receive the pre-parallel snapshot (deterministic), the writer keeps
# the exclusive live location, and the parent observes the writer's value
# after the join. Everything else stays fail-closed:
#
#   - snapshot_read            runs on BOTH backends and prints 42 / 1
#     (the reader is channel-sequenced AFTER the write, so a shared-pointer
#     regression would print 42 / 42 -- the fixture discriminates)
#   - reject_write_write       two writer arms            -> reject
#   - reject_string_read_write non-primitive read-write   -> reject

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[parallel-snapshot] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/parallel_snapshot"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[parallel-snapshot] FAIL: $*" >&2; exit 1; }

compile() {
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
    local fixture="$1" needle="$2"
    if compile c "$fixture" "rej_${fixture%.pgy}.exe"; then
        fail "$fixture compiled but must fail closed"
    fi
    grep -Fq "$needle" "$OUT_DIR/rej_${fixture%.pgy}.exe.log" ||
        fail "$fixture failed without the expected diagnostic: $needle"
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

expect_runs c    snapshot_read.pgy $'42\n1'
expect_runs llvm snapshot_read.pgy $'42\n1'

expect_reject reject_write_write.pgy       "write-write race"
expect_reject reject_string_read_write.pgy "read-write race"

echo "[parallel-snapshot] reader snapshot admitted (42/1 both backends); write-write and non-primitive read-write fail closed"
