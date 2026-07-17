#!/usr/bin/env bash
# AIR/MIR evidence is an anchored one-shot import.  The gate checks both the
# native negative path and the serialized binding token exposed to tooling.

set -euo pipefail

SCRIPT_DIR="${BASH_SOURCE[0]%/*}"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

AIR_TEST="${AIR_TEST_BIN:-$ROOT_DIR/bin/test_air.exe}"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ ! -x "$AIR_TEST" && -x "$ROOT_DIR/bin/test_air" ]]; then
    AIR_TEST="$ROOT_DIR/bin/test_air"
fi
if [[ ! -x "$AIR_TEST" || ! -x "$PGY" ]]; then
    echo "[air-mir-binding] missing AIR test or compiler binary" >&2
    exit 1
fi

PYTHON_BIN=""
for candidate in python3 python; do
    if command -v "$candidate" >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v "$candidate")"
        break
    fi
done
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[air-mir-binding] python3/python is required for JSON validation" >&2
    exit 1
fi

OUTPUT="$($AIR_TEST)"
[[ "$OUTPUT" == *"AIR rejects a second MIR evidence binding"* ]]
[[ "$OUTPUT" == *"AIR tests:"* && "$OUTPUT" == *"0 failed"* ]]

for pair in \
    "src/compiler/air.h|mir_evidence_binding_fingerprint" \
    "src/compiler/air_evidence_mir.c|AIR MIR evidence is already anchored" \
    "src/compiler/air_evidence_certificate.c|mir_evidence_bound" \
    "src/compiler/air_dump_json.c|mir_evidence_collection_started" \
    "src/compiler/air_validate.c|AIR MIR evidence binding is incomplete"; do
    file="${pair%%|*}"
    text="${pair#*|}"
    grep -Fq -- "$text" "$ROOT_DIR/$file"
done

WORK_DIR="$(mktemp -d "${TMPDIR:-${TEMP:-/tmp}}/pgy_air_mir_binding.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
JSON="$WORK_DIR/air.json"
"$PGY" --air-json "$ROOT_DIR/tests/cases/semantic_loop_flow/summary_hit.pgy" \
    >"$JSON"
"$PYTHON_BIN" - "$JSON" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    payload = json.load(handle)
summary = payload.get("summary", {})
if not summary.get("mir_evidence_collection_started"):
    raise SystemExit("AIR JSON did not expose MIR evidence collection anchor")
if not summary.get("mir_evidence_bound"):
    raise SystemExit("AIR JSON did not expose bound MIR evidence")
if not summary.get("mir_evidence_binding_fingerprint"):
    raise SystemExit("AIR JSON did not expose MIR binding fingerprint")
PY

echo "[air-mir-binding] AIR owns a one-shot MIR evidence anchor and serialized fingerprint"
