#!/usr/bin/env bash
#
# WASM backend parity gate.
#
# Proves the web-distribution path: a Pergyra program compiled through the
# C backend to wasm32-wasi (via the zig-bundled clang) and run on node's WASI
# produces byte-identical stdout to the native C build. This is the
# "runs on every device, arch-independent" coverage for the killer use case
# (the web dungeon crawler) -- a browser/Android-browser runs the same wasm.
#
#   pgy fixture --backend=c -> native run-stdout
#   pgy fixture --emit-c | zig cc -target wasm32-wasi | node WASI -> wasm run-stdout
#   must be equal.
#
# Toolchain (zig via `pip install ziglang`, node with WASI) is optional: the
# gate SKIPs cleanly when it is absent, so it never blocks a machine that
# cannot build wasm, while still catching regressions where it can.

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[wasm-parity] SKIP missing compiler binary: $PGY"
    exit 0
fi

# A python that has the ziglang module (bundles clang+lld+wasi-libc for wasm32).
PYTHON=""
for cand in python3 python py; do
    if command -v "$cand" >/dev/null 2>&1 && "$cand" -m ziglang version >/dev/null 2>&1; then
        PYTHON="$cand"
        break
    fi
done
if [[ -z "$PYTHON" ]]; then
    echo "[wasm-parity] SKIP ziglang not available (pip install ziglang)"
    exit 0
fi
if ! command -v node >/dev/null 2>&1; then
    echo "[wasm-parity] SKIP node not on PATH"
    exit 0
fi
if ! node -e "require('node:wasi')" >/dev/null 2>&1; then
    echo "[wasm-parity] SKIP node lacks the WASI module"
    exit 0
fi

RUNNER="$ROOT_DIR/examples/reactive_dom_demo/web/run_wasi.mjs"
SHIM_SRC="$ROOT_DIR/examples/reactive_dom_demo/web/ucontext_shim.h"
if [[ ! -f "$RUNNER" || ! -f "$SHIM_SRC" ]]; then
    echo "[wasm-parity] SKIP missing wasi runner/shim under examples/reactive_dom_demo/web" >&2
    exit 0
fi

B="$ROOT_DIR/.tmp/wasm_parity"
rm -rf "$B"
mkdir -p "$B/shim"
# wasm32-wasi has no ucontext.h; the runtime header includes it, so shim it.
cp "$SHIM_SRC" "$B/shim/ucontext.h"

# Deterministic fixtures. These also exercise recent surface work (collection
# literals, control flow) to confirm it survives the wasm lowering.
emit_fixture() {
    case "$1" in
    arith) cat <<'EOF'
func Main() -> Void {
    let a: Int = 21;
    Log(ToString(a + a));
    Log(Concat("wasm", "-ok"));
}
EOF
        ;;
    control_flow) cat <<'EOF'
func Main() -> Void {
    let mut i: Int = 0;
    while i < 3 {
        if i > 0 { Log(ToString(i)); }
        i = i + 1;
    }
}
EOF
        ;;
    collections) cat <<'EOF'
func Main() -> Void {
    let s: Set<Int> = {1, 2, 2, 3};
    Log(ToString(SetSize(s)));
    let l: List<Int> = [10, 20, 30];
    Log(ToString(ListGet(l, 1)));
}
EOF
        ;;
    esac
}

FIXTURES=(arith control_flow collections)
pass=0
for base in "${FIXTURES[@]}"; do
    src="$B/$base.pgy"
    emit_fixture "$base" > "$src"

    # Native C build.
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$src")" \
            --backend=c -o "$(pgy_path_for_compiler "$PGY" "$B/${base}_native.exe")" \
            >/dev/null 2>&1); then
        echo "[wasm-parity] $base: native C build failed" >&2
        exit 1
    fi
    native_out="$(cd "$ROOT_DIR" && "$B/${base}_native.exe" 2>/dev/null | tr -d '\r')"

    # wasm build: pgy --emit-c, then zig cc to wasm32-wasi.
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$src")" \
            --emit-c -o "$(pgy_path_for_compiler "$PGY" "$B/$base.c")" >/dev/null 2>&1); then
        echo "[wasm-parity] $base: --emit-c failed" >&2
        exit 1
    fi
    if ! (cd "$ROOT_DIR" && "$PYTHON" -m ziglang cc -target wasm32-wasi -O2 \
            -Wno-shift-count-overflow \
            -I "$B/shim" -I "$ROOT_DIR/src" -I "$ROOT_DIR/src/runtime" \
            -I "$ROOT_DIR/third_party" \
            -DPGY_PROJECT_ROOT="\"$ROOT_DIR\"" \
            "$B/$base.c" -o "$B/$base.wasm" >"$B/$base.zig.log" 2>&1); then
        echo "[wasm-parity] $base: zig cc -> wasm32-wasi failed" >&2
        tail -5 "$B/$base.zig.log" >&2
        exit 1
    fi
    wasm_out="$(node --no-warnings "$RUNNER" "$B/$base.wasm" 2>/dev/null | tr -d '\r')"

    if [[ -z "$native_out" || "$native_out" != "$wasm_out" ]]; then
        echo "[wasm-parity] $base: wasm run-stdout differs from native" >&2
        diff <(printf '%s' "$native_out") <(printf '%s' "$wasm_out") | head -10 >&2
        exit 1
    fi
    pass=$((pass + 1))
done

echo "[wasm-parity] ok (${pass} fixtures; pgy --emit-c | zig cc wasm32-wasi | node WASI == native C)"
