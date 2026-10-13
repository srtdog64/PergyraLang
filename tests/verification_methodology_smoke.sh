#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
if command -v rocq >/dev/null 2>&1; then
    pgy_rocq_check_isolated "$ROOT_DIR/docs/semantics/proofs/VerificationMethodology.v"
    echo "[verification-methodology] Rocq model freshly kernel-checked"
elif [ "${PGY_ALLOW_MISSING_COQ:-0}" = "1" ]; then
    echo "[verification-methodology] Rocq model DECLARED SKIP -- no Rocq," \
         "PGY_ALLOW_MISSING_COQ=1; the model was NOT checked here."
else
    echo "[verification-methodology] FAIL -- no Rocq;" \
         "the model would go unchecked. Install pinned Rocq, or set" \
         "PGY_ALLOW_MISSING_COQ=1 to declare the skip." >&2
    exit 1
fi

echo "[verification-methodology] methodology gate ok"
