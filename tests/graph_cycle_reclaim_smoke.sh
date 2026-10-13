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
