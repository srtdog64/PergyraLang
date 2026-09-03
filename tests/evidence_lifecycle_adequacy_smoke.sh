#!/usr/bin/env bash
# Bind the bounded Rocq evidence-lifecycle model to its canonical design owner,
# proof registration, kernel/axiom-budget gate, and live AIR lifetime manifest.
# The Rocq kernel proves the model; this gate prevents the model from silently
# becoming an unregistered vocabulary island.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

OWNER_DOC="docs/semantics/09_abstraction_loss_contracts.md"
PROOF_DOC="docs/semantics/proofs/EvidenceLifecycleCore.md"
PROOF="docs/semantics/proofs/EvidenceLifecycleCore.v"
MANIFEST="docs/semantics/evidence_kind_manifest.md"
LIFETIME_GATE="tests/evidence_lifetime_smoke.sh"
FORMAL_GATE="tests/formal_semantics_smoke.sh"
KERNEL_GATE="tests/coq_kernel_check.sh"

fail() {
    echo "[evidence-lifecycle] $*" >&2
    exit 1
}

require_file() {
    [[ -f "$ROOT_DIR/$1" ]] || fail "missing file: $1"
}

require_text() {
    local rel="$1" text="$2" why="$3"
    grep -Fq -- "$text" "$ROOT_DIR/$rel" ||
        fail "$rel no longer contains \"$text\" -- $why"
}

for required in \
    "$OWNER_DOC" "$PROOF_DOC" "$PROOF" "$MANIFEST" \
    "$LIFETIME_GATE" "$FORMAL_GATE" "$KERNEL_GATE"; do
    require_file "$required"
done

for term in \
    "## Evidence Lifecycle Aesthetic" \
    "Carry the authority established by evidence" \
    "prove richly, carry minimally" \
    "Identity" \
    "Validity" \
    "Diagnostic" \
    "Construction" \
    '`Retain`' \
    '`Reference`' \
    '`Summarize`' \
    '`Materialize`' \
    '`Erase`' \
    "removing it would force a downstream" \
    "Semantic uncertainty and representation payload should be monotone"; do
    require_text "$OWNER_DOC" "$term" \
        "the canonical evidence-lifecycle aesthetic must remain explicit"
done

for term in \
    "## Objective card" \
    "not a second authority" \
    "The lifecycle operations are" \
    "does not close any SoT registry row" \
    "tests/evidence_lifetime_smoke.sh" \
    "tests/air_erasure"; do
    require_text "$PROOF_DOC" "$term" \
        "the model scope and live adequacy boundary must remain honest"
done

for theorem in \
    "Theorem missing_admission_fails_closed" \
    "Theorem identity_reference_carries_authority" \
    "Theorem validity_summarizes_to_receipt" \
    "Theorem construction_erasure_preserves_established_authority" \
    "Theorem construction_erasure_requires_discharge_and_last_consumer" \
    "Theorem materialization_requires_explicit_runtime_need" \
    "Theorem receipt_justified_iff_redecision_when_authority_required" \
    "Theorem no_redecision_means_no_receipt_justification" \
    "Theorem justified_validity_receipt_carries_authority" \
    "Theorem compression_step_transitive" \
    "Theorem compression_trace_nonincreasing" \
    "Theorem admitted_interpretation_does_not_reopen"; do
    require_text "$PROOF" "$theorem" \
        "the canonical document cites the bounded Rocq result"
done

if grep -Eq '^[[:space:]]*(Axiom|Admitted)([[:space:]]|\.)' "$ROOT_DIR/$PROOF"; then
    fail "$PROOF introduced an axiom or admitted theorem"
fi

require_text "$FORMAL_GATE" \
    "docs/semantics/proofs/EvidenceLifecycleCore.v" \
    "the proof must be in the explicit Coq/Rocq compile inventory"
require_text "$KERNEL_GATE" \
    'for proof_abs in "$PROOFS_DIR"/*.v; do' \
    "the Rocq kernel and axiom-budget gate must discover every proof"
require_text "$KERNEL_GATE" \
    "axiom budget drifted" \
    "new proof assumptions must fail the corpus budget"

for term in "last consumer" "summarize" "docs/semantics/09"; do
    require_text "$MANIFEST" "$term" \
        "live AIR evidence kinds must remain tied to the lifecycle owner"
done

# This existing gate supplies the executable two-way enum/manifest falsifier;
# do not duplicate that source-of-truth correspondence here.
bash "$ROOT_DIR/$LIFETIME_GATE"

echo "[evidence-lifecycle] ok (owner vocabulary, bounded Rocq model, explicit" \
     "proof inventory, axiom budget, and live AIR lifetime manifest remain linked)"
