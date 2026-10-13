#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
PROBE="$ROOT_DIR/src/self_hosted/tools/binary_projection_preflight_probe/main.pgy"

test -f "$PROBE"

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
