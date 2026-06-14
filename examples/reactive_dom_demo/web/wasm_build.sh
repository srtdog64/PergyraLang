#!/usr/bin/env bash
#
# rung-2, verified path. Compiles the Pergyra reactive demo to real
# WebAssembly using the zig-bundled clang (pip install ziglang) and runs it on
# node's WASI. This path was verified end to end: the reactive logic runs in
# WASM and prints the same DOM snapshots as the native build, count 0/1/2.
#
# Why zig: it bundles clang + lld with a wasm32 target and wasi-libc, and it is
# a single pip install, so no system toolchain is needed.
#
# The default Pergyra runtime header pulls in ucontext.h for the coroutine
# scheduler, which wasm32-wasi does not provide. The reactive demo never calls
# the scheduler, so a tiny ucontext shim (ucontext_shim.h, exposed as
# ucontext.h on the include path) satisfies the include and the unused stubs
# are dead-code eliminated.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PGY="${PGY:-$ROOT_DIR/bin/pgy}"
DEMO="${1:-$ROOT_DIR/examples/reactive_dom_demo/rung1.pgy}"

python3 -m ziglang version >/dev/null 2>&1 || {
    echo "ziglang not found. Run: pip install ziglang --break-system-packages" >&2
    exit 1
}
[ -x "$PGY" ] || { echo "pgy not built at $PGY (run make pgy)." >&2; exit 1; }

SHIM_DIR="$(mktemp -d)"
cp "$OUT_DIR/ucontext_shim.h" "$SHIM_DIR/ucontext.h"

echo "[rung-2] pgy: reactive demo -> C"
"$PGY" "$DEMO" --emit-c -o "$OUT_DIR/demo.c"

echo "[rung-2] zig cc: C -> wasm32-wasi"
python3 -m ziglang cc -target wasm32-wasi -O2 \
    -I "$SHIM_DIR" -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" -I "$ROOT_DIR/third_party" \
    -DPGY_PROJECT_ROOT="\"$ROOT_DIR\"" \
    "$OUT_DIR/demo.c" -o "$OUT_DIR/demo.wasm"

echo "[rung-2] node WASI: run the reactive logic in WebAssembly"
node --no-warnings "$OUT_DIR/run_wasi.mjs" "$OUT_DIR/demo.wasm"
