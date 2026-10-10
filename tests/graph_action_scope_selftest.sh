#!/usr/bin/env bash
# Plant regressions only in an isolated copy; never edit the shared checkout.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$ROOT_DIR/.tmp/graph-action-scope"
receipt="$(mktemp -d "$ROOT_DIR/.tmp/graph-action-scope/selftest.XXXXXX")"
inputs=(
    docs/semantics/proofs/OwnershipCleanCore.v
    docs/semantics/proofs/OwnershipGraphLinks.v
    docs/semantics/proofs/OwnershipGraphActionScope.v
    tests/coq/GraphActionScopeAudit.v
    tests/coq_kernel_check.sh
    tests/graph_action_scope_smoke.sh
    scripts/rocq_toolchain_owner.sh
)
snapshot() {
    local target="$1" input
    for input in "${inputs[@]}"; do
        mkdir -p "$target/$(dirname "$input")"
        cp "$ROOT_DIR/$input" "$target/$input"
    done
}
expect_refusal() {
    local label="$1" target="$2" expected="$3"
    if PGY_GRAPH_ACTION_RECEIPT_DIR="$receipt/$label" \
        bash "$target/tests/graph_action_scope_smoke.sh" >"$receipt/$label.log" 2>&1; then
        echo "[graph-action-scope-selftest] FAIL: $label was accepted" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" "$receipt/$label.log"; then
        cat "$receipt/$label.log" >&2
        echo "[graph-action-scope-selftest] FAIL: $label refused for the wrong reason" >&2
        exit 1
    fi
    echo "[graph-action-scope-selftest] refused: $label"
}

snapshot "$work/missing"
perl -pi -e 's/^Example deleted_compensation_target_gets_stuck/Example removed_deleted_compensation_target_gets_stuck/' \
    "$work/missing/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal missing "$work/missing" 'lost deleted_compensation_target_gets_stuck'

# Reads of unacquired links admitted: the step theorem must stop compiling.
snapshot "$work/admission"
perl -pi -e 's/\| BRead l => In l ls end\./| BRead l => True end./' \
    "$work/admission/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal admission "$work/admission" 'proof compilation failed: OwnershipGraphActionScope.v'

# A weakened owner statement must break the independent consumer.
snapshot "$work/weakened"
perl -0pi -e 's/Theorem saga_never_fails_deref\b.*?Qed\./Theorem saga_never_fails_deref : True. Proof. exact I. Qed./s' \
    "$work/weakened/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal weakened "$work/weakened" 'proof compilation failed: GraphActionScopeAudit.v'

# Drop a real successful prefix from the receipt without changing execution.
snapshot "$work/prefix"
perl -pi -e 's/\(S \(completed \(snd r\)\)\)/0/' \
    "$work/prefix/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal prefix "$work/prefix" 'proof compilation failed: OwnershipGraphActionScope.v'

# Exact graph-state and non-refused-result projection, not a theorem-name check.
snapshot "$work/receipt-contract"
perl -0pi -e 's/Theorem run_body_receipt_projects\b.*?Qed\./Theorem run_body_receipt_projects : True. Proof. exact I. Qed./s' \
    "$work/receipt-contract/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal receipt-contract "$work/receipt-contract" 'proof compilation failed: GraphActionScopeAudit.v'

# A compensating failure must not overwrite the failed forward step's receipt.
snapshot "$work/original-failure"
perl -pi -e 's/SagaStuck k j why \(snd result\)/SagaStuck k j (snd result) (snd result)/' \
    "$work/original-failure/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal original-failure "$work/original-failure" 'proof compilation failed: OwnershipGraphActionScope.v'

# Compensation completion is not forward success or verified restoration.
snapshot "$work/false-success"
perl -pi -e 's/\| \[\] => \(g, SagaCompensationFinished k why\)/| [] => (g, SagaDone)/' \
    "$work/false-success/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal false-success "$work/false-success" 'proof compilation failed: OwnershipGraphActionScope.v'

# Full-step compensation must not be silently applied to a failed prefix.
snapshot "$work/partial-undo"
perl -pi -e 's/\(fst result\) done i \(snd result\)/(fst result) ((i, compensation s) :: done) i (snd result)/' \
    "$work/partial-undo/docs/semantics/proofs/OwnershipGraphActionScope.v"
expect_refusal partial-undo "$work/partial-undo" 'proof compilation failed: OwnershipGraphActionScope.v'

snapshot "$work/axiom"
perl -0pi -e '$_ .= "\nAxiom planted_action_assumption : False.\n"' \
    "$work/axiom/tests/coq/GraphActionScopeAudit.v"
expect_refusal axiom "$work/axiom" 'axiom budget drifted'

snapshot "$work/drift"
perl -0pi -e '$_ .= q{
perl -0pi -e "s/Independent consumer/Changed consumer/" "$ROOT_DIR/tests/coq/GraphActionScopeAudit.v"
}' "$work/drift/tests/coq_kernel_check.sh"
expect_refusal drift "$work/drift" 'source generation changed during this run'

snapshot "$work/clean"
PGY_GRAPH_ACTION_RECEIPT_DIR="$receipt/clean" \
    bash "$work/clean/tests/graph_action_scope_smoke.sh" >"$receipt/clean.log" 2>&1
expect_refusal clean "$work/clean" 'refusing to overwrite an existing receipt'
echo "[graph-action-scope-selftest] PASS (11 planted regressions; receipt: $receipt)"
