#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PROBE="${PGY_VERIFIED_PROJECTION_PLAN_PROBE:-}"
if [[ -z "$PROBE" || ! -x "$PROBE" ]]; then
    echo "[verified-projection-plan] missing compiled probe: $PROBE" >&2
    exit 1
fi
"$PROBE"

echo "[verified-projection-plan] projection plan probe passed"
