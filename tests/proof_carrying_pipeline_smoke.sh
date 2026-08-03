#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[proof-carrying-pipeline] $*" >&2
    exit 1
}

require_text() {
    local rel="$1"
    local text="$2"
    grep -Fq -- "$text" "$ROOT_DIR/$rel" ||
        fail "$rel missing text: $text"
}

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python is required for certificate envelope validation"
    fi
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_EXPLICIT=1
fi
PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[proof-carrying-pipeline] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    fail "missing compiler binary: $PGY"
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[proof-carrying-pipeline] SKIP compiler binary is not runnable here: $PGY"
        exit 0
    fi
    pgy_require_runnable_binary_here "proof-carrying-pipeline" "$PGY"
fi

require_text "docs/semantics/17_proof_carrying_pipeline.md" "pgy.proof-carrying-ir.v1"
require_text "docs/semantics/17_proof_carrying_pipeline.md" "valid certificate + valid owner payloads"
require_text "docs/semantics/17_proof_carrying_pipeline.md" "negative rejection when a required certificate fact is removed"
require_text "docs/semantics/pass_contract_manifest.md" "proof_certificate_pipeline"
require_text "docs/semantics/16_language_contract_golden_spine.md" "Proof-carrying IR"

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_proof_cert.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

SOURCE="$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy"
AIR_JSON="$WORK_DIR/air.json"
MIR_JSON="$WORK_DIR/mir.json"
CERT_JSON="$WORK_DIR/certificate.json"

"$PGY" --air-json "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend=c >"$AIR_JSON" 2>"$WORK_DIR/air.err"
"$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend=c \
    >"$MIR_JSON" 2>"$WORK_DIR/mir.err"

"$PYTHON_BIN" - "$SOURCE" "$AIR_JSON" "$MIR_JSON" "$CERT_JSON" <<'PY'
import copy
import hashlib
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
air_path = pathlib.Path(sys.argv[2])
mir_path = pathlib.Path(sys.argv[3])
cert_path = pathlib.Path(sys.argv[4])

AIR_REQUIRED = {
    "hir_cfg",
    "rir_boundary",
    "rir_authority",
    "dag_metadata",
    "mir_cleanup",
    "mir_terminator",
}
MIR_REQUIRED = {
    "cfg_blocks",
    "source_shape",
    "expr0",
    "cleanup",
}
REQUIRED_LAYERS = {"air", "dag", "mir", "abi", "backend"}

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def require(condition, message, errors):
    if not condition:
        errors.append(message)

def validate_certificate(cert, errors):
    require(cert.get("schema") == "pgy.proof-carrying-ir.v1",
            "wrong certificate schema", errors)
    layers = {layer.get("id"): layer for layer in cert.get("layers", [])}
    require(set(layers) == REQUIRED_LAYERS, "certificate layer set drifted", errors)
    require(set(layers["air"].get("required_evidence", [])) == AIR_REQUIRED,
            "AIR required evidence set drifted", errors)
    require(set(layers["mir"].get("required_facts", [])) == MIR_REQUIRED,
            "MIR required fact set drifted", errors)
    for layer_id in ("air", "mir"):
        layer = layers[layer_id]
        require(isinstance(layer.get("digest_sha256"), str)
                and len(layer["digest_sha256"]) == 64,
                f"{layer_id} digest is missing", errors)
    require(layers["abi"].get("status") == "manifest-only",
            "ABI layer must be explicit manifest-only until ABI JSON exists", errors)
    require(layers["backend"].get("consumption") == "fact-or-fail-closed",
            "backend layer must stay fact-or-fail-closed", errors)

air = json.loads(air_path.read_text(encoding="utf-8"))
mir = json.loads(mir_path.read_text(encoding="utf-8"))
errors = []

require(air.get("schema") == "pgy.air.graph.v1", "AIR schema mismatch", errors)
summary = air.get("summary", {})
require(summary.get("strict_evidence") is True, "AIR strict evidence missing", errors)
require(summary.get("drift_count") == 0, "AIR drift_count must be zero", errors)
evidence_kinds = {entry.get("kind") for entry in air.get("evidence", [])}
require(AIR_REQUIRED <= evidence_kinds,
        "AIR evidence missing: " + ",".join(sorted(AIR_REQUIRED - evidence_kinds)),
        errors)
require(all(entry.get("fallback_count", 0) == 0 for entry in air.get("evidence", [])
            if entry.get("kind") in AIR_REQUIRED),
        "AIR required evidence contains fallback_count != 0", errors)

require(mir.get("schema") == "pgy.mir.v1", "MIR schema mismatch", errors)
routines = mir.get("routines", [])
instructions = [
    inst
    for routine in routines
    for block in routine.get("blocks", [])
    for inst in block.get("instructions", [])
]
require(any(routine.get("kind") == "intent" for routine in routines),
        "MIR intent routine missing", errors)
require(any("blocks" in routine for routine in routines),
        "MIR cfg block inventory missing", errors)
require(any(inst.get("source_type") for inst in instructions),
        "MIR source_shape fact missing", errors)
require(any(inst.get("expr0") for inst in instructions),
        "MIR expr0 fact missing", errors)
require(any(inst.get("kind") == "cleanup" for inst in instructions),
        "MIR cleanup fact missing", errors)

certificate = {
    "schema": "pgy.proof-carrying-ir.v1",
    "source": source.as_posix(),
    "policy": {
        "semantic_fallback": "forbidden",
        "backend_consumption": "fact-or-fail-closed",
        "negative_check": "delete-required-fact",
    },
    "layers": [
        {
            "id": "air",
            "payload_schema": "pgy.air.graph.v1",
            "digest_sha256": digest(air_path),
            "required_evidence": sorted(AIR_REQUIRED),
            "verifier": "air-json-schema-test-smoke",
        },
        {
            "id": "dag",
            "payload_schema": "type-resolution-metadata",
            "status": "manifest-only",
            "required_facts": ["generic_default_rows", "ability_bound_rows", "metadata_dead_ends_zero"],
            "verifier": "type-resolution-dag-test-smoke",
        },
        {
            "id": "mir",
            "payload_schema": "pgy.mir.v1",
            "digest_sha256": digest(mir_path),
            "required_facts": sorted(MIR_REQUIRED),
            "verifier": "cfg-body-dataflow-test-smoke",
        },
        {
            "id": "abi",
            "payload_schema": "mir-runtime-abi-facts",
            "status": "manifest-only",
            "required_facts": ["ownership_shape", "slot_handle_shape", "explicit_tag_option_layout"],
            "verifier": "abi-ownership-shape-test-smoke",
        },
        {
            "id": "backend",
            "payload_schema": "backend-consumption-trace",
            "status": "manifest-only",
            "consumption": "fact-or-fail-closed",
            "verifier": "backend-fail-closed-test-smoke",
        },
    ],
}
validate_certificate(certificate, errors)

bad = copy.deepcopy(certificate)
bad["layers"][0]["required_evidence"].remove("rir_authority")
bad_errors = []
validate_certificate(bad, bad_errors)
require(bad_errors, "negative certificate deletion was accepted", errors)

if errors:
    for error in errors:
        print(f"[proof-carrying-pipeline] {error}", file=sys.stderr)
    raise SystemExit(1)

cert_path.write_text(json.dumps(certificate, sort_keys=True, separators=(",", ":")) + "\n",
                     encoding="utf-8")
PY

grep -Fq '"schema":"pgy.proof-carrying-ir.v1"' "$CERT_JSON" ||
    fail "certificate schema not emitted"
grep -Fq '"backend_consumption":"fact-or-fail-closed"' "$CERT_JSON" ||
    fail "certificate backend consumption policy missing"

echo "[proof-carrying-pipeline] certificate envelope ok"
