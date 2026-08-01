#!/usr/bin/env bash
# Focused pressure shard for the already-built Pergyra driver seed.
#
# This does not replace the full bootstrap gate. It reuses that gate's exact
# seed binary and test-harness manifest so MIR throughput can be falsified
# without charging parser/codegen/native compilation to every five-minute run.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD_DIR="${PGY_SELFHOST_DRIVER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/bootstrap}"
PATHS_FILE="$BUILD_DIR/driver_bootstrap_paths.txt"
DRIVER_SEED="$BUILD_DIR/driver_seed.exe"
OUTPUT="$BUILD_DIR/driver_source.focused.mir.json"

fail() {
    echo "[self-host-driver-full-mir-shard] $*" >&2
    exit 1
}

[[ "${PGY_BUILD_PRESSURE_ACTIVE:-0}" == "1" ]] ||
    fail "pressure shard requires measure_build_pressure.ps1"
[[ -x "$DRIVER_SEED" ]] || fail "missing runnable driver seed: $DRIVER_SEED"
[[ -f "$PATHS_FILE" ]] || fail "missing driver test-harness manifest: $PATHS_FILE"

paths=()
while IFS= read -r line; do
    [[ -n "$line" ]] || continue
    paths+=("$line")
done <"$PATHS_FILE"
[[ "${#paths[@]}" -eq 9 ]] ||
    fail "driver test-harness manifest expected 9 paths, got ${#paths[@]}"

DRIVER_SOURCE_REL="${paths[8]}"
[[ -f "$ROOT_DIR/$DRIVER_SOURCE_REL" ]] ||
    fail "missing driver source from manifest: $DRIVER_SOURCE_REL"

case "$OUTPUT" in
    "$ROOT_DIR"/*) OUTPUT_REL="${OUTPUT#"$ROOT_DIR"/}" ;;
    *) fail "focused output must remain under repository root: $OUTPUT" ;;
esac

cd "$ROOT_DIR"
"$DRIVER_SEED" \
    --emit-mir-json-verified \
    "$DRIVER_SOURCE_REL" \
    "$OUTPUT_REL" \
    --pressure-owned-full-fixpoint
