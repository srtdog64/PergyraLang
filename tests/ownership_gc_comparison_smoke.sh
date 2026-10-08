#!/usr/bin/env bash
# Bounded formal comparison only; no production collector or timing claim.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
for input in \
    docs/semantics/proofs/OwnershipCleanCore.v \
    docs/semantics/proofs/OwnershipCleanGCComparison.v \
    tests/coq/OwnershipCleanGCComparisonAudit.v; do
    cp "$ROOT_DIR/$input" "$work/"
done
receipt_dir="$ROOT_DIR/.tmp/ownership-cleanup/gc-comparison"
mkdir -p "$receipt_dir"
PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh" >"$receipt_dir/kernel.log" 2>&1 || {
        cat "$receipt_dir/kernel.log" >&2; exit 1;
    }
tail -n 1 "$receipt_dir/kernel.log"
echo '[ownership-gc-comparison] PASS (typed comparisons and counterexamples; zero axioms; abstract costs only)'
