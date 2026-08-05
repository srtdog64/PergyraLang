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

# Subject of this gate:
#   the parallel snapshot projection changed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() { echo "[parallel-snapshot] FAIL: $*" >&2; exit 1; }

# Capture disposition is executable truth: semantic owns the verdict, MIR
# owns the backend contract, and AST has no compatibility storage.
if grep -R -E -q \
    'ASTParallelSnapshotRow|ast_parallel_(reset_dispositions|add_snapshot_row|seal_dispositions|dispositions_sealed|snapshot_row_find)|snapshot_rows|dispositions_sealed' \
    "$ROOT_DIR/src/parser" "$ROOT_DIR/src/codegen"; then
    fail "parallel snapshot disposition leaked back into AST/backend storage"
fi
grep -Fq 'mir_parallel_capture_boundary_find' \
    "$ROOT_DIR/src/codegen/transpiler_async_parallel_emit.c" \
    || fail "C parallel emitter does not consume the MIR boundary fact"
grep -Fq 'mir_parallel_capture_boundary_find' \
    "$ROOT_DIR/src/codegen/llvm_stmt_parallel_async.c" \
    || fail "LLVM parallel emitter does not consume the MIR boundary fact"
grep -Fq 'parallel_capture_boundaries' \
    "$ROOT_DIR/src/compiler/mir_json_dump.c" \
    || fail "MIR JSON omits parallel capture boundary facts"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[parallel-snapshot] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/parallel_snapshot"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

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

# C-only platforms (macOS CI, Windows C-only) narrow the voice set via env;
# default exercises both backends.
BACKENDS="${PGY_PARALLEL_SNAPSHOT_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    expect_runs "$backend" snapshot_read.pgy $'42\n1'
done

expect_reject reject_write_write.pgy       "write-write race"
expect_reject reject_string_read_write.pgy "read-write race"

echo "[parallel-snapshot] reader snapshot admitted (42/1 on: $BACKENDS); write-write and non-primitive read-write fail closed"
