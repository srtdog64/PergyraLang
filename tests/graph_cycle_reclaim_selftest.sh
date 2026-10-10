#!/usr/bin/env bash
# Plant regressions only in an isolated copy; never edit the shared checkout.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$ROOT_DIR/.tmp/graph-cycle-reclaim"
receipt="$(mktemp -d "$ROOT_DIR/.tmp/graph-cycle-reclaim/selftest.XXXXXX")"
inputs=(
    docs/semantics/proofs/OwnershipCleanCore.v
    docs/semantics/proofs/OwnershipGraphLinks.v
    docs/semantics/proofs/OwnershipGraphCycleReclaim.v
    docs/semantics/proofs/OwnershipGraphRootCompleteness.v
    docs/semantics/proofs/OwnershipTeardown.v
    docs/semantics/proofs/OwnershipTeardownAuthority.v
    docs/semantics/proofs/OwnershipTeardownAtomicBatch.v
    tests/coq/GraphCycleReclaimAudit.v
    tests/coq_kernel_check.sh
    tests/graph_cycle_reclaim_smoke.sh
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
    if PGY_GRAPH_RECLAIM_RECEIPT_DIR="$receipt/$label" \
        bash "$target/tests/graph_cycle_reclaim_smoke.sh" >"$receipt/$label.log" 2>&1; then
        echo "[graph-cycle-reclaim-selftest] FAIL: $label was accepted" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" "$receipt/$label.log"; then
        cat "$receipt/$label.log" >&2
        echo "[graph-cycle-reclaim-selftest] FAIL: $label refused for the wrong reason" >&2
        exit 1
    fi
    echo "[graph-cycle-reclaim-selftest] refused: $label"
}

snapshot "$work/missing"
perl -pi -e 's/^Theorem returned_link_must_stay_rooted/Theorem removed_returned_link_must_stay_rooted/' \
    "$work/missing/docs/semantics/proofs/OwnershipGraphRootCompleteness.v"
expect_refusal missing "$work/missing" 'lost returned_link_must_stay_rooted'

snapshot "$work/missing_batch"
perl -pi -e 's/^Example audit_borrowed_batch_is_not_atomic/Example removed_audit_borrowed_batch_is_not_atomic/' \
    "$work/missing_batch/tests/coq/GraphCycleReclaimAudit.v"
expect_refusal missing_batch "$work/missing_batch" 'lost audit_borrowed_batch_is_not_atomic'

snapshot "$work/missing_atomic"
perl -pi -e 's/^Example audit_borrowed_batch_atomic_refusal/Example removed_audit_borrowed_batch_atomic_refusal/' \
    "$work/missing_atomic/tests/coq/GraphCycleReclaimAudit.v"
expect_refusal missing_atomic "$work/missing_atomic" 'lost audit_borrowed_batch_atomic_refusal'

snapshot "$work/missing_initial"
perl -pi -e 's/^Example audit_canonical_shrinking_units_refused/Example removed_audit_canonical_shrinking_units_refused/' \
    "$work/missing_initial/tests/coq/GraphCycleReclaimAudit.v"
expect_refusal missing_initial "$work/missing_initial" 'lost audit_canonical_shrinking_units_refused'

snapshot "$work/missing_snapshot"
perl -pi -e 's/^Example audit_full_inventory_snapshot_observes_reclaim/Example removed_audit_full_inventory_snapshot_observes_reclaim/' \
    "$work/missing_snapshot/tests/coq/GraphCycleReclaimAudit.v"
expect_refusal missing_snapshot "$work/missing_snapshot" 'lost audit_full_inventory_snapshot_observes_reclaim'

snapshot "$work/weakened"
perl -0pi -e 's/Theorem ledger_consumption_revokes\b.*?Qed\./Theorem ledger_consumption_revokes : True. Proof. exact I. Qed./s' \
    "$work/weakened/docs/semantics/proofs/OwnershipGraphRootCompleteness.v"
expect_refusal weakened "$work/weakened" 'proof compilation failed: GraphCycleReclaimAudit.v'

snapshot "$work/weakened_canonical"
perl -0pi -e 's/Theorem retire_batch_refusal_unchanged\b.*?Qed\./Theorem retire_batch_refusal_unchanged : True. Proof. exact I. Qed./s' \
    "$work/weakened_canonical/docs/semantics/proofs/OwnershipTeardownAtomicBatch.v"
expect_refusal weakened_canonical "$work/weakened_canonical" 'proof compilation failed: OwnershipTeardownAtomicBatch.v'

snapshot "$work/partial_publish"
perl -0pi -e 's/\| CountedBatchRefused _ why => CountedBatchRefused c why/| CountedBatchRefused middle why => CountedBatchRefused middle why/ or die "atomic publication branch not found"' \
    "$work/partial_publish/docs/semantics/proofs/OwnershipGraphCycleReclaim.v"
expect_refusal partial_publish "$work/partial_publish" 'proof compilation failed: OwnershipGraphCycleReclaim.v'

snapshot "$work/axiom"
perl -0pi -e '$_ .= "\nAxiom planted_graph_assumption : False.\n"' \
    "$work/axiom/tests/coq/GraphCycleReclaimAudit.v"
expect_refusal axiom "$work/axiom" 'axiom budget drifted'

snapshot "$work/drift"
perl -0pi -e '$_ .= q{
perl -0pi -e "s/Independent consumers/Changed consumers/" "$ROOT_DIR/tests/coq/GraphCycleReclaimAudit.v"
}' "$work/drift/tests/coq_kernel_check.sh"
expect_refusal drift "$work/drift" 'source generation changed during this run'

snapshot "$work/clean"
PGY_GRAPH_RECLAIM_RECEIPT_DIR="$receipt/clean" \
    bash "$work/clean/tests/graph_cycle_reclaim_smoke.sh" >"$receipt/clean.log" 2>&1
expect_refusal clean "$work/clean" 'refusing to overwrite an existing receipt'
echo "[graph-cycle-reclaim-selftest] PASS (11 planted regressions; receipt: $receipt)"
