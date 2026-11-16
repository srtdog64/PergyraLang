#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(pwd)"
if [[ -n "${PGY_BIN:-}" && -x "${PGY_BIN}" ]]; then
    echo "[self-host-parity:machine-layer-mir] delegated live producer gate"
    # The gate takes the MIR lowering tool only from the shared build.
    PGY_BIN="${PGY_BIN}" "$ROOT_DIR/tests/self_hosted/mir_lower_tool_build.sh" \
        "$ROOT_DIR/.tmp/self_hosted/mir_lower_tool"
    PGY_MIR_LOWER_TOOL_DIR="$ROOT_DIR/.tmp/self_hosted/mir_lower_tool" \
        PGY_BIN="${PGY_BIN}" "$ROOT_DIR/tests/self_hosted/mir_machine_layer_smoke.sh"
else
    echo "[self-host-parity:machine-layer-mir] SKIP missing compiler binary"
fi
