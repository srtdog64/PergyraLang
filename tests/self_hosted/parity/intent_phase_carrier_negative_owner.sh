#!/usr/bin/env bash
set -euo pipefail

# The admitted MIR boundary owns phase vocabulary, exact step attachment,
# cardinality, on-only result/type, graph presence, and compensate order.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-phase-carrier] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-phase-carrier" "$PGY" \
    || fail "PGY_BIN is not runnable"
PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"

FIXTURE_REL="tests/self_hosted/parity/fixture/intent_phase_carrier_admission.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_PHASE_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_phase_carrier}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-phase-carrier" "$DRIVER" \
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
SELF_C="$BUILD_DIR/self.c"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    >"$SELF_MIR" 2>"$BUILD_DIR/self.mir.err") \
    || { cat "$SELF_MIR" "$BUILD_DIR/self.mir.err" >&2; fail "self MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "${SELF_MIR#"$ROOT_DIR/"}" \
    >"$SELF_C" 2>"$BUILD_DIR/self.c.err") \
    || { cat "$SELF_C" "$BUILD_DIR/self.c.err" >&2; fail "baseline phase MIR was rejected"; }
grep -Eq '^#include|^typedef' "$SELF_C" \
    || fail "baseline phase MIR emitted no C artifact"

"$PYTHON_BIN" - "$SELF_MIR" "$BUILD_DIR" <<'PY'
import copy
import json
from pathlib import Path
import sys

base = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))

def rows(document):
    intent = next(row for row in document["routines"]
                  if row["kind"] == "intent" and row["name"] == "RunIntent")
    return intent["blocks"][0]["instructions"]

def phase_rows(document, name, phase):
    return [row for row in rows(document)
            if row.get("name") == name and row.get("arg0") == phase]

checks = phase_rows(base, "IntentCheck", "guard")
posts = phase_rows(base, "IntentCheck", "post")
expects = phase_rows(base, "IntentCheck", "expect")
ons = phase_rows(base, "IntentEval", "on")
compensates = phase_rows(base, "IntentEval", "compensate")
assert len(checks) == len(posts) == len(expects) == len(ons) == 1
assert [row["expr0"] for row in compensates] == [
    "IntentRunSettled(outcome)", "IntentRunAccepted(outcome)"
]
assert ons[0]["result"] == "outcome"
assert ons[0]["abi_type_name"] == "IntentRunOutcome"
assert all(row["result"] is None and row["abi_type_name"] is None
           for row in checks + posts + expects + compensates)
assert all(row["arg1"] == "Run" and row["slot_anchor"] == "Run"
           for row in checks + posts + expects + ons + compensates)

mutations = {}

document = copy.deepcopy(base)
phase_rows(document, "IntentCheck", "guard")[0]["arg0"] = "unknown"
mutations["unknown-phase"] = document

document = copy.deepcopy(base)
orphan = phase_rows(document, "IntentCheck", "post")[0]
orphan["arg1"] = "Foreign"
orphan["slot_anchor"] = "Foreign"
mutations["orphan-step"] = document

document = copy.deepcopy(base)
phase_rows(document, "IntentEval", "compensate")[0]["slot_anchor"] = "Foreign"
mutations["wrong-slot"] = document

document = copy.deepcopy(base)
bad = phase_rows(document, "IntentEval", "compensate")[0]
bad["result"] = "illegal"
bad["abi_type_name"] = "Bool"
mutations["compensate-result-type"] = document

document = copy.deepcopy(base)
bad = phase_rows(document, "IntentCheck", "guard")[0]
bad["result"] = "illegal"
bad["abi_type_name"] = "Bool"
mutations["check-result-type"] = document

document = copy.deepcopy(base)
phase_rows(document, "IntentEval", "on")[0]["abi_type_name"] = None
mutations["on-result-type-asymmetry"] = document

document = copy.deepcopy(base)
phase_rows(document, "IntentEval", "compensate")[0]["expr0_graph"] = None
mutations["missing-graph"] = document

document = copy.deepcopy(base)
phase_rows(document, "IntentEval", "compensate")[0]["name"] = "IntentCheck"
phase_rows(document, "IntentCheck", "compensate")[0]["arg0"] = "post"
mutations["duplicate-post"] = document

document = copy.deepcopy(base)
phase_rows(document, "IntentEval", "compensate")[0]["arg0"] = "on"
mutations["duplicate-on"] = document

for name, document in mutations.items():
    (Path(sys.argv[2]) / f"negative-{name}.mir.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )
PY

expected_diagnostic() {
    case "$1" in
        negative-unknown-phase)
            echo 'MIR intent phase carrier names an unknown phase' ;;
        negative-orphan-step|negative-wrong-slot)
            echo 'MIR intent phase carrier step identity is invalid' ;;
        negative-compensate-result-type)
            echo 'MIR intent compensate result/type shape is invalid' ;;
        negative-check-result-type)
            echo 'MIR intent check result/type shape is invalid' ;;
        negative-on-result-type-asymmetry)
            echo 'MIR intent on result/type shape is invalid' ;;
        negative-missing-graph)
            echo 'MIR intent phase carrier expression graph is missing' ;;
        negative-duplicate-post)
            echo 'MIR intent post phase cardinality is invalid' ;;
        negative-duplicate-on)
            echo 'MIR intent on phase cardinality is invalid' ;;
        *) return 1 ;;
    esac
}

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
    diagnostic="$(expected_diagnostic "$name")" \
        || fail "$name has no expected diagnostic contract"
    grep -Fq "$diagnostic" "$negative.out" "$negative.err" \
        || fail "$name rejected through the wrong boundary"
done

echo "[self-host-intent-phase-carrier] phase order + admitted MIR negatives: PASS"
