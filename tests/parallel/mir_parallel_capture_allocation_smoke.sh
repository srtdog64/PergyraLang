#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PGY_TEST_BUILD_DIR:-$ROOT_DIR/.tmp/mir-parallel-capture-allocation}"
CC_BIN="${CC:-cc}"

mkdir -p "$BUILD_DIR"

COMMON_FLAGS=(
    -std=c11
    -Wall
    -Wextra
    -Werror
    -g
    -O1
    -fno-omit-frame-pointer
    -fsanitize=address,undefined
    -I"$ROOT_DIR/src"
)

"$CC_BIN" "${COMMON_FLAGS[@]}" -Dcalloc=pgy_test_calloc \
    -c "$ROOT_DIR/src/compiler/mir_parallel_capture_facts.c" \
    -o "$BUILD_DIR/mir_parallel_capture_facts.o"
"$CC_BIN" "${COMMON_FLAGS[@]}" \
    -c "$ROOT_DIR/tests/parallel/mir_parallel_capture_allocation_unit.c" \
    -o "$BUILD_DIR/mir_parallel_capture_allocation_unit.o"
"$CC_BIN" "${COMMON_FLAGS[@]}" \
    "$BUILD_DIR/mir_parallel_capture_facts.o" \
    "$BUILD_DIR/mir_parallel_capture_allocation_unit.o" \
    -o "$BUILD_DIR/mir_parallel_capture_allocation_unit"

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
    "$BUILD_DIR/mir_parallel_capture_allocation_unit"
