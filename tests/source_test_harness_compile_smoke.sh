#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC_BIN="${CC:-${PGY_CC:-}}"
OUT_DIR="${PGY_TEST_HARNESS_BUILD_DIR:-$ROOT_DIR/.tmp/source-test-harness-compile}"

if [[ -z "$CC_BIN" ]]; then
    if command -v cc >/dev/null 2>&1; then
        CC_BIN="$(command -v cc)"
    elif command -v gcc >/dev/null 2>&1; then
        CC_BIN="$(command -v gcc)"
    else
        echo "[source-test-harness-compile] missing C compiler" >&2
        exit 1
    fi
fi

if ! command -v "$CC_BIN" >/dev/null 2>&1; then
    echo "[source-test-harness-compile] missing C compiler: $CC_BIN" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

probe_src="$OUT_DIR/compiler-probe.c"
probe_obj="$OUT_DIR/compiler-probe.o"
printf '%s\n' 'int main(void) { return 0; }' > "$probe_src"
if ! "$CC_BIN" -std=c11 -c "$probe_src" -o "$probe_obj" >/dev/null 2>&1; then
    echo "[source-test-harness-compile] C compiler is not usable from this shell; skipping harness compile smoke"
    exit 0
fi

mapfile -t test_sources < <(
    find "$ROOT_DIR/src" -maxdepth 1 -type f -name 'test_*.c' | sort
)

if [[ "${#test_sources[@]}" -eq 0 ]]; then
    echo "[source-test-harness-compile] no src/test_*.c harnesses found" >&2
    exit 1
fi

for src in "${test_sources[@]}"; do
    base="$(basename "$src" .c)"
    "$CC_BIN" \
        -Wall -Wextra \
        -Werror=implicit-function-declaration \
        -Werror=implicit-int \
        -std=c11 -O2 -g \
        -D__USE_MINGW_ANSI_STDIO=1 \
        -I"$ROOT_DIR/src" \
        -c "$src" \
        -o "$OUT_DIR/${base}.o"
done

echo "[source-test-harness-compile] compiled ${#test_sources[@]} src/test_*.c harnesses"
