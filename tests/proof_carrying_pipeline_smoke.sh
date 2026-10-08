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

PYTHONPATH="$ROOT_DIR/scripts${PYTHONPATH:+:$PYTHONPATH}" \
"$PYTHON_BIN" - "$SOURCE" "$AIR_JSON" "$MIR_JSON" "$CERT_JSON" <<'PY'
import copy
import json
import pathlib
import sys
from proof_certificate_admission import (
    AIR_REQUIRED, MIR_REQUIRED, digest, binding_digest, require, validate_certificate,
)

source = pathlib.Path(sys.argv[1])
air_path = pathlib.Path(sys.argv[2])
mir_path = pathlib.Path(sys.argv[3])
cert_path = pathlib.Path(sys.argv[4])

errors = []

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
