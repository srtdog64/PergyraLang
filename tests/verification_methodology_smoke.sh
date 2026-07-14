#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[verification-methodology] $*" >&2
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

for rel in \
    docs/139_golden_adt_verification_methodology.md \
    docs/semantics/README.md \
    docs/semantics/proofs/VerificationMethodology.v \
    docs/semantics/proofs/VerificationMethodology.md \
    tests/formal_semantics_smoke.sh; do
    require_file "$rel"
done

for term in \
    "Prose contract" \
    "Smoke gate" \
    "Golden fixture" \
    "Differential oracle" \
    "Property/metamorphic test" \
    "Verifier" \
    "Mechanized model" \
    "Algebraic Data Types" \
    "Abstract Data Types" \
    "Operational semantics" \
    "Trace semantics" \
    "Axiomatic semantics" \
    "Typestate" \
    "Linear/affine ownership" \
    "Effect system" \
    "Capability calculus" \
    "Session/protocol types" \
    "Refinement types" \
    "Abstract interpretation" \
    "Separation logic" \
    "Model checking" \
    "Differential testing" \
    "Property/metamorphic testing" \
    "Mechanized proof" \
    "Golden layout tests come after the proof facts."; do
    require_text "docs/139_golden_adt_verification_methodology.md" "$term"
done

for term in \
    "Inductive Method" \
    "Inductive Claim" \
    "Definition permits" \
    "Definition golden_only" \
    "Definition smoke_only" \
    "Theorem golden_only_not_model_soundness" \
    "Theorem golden_only_not_hard_self_host_slice" \
    "Theorem smoke_only_not_hard_self_host_slice" \
    "Theorem mechanized_model_not_implementation_parity" \
    "Theorem differential_not_model_soundness" \
    "Theorem hard_self_host_requires_differential" \
    "Theorem hard_self_host_requires_verifier" \
    "Theorem hard_self_host_requires_owner" \
    "Theorem layout_niche_requires_typestate" \
    "Theorem layout_niche_requires_verifier" \
    "Theorem materialization_requires_trace_and_capability" \
    "Theorem materialization_requires_verifier" \
    "Theorem verifier_with_owner_permits_fact_consumption"; do
    require_text "docs/semantics/proofs/VerificationMethodology.v" "$term"
done

for term in \
    "a golden fixture alone does not prove model soundness" \
    "a mechanized model alone does not prove implementation parity" \
    "runtime materialization requires trace evidence, capability evidence, and a" \
    "This is not whole-compiler verification."; do
    require_text "docs/semantics/proofs/VerificationMethodology.md" "$term"
done

require_text "docs/semantics/README.md" "proofs/VerificationMethodology.v"
require_text "tests/formal_semantics_smoke.sh" "docs/semantics/proofs/VerificationMethodology.v"

# Rocq 9 renamed the CLI (`rocq compile` replaces coqc) and its official image
# ships only the new name, while apt coq ships only the legacy one -- detect
# rather than assume. And a runner with no prover used to skip this check while
# the gate still reported green; that absence is now fatal unless declared.
coq_compile=""
if command -v rocq >/dev/null 2>&1; then
    coq_compile="rocq compile"
elif command -v coqc >/dev/null 2>&1; then
    coq_compile="coqc"
fi

if [ -n "$coq_compile" ]; then
    coq_timeout="${PGY_COQ_SMOKE_TIMEOUT_SECONDS:-60}"
    if command -v timeout >/dev/null 2>&1; then
        (cd "$ROOT_DIR" && timeout "$coq_timeout" $coq_compile docs/semantics/proofs/VerificationMethodology.v)
    else
        (cd "$ROOT_DIR" && $coq_compile docs/semantics/proofs/VerificationMethodology.v)
    fi
    echo "[verification-methodology] Coq model ok (checked with '$coq_compile')"
elif [ "${PGY_ALLOW_MISSING_COQ:-0}" = "1" ]; then
    echo "[verification-methodology] Coq model DECLARED SKIP -- no prover (looked" \
         "for rocq, coqc), PGY_ALLOW_MISSING_COQ=1; the model was NOT checked here."
else
    echo "[verification-methodology] FAIL -- no prover found (looked for rocq," \
         "coqc); the Coq model would go unchecked. Install Coq/Rocq, or set" \
         "PGY_ALLOW_MISSING_COQ=1 to declare the skip." >&2
    exit 1
fi

echo "[verification-methodology] methodology gate ok"
