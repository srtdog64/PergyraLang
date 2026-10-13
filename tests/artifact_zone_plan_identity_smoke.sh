#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="${PGY_VERIFIED_PROJECTION_PLAN_PROBE:-$ROOT_DIR/build/verified_projection_plan_probe.exe}"

if [[ ! -f "$PROBE" ]]; then
    echo "[artifact-zone] verified projection probe is missing: $PROBE" >&2
    exit 1
fi

probe_output="$($PROBE)"
grep -Fq "OBS0 erase and OBS1 materialize rows verified" <<<"$probe_output"
echo "[artifact-zone] native_C_LLVM_artifact_plan_identity: anchored revision and digest"
