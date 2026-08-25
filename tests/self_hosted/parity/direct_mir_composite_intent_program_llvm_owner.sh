#!/usr/bin/env bash
# Canonical nested intent orchestration must execute through direct self-host
# LLVM; malformed admitted facts must fail before artifact publication.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

LABEL="self-host-direct-mir-composite-intent-program-llvm"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
WORK_REL=".tmp/self_hosted/direct_mir_composite_intent_program_llvm"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="examples/composite_intent_orchestration/main.pgy"
SOURCE="$ROOT_DIR/$SOURCE_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "missing Python"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

FAILURE_REL="$WORK_REL/failure.pgy"
"$PYTHON_BIN" - "$SOURCE" "$WORK_DIR/failure.pgy" <<'PY'
from pathlib import Path
import sys
text = Path(sys.argv[1]).read_text(encoding="utf-8")
needle = "reserved = reserved + 1;"
if text.count(needle) != 1:
    raise SystemExit("canonical reserve mutation anchor drifted")
Path(sys.argv[2]).write_text(
    text.replace(needle, "reserved = reserved + 0;"),
    encoding="utf-8", newline="\n",
)
PY

MIR_REL="$WORK_REL/success.mir.json"
LLVM_REL="$WORK_REL/success.ll"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/mir.out" 2>"$WORK_DIR/mir.err" || {
        cat "$WORK_DIR/mir.out" "$WORK_DIR/mir.err" >&2
        fail "self MIR production failed"
    }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$MIR_REL" -o "$LLVM_REL") \
    >"$WORK_DIR/llvm.out" 2>"$WORK_DIR/llvm.err" || {
        cat "$WORK_DIR/llvm.out" "$WORK_DIR/llvm.err" >&2
        fail "composite direct-MIR LLVM projection failed"
    }
for anchor in \
    'define internal i1 @ReserveStock' \
    'define internal i1 @FulfillOrder' \
    'define internal i1 @ProcessOrder' \
    'i1 false, i32 9)' \
    'call void @pgy_intent_trace_materialize_export' \
    'call void @Clerk_RollbackReserve'; do
    grep -Fq "$anchor" "$WORK_DIR/success.ll" ||
        fail "LLVM omitted $anchor"
done
! grep -Fq 'scalar-program-route' "$WORK_DIR/llvm.err" ||
    fail "composite program re-entered the scalar route"

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
driver_for_pgy="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
for case_name in success failure; do
    source_rel="$SOURCE_REL"
    [[ "$case_name" == failure ]] && source_rel="$FAILURE_REL"
    public_bin="$WORK_DIR/$case_name.public$suffix"
    native_bin="$WORK_DIR/$case_name.native$suffix"
    public_arg="$(pgy_path_for_compiler "$PGY" "$public_bin")"
    native_arg="$(pgy_path_for_compiler "$PGY" "$native_bin")"
    (cd "$ROOT_DIR" && env PGY_SELF_DRIVER_BIN="$driver_for_pgy" \
        PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$source_rel" --backend=llvm \
        -o "$public_arg") >"$WORK_DIR/$case_name.public.compile.out" \
        2>"$WORK_DIR/$case_name.public.compile.err" || {
            cat "$WORK_DIR/$case_name.public.compile.err" >&2
            fail "$case_name public LLVM compile failed"
        }
    ! grep -Fq '[pipeline timing]' \
        "$WORK_DIR/$case_name.public.compile.err" ||
        fail "$case_name public route re-entered the native compiler"
    (cd "$ROOT_DIR" && "$PGY" "$source_rel" --native-pipeline \
        --backend=llvm -o "$native_arg") \
        >"$WORK_DIR/$case_name.native.compile.out" \
        2>"$WORK_DIR/$case_name.native.compile.err" || {
            cat "$WORK_DIR/$case_name.native.compile.err" >&2
            fail "$case_name native LLVM compile failed"
        }
    "$public_bin" | tr -d '\r' >"$WORK_DIR/$case_name.public.run"
    "$native_bin" | tr -d '\r' >"$WORK_DIR/$case_name.native.run"
    cmp -s "$WORK_DIR/$case_name.public.run" \
        "$WORK_DIR/$case_name.native.run" ||
        fail "$case_name public/native output differs"
done
grep -Fxq '[Intent] ProcessOrder=true' "$WORK_DIR/success.public.run" ||
    fail "canonical success did not propagate"
grep -Fxq '[Intent] ProcessOrder=false' "$WORK_DIR/failure.public.run" ||
    fail "compensated failure did not propagate"
grep -Fxq '[CanonicalClerk] reserved=-1 charged=0 shipped=0' \
    "$WORK_DIR/failure.public.run" ||
    fail "compensation did not reach the canonical subject"

"$PYTHON_BIN" - "$WORK_DIR/success.mir.json" "$WORK_DIR" <<'PY'
from copy import deepcopy
import json
from pathlib import Path
import sys

source = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
out = Path(sys.argv[2])
def routine(doc, name):
    return next(row for row in doc["routines"] if row["name"] == name)
def write(name, mutate):
    doc = deepcopy(source)
    mutate(doc)
    (out / f"{name}.mir.json").write_text(
        json.dumps(doc, separators=(",", ":")), encoding="utf-8", newline="\n"
    )
def duplicate_mode(doc):
    block = routine(doc, "ProcessOrder")["blocks"][0]["instructions"]
    mode = next(row for row in block if row["name"] == "IntentMode")
    block.insert(2, deepcopy(mode))
def remove_compensation(doc):
    block = routine(doc, "ReserveStock")["blocks"][0]["instructions"]
    block[:] = [row for row in block if not (
        row["name"] == "IntentEval" and row["arg0"] == "compensate")]
def drift_action_graph(doc):
    inst = routine(doc, "Reserve")["blocks"][0]["instructions"][0]
    inst["expr0_graph"]["nodes"][-1]["kind"] = "multiply"
def drift_zone_slot(doc):
    zone = next(row for row in doc["decls"] if row["name"] == "OrderZone")
    zone["fields"][0]["field_kind"] = "field"
for name, mutation in (
    ("duplicate-mode", duplicate_mode),
    ("missing-compensation", remove_compensation),
    ("action-graph-drift", drift_action_graph),
    ("zone-slot-drift", drift_zone_slot),
):
    write(name, mutation)
PY

for negative in duplicate-mode missing-compensation action-graph-drift zone-slot-drift; do
    artifact="$WORK_DIR/$negative.ll"
    rm -f "$artifact"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
        "$WORK_REL/$negative.mir.json" -o "$WORK_REL/$negative.ll") \
        >"$WORK_DIR/$negative.out" 2>"$WORK_DIR/$negative.err"; then
        fail "$negative unexpectedly projected"
    fi
    [[ ! -e "$artifact" ]] || fail "$negative published an artifact"
done

echo "[$LABEL] success/failure parity and four no-artifact negatives: PASS"
