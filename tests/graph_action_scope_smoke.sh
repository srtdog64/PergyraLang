#!/usr/bin/env bash
# Action-scoped references and sagas over the graph store; no surface,
# compiler, runtime or cost claim. The modules must kernel-check with no
# assumptions. The independent consumer pins the propositions; named
# falsifiers must stay in the fixed corpus, and the receipt binds its inputs
# before and after the run.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
inputs=(
    docs/semantics/proofs/OwnershipCleanCore.v
    docs/semantics/proofs/OwnershipGraphLinks.v
    docs/semantics/proofs/OwnershipGraphActionScope.v
    tests/coq/GraphActionScopeAudit.v
)
receipt_base="$ROOT_DIR/.tmp/graph-action-scope"
mkdir -p "$receipt_base"
receipt_dir="${PGY_GRAPH_ACTION_RECEIPT_DIR:-}"
if [ -z "$receipt_dir" ]; then
    receipt_dir="$(mktemp -d "$receipt_base/run.XXXXXX")"
else
    mkdir -p "$receipt_dir"
    if [ -e "$receipt_dir/source.sha256" ] || [ -e "$receipt_dir/kernel.log" ]; then
        echo '[graph-action-scope] FAIL: refusing to overwrite an existing receipt' >&2
        exit 1
    fi
fi
(cd "$ROOT_DIR" && sha256sum "${inputs[@]}" tests/graph_action_scope_smoke.sh \
    tests/coq_kernel_check.sh scripts/rocq_toolchain_owner.sh) >"$receipt_dir/source.sha256"
for input in "${inputs[@]}"; do
    cp "$ROOT_DIR/$input" "$work/"
    expected="$(awk -v path="$input" '$2 == path { print $1 }' "$receipt_dir/source.sha256")"
    observed="$(sha256sum "$work/$(basename "$input")")"
    if [ "$expected" != "${observed%% *}" ]; then
        echo "[graph-action-scope] FAIL: input changed while snapshotting: $input" >&2
        exit 1
    fi
done

# Each falsifier is a claim the gate defends; deleting one is a regression.
require_name() {
    local file="$1" name="$2"
    if ! grep -Eq "^(Theorem|Lemma|Corollary|Example) $name\b" "$work/$(basename "$file")"; then
        echo "[graph-action-scope] FAIL: $file lost $name" >&2
        exit 1
    fi
}
for name in spared_step spared_run_keeps_link stale_needs_a_destroyer \
            held_refuses_destroyers held_step held_run_keeps_link \
            admitted_step_never_fails_deref run_step_restores_borrows \
            graph_refusal_unchanged run_body_receipt_sound run_body_receipt_projects \
            run_step_receipt_bound completed_step_accounts_for_whole_body \
            acquisition_failure_has_no_body_prefix \
            saga_never_fails_deref destroyer_gets_the_refusal \
            partial_acquisition_is_released neighbor_needs_its_own_acquisition \
            unguarded_delete_breaks_hold stale_at_step_start_compensates \
            deleted_compensation_target_gets_stuck partial_forward_effect_is_reported \
            noop_compensation_is_not_restore partial_compensation_preserves_both_failures \
            receipt_counts_successful_reads_and_writes; do
    require_name docs/semantics/proofs/OwnershipGraphActionScope.v "$name"
done
for name in audit_admitted_step_never_fails_deref audit_saga_never_fails_deref \
            audit_run_step_restores_borrows audit_spared_run_keeps_link \
            audit_body_receipt_projects audit_receipt_extent \
            audit_held_run_keeps_link audit_admission_and_destroyers \
            audit_holding_moves_the_failure audit_saga_outcomes \
            audit_effect_counterexamples_admitted audit_partial_forward_not_rolled_back \
            audit_noop_compensation_keeps_effect audit_partial_compensation_keeps_original_failure \
            audit_reads_are_not_mutation_counts audit_outer_hold_survives_partial_body \
            audit_store_identity_is_not_reused; do
    require_name tests/coq/GraphActionScopeAudit.v "$name"
done

# The replaced outcome must not return as a constructor or compatibility alias.
if grep -Eq '\bSagaCompensated\b' "$work/OwnershipGraphActionScope.v" "$work/GraphActionScopeAudit.v"; then
    echo '[graph-action-scope] FAIL: ambiguous compensation outcome returned' >&2
    exit 1
fi

PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' bash "$ROOT_DIR/tests/coq_kernel_check.sh" \
    >"$receipt_dir/kernel.log" 2>&1 || {
        cat "$receipt_dir/kernel.log" >&2; exit 1;
    }
(cd "$ROOT_DIR" && sha256sum --check "$receipt_dir/source.sha256") \
    >"$receipt_dir/source-after.log" 2>&1 || {
        cat "$receipt_dir/source-after.log" >&2
        echo '[graph-action-scope] FAIL: source generation changed during this run' >&2
        exit 1
    }
tail -n 1 "$receipt_dir/kernel.log"
echo "[graph-action-scope] receipt: $receipt_dir"
echo '[graph-action-scope] PASS (admitted reference safety, exact execution-prefix receipts and both failure causes; no assumptions; effect restoration, surface, inference, authority binding, runtime and cost OPEN)'
