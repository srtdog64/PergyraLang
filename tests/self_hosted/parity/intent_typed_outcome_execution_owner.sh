#!/usr/bin/env bash
set -euo pipefail

# REACHABLE executable rung: one subject.action evaluation returns an exact
# enum<tobject> outcome. The intent binds that value once and consumes it from
# expect in native C, native LLVM, and the admitted self-host MIR -> C path.
# Multi-step compensation remains a later rung.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-typed-outcome] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-typed-outcome" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE_REL="tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_TYPED_OUTCOME_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_typed_outcome_gate}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-typed-outcome" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { tail -c 65536 "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

SELF_MIR="$BUILD_DIR/self.mir.json"
SELF_FROM_MIR_C="$BUILD_DIR/self.from-mir.c"
SELF_DIRECT_C="$BUILD_DIR/self.direct.c"
SELF_EXE="$BUILD_DIR/self.exe"
NATIVE_C_EXE="$BUILD_DIR/native.c.exe"
NATIVE_LLVM_EXE="$BUILD_DIR/native.llvm.exe"
NATIVE_EMITTED_C="$BUILD_DIR/native.emitted.c"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    >"$SELF_MIR" 2>"$BUILD_DIR/self.mir.err") \
    || { cat "$SELF_MIR" "$BUILD_DIR/self.mir.err" >&2; fail "self MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "${SELF_MIR#"$ROOT_DIR/"}" \
    >"$SELF_FROM_MIR_C" 2>"$BUILD_DIR/self.from-mir.err") \
    || { cat "$SELF_FROM_MIR_C" "$BUILD_DIR/self.from-mir.err" >&2; fail "admitted MIR C emission failed"; }
(cd "$ROOT_DIR" && "$DRIVER" "$FIXTURE_REL" --emit-c-verified \
    >"$SELF_DIRECT_C" 2>"$BUILD_DIR/self.direct.err") \
    || { cat "$SELF_DIRECT_C" "$BUILD_DIR/self.direct.err" >&2; fail "direct source C emission failed"; }
cmp -s "$SELF_FROM_MIR_C" "$SELF_DIRECT_C" \
    || fail "direct source entrypoint bypassed admitted outcome MIR"

"$PYTHON_BIN" - "$SELF_MIR" <<'PY'
import json
from pathlib import Path
import sys

document = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
intent = next(row for row in document["routines"]
              if row["kind"] == "intent" and row["name"] == "RunIntent")
rows = [instruction for block in intent["blocks"]
        for instruction in block["instructions"]]
bindings = [row for row in rows if row.get("name") == "IntentOutcomeBinding"]
evaluations = [row for row in rows
               if row.get("name") == "IntentEval" and row.get("arg0") == "on"]
assert len(bindings) == 1, bindings
assert len(evaluations) == 1, evaluations
binding = bindings[0]
evaluation = evaluations[0]
assert binding["result"] == "outcome"
assert binding["slot_anchor"] == "outcome"
assert binding["abi_type_name"] == "IntentRunOutcome"
assert binding["source_type"] == "AST_INTENT_STEP"
assert binding["arg0"].isdigit() and int(binding["arg0"]) > 0
assert binding["arg1"] == "Run"
assert evaluation["result"] == binding["result"]
assert evaluation["abi_type_name"] == binding["abi_type_name"]
PY

grep -Eq '^[[:space:]]*IntentRunOutcome[[:space:]]+const[[:space:]]+outcome[[:space:]]*=' "$SELF_DIRECT_C" \
    || fail "self C lost the exact typed immutable outcome binding"
grep -Eq 'if[[:space:]]+\(!\([[:space:]]*IntentRunAccepted\([[:space:]]*outcome[[:space:]]*\)[[:space:]]*\)\)' "$SELF_DIRECT_C" \
    || fail "self C expect did not consume the bound outcome"

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$SELF_DIRECT_C" -o "$SELF_EXE"
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --backend=c -o "$NATIVE_C_EXE" \
    >"$BUILD_DIR/native.c.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.c.compile.log" >&2; fail "native C compile failed"; }
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --backend=c --emit-c \
    -o "$(pgy_path_for_compiler "$PGY" "$NATIVE_EMITTED_C")" \
    >"$BUILD_DIR/native.c.emit.log" 2>&1) \
    || { cat "$BUILD_DIR/native.c.emit.log" >&2; fail "native C emission failed"; }
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --native-pipeline --backend=llvm -o "$NATIVE_LLVM_EXE" \
    >"$BUILD_DIR/native.llvm.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.llvm.compile.log" >&2; fail "native LLVM compile failed"; }

"$PYTHON_BIN" - "$NATIVE_EMITTED_C" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
start = text.rindex("bool IntentRunAccepted(")
end = text.index("\n}\n\n", start)
body = text[start:end]
for variant in ("IntentRunCommitted", "IntentRunRejected"):
    tag = f".tag == IntentRunOutcome_TAG_{variant}"
    payload = f".{variant}._0"
    assert body.count(tag) == 1, (variant, body)
    assert body.count(payload) == 1, (variant, body)
    assert body.index(tag) < body.index(payload), (variant, body)
PY

"$SELF_EXE" | tr -d '\r' >"$BUILD_DIR/self.run"
"$NATIVE_C_EXE" | tr -d '\r' >"$BUILD_DIR/native.c.run"
"$NATIVE_LLVM_EXE" | tr -d '\r' >"$BUILD_DIR/native.llvm.run"
printf '%s\n' \
    'accepted=true' \
    'calls=1' \
    'rejected=false' \
    'calls=2' >"$BUILD_DIR/expected.run"
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/self.run" \
    || { cat "$BUILD_DIR/self.run" >&2; fail "self runtime output drifted"; }
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.c.run" \
    || fail "self/native C outcome execution differs"
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.llvm.run" \
    || fail "self/native LLVM outcome execution differs"

"$PYTHON_BIN" - "$SELF_MIR" "$BUILD_DIR" <<'PY'
import copy
import json
from pathlib import Path
import sys

base = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))

def intent_rows(document):
    intent = next(row for row in document["routines"]
                  if row["kind"] == "intent" and row["name"] == "RunIntent")
    return intent["blocks"][0]["instructions"]

def one(rows, name):
    matches = [row for row in rows if row.get("name") == name]
    assert len(matches) == 1, (name, matches)
    return matches[0]

mutations = {}

document = copy.deepcopy(base)
rows = intent_rows(document)
rows.remove(one(rows, "IntentOutcomeBinding"))
mutations["missing-binding"] = document

document = copy.deepcopy(base)
one(intent_rows(document), "IntentOutcomeBinding")["result"] = "foreign"
mutations["binding-result"] = document

document = copy.deepcopy(base)
one(intent_rows(document), "IntentOutcomeBinding")["abi_type_name"] = "Bool"
mutations["binding-type"] = document

document = copy.deepcopy(base)
one(intent_rows(document), "IntentOutcomeBinding")["arg0"] = "0"
mutations["action-identity"] = document

document = copy.deepcopy(base)
rows = intent_rows(document)
rows.append(copy.deepcopy(one(rows, "IntentOutcomeBinding")))
mutations["duplicate-binding"] = document

document = copy.deepcopy(base)
one(intent_rows(document), "IntentEval")["result"] = "foreign"
mutations["eval-result"] = document

for name, document in mutations.items():
    (Path(sys.argv[2]) / f"negative-{name}.mir.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )
PY

for negative in "$BUILD_DIR"/negative-*.mir.json; do
    name="$(basename "$negative" .mir.json)"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "${negative#"$ROOT_DIR/"}" >"$negative.out" 2>"$negative.err"); then
        fail "$name was accepted"
    fi
    if grep -Eq '^#include|^typedef|bool RunIntent\(' \
        "$negative.out" "$negative.err"; then
        fail "$name emitted a partial C artifact before rejection"
    fi
done

echo "[self-host-intent-typed-outcome] enum<tobject> binding + exact-once parity + MIR negatives: PASS"
