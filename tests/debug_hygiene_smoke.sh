#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[debug-hygiene] $*" >&2
    exit 1
}

debug_defines="$(
    grep -RInE '^[[:space:]]*#[[:space:]]*define[[:space:]]+PGY_DEBUG([[:space:]]|$)' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
debug_defines="$(
    printf '%s\n' "$debug_defines" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' \
        | grep -v 'pgy_runtime_intent_trace_inline.h:8:' || true
)"
if [ -n "$debug_defines" ]; then
    printf '%s\n' "$debug_defines" >&2
    fail "production sources must not define PGY_DEBUG by default"
fi

if grep -RInE '(^|[[:space:]])-DPGY_DEBUG([[:space:]]|$)' "$ROOT_DIR/Makefile" "$ROOT_DIR"/mk 2>/dev/null; then
    fail "release build flags must not enable PGY_DEBUG by default"
fi

if grep -RInE 'PGY_DEBUG_LLVM_(DETAIL|STAGE|VERIFY)=1|PGY_DEBUG_PIPELINE_STAGE=1|PGY_DEBUG_MIR_LOWER=1' \
    "$ROOT_DIR/Makefile" "$ROOT_DIR/src" 2>/dev/null; then
    fail "developer debug traces must be opt-in environment variables, not default build settings"
fi

require_env_gate() {
    local name="$1"
    if ! grep -RIn "getenv(\"$name\")" "$ROOT_DIR/src" >/dev/null 2>&1; then
        fail "expected debug env gate missing: $name"
    fi
}

require_env_gate PGY_DEBUG_LLVM_STAGE
require_env_gate PGY_DEBUG_LLVM_DETAIL
require_env_gate PGY_DEBUG_LLVM_VERIFY
require_env_gate PGY_DEBUG_PIPELINE_STAGE
require_env_gate PGY_DEBUG_MIR_LOWER

detail_getenv_sites="$(
    grep -RIn 'getenv("PGY_DEBUG_LLVM_DETAIL")' "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
detail_getenv_count="$(printf '%s\n' "$detail_getenv_sites" | grep -c . || true)"
if [ "$detail_getenv_count" -ne 1 ]; then
    printf '%s\n' "$detail_getenv_sites" >&2
    fail "PGY_DEBUG_LLVM_DETAIL must be read through one LLVM debug-detail helper"
fi
if ! printf '%s\n' "$detail_getenv_sites" \
    | grep -Fq 'src/codegen/llvm_debug_flags.c'; then
    printf '%s\n' "$detail_getenv_sites" >&2
    fail "PGY_DEBUG_LLVM_DETAIL env read must live in llvm_debug_flags.c"
fi

for name in PGY_DEBUG_LLVM_STAGE PGY_DEBUG_LLVM_VERIFY; do
    sites="$(
        grep -RIn "getenv(\"$name\")" "$ROOT_DIR/src/codegen" \
            --include='*.c' --include='*.h' || true
    )"
    count="$(printf '%s\n' "$sites" | grep -c . || true)"
    if [ "$count" -ne 1 ]; then
        printf '%s\n' "$sites" >&2
        fail "$name must be read through one LLVM debug helper"
    fi
    if ! printf '%s\n' "$sites" | grep -Fq 'src/codegen/llvm_debug_flags.c'; then
        printf '%s\n' "$sites" >&2
        fail "$name env read must live in llvm_debug_flags.c"
    fi
done

echo "[debug-hygiene] debug traces are opt-in and PGY_DEBUG is not release-defaulted"
