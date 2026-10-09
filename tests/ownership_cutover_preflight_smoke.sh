#!/usr/bin/env bash
# Bounded importing contract/proof check, not production ownership closure.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
pgy_rocq_require
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
for model in OwnershipCleanCore OwnershipCleanExits OwnershipCleanCallRecovery \
             OwnershipTeardown OwnershipTeardownAuthority OwnershipCleanViews \
             OwnershipCleanCallLowering OwnershipCleanViewScope; do
    cp "$ROOT_DIR/docs/semantics/proofs/$model.v" "$work/"
done
cp "$ROOT_DIR/tests/coq/OwnershipCutoverPreflightAudit.v" "$work/"
PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh"
echo '[ownership-preflight] PASS (importing model only; production refinement OPEN)'
