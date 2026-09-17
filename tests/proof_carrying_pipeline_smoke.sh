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

"$PGY" --native-pipeline --air-json "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --backend=c >"$AIR_JSON" 2>"$WORK_DIR/air.err"
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

def binding_digest(source_path, air_payload_path, mir_payload_path):
    payload = {
        "air_sha256": digest(air_payload_path),
        "mir_sha256": digest(mir_payload_path),
        "schema": "pgy.proof-input-binding.v1",
        "source_sha256": digest(source_path),
    }
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()

def require(condition, message, errors):
    if not condition:
        errors.append(message)

def validate_certificate(cert, source_path, air_payload_path, mir_payload_path, errors):
    require(cert.get("schema") == "pgy.proof-carrying-ir.v1",
            "wrong certificate schema", errors)
    require(cert.get("source") == source_path.as_posix(),
            "certificate source identity drifted", errors)
    require(cert.get("source_digest_sha256") == digest(source_path),
            "certificate source digest drifted", errors)
    require(cert.get("binding_digest_sha256") ==
            binding_digest(source_path, air_payload_path, mir_payload_path),
            "certificate source/AIR/MIR binding digest drifted", errors)

    layer_rows = cert.get("layers", [])
    require(isinstance(layer_rows, list), "certificate layers must be an array", errors)
    if not isinstance(layer_rows, list):
        return
    layer_ids = [layer.get("id") for layer in layer_rows if isinstance(layer, dict)]
    layers = {layer.get("id"): layer for layer in layer_rows if isinstance(layer, dict)}
    require(len(layer_ids) == len(layer_rows),
            "certificate layer row is not an object", errors)
    require(len(layer_ids) == len(set(layer_ids)),
            "certificate layer ids are duplicated", errors)
    require(set(layers) == REQUIRED_LAYERS, "certificate layer set drifted", errors)
    if set(layers) != REQUIRED_LAYERS:
        return
    require(set(layers["air"].get("required_evidence", [])) == AIR_REQUIRED,
            "AIR required evidence set drifted", errors)
    require(set(layers["mir"].get("required_facts", [])) == MIR_REQUIRED,
            "MIR required fact set drifted", errors)
    for layer_id in ("air", "mir"):
        layer = layers[layer_id]
        require(isinstance(layer.get("digest_sha256"), str)
                and len(layer["digest_sha256"]) == 64,
                f"{layer_id} digest is missing", errors)
    require(layers["air"].get("digest_sha256") == digest(air_payload_path),
            "AIR payload digest drifted", errors)
    require(layers["mir"].get("digest_sha256") == digest(mir_payload_path),
            "MIR payload digest drifted", errors)
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
    "source_digest_sha256": digest(source),
    "binding_digest_sha256": binding_digest(source, air_path, mir_path),
    "policy": {
        "semantic_fallback": "forbidden",
        "backend_consumption": "fact-or-fail-closed",
        "negative_check": "delete-required-fact+mutate-bound-input",
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
validate_certificate(certificate, source, air_path, mir_path, errors)

bad = copy.deepcopy(certificate)
bad["layers"][0]["required_evidence"].remove("rir_authority")
bad_errors = []
validate_certificate(bad, source, air_path, mir_path, bad_errors)
require(bad_errors, "negative certificate deletion was accepted", errors)

duplicate_layer = copy.deepcopy(certificate)
duplicate_layer["layers"].append(copy.deepcopy(duplicate_layer["layers"][0]))
duplicate_errors = []
validate_certificate(
    duplicate_layer, source, air_path, mir_path, duplicate_errors
)
require(duplicate_errors, "duplicate certificate layer was accepted", errors)

bound_source = cert_path.parent / "bound-source.pgy"
bound_source.write_bytes(source.read_bytes())
bound_certificate = copy.deepcopy(certificate)
bound_certificate["source"] = bound_source.as_posix()
bound_certificate["source_digest_sha256"] = digest(bound_source)
bound_certificate["binding_digest_sha256"] = binding_digest(
    bound_source, air_path, mir_path
)
bound_errors = []
validate_certificate(
    bound_certificate, bound_source, air_path, mir_path, bound_errors
)
require(not bound_errors, "fresh source-bound certificate was rejected", errors)

bound_source.write_bytes(bound_source.read_bytes() + b"\n// red-team mutation\n")
mutated_source_errors = []
validate_certificate(
    bound_certificate, bound_source, air_path, mir_path, mutated_source_errors
)
require(mutated_source_errors, "source mutation kept an old certificate valid", errors)

repaired_source_only = copy.deepcopy(bound_certificate)
repaired_source_only["source_digest_sha256"] = digest(bound_source)
repaired_source_errors = []
validate_certificate(
    repaired_source_only, bound_source, air_path, mir_path,
    repaired_source_errors
)
require(repaired_source_errors,
        "source digest repair bypassed the composite binding", errors)

bound_air = cert_path.parent / "bound-air.json"
bound_air.write_bytes(air_path.read_bytes())
air_certificate = copy.deepcopy(certificate)
air_certificate["layers"][0]["digest_sha256"] = digest(bound_air)
air_certificate["binding_digest_sha256"] = binding_digest(
    source, bound_air, mir_path
)
bound_air.write_bytes(bound_air.read_bytes() + b"\n")
mutated_air_errors = []
validate_certificate(
    air_certificate, source, bound_air, mir_path, mutated_air_errors
)
require(mutated_air_errors, "AIR payload mutation kept an old certificate valid", errors)

bound_mir = cert_path.parent / "bound-mir.json"
bound_mir.write_bytes(mir_path.read_bytes())
mir_certificate = copy.deepcopy(certificate)
mir_certificate["layers"][2]["digest_sha256"] = digest(bound_mir)
mir_certificate["binding_digest_sha256"] = binding_digest(
    source, air_path, bound_mir
)
bound_mir.write_bytes(bound_mir.read_bytes() + b"\n")
mutated_mir_errors = []
validate_certificate(
    mir_certificate, source, air_path, bound_mir, mutated_mir_errors
)
require(mutated_mir_errors, "MIR payload mutation kept an old certificate valid", errors)

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
grep -Fq '"source_digest_sha256":"' "$CERT_JSON" ||
    fail "certificate source digest missing"
grep -Fq '"binding_digest_sha256":"' "$CERT_JSON" ||
    fail "certificate composite binding digest missing"

echo "[proof-carrying-pipeline] certificate envelope and source/payload red-team binding ok"
