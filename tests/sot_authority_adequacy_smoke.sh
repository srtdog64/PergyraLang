#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROOF="docs/semantics/proofs/SoTAuthority.v"

fail() {
    echo "[sot-authority] $*" >&2
    exit 1
}

require_file() {
    [[ -f "$ROOT_DIR/$1" ]] || fail "missing $1"
}

require_file "$PROOF"

source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
if command -v rocq >/dev/null 2>&1; then
    pgy_rocq_check_isolated "$PROOF"
    echo "[sot-authority] Rocq model freshly kernel-checked"
elif [ "${PGY_ALLOW_MISSING_COQ:-0}" = "1" ]; then
    echo "[sot-authority] Rocq model DECLARED SKIP -- no Rocq," \
         "PGY_ALLOW_MISSING_COQ=1; the model was NOT checked on this runner."
else
    echo "[sot-authority] FAIL -- no Rocq; the" \
         "model would go unchecked." >&2
    echo "  Install pinned Rocq, or set PGY_ALLOW_MISSING_COQ=1 to declare the skip." >&2
    exit 1
fi

echo "[sot-authority] live owner/consumer binding and negative mutations ok"
