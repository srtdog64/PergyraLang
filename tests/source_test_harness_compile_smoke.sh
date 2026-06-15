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

require_term() {
    local file="$1"
    local term="$2"
    if ! grep -Fq "$term" "$file"; then
        echo "[source-test-harness-compile] missing harness ownership term in $file: $term" >&2
        exit 1
    fi
}

reject_regex() {
    local file="$1"
    local regex="$2"
    local label="$3"
    if grep -Eq "$regex" "$file"; then
        echo "[source-test-harness-compile] forbidden harness ownership pattern in $file: $label" >&2
        grep -En "$regex" "$file" >&2 || true
        exit 1
    fi
}

transpile_helpers="$ROOT_DIR/src/tests/transpile/test_transpile_helpers_1.cases.h"
require_term "$transpile_helpers" "make_func_param"
require_term "$transpile_helpers" "make_class_field"

for fixture in \
    "$ROOT_DIR/src/tests/transpile/test_transpile_program_part_a.cases.h" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_program_part_b.cases.h" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_stdlib_part_b_1.cases.h" \
    "$ROOT_DIR/src/tests/transpile/test_transpile_stdlib_part_b_2.cases.h"; do
    reject_regex "$fixture" 'FuncParam [A-Za-z_, ]+;' 'stack FuncParam in AST-owned program fixture'
    reject_regex "$fixture" 'ClassField [A-Za-z_, ]+;' 'stack ClassField in AST-owned program fixture'
done

probe_src="$OUT_DIR/compiler-probe.c"
probe_obj="$OUT_DIR/compiler-probe.o"
printf '%s\n' 'int main(void) { return 0; }' > "$probe_src"
if ! "$CC_BIN" -std=c11 -c "$probe_src" -o "$probe_obj" >/dev/null 2>&1; then
    echo "[source-test-harness-compile] C compiler is not usable from this shell; skipping harness compile smoke"
    exit 0
fi

test_sources=()
while IFS= read -r src; do
    test_sources+=("$src")
done < <(find "$ROOT_DIR/src" -maxdepth 1 -type f -name 'test_*.c' | sort)

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
        -DPGY_PROJECT_ROOT="\"$ROOT_DIR\"" \
        -DPGY_SRC_DIR="\"$ROOT_DIR/src\"" \
        -DPGY_RUNTIME_DIR="\"$ROOT_DIR/src/runtime\"" \
        -DPGY_RUNTIME_LIB_C="\"$ROOT_DIR/src/runtime/pgy_runtime_lib.c\"" \
        -I"$ROOT_DIR/src" \
        -c "$src" \
        -o "$OUT_DIR/${base}.o"
done

echo "[source-test-harness-compile] compiled ${#test_sources[@]} src/test_*.c harnesses"
