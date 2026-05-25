#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY_BIN" != *.exe && -x "${PGY_BIN}.exe" ]]; then
    PGY_BIN="${PGY_BIN}.exe"
fi
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-dogfood-webgl.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
SOURCE_FILE="$ROOT_DIR/examples/wasm_hello/main.pgy"

# This gate validates the beta dogfood host bridge only. It must not become a
# stable WebGL language-surface gate; renderer APIs live under post-beta
# module ecosystem work such as pgy.render.webgl.

if [[ ! -x "$PGY_BIN" ]]; then
    echo "[dogfood-webgl] missing compiler binary: $PGY_BIN" >&2
    exit 1
fi
if ! "$PGY_BIN" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    echo "[dogfood-webgl] compiler binary is not runnable: $PGY_BIN" >&2
    cat "$WORK_DIR/pgy-help.err" >&2
    exit 1
fi

if [[ ! -f "$SOURCE_FILE" ]]; then
    echo "[dogfood-webgl] missing dogfood example: $SOURCE_FILE" >&2
    exit 1
fi

if ! "$PGY_BIN" "$(pgy_path_for_compiler "$PGY_BIN" "$SOURCE_FILE")" --emit-c \
    -o "$(pgy_path_for_compiler "$PGY_BIN" "$WORK_DIR/dogfood_webgl.c")" >"$WORK_DIR/emit.out" 2>"$WORK_DIR/emit.err"; then
    echo "[dogfood-webgl] C emit failed" >&2
    cat "$WORK_DIR/emit.out" >&2
    cat "$WORK_DIR/emit.err" >&2
    exit 1
fi

for term in \
    'void pgy_host_log' \
    'void pgy_webgl_frame'; do
    if ! grep -Fq "$term" "$WORK_DIR/dogfood_webgl.c"; then
        echo "[dogfood-webgl] emitted C missing bridge term: $term" >&2
        sed -n '1,160p' "$WORK_DIR/dogfood_webgl.c" >&2
        exit 1
    fi
done

if ! grep -Eq 'pgy_host_log\(1\)|pgy_host_log\([^;]*1' "$WORK_DIR/dogfood_webgl.c"; then
    echo "[dogfood-webgl] emitted C missing host log call" >&2
    sed -n '1,200p' "$WORK_DIR/dogfood_webgl.c" >&2
    exit 1
fi

if ! grep -Eq 'pgy_webgl_frame\(0\)|pgy_webgl_frame\([^;]*0' "$WORK_DIR/dogfood_webgl.c"; then
    echo "[dogfood-webgl] emitted C missing frame callback call" >&2
    sed -n '1,200p' "$WORK_DIR/dogfood_webgl.c" >&2
    exit 1
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "[dogfood-webgl] C host bridge emitted; emcc not found, skipping wasm link"
    exit 0
fi

if ! emcc "$WORK_DIR/dogfood_webgl.c" \
    -I"$ROOT_DIR/src" \
    -I"$ROOT_DIR/src/runtime" \
    -sERROR_ON_UNDEFINED_SYMBOLS=0 \
    -sENVIRONMENT=web \
    -o "$WORK_DIR/dogfood_webgl.html" \
    >"$WORK_DIR/emcc.out" 2>"$WORK_DIR/emcc.err"; then
    echo "[dogfood-webgl] emcc link failed" >&2
    cat "$WORK_DIR/emcc.out" >&2
    cat "$WORK_DIR/emcc.err" >&2
    exit 1
fi

if [[ ! -f "$WORK_DIR/dogfood_webgl.html" || ! -f "$WORK_DIR/dogfood_webgl.js" ]]; then
    echo "[dogfood-webgl] emcc did not produce expected html/js shell" >&2
    find "$WORK_DIR" -maxdepth 1 -type f -print >&2
    exit 1
fi

echo "[dogfood-webgl] C host bridge and optional Emscripten wasm shell ok"
