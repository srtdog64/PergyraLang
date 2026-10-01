#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
source tests/pgy_binary_path_helpers.sh
pgy_prepend_windows_runtime_paths
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/file-read-error.XXXXXX)"
CC_BIN="${CC:-gcc}"

for variant in inline exported; do
    flags=()
    [[ "$variant" == exported ]] && flags+=(-DPGY_TEST_EXPORT_RUNTIME)
    "$CC_BIN" -std=c11 -O0 -Wall -Wextra -Werror=implicit-function-declaration \
        "${flags[@]}" -Isrc tests/file_read_error_runtime.c \
        -o "$WORK/$variant.exe" -pthread -lm
    "$WORK/$variant.exe" "$WORK/$variant.txt" \
        >"$WORK/$variant.out" 2>"$WORK/$variant.err"
    grep -Fxq 'file read error runtime: ok' "$WORK/$variant.out"
    [[ ! -s "$WORK/$variant.err" ]]
done
cmp "$WORK/inline.out" "$WORK/exported.out"
echo "[file-read-error] inline/exported success, EOF, read failure, invalid handle: PASS ($WORK)"
