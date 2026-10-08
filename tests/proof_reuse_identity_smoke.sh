#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
for module in IntentConflict IntentSpine SlotLifecycleCore CollectionOwnershipTransfer ForeignStringOwnership; do
    cp "$ROOT_DIR/docs/semantics/proofs/$module.v" "$WORK_DIR/"
done
cp "$ROOT_DIR/tests/coq/ReuseIdentityAudit.v" "$WORK_DIR/"
PGY_COQ_PROOFS_DIR="$WORK_DIR" PGY_COQ_EXPECTED_AXIOMS='' \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh"
echo '[proof-reuse-identity] five actual owners plus permanent typed consumer PASS'
