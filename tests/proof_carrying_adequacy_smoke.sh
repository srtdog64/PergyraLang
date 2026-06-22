#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[proof-carrying-adequacy] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -e "$ROOT_DIR/$rel" ]] || fail "missing file: $rel"
}

require_text() {
    local rel="$1"
    local text="$2"
    grep -Fq -- "$text" "$ROOT_DIR/$rel" ||
        fail "$rel missing text: $text"
}

require_file "docs/semantics/proofs/ProofCarryingIR.v"
require_file "docs/semantics/proofs/ProofCarryingIR.md"
require_file "docs/semantics/17_proof_carrying_pipeline.md"
require_file "tests/proof_carrying_pipeline_smoke.sh"
require_file "docs/semantics/pass_contract_manifest.md"
require_file "tests/formal_semantics_smoke.sh"

require_text "docs/semantics/proofs/ProofCarryingIR.v" "Inductive CertLayer"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Inductive AIRFact"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Inductive MIRFact"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "FactOrFailClosed"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "CompatMaySucceed"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Definition ValidCertificate"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem valid_certificate_allows_backend_consumption"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem missing_air_authority_fails_closed"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem missing_mir_expr0_fails_closed"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem compat_success_policy_fails_closed"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem negative_deletion_gate_required"
require_text "docs/semantics/proofs/ProofCarryingIR.v" "Theorem valid_certificate_requires_air_and_mir_facts"

require_text "docs/semantics/proofs/ProofCarryingIR.md" "valid certificate + valid owner payloads"
require_text "docs/semantics/proofs/ProofCarryingIR.md" "missing required certificate fact"
require_text "docs/semantics/proofs/ProofCarryingIR.md" "This is not whole-compiler verification"

require_text "docs/semantics/17_proof_carrying_pipeline.md" "pgy.proof-carrying-ir.v1"
require_text "docs/semantics/17_proof_carrying_pipeline.md" "Stage 2: Mechanized Checker Core"
require_text "tests/proof_carrying_pipeline_smoke.sh" "AIR_REQUIRED"
require_text "tests/proof_carrying_pipeline_smoke.sh" "MIR_REQUIRED"
require_text "tests/proof_carrying_pipeline_smoke.sh" "rir_authority"
require_text "tests/proof_carrying_pipeline_smoke.sh" "expr0"
require_text "tests/proof_carrying_pipeline_smoke.sh" "negative certificate deletion was accepted"
require_text "docs/semantics/pass_contract_manifest.md" "proof_certificate_pipeline"
require_text "tests/formal_semantics_smoke.sh" "docs/semantics/proofs/ProofCarryingIR.v"

echo "[proof-carrying-adequacy] checker-core model is bound to live certificate gate"
