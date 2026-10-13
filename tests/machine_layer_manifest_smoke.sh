#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="${PGY_MACHINE_LAYER_MANIFEST_PROBE:-$ROOT_DIR/bin/test_machine_layer_manifest}"

if [[ ! -x "$PROBE" && -x "${PROBE}.exe" ]]; then
    PROBE="${PROBE}.exe"
fi
if [[ ! -x "$PROBE" ]]; then
    echo "[machine-layer-manifest] missing probe: $PROBE" >&2
    exit 1
fi

output="$($PROBE)"
grep -Fq -- "C/LLVM distinct projections, physical declaration provider, runtime fingerprint/mapping bind" <<<"$output" || {
    echo "[machine-layer-manifest] probe output missing distinct projection evidence" >&2
    exit 1
}

echo "[machine-layer-manifest] C and LLVM consume distinct physical projections with checked physical declaration and fail-closed owner validation"
