#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROOF="$ROOT_DIR/docs/semantics/proofs/MachineLayerCore.v"

fail() {
    echo "[machine-layer] $*" >&2
    exit 1
}

[[ -f "$PROOF" ]] || fail "missing MachineLayerCore.v"

source "$ROOT_DIR/scripts/rocq_toolchain_owner.sh"
if command -v rocq >/dev/null 2>&1; then
    pgy_rocq_check_isolated "$PROOF"
elif [[ "${PGY_ALLOW_MISSING_COQ:-0}" == "1" ]]; then
    echo "[machine-layer] proof DECLARED SKIP -- no Rocq, PGY_ALLOW_MISSING_COQ=1; the model was NOT checked on this runner."
else
    fail "pinned Rocq is required for the focused machine-layer gate; set PGY_ALLOW_MISSING_COQ=1 only when a dedicated proof job checks the same corpus"
fi

echo "[machine-layer] GREEN -- address evidence, machine state transition, and positive/negative witnesses"
