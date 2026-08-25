#!/usr/bin/env bash
# One source-owned subject/action/zone/intent program reaches the public
# self-host LLVM route. Native LLVM remains the independent execution oracle;
# malformed admitted facts must fail without publishing an artifact.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

LABEL="self-host-direct-mir-legacy-intent-program-llvm"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
WORK_REL=".tmp/self_hosted/direct_mir_legacy_intent_program_llvm"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/parity/fixture/direct_mir_legacy_intent_program_llvm.pgy"
SELF_MIR_REL="$WORK_REL/self.mir.json"
SELF_MIR="$ROOT_DIR/$SELF_MIR_REL"
LLVM_REL="$WORK_REL/self.ll"
LLVM="$ROOT_DIR/$LLVM_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "missing Python"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$SELF_MIR_REL") \
    >"$WORK_DIR/self-mir.out" 2>"$WORK_DIR/self-mir.err" || {
        cat "$WORK_DIR/self-mir.out" "$WORK_DIR/self-mir.err" >&2
        fail "self MIR production failed"
    }
for fact in \
    '"kind":"subject","nominal_kind":"subject","name":"Counter"' \
    '"kind":"zone","nominal_kind":"zone","name":"CounterZone"' \
    '"field_kind":"subject_slot"' \
    '"name":"IncrementOnce","kind":"intent"' \
    '"name":"IntentMode"' \
    '"arg0":"exclusive","arg1":"IncrementOnce"' \
    '"name":"IntentEval"' \
    '"arg0":"priority","arg1":"IncrementOnce"' \
    '"expr0":"4"'; do
    grep -Fq "$fact" "$SELF_MIR" || fail "MIR omitted $fact"
done

(cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
    "$SELF_MIR_REL" -o "$LLVM_REL") \
    >"$WORK_DIR/self-llvm.out" 2>"$WORK_DIR/self-llvm.err" || {
        cat "$WORK_DIR/self-llvm.out" "$WORK_DIR/self-llvm.err" >&2
        fail "self direct-MIR LLVM projection failed"
    }
grep -Fq 'atomicrmw add ptr %generation, i32 1 release' "$LLVM" ||
    fail "zone generation synchronization is missing"
grep -Fq 'call i32 @pgy_intent_enter_export(ptr @.pgy.intent.name, ptr %subject.slot, i32 1, i1 false, i32 4)' "$LLVM" ||
    fail "mode/priority did not reach the runtime admission call"
grep -Fq 'call i1 @IncrementOnce(ptr %zone, ptr %subject)' "$LLVM" ||
    fail "Main did not reach the intent routine"
[[ "$(grep -Fc 'call void @pgy_mir_cleanup_op_export' "$LLVM")" -eq 9 ]] ||
    fail "rollback/invalidation cleanup emission drifted"

suffix=""
if [[ "$PGY" == *.exe ]]; then suffix=".exe"; fi
SELF_BIN="$WORK_DIR/self$suffix"
NATIVE_BIN="$WORK_DIR/native$suffix"
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE_REL" --backend=llvm \
    -o "$WORK_REL/self$suffix") \
    >"$WORK_DIR/self-compile.out" 2>"$WORK_DIR/self-compile.err" || {
        cat "$WORK_DIR/self-compile.out" "$WORK_DIR/self-compile.err" >&2
        fail "public self-host LLVM compile failed"
    }
! grep -Fq '[pipeline timing]' "$WORK_DIR/self-compile.err" ||
    fail "public self-host LLVM route re-entered the native compiler"
(cd "$ROOT_DIR" && "$PGY" "$SOURCE_REL" --native-pipeline --backend=llvm \
    -o "$WORK_REL/native$suffix") \
    >"$WORK_DIR/native-compile.out" 2>"$WORK_DIR/native-compile.err" || {
        cat "$WORK_DIR/native-compile.out" "$WORK_DIR/native-compile.err" >&2
        fail "native LLVM oracle compile failed"
    }
"$SELF_BIN" | tr -d '\r' >"$WORK_DIR/self.run"
"$NATIVE_BIN" | tr -d '\r' >"$WORK_DIR/native.run"
printf 'true\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/self.run" ||
    fail "public self-host LLVM runtime result drifted"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/native.run" ||
    fail "native LLVM oracle runtime result drifted"

"$PYTHON_BIN" - "$SELF_MIR" "$WORK_DIR" <<'PY'
import copy
import json
import os
import sys

source, output_dir = sys.argv[1:]
with open(source, encoding="utf-8-sig") as stream:
    baseline = json.load(stream)

def intent_rows(document):
    routine = next(row for row in document["routines"]
                   if row.get("kind") == "intent")
    return [instruction for block in routine["blocks"]
            for instruction in block["instructions"]]

def write(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    path = os.path.join(output_dir, name + ".mir.json")
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")

def duplicate_mode(document):
    routine = next(row for row in document["routines"]
                   if row.get("kind") == "intent")
    block = routine["blocks"][0]
    row = next(item for item in block["instructions"]
               if item.get("name") == "IntentMode")
    duplicate = copy.deepcopy(row)
    duplicate["id"] = 999991
    block["instructions"].insert(2, duplicate)

def invalid_priority(document):
    row = next(item for item in intent_rows(document)
               if item.get("name") == "IntentEval" and
               item.get("arg0") == "priority")
    row["expr0"] = "not-an-int"
    row["expr0_graph"]["nodes"][0]["text"] = "not-an-int"

def drift_zone_field(document):
    zone = next(row for row in document["decls"]
                if row.get("kind") == "zone")
    zone["fields"][0]["name"] = "other_counter"

def drift_action_target(document):
    method = next(row for row in document["routines"]
                  if row.get("kind") == "method")
    method["blocks"][0]["instructions"][0]["expr1"] = "other_value"

def remove_invalidation_cleanup(document):
    routine = next(row for row in document["routines"]
                   if row.get("kind") == "intent")
    block = routine["blocks"][3]
    block["instructions"] = [row for row in block["instructions"]
                             if row.get("name") != "DetachInvalidation" or
                             row.get("arg1") != "increment"]

for name, mutation in (
    ("duplicate-mode", duplicate_mode),
    ("invalid-priority", invalid_priority),
    ("zone-field-drift", drift_zone_field),
    ("action-target-drift", drift_action_target),
    ("missing-invalidation-cleanup", remove_invalidation_cleanup),
):
    write(name, mutation)
PY

for mutation in duplicate-mode invalid-priority zone-field-drift \
    action-target-drift missing-invalidation-cleanup; do
    output_rel="$WORK_REL/$mutation.ll"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    set +e
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json-backend=llvm \
        "$WORK_REL/$mutation.mir.json" -o "$output_rel") \
        >"$WORK_DIR/$mutation.out" 2>"$WORK_DIR/$mutation.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "LLVM accepted $mutation"
    [[ ! -e "$output" ]] || fail "LLVM published an artifact for $mutation"
done

echo "[$LABEL] public native/self LLVM parity + five no-artifact negatives: PASS"
