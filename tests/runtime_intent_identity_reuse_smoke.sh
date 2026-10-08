#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CC_BIN="${CC:-cc}"
for lane in inline linked; do
    defs=()
    if [[ "$lane" == linked ]]; then defs+=(-DPGY_INTENT_IDENTITY_LINKED); fi
    for trace in 0 1; do
        "$CC_BIN" -std=gnu11 -D_POSIX_C_SOURCE=200809L -O1 -ffunction-sections -fdata-sections \
            -DPGY_INTENT_OBSERVABILITY_ENABLED="$trace" "${defs[@]}" \
            -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            "$ROOT_DIR/tests/runtime_intent_identity_reuse_probe.c" \
            -Wl,--gc-sections -pthread -lm -o "$WORK_DIR/$lane-$trace"
        "$WORK_DIR/$lane-$trace"
        echo "[intent-identity-reuse] $lane trace=$trace PASS"
    done
done
