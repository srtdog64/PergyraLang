#!/usr/bin/env bash
# Active hard-self-host lane: self-produced MIR and ABI facts, C as oracle only.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
BACKENDS="${PGY_SELFHOST_DRIVER_BACKENDS:-c}"
BUILD_ROOT="${PGY_SELFHOST_MIR_ABI_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_abi_first}"

mkdir -p "$BUILD_ROOT"

"${BASH}" "$ROOT_DIR/tests/self_host_hard_contract_smoke.sh"
PGY_BIN="$PGY" PGY_SELFHOST_BUILD_DIR="$BUILD_ROOT/abi_layout" "${BASH}" \
    "$ROOT_DIR/tests/self_hosted/parity/abi_layout_row_manifest_parity.sh"
PGY_BIN="$PGY" PGY_SELFHOST_BUILD_DIR="$BUILD_ROOT/runtime_call_abi" "${BASH}" \
    "$ROOT_DIR/tests/self_hosted/parity/runtime_call_abi_row_manifest_parity.sh"

PGY_BIN="$PGY" \
PGY_SELFHOST_BUILD_DIR="$BUILD_ROOT/driver_rung2" \
PGY_SELFHOST_DRIVER_BACKENDS="$BACKENDS" \
PGY_SELFHOST_DRIVER_MIR_FIXTURE_FILTER="device_slot_machine_layer,device_slot_remote,device_slot_routine" \
    "${BASH}" \
    "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_body_parity.sh"

echo "[self-host-mir-abi-first] self MIR/ABI producer lane ok: backends=$BACKENDS fixtures=3"
