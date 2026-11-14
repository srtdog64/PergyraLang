#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(pwd)"
if [[ -n "${PGY_BIN:-}" && -x "${PGY_BIN}" ]]; then
    echo "[self-host-parity:machine-layer-rir] delegated live producer gate"
    PGY_BIN="${PGY_BIN}" "$ROOT_DIR/tests/self_hosted/mir_machine_layer_smoke.sh"
else
    echo "[self-host-parity:machine-layer-rir] SKIP missing compiler binary"
fi
