#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[memory-string-safety] $*" >&2
    exit 1
}

unsafe_calls="$(
    grep -RInE '\b(sprintf|vsprintf|strcpy|strncpy|strcat|strncat|gets)[[:space:]]*\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
unsafe_calls="$(
    printf '%s\n' "$unsafe_calls" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$unsafe_calls" ]; then
    printf '%s\n' "$unsafe_calls" >&2
    fail "production code must use project-owned bounded formatting/copy/append helpers, not unsafe C string APIs"
fi

strtok_calls="$(
    grep -RInE '\bstrtok[[:space:]]*\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
strtok_calls="$(
    printf '%s\n' "$strtok_calls" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$strtok_calls" ]; then
    printf '%s\n' "$strtok_calls" >&2
    fail "production code must not use process-global strtok tokenizer state"
fi

truncated_stack_copies="$(
    grep -RInE 'memcpy\([^;]*stack_buf,[[:space:]]*\(size_t\)len[[:space:]]*\+[[:space:]]*1\)' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
truncated_stack_copies="$(
    printf '%s\n' "$truncated_stack_copies" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$truncated_stack_copies" ]; then
    printf '%s\n' "$truncated_stack_copies" >&2
    fail "snprintf stack buffers must not be copied using the required length after truncation"
fi

unsafe_offset_accumulators="$(
    grep -RInE '\+=[[:space:]]*(\(size_t\))?snprintf\(' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
unsafe_offset_accumulators="$(
    printf '%s\n' "$unsafe_offset_accumulators" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [ -n "$unsafe_offset_accumulators" ]; then
    printf '%s\n' "$unsafe_offset_accumulators" >&2
    fail "incremental string assembly must use pergyra_str_append/pergyra_str_appendf, not raw snprintf offset accumulation"
fi

echo "[memory-string-safety] unsafe production string APIs are gated"
