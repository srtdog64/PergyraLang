#!/usr/bin/env bash
# Mixed function/method/intent execution must stay on the sealed direct-MIR
# nested-intent route and publish no LLVM for malformed owned facts.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

LABEL="self-host-direct-mir-nested-intent-program-llvm"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
WORK_REL=".tmp/self_hosted/direct_mir_nested_intent_program_llvm"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "missing Python"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

MIR_REL="$WORK_REL/success.mir.json"
LLVM_REL="$WORK_REL/success.ll"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/mir.out" \
    2>"$WORK_DIR/mir.err" || {
        cat "$WORK_DIR/mir.err" >&2
        fail "self MIR production failed"
    }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$MIR_REL" -o "$LLVM_REL") >"$WORK_DIR/llvm.out" \
    2>"$WORK_DIR/llvm.err" || {
        cat "$WORK_DIR/llvm.err" >&2
        fail "nested intent direct-MIR LLVM projection failed"
    }
for anchor in \
    'define internal i1 @PriorityProbe_Capture(ptr %self)' \
    'define internal i1 @InnerPriority(ptr %pgy.param.0, ptr %pgy.param.1, i32 %pgy.param.2)' \
    'i1 true, i32 %pgy.param.2)' \
    'define internal i1 @OuterPriority(ptr %pgy.param.0, ptr %pgy.param.1)' \
    'i1 false, i32 1)' \
    'call i1 @InnerPriority(ptr %pgy.param.0, ptr %pgy.param.1, i32 9)' \
    'call i1 @PriorityProbe_Capture(ptr %zone.slot)'; do
    grep -Fq "$anchor" "$WORK_DIR/success.ll" ||
        fail "LLVM omitted $anchor"
done
! grep -Fq 'scalar-program-route' "$WORK_DIR/llvm.err" ||
    fail "nested intent program re-entered the scalar route"

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
driver_for_pgy="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
public_bin="$WORK_DIR/priority.public$suffix"
native_bin="$WORK_DIR/priority.native$suffix"
(cd "$ROOT_DIR" && env PGY_SELF_DRIVER_BIN="$driver_for_pgy" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE_REL" --backend=llvm \
    -o "$(pgy_path_for_compiler "$PGY" "$public_bin")") \
    >"$WORK_DIR/public.compile.out" 2>"$WORK_DIR/public.compile.err" || {
        cat "$WORK_DIR/public.compile.err" >&2
        fail "public self-host LLVM compile failed"
    }
! grep -Fq '[pipeline timing]' "$WORK_DIR/public.compile.err" ||
    fail "public route re-entered the native compiler"
(cd "$ROOT_DIR" && "$PGY" "$SOURCE_REL" --native-pipeline --backend=llvm \
    -o "$(pgy_path_for_compiler "$PGY" "$native_bin")") \
    >"$WORK_DIR/native.compile.out" 2>"$WORK_DIR/native.compile.err" || {
        cat "$WORK_DIR/native.compile.err" >&2
        fail "native LLVM compile failed"
    }
"$public_bin" | tr -d '\r' >"$WORK_DIR/public.run"
"$native_bin" | tr -d '\r' >"$WORK_DIR/native.run"
cat >"$WORK_DIR/expected.run" <<'EXPECTED'
active.count=2
active.0.name=OuterPriority
active.0.priority=1
active.0.concurrent=false
active.1.name=InnerPriority
active.1.priority=9
active.1.concurrent=true
outer.ok=true
captures=1
EXPECTED
cmp -s "$WORK_DIR/public.run" "$WORK_DIR/expected.run" || fail "public nested intent output differs from the exact contract"
cmp -s "$WORK_DIR/native.run" "$WORK_DIR/expected.run" || fail "native nested intent output differs from the exact contract"
"$PYTHON_BIN" - "$WORK_DIR/success.mir.json" "$WORK_DIR" <<'PY'
from copy import deepcopy
import json
from pathlib import Path
import sys
source = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8-sig"))
out = Path(sys.argv[2])
def routine(doc, name):
    return next(row for row in doc["routines"] if row["name"] == name)
def write(name, mutate):
    doc = deepcopy(source)
    mutate(doc)
    (out / f"{name}.mir.json").write_text(
        json.dumps(doc, separators=(",", ":")), encoding="utf-8", newline="\n")
def missing_inner_priority(doc):
    block = routine(doc, "InnerPriority")["blocks"][0]["instructions"]
    block[:] = [row for row in block if not (
        row["name"] == "IntentEval" and row["arg0"] == "priority")]
def drift_priority_graph(doc):
    block = routine(doc, "InnerPriority")["blocks"][0]["instructions"]
    row = next(row for row in block if
        row["name"] == "IntentEval" and row["arg0"] == "priority")
    row["expr0_graph"]["nodes"][0]["text"] = "wrongPriority"
def duplicate_source_identity(doc):
    routine(doc, "InnerPriority")["source_syntax_id"] = routine(
        doc, "OuterPriority")["source_syntax_id"]
def crosswire_method_owner(doc):
    routine(doc, "Capture")["owner"] = "WrongProbe"
def zero_receiver_source_identity(doc):
    routine(doc, "Capture")["params"][0]["source_syntax_id"] = 0
def crosswire_action_name(doc):
    block = routine(doc, "OuterPriority")["blocks"][0]["instructions"]
    row = next(row for row in block if
        row["name"] == "IntentEval" and row["arg0"] == "on")
    row["expr0"] = row["expr0"].replace("InnerPriority", "OuterPriority")
    for node in row["expr0_graph"]["nodes"]:
        node["text"] = node["text"].replace("InnerPriority", "OuterPriority")
        if node["call_target_name"] == "InnerPriority": node["call_target_name"] = "OuterPriority"
for name, mutation in (
    ("missing-inner-priority", missing_inner_priority),
    ("priority-graph-drift", drift_priority_graph),
    ("duplicate-source-identity", duplicate_source_identity),
    ("method-owner-crosswire", crosswire_method_owner),
    ("receiver-source-identity-zero", zero_receiver_source_identity),
    ("action-name-crosswire", crosswire_action_name),
):
    write(name, mutation)
PY
for negative in missing-inner-priority priority-graph-drift duplicate-source-identity \
    method-owner-crosswire receiver-source-identity-zero action-name-crosswire; do
    artifact="$WORK_DIR/$negative.ll"
    rm -f "$artifact"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
        "$WORK_REL/$negative.mir.json" -o "$WORK_REL/$negative.ll") \
        >"$WORK_DIR/$negative.out" 2>"$WORK_DIR/$negative.err"; then
        fail "$negative unexpectedly projected"
    fi
    [[ ! -e "$artifact" ]] || fail "$negative published an artifact"
    case "$negative" in
        missing-inner-priority) receipt="direct MIR nested intent carrier spine is invalid" ;;
        priority-graph-drift) receipt="direct MIR nested intent priority binding is missing" ;;
        duplicate-source-identity) receipt="MIR machine-layer facts are missing or invalid" ;;
        method-owner-crosswire) receipt="MIR machine-layer facts are missing or invalid" ;;
        receiver-source-identity-zero) receipt="direct MIR nested intent implicit receiver is invalid" ;;
        action-name-crosswire) receipt="direct MIR nested intent callable identity is stale" ;;
    esac
    grep -Fq "$receipt" "$WORK_DIR/$negative.out" "$WORK_DIR/$negative.err" ||
        fail "$negative did not fail at its owned boundary"
    ! grep -Fq 'scalar-program-route' "$WORK_DIR/$negative.out" \
        "$WORK_DIR/$negative.err" || fail "$negative fell through to scalar"
done
source "$ROOT_DIR/tests/self_hosted/parity/direct_mir_nested_intent_program_c_owner.sh"
