#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
if command -v rocq >/dev/null 2>&1; then
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
    for module in ProofSpine OwnershipCleanCore OwnershipCleanComposition \
                  OwnershipCleanReadOnly OwnershipCleanExits; do
        cp "$ROOT_DIR/docs/semantics/proofs/$module.v" "$work/"
    done
    cp "$ROOT_DIR/tests/coq/OwnershipCleanupSpineAudit.v" "$work/"
    PGY_COQ_PROOFS_DIR="$work" PGY_COQ_EXPECTED_AXIOMS='' \
        bash "$ROOT_DIR/tests/coq_kernel_check.sh"
    echo "[proof-spine] concrete cleanup contracts and missing-contract refusals kernel-checked"
elif [ "${PGY_ALLOW_MISSING_COQ:-0}" = "1" ]; then
    echo "[proof-spine] Rocq spine DECLARED SKIP -- no Rocq," \
         "PGY_ALLOW_MISSING_COQ=1; the spine was NOT checked on this runner."
else
    echo "[proof-spine] FAIL -- no Rocq; the" \
         "spine would go unchecked. Install pinned Rocq, or set" \
         "PGY_ALLOW_MISSING_COQ=1 to declare the skip." >&2
    exit 1
fi

echo "[proof-spine] proof pack spine ok"
