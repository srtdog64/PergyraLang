#!/usr/bin/env bash
set -euo pipefail

# One admitted IntentMode carrier must drive nested runtime concurrency.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-mode] $*" >&2
    exit 1
}

DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
BUILD_DIR="${PGY_SELFHOST_INTENT_PRIORITY_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_priority_nested_observability}"

pgy_require_runnable_binary_here "self-host-intent-mode" "$DRIVER" ||
    fail "prebuilt production driver is required"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"

PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}" \
PGY_SELFHOST_PREBUILT_DRIVER="$DRIVER" \
PGY_SELFHOST_INTENT_PRIORITY_BUILD_DIR="$BUILD_DIR" \
    bash "$ROOT_DIR/tests/self_hosted/parity/intent_priority_nested_observability_owner.sh"

MIR_JSON="$BUILD_DIR/priority.self.mir.json"
NATIVE_MIR_JSON="$BUILD_DIR/priority.native.mir.json"
"$PYTHON_BIN" - "$MIR_JSON" "$NATIVE_MIR_JSON" \
    "$BUILD_DIR/mode.missing.mir.json" \
    "$BUILD_DIR/mode.duplicate.mir.json" <<'PY'
import copy
import json
import sys

self_path, native_path, missing_path, duplicate_path = sys.argv[1:]

def load(path):
    with open(path, encoding="utf-8-sig") as stream:
        return json.load(stream)

def modes(document):
    result = {}
    for routine in document.get("routines", []):
        if routine.get("kind") != "intent":
            continue
        rows = [row for block in routine.get("blocks", [])
                for row in block.get("instructions", [])
                if row.get("name") == "IntentMode"]
        if len(rows) != 1:
            raise SystemExit(f"{routine.get('name')} mode is not unique")
        result[routine.get("name")] = rows[0]
    return result

self_doc = load(self_path)
native_doc = load(native_path)
expected = {"OuterPriority": "exclusive", "InnerPriority": "concurrent"}
for label, document in (("self", self_doc), ("native", native_doc)):
    rows = modes(document)
    if set(rows) != set(expected):
        raise SystemExit(f"{label} mode routine identity differs")
    for name, mode in expected.items():
        row = rows[name]
        if (row.get("kind") != "stmt" or row.get("arg0") != mode or
                row.get("arg1") != name or row.get("slot_anchor") != name or
                row.get("result") is not None or
                row.get("abi_type_name") is not None or
                row.get("expr0") is not None or row.get("expr1") is not None or
                row.get("source_type") != "AST_INTENT_DECL" or
                row.get("uses") != []):
            raise SystemExit(f"{label} {name} mode carrier shape differs")

missing = copy.deepcopy(self_doc)
for routine in missing["routines"]:
    if routine.get("name") == "InnerPriority":
        for block in routine["blocks"]:
            block["instructions"] = [row for row in block["instructions"]
                                     if row.get("name") != "IntentMode"]
with open(missing_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(missing, stream, separators=(",", ":"))
    stream.write("\n")

duplicate = copy.deepcopy(self_doc)
for routine in duplicate["routines"]:
    if routine.get("name") != "InnerPriority":
        continue
    for block in routine["blocks"]:
        for index, row in enumerate(list(block["instructions"])):
            if row.get("name") == "IntentMode":
                copied = copy.deepcopy(row)
                copied["id"] = 999998
                block["instructions"].insert(index + 1, copied)
                break
with open(duplicate_path, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(duplicate, stream, separators=(",", ":"))
    stream.write("\n")
PY

for mutation in missing duplicate; do
    set +e
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "${BUILD_DIR#"$ROOT_DIR/"}/mode.$mutation.mir.json") \
        >"$BUILD_DIR/mode.$mutation.out" \
        2>"$BUILD_DIR/mode.$mutation.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$mutation mode MIR was accepted"
done
grep -Fq 'MIR intent mode carrier is missing' \
    "$BUILD_DIR/mode.missing.out" "$BUILD_DIR/mode.missing.err" ||
    fail "missing mode lost its owned diagnostic"
grep -Fq 'MIR intent mode carrier is duplicated' \
    "$BUILD_DIR/mode.duplicate.out" "$BUILD_DIR/mode.duplicate.err" ||
    fail "duplicate mode lost its owned diagnostic"

grep -Fq 'pgy_intent_enter_export("OuterPriority", __intent_subjects, 1, false, 1);' \
    "$BUILD_DIR/priority.self.c" || fail "outer exclusive mode did not reach self C"
grep -Fq 'pgy_intent_enter_export("InnerPriority", __intent_subjects, 1, true, requested);' \
    "$BUILD_DIR/priority.self.c" || fail "inner concurrent mode did not reach self C"
for line in \
    'active.0.concurrent=false' \
    'active.1.concurrent=true'; do
    grep -Fxq "$line" "$BUILD_DIR/priority.public.run" ||
        fail "missing runtime mode observation: $line"
done

echo "[self-host-intent-mode] MIR carriage + nested runtime parity: PASS"
