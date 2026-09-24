#!/usr/bin/env bash
#
# parallel_capture_reach_smoke.sh -- storage a parallel task reaches through
# a captured binding beyond the binding's own name
# (src/semantic/parallel_capture_storage_reach.c and
# src/semantic/parallel_capture_write_reach.c).
#
# The capture checks in type_checker_flow_parallel.c decide by name. Two
# shapes reached shared storage without the name and were admitted as data
# races (docs/audits, third round, DRF-1 and DRF-2):
#
#   - a method call that writes the receiver (`counter.Step(1)` in two
#     tasks), directly, one call deeper, or through a bare field name, on a
#     subject or a zone;
#   - an aggregate whose field holds a collection (`Holder(arr)` shares the
#     Array's elements), also nested, wrapped in Option, or a zone field.
#
# Both now fail closed. The admitted shapes stay admitted and run on both
# native backends: one writer task, read-only method calls, ref borrows, a
# struct of scalars, and a zone used by one task. An own move stays with the
# resource snapshot owner, whose diagnostic must keep firing.

set -euo pipefail

# Subject of this gate:
#   the native semantic parallel capture checks changed.
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

fail() { echo "[parallel-capture-reach] FAIL: $*" >&2; exit 1; }

# Both capture checks consult the reach owner; neither may fall back to the
# name-only answer.
CHECKER="$ROOT_DIR/src/semantic/type_checker_flow_parallel.c"
grep -Fq 'parallel_task_writes_through_binding(' "$CHECKER" ||
    fail "the scalar race check no longer counts writes through a binding"
grep -Fq 'parallel_capture_type_reaches_storage(' "$CHECKER" ||
    fail "the collection capture check no longer follows aggregate fields"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[parallel-capture-reach] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/parallel_capture_reach"
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
    # The checks are semantic-layer, so one backend's voice suffices.
    local fixture="$1" needle="$2"
    if compile c "$fixture" "rej_${fixture%.pgy}.exe"; then
        fail "$fixture compiled but must fail closed"
    fi
    grep -Fq "$needle" "$OUT_DIR/rej_${fixture%.pgy}.exe.log" ||
        fail "$fixture failed without the expected diagnostic: $needle ($(grep -m1 ERROR "$OUT_DIR/rej_${fixture%.pgy}.exe.log"))"
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

# DRF-1: writes through a method call or a handed-on reference.
expect_reject reject_subject_method_both_tasks.pgy        "write-write race"
expect_reject reject_subject_nested_method.pgy            "write-write race"
expect_reject reject_subject_implicit_field_write.pgy     "write-write race"
expect_reject reject_subject_default_param_both_tasks.pgy "write-write race"
expect_reject reject_subject_method_vs_field_read.pgy     "read-write race"
expect_reject reject_ref_borrow_vs_method_write.pgy       "read-write race"
expect_reject reject_zone_method_writes_shared_field.pgy  "write-write race"

# DRF-2: collection storage reached through an aggregate.
expect_reject reject_aggregate_collection_field.pgy \
    "Parallel task cannot capture 'holder': its field 'data' reaches shared storage (Array)"
expect_reject reject_nested_aggregate_collection_field.pgy \
    "Parallel task cannot capture 'outer': its field 'inner.data' reaches shared storage (Array)"
expect_reject reject_option_collection_capture.pgy \
    "Parallel task cannot capture 'boxed': its type 'Option<Array<Int>>' reaches shared storage (Array)"
expect_reject reject_zone_shared_collection_field.pgy \
    "Parallel task cannot capture 'ledger': its field 'rows' reaches shared storage (Array)"

# An own move is the resource snapshot owner's conflict, not a write race.
expect_reject reject_own_move_vs_field_read.pgy \
    "Parallel tasks cannot consume the same resource/boundary"

# C-only platforms narrow the voice set via env; default exercises both.
BACKENDS="${PGY_PARALLEL_CAPTURE_REACH_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    expect_runs "$backend" subject_single_writer_task.pgy          $'1000\n1000'
    expect_runs "$backend" subject_read_only_method_both_tasks.pgy '21'
    expect_runs "$backend" subject_ref_borrow_both_tasks.pgy       '12'
    expect_runs "$backend" struct_scalar_fields_both_tasks.pgy     '18'
    expect_runs "$backend" zone_single_task.pgy                    $'0\n2'
done

echo "[parallel-capture-reach] method writes, handed-on references, and aggregate collection fields fail closed; exclusive and read-only sharing run on: $BACKENDS"
