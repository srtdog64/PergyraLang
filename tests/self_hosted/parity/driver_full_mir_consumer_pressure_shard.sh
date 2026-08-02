#!/usr/bin/env bash
# Focused observed pressure shard for the already-built Pergyra MIR consumer.
# It reuses a verified full MIR artifact and reports the last reached owner;
# parser, source-to-MIR, codegen-seed, and native-oracle work stay outside it.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD_DIR="${PGY_SELFHOST_DRIVER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/bootstrap}"
DRIVER_SEED="$BUILD_DIR/driver_seed.exe"
INPUT="${PGY_SELFHOST_DRIVER_MIR_INPUT:-$BUILD_DIR/driver_source.focused.mir.json}"
OUTPUT="${PGY_SELFHOST_DRIVER_GEN2_OUTPUT:-$BUILD_DIR/driver_gen2.focused.c}"

fail() {
    echo "[self-host-driver-full-mir-consumer-shard] $*" >&2
    exit 1
}

[[ "${PGY_BUILD_PRESSURE_ACTIVE:-0}" == "1" ]] ||
    fail "pressure shard requires measure_build_pressure.ps1"
[[ -x "$DRIVER_SEED" ]] || fail "missing runnable driver seed: $DRIVER_SEED"
[[ -s "$INPUT" ]] || fail "missing full MIR input: $INPUT"

case "$INPUT" in
    "$ROOT_DIR"/*) INPUT_REL="${INPUT#"$ROOT_DIR"/}" ;;
    *) fail "full MIR input must remain under repository root: $INPUT" ;;
esac
case "$OUTPUT" in
    "$ROOT_DIR"/*) OUTPUT_REL="${OUTPUT#"$ROOT_DIR"/}" ;;
    *) fail "gen2 output must remain under repository root: $OUTPUT" ;;
esac

cd "$ROOT_DIR"
"$DRIVER_SEED" \
    --mir-json \
    "$INPUT_REL" \
    --observe-mir-consumer-stages \
    -o "$OUTPUT_REL"
