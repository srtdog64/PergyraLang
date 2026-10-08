#!/usr/bin/env bash
# Typed allocation/frame + retirement composition; no production or cost claim.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
for input in \
    docs/semantics/proofs/OwnershipCleanCore.v \
    docs/semantics/proofs/SlotCalculus.v \
    docs/semantics/proofs/OwnershipGraphLinks.v \
    docs/semantics/proofs/AssumptionBudget.v \
    tests/coq/MemoryBoundaryCompositionAudit.v; do
    cp "$ROOT_DIR/$input" "$work/"
done
receipt_dir="$ROOT_DIR/.tmp/memory-boundary-composition"
mkdir -p "$receipt_dir"
PGY_COQ_PROOFS_DIR="$work" bash "$ROOT_DIR/tests/coq_kernel_check.sh" \
    >"$receipt_dir/kernel.log" 2>&1 || {
        cat "$receipt_dir/kernel.log" >&2; exit 1;
    }
tail -n 1 "$receipt_dir/kernel.log"
echo '[memory-boundary-composition] PASS (allocation/growth frame + canonical/graph/Slot retirement; existing two approved abstractions; production composition OPEN)'
