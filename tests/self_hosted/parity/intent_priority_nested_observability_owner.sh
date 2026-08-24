#!/usr/bin/env bash
set -euo pipefail

# DIR-owned priority must survive self MIR and MIR-lower into the runtime
# admission call. Native C remains the independent execution oracle.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-priority] $*" >&2
    exit 1
}

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
BUILD_DIR="${PGY_SELFHOST_INTENT_PRIORITY_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_priority_nested_observability}"
SOURCE_REL="tests/self_hosted/parity/fixture/intent_priority_nested_observability.pgy"
MIR_JSON="$BUILD_DIR/priority.self.mir.json"
NATIVE_MIR_JSON="$BUILD_DIR/priority.native.mir.json"

pgy_require_runnable_binary_here "self-host-intent-priority" "$PGY" ||
    fail "PGY_BIN is not runnable"
pgy_require_runnable_binary_here "self-host-intent-priority" "$DRIVER" ||
    fail "prebuilt production driver is required"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
mkdir -p "$BUILD_DIR"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL") \
    >"$MIR_JSON" 2>"$BUILD_DIR/priority.self.mir.err" ||
    { cat "$BUILD_DIR/priority.self.mir.err" >&2; fail "self MIR production failed"; }
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$SOURCE_REL") \
    >"$NATIVE_MIR_JSON" 2>"$BUILD_DIR/priority.native.mir.err" ||
    { cat "$BUILD_DIR/priority.native.mir.err" >&2; fail "native MIR oracle failed"; }

"$PYTHON_BIN" - "$MIR_JSON" "$NATIVE_MIR_JSON" \
    "$BUILD_DIR/priority.missing-graph.mir.json" \
    "$BUILD_DIR/priority.duplicate.mir.json" <<'PY'
import copy
import json
import sys

self_path, native_path, missing_path, duplicate_path = sys.argv[1:]

def load(path):
    with open(path, encoding="utf-8-sig") as stream:
        return json.load(stream)

def priorities(document):
    result = {}
    for routine in document.get("routines", []):
        if routine.get("kind") != "intent":
            continue
        rows = []
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if (instruction.get("name") == "IntentEval" and
                        instruction.get("arg0") == "priority"):
                    rows.append(instruction)
        if rows:
            if len(rows) != 1:
                raise SystemExit(f"{routine.get('name')} priority is not unique")
            result[routine.get("name")] = rows[0]
    return result

self_doc = load(self_path)
native_doc = load(native_path)
self_rows = priorities(self_doc)
native_rows = priorities(native_doc)
expected = {"OuterPriority": "1", "InnerPriority": "requested"}
if set(self_rows) != set(expected) or set(native_rows) != set(expected):
    raise SystemExit("native/self priority routine identity differs")
for name, expression in expected.items():
    for label, row in (("self", self_rows[name]), ("native", native_rows[name])):
        if (row.get("arg1") != name or row.get("slot_anchor") != name or
                row.get("expr0") != expression or row.get("result") is not None or
                not isinstance(row.get("expr0_graph"), dict)):
            raise SystemExit(f"{label} {name} priority carrier shape differs")

missing = copy.deepcopy(self_doc)
missing_rows = priorities(missing)
missing_rows["InnerPriority"].pop("expr0_graph", None)
with open(missing_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(missing, stream, separators=(",", ":"))
    stream.write("\n")

duplicate = copy.deepcopy(self_doc)
for routine in duplicate["routines"]:
    if routine.get("name") != "InnerPriority":
        continue
    for block in routine["blocks"]:
        for index, row in enumerate(list(block["instructions"])):
            if row.get("name") == "IntentEval" and row.get("arg0") == "priority":
                copied = copy.deepcopy(row)
                copied["id"] = 999999
                block["instructions"].insert(index + 1, copied)
                break
with open(duplicate_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(duplicate, stream, separators=(",", ":"))
    stream.write("\n")
PY

for mutation in missing-graph duplicate; do
    set +e
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "${BUILD_DIR#"$ROOT_DIR/"}/priority.$mutation.mir.json") \
        >"$BUILD_DIR/$mutation.out" 2>"$BUILD_DIR/$mutation.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$mutation priority MIR was accepted"
done
grep -Fq 'MIR intent priority carrier shape is invalid' \
    "$BUILD_DIR/missing-graph.out" "$BUILD_DIR/missing-graph.err" ||
    fail "missing priority graph lost its owned diagnostic"
grep -Fq 'MIR intent priority carrier is duplicated' \
    "$BUILD_DIR/duplicate.out" "$BUILD_DIR/duplicate.err" ||
    fail "duplicate priority lost its owned diagnostic"

self_c="$BUILD_DIR/priority.self.c"
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified) \
    >"$self_c" 2>"$BUILD_DIR/priority.self.c.err" ||
    { cat "$BUILD_DIR/priority.self.c.err" >&2; fail "self C emission failed"; }
grep -Eq 'pgy_intent_enter_export\("OuterPriority", __intent_subjects, 1, (true|false), 1\);' \
    "$self_c" || fail "outer literal priority did not reach self C"
grep -Eq 'pgy_intent_enter_export\("InnerPriority", __intent_subjects, 1, (true|false), requested\);' \
    "$self_c" || fail "inner value priority did not reach self C"

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
driver_for_pgy="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
public_exe="$BUILD_DIR/priority.public$suffix"
native_exe="$BUILD_DIR/priority.native$suffix"
(cd "$ROOT_DIR" && env PGY_SELF_DRIVER_BIN="$driver_for_pgy" "$PGY" \
    "$SOURCE_REL" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$public_exe")") \
    >"$BUILD_DIR/priority.public.compile.out" \
    2>"$BUILD_DIR/priority.public.compile.err" ||
    { cat "$BUILD_DIR/priority.public.compile.err" >&2; fail "public compile failed"; }
(cd "$ROOT_DIR" && "$PGY" "$SOURCE_REL" --native-pipeline --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$native_exe")") \
    >"$BUILD_DIR/priority.native.compile.out" \
    2>"$BUILD_DIR/priority.native.compile.err" ||
    { cat "$BUILD_DIR/priority.native.compile.err" >&2; fail "native compile failed"; }
"$public_exe" | tr -d '\r' >"$BUILD_DIR/priority.public.run"
"$native_exe" | tr -d '\r' >"$BUILD_DIR/priority.native.run"
cmp -s "$BUILD_DIR/priority.public.run" "$BUILD_DIR/priority.native.run" ||
    fail "public/native priority output differs"
for line in \
    'active.count=2' \
    'active.0.name=OuterPriority' \
    'active.0.priority=1' \
    'active.1.name=InnerPriority' \
    'active.1.priority=9' \
    'outer.ok=true' \
    'captures=1'; do
    grep -Fxq "$line" "$BUILD_DIR/priority.public.run" ||
        fail "missing runtime observation: $line"
done

echo "[self-host-intent-priority] MIR carriage + nested runtime parity: PASS"
