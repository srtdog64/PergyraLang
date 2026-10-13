#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
if command -v rocq >/dev/null 2>&1; then
    pgy_rocq_check_isolated "$ROOT_DIR/docs/semantics/proofs/ProofSpine.v"
    echo "[proof-spine] Rocq spine freshly kernel-checked"
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
