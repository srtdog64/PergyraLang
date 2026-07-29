#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TICKET_OWNER="$ROOT_DIR/src/self_hosted/lib/snapshot_ticket.pgy"
PROJECTION_OWNER="$ROOT_DIR/src/self_hosted/lib/binary_projection_preflight_owner.pgy"
PROBE="$ROOT_DIR/src/self_hosted/tools/binary_projection_preflight_probe/main.pgy"

require_text() {
    local path="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$path"; then
        echo "missing required binary projection owner text: $path: $text" >&2
        exit 1
    fi
}

reject_text() {
    local path="$1"
    local text="$2"
    if grep -Fq -- "$text" "$path"; then
        echo "forbidden binary projection owner text: $path: $text" >&2
        exit 1
    fi
}

for path in "$TICKET_OWNER" "$PROJECTION_OWNER" "$PROBE"; do
    test -f "$path"
done

require_text "$TICKET_OWNER" "struct SnapshotTicket"
require_text "$TICKET_OWNER" "ticket.generation == current_generation"
require_text "$PROJECTION_OWNER" 'import "../mir_lower/abi_layout_fact_owner.pgy";'
require_text "$PROJECTION_OWNER" "MirAbiLayoutIdFromCapture(layout)"
require_text "$PROJECTION_OWNER" "ticket.endianness != observed_endianness"
require_text "$PROJECTION_OWNER" "-> Option<BinaryProjectionReceipt>"
require_text "$PROBE" "generation N ticket survived generation N+1 reuse"
require_text "$PROBE" "same-name layout with a changed offset was admitted"
require_text "$PROBE" "endianness mismatch was admitted"
require_text "$PROBE" "missing endianness was defaulted"

reject_text "$PROJECTION_OWNER" "field_offsets["
reject_text "$PROJECTION_OWNER" "target-c-default"
reject_text "$PROJECTION_OWNER" "Unchecked"
reject_text "$PROJECTION_OWNER" "RawProjection"

if [[ -n "${PGY_BIN:-}" && -x "$PGY_BIN" ]]; then
    BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/.tmp/binary_projection_preflight}"
    mkdir -p "$BUILD_DIR"
    "$PGY_BIN" "$PROBE" --backend=c -o "$BUILD_DIR/probe_c.exe"
    "$PGY_BIN" "$PROBE" --backend=llvm -o "$BUILD_DIR/probe_llvm.exe"
    c_output="$("$BUILD_DIR/probe_c.exe")"
    llvm_output="$("$BUILD_DIR/probe_llvm.exe")"
    if [[ "$c_output" != "binary projection preflight: PASS" ]]; then
        echo "unexpected C binary projection probe output: $c_output" >&2
        exit 1
    fi
    if [[ "$llvm_output" != "$c_output" ]]; then
        echo "binary projection C/LLVM output mismatch: C=$c_output LLVM=$llvm_output" >&2
        exit 1
    fi
else
    echo "binary projection preflight executable leg: SKIP (set PGY_BIN to a current executable)"
fi

echo "binary projection preflight owner smoke: PASS"
