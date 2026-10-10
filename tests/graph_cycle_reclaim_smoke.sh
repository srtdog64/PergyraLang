#!/usr/bin/env bash
# Store-local cycle reclamation and its root completeness; no production,
# runtime-counter or cost claim. The modules must kernel-check with no
# assumptions. Independent consumers pin propositions; named falsifiers must
# stay in the fixed corpus, and the receipt binds its inputs before and after.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
inputs=(
    docs/semantics/proofs/OwnershipCleanCore.v
    docs/semantics/proofs/OwnershipGraphLinks.v
    docs/semantics/proofs/OwnershipGraphCycleReclaim.v
    docs/semantics/proofs/OwnershipGraphRootCompleteness.v
    docs/semantics/proofs/OwnershipTeardown.v
    docs/semantics/proofs/OwnershipTeardownAuthority.v
    docs/semantics/proofs/OwnershipTeardownAtomicBatch.v
    tests/coq/GraphCycleReclaimAudit.v
)
receipt_base="$ROOT_DIR/.tmp/graph-cycle-reclaim"
mkdir -p "$receipt_base"
receipt_dir="${PGY_GRAPH_RECLAIM_RECEIPT_DIR:-}"
if [ -z "$receipt_dir" ]; then
    receipt_dir="$(mktemp -d "$receipt_base/run.XXXXXX")"
else
    mkdir -p "$receipt_dir"
    if [ -e "$receipt_dir/source.sha256" ] || [ -e "$receipt_dir/kernel.log" ]; then
        echo '[graph-cycle-reclaim] FAIL: refusing to overwrite an existing receipt' >&2
        exit 1
    fi
fi
(cd "$ROOT_DIR" && sha256sum "${inputs[@]}" tests/graph_cycle_reclaim_smoke.sh \
    tests/coq_kernel_check.sh scripts/rocq_toolchain_owner.sh) >"$receipt_dir/source.sha256"
for input in "${inputs[@]}"; do
    cp "$ROOT_DIR/$input" "$work/"
    expected="$(awk -v path="$input" '$2 == path { print $1 }' "$receipt_dir/source.sha256")"
    observed="$(sha256sum "$work/$(basename "$input")")"
    if [ "$expected" != "${observed%% *}" ]; then
        echo "[graph-cycle-reclaim] FAIL: input changed while snapshotting: $input" >&2
        exit 1
    fi
done

# Each falsifier is a claim the gate defends; deleting one is a regression.
require_name() {
    local file="$1" name="$2"
    if ! grep -Eq "^(Theorem|Lemma|Example) $name\b" "$work/$(basename "$file")"; then
        echo "[graph-cycle-reclaim] FAIL: $file lost $name" >&2
        exit 1
    fi
}
for name in trial_garbage_with_unreachable reclaim_preserves_root_view \
            counts_track_every_operation count_after_delete_self \
            count_zero_never_frees_cycle naive_trial_deletes_reachable \
            small_budget_defers forgotten_root_becomes_stale \
            reclaimed_slot_reuse_is_visible \
            index_decrement_after_reuse_deletes_reachable \
            counted_run_preserves_counts counted_reclaim_projects \
            counted_delete_batch_refused_unchanged \
            counted_delete_batch_accepted_projects; do
    require_name docs/semantics/proofs/OwnershipGraphCycleReclaim.v "$name"
done
for name in links_of_complete reclaim_simulates \
            reclaim_preserves_every_reference_run rexec_deterministic \
            reclaim_without_right_is_identity dead_node_is_reclaimed \
            aggregate_only_link_needs_deep_roots \
            caller_frame_link_needs_frame_roots \
            view_only_backing_needs_view_roots returned_link_must_stay_rooted \
            ledger_grant_admits ledger_transfer_revokes ledger_consumption_revokes \
            checked_ledger_reclaim_simulates incomplete_reclaim_roots_refused; do
    require_name docs/semantics/proofs/OwnershipGraphRootCompleteness.v "$name"
done
for name in retire_batch_refusal_unchanged retire_batch_accepted_iff \
            retire_batch_preserves_authority retire_batch_node_rights_unchanged \
            retire_batch_leases_unchanged retire_batch_later_refusal \
            retire_batch_initial_requests_checked retire_batch_initial_requests_exact \
            retire_batch_initial_units_disjoint retire_batch_targets_unique; do
    require_name docs/semantics/proofs/OwnershipTeardownAtomicBatch.v "$name"
done
for name in audit_manual_lifecycle_retains_unreachable \
            audit_borrowed_batch_is_not_atomic \
            audit_borrowed_batch_atomic_refusal \
            audit_checked_boundary_borrow_refused \
            audit_graph_batch_duplicate_and_stale_refused \
            audit_canonical_batch_late_lease_atomic \
            audit_canonical_batch_late_foreign_holder_atomic \
            audit_canonical_node_batch_keeps_root_right \
            audit_canonical_root_node_overlap_unchanged \
            audit_canonical_shrinking_units_refused \
            audit_canonical_root_end_consumes_once \
            audit_full_inventory_snapshot_observes_reclaim; do
    require_name tests/coq/GraphCycleReclaimAudit.v "$name"
done

PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' bash "$ROOT_DIR/tests/coq_kernel_check.sh" \
    >"$receipt_dir/kernel.log" 2>&1 || {
        cat "$receipt_dir/kernel.log" >&2; exit 1;
    }
(cd "$ROOT_DIR" && sha256sum --check "$receipt_dir/source.sha256") \
    >"$receipt_dir/source-after.log" 2>&1 || {
        cat "$receipt_dir/source-after.log" >&2
        echo '[graph-cycle-reclaim] FAIL: source generation changed during this run' >&2
        exit 1
    }
tail -n 1 "$receipt_dir/kernel.log"
echo "[graph-cycle-reclaim] receipt: $receipt_dir"
echo '[graph-cycle-reclaim] PASS (explicit atomic model batches, checked roots/counts and canonical retirement; no assumptions; production binding/native commit and cost OPEN)'
