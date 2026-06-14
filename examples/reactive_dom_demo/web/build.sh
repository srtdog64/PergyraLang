#!/usr/bin/env bash
#
# rung-2 web build kit. Produces a browser-running WASM build of the Pergyra
# reactive demo. Run in an environment that has emscripten (emcc) on PATH;
# this is the substrate step the build sandbox cannot do (no wasm toolchain).
#
# What it does:
#   1. compile the Pergyra reactive demo to self-contained C via pgy
#   2. compile that C to WASM + JS + HTML via emcc
#   3. open demo.html in a browser to see the reactive logic run in WASM
#
# The reactive core (CounterApp_Bump and the _sync propagation functions) is
# pure Pergyra-compiled logic and needs no porting. emcc supplies the libc
# surface (printf, malloc) the runtime calls, so no manual shim is required for
# the console version below. The interactive click-to-Bump version is noted at
# the end.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PGY="${PGY:-$ROOT_DIR/bin/pgy}"
DEMO="$ROOT_DIR/examples/reactive_dom_demo/rung1.pgy"

command -v emcc >/dev/null 2>&1 || {
    echo "emcc not found. Install emscripten (https://emscripten.org) and re-run." >&2
    exit 1
}
[ -x "$PGY" ] || { echo "pgy not built at $PGY (run make pgy)." >&2; exit 1; }

echo "[rung-2] pgy: reactive demo -> C"
"$PGY" "$DEMO" --emit-c -o "$OUT_DIR/demo.c"

echo "[rung-2] emcc: C -> WASM + JS + HTML"
emcc "$OUT_DIR/demo.c" \
    -O2 \
    -o "$OUT_DIR/demo.html"

echo "[rung-2] done. Open $OUT_DIR/demo.html in a browser."
echo "The page runs the Pergyra reactive logic in WASM; the console shows the"
echo "DOM snapshots count: 0, 1, 2 produced by Bump() driving the projection."
echo
echo "Interactive version (click -> Bump -> live DOM): restructure rung1.pgy to"
echo "expose WebInit/WebBump/WebRender as exported entry points, build with"
echo "  emcc ... -sEXPORTED_FUNCTIONS=_WebInit,_WebBump,_WebRender \\"
echo "          -sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
echo "then wire host.html so a button click calls WebBump and writes the"
echo "returned render string into #app. Only pgy_log_string is DOM-bound; the"
echo "reactive functions run unchanged in WASM."
